#include "layer_shell.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QSocketNotifier>
#include <QTimer>
#include <QtGlobal>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "atlas.h"
#include "clock.h"

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>

namespace {

constexpr uint64_t kMaxFrameBytes = 256ull * 1024ull * 1024ull;

class LayerShellProof {
public:
    LayerShellProof(QGuiApplication &app, Clock &clock)
        : app_(app), clock_(clock), renderWindow_(&renderControl_) {}
    ~LayerShellProof() { cleanup(); }

    bool start() {
        display_ = wl_display_connect(nullptr);
        if (!display_) return fail("could not connect to WAYLAND_DISPLAY");

        registry_ = wl_display_get_registry(display_);
        static const wl_registry_listener listener = {
            [](void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
                auto *self = static_cast<LayerShellProof *>(data);
                if (std::strcmp(interface, wl_compositor_interface.name) == 0 && version >= 4)
                    self->compositor_ = static_cast<wl_compositor *>(wl_registry_bind(registry, name, &wl_compositor_interface, 4));
                else if (std::strcmp(interface, wl_shm_interface.name) == 0)
                    self->shm_ = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
                else if (std::strcmp(interface, wl_output_interface.name) == 0 && !self->output_)
                    self->output_ = static_cast<wl_output *>(wl_registry_bind(
                        registry, name, &wl_output_interface, qMin(version, 4u)));
                else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0 && version >= 4)
                    self->layerShell_ = static_cast<zwlr_layer_shell_v1 *>(wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 4));
            },
            [](void *, wl_registry *, uint32_t) {}
        };
        wl_registry_add_listener(registry_, &listener, this);
        if (wl_display_roundtrip(display_) < 0)
            return fail("Wayland registry roundtrip failed");
        if (!compositor_ || !shm_ || !output_ || !layerShell_)
            return fail("compositor lacks wl_compositor, wl_shm, wl_output, or wlr-layer-shell v4");

        surface_ = wl_compositor_create_surface(compositor_);
        if (!surface_) return fail("could not create Wayland surface");
        layerSurface_ = zwlr_layer_shell_v1_get_layer_surface(
            layerShell_, surface_, output_, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "meridian-wallpaper-proof");
        if (!layerSurface_) return fail("could not create layer-shell surface");

        static const zwlr_layer_surface_v1_listener layerListener = {
            [](void *data, zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height) {
                auto *self = static_cast<LayerShellProof *>(data);
                self->configure(surface, serial, width, height);
            },
            [](void *data, zwlr_layer_surface_v1 *) {
                auto *self = static_cast<LayerShellProof *>(data);
                self->fail("layer-shell surface was closed");
            }
        };
        zwlr_layer_surface_v1_add_listener(layerSurface_, &layerListener, this);
        zwlr_layer_surface_v1_set_anchor(layerSurface_,
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_exclusive_zone(layerSurface_, -1);
        zwlr_layer_surface_v1_set_keyboard_interactivity(layerSurface_, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
        wl_region *empty = wl_compositor_create_region(compositor_);
        if (!empty) return fail("could not create empty input region");
        wl_surface_set_input_region(surface_, empty);
        wl_region_destroy(empty);
        // Required initial commit: no buffer may be attached before configure.
        wl_surface_commit(surface_);
        if (wl_display_roundtrip(display_) < 0)
            return fail("Wayland configure roundtrip failed");
        if (failed_) return false;
        if (!configured_) return fail("layer-shell surface did not configure");

        notifier_ = new QSocketNotifier(wl_display_get_fd(display_), QSocketNotifier::Read, &app_);
        QObject::connect(notifier_, &QSocketNotifier::activated, &app_, [this] {
            if (!display_ || wl_display_dispatch(display_) < 0) {
                fail("Wayland connection closed while dispatching events");
                app_.exit(1);
            }
            wl_display_flush(display_);
        });
        QObject::connect(&clock_, &Clock::changed, &app_, [this] {
            const qint64 minute = clock_.utc().toSecsSinceEpoch() / 60;
            if (minute == lastMinute_) return;
            lastMinute_ = minute;
            pendingRender_ = true;
            renderWhenReleased();
        });
        if (wl_display_flush(display_) < 0)
            return fail("could not flush Wayland requests");
        return true;
    }

private:
    static constexpr size_t kBufferCount = 2;

    struct BufferSlot {
        LayerShellProof *owner = nullptr;
        wl_buffer *buffer = nullptr;
        uint32_t *pixels = nullptr;
        size_t bytes = 0;
        int fd = -1;
        bool released = true;
    };

    bool fail(const char *message) {
        std::fprintf(stderr, "Meridian layer-shell proof: %s\n", message);
        std::fflush(stderr);
        failed_ = true;
        if (started_) app_.exit(1);
        return false;
    }

    void configure(zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height) {
        zwlr_layer_surface_v1_ack_configure(surface, serial);
        if (failed_) return;
        if (width == 0 || height == 0 || width > 16384 || height > 16384) {
            fail("compositor supplied an invalid layer size");
            return;
        }
        if (configured_) return;
        width_ = width;
        height_ = height;
        if (!allocateBuffers()) {
            fail("could not allocate wl_shm buffers");
            return;
        }
        if (!renderScene(slots_[0])) {
            fail("could not render MeridianScene into wl_shm buffer");
            return;
        }
        lastMinute_ = clock_.utc().toSecsSinceEpoch() / 60;
        pendingRender_ = false;
        slots_[0].released = false;
        wl_surface_attach(surface_, slots_[0].buffer, 0, 0);
        wl_surface_damage_buffer(surface_, 0, 0, width_, height_);
        wl_surface_commit(surface_);
        configured_ = true;
    }

    bool allocateBuffers() {
        const uint64_t stride = uint64_t(width_) * 4;
        const uint64_t bytes = stride * uint64_t(height_);
        if (stride > INT32_MAX || bytes == 0 || bytes > kMaxFrameBytes || bytes > SIZE_MAX) return false;
        static const wl_buffer_listener bufferListener = {
            [](void *data, wl_buffer *) {
                auto *slot = static_cast<BufferSlot *>(data);
                slot->released = true;
                slot->owner->renderWhenReleased();
            }
        };

        for (BufferSlot &slot : slots_) {
            slot.owner = this;
            slot.bytes = static_cast<size_t>(bytes);
            char name[] = "/meridian-layer-XXXXXX";
            slot.fd = memfd_create(name, MFD_CLOEXEC);
            if (slot.fd < 0 || ftruncate(slot.fd, static_cast<off_t>(bytes)) < 0) return false;
            slot.pixels = static_cast<uint32_t *>(mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, slot.fd, 0));
            if (slot.pixels == MAP_FAILED) {
                slot.pixels = nullptr;
                return false;
            }
            wl_shm_pool *pool = wl_shm_create_pool(shm_, slot.fd, static_cast<int32_t>(bytes));
            if (!pool) return false;
            slot.buffer = wl_shm_pool_create_buffer(
                pool, 0, width_, height_, static_cast<int32_t>(stride), WL_SHM_FORMAT_XRGB8888);
            wl_shm_pool_destroy(pool);
            if (!slot.buffer) return false;
            wl_buffer_add_listener(slot.buffer, &bufferListener, &slot);
        }
        return true;
    }

    void renderWhenReleased() {
        if (!pendingRender_ || !configured_ || failed_) return;

        size_t nextSlot = kBufferCount;
        for (size_t i = 0; i < kBufferCount; ++i) {
            if (slots_[i].released) {
                nextSlot = i;
                break;
            }
        }
        // Both slots are still owned by the compositor. Keep the minute tick
        // coalesced in pendingRender_ and let the next release retry this.
        if (nextSlot == kBufferCount) return;

        if (!renderScene(slots_[nextSlot])) {
            fail("could not redraw MeridianScene");
            return;
        }
        pendingRender_ = false;
        slots_[nextSlot].released = false;
        wl_surface_attach(surface_, slots_[nextSlot].buffer, 0, 0);
        wl_surface_damage_buffer(surface_, 0, 0, width_, height_);
        wl_surface_commit(surface_);
        if (wl_display_flush(display_) < 0) fail("could not flush redraw request");
    }

    bool renderScene(BufferSlot &slot) {
        image_ = QImage(static_cast<int>(width_), static_cast<int>(height_), QImage::Format_ARGB32_Premultiplied);
        if (image_.isNull()) return false;
        image_.fill(Qt::transparent);
        renderWindow_.setColor(Qt::transparent);
        renderWindow_.setRenderTarget(QQuickRenderTarget::fromPaintDevice(&image_));
        renderWindow_.resize(static_cast<int>(width_), static_cast<int>(height_));
        if (!scene_) {
            engine_.rootContext()->setContextProperty("clockModel", &clock_);
            engine_.rootContext()->setContextProperty("appCaptureMode", false);
            engine_.rootContext()->setContextProperty("appSaverMode", false);
            engine_.rootContext()->setContextProperty("appWallpaperMode", true);
            engine_.load(QUrl("qrc:/qml/MeridianScene.qml"));
            if (engine_.rootObjects().isEmpty()) return false;
            scene_ = qobject_cast<QQuickItem *>(engine_.rootObjects().first());
            if (!scene_) return false;
            scene_->setParentItem(renderWindow_.contentItem());
        }
        scene_->setWidth(static_cast<qreal>(width_));
        scene_->setHeight(static_cast<qreal>(height_));
        renderControl_.polishItems();
        renderControl_.sync();
        renderControl_.render();
        for (uint32_t y = 0; y < height_; ++y)
            std::memcpy(slot.pixels + y * width_, image_.constScanLine(static_cast<int>(y)), size_t(width_) * 4);
        return true;
    }

    void cleanup() {
        if (notifier_) notifier_->setEnabled(false);
        if (display_) {
            for (BufferSlot &slot : slots_) {
                if (slot.buffer) {
                    wl_buffer_destroy(slot.buffer);
                    slot.buffer = nullptr;
                }
            }
            if (layerSurface_) {
                zwlr_layer_surface_v1_destroy(layerSurface_);
                layerSurface_ = nullptr;
            }
            if (surface_) {
                wl_surface_destroy(surface_);
                surface_ = nullptr;
            }
            if (output_) {
                wl_output_destroy(output_);
                output_ = nullptr;
            }
            if (registry_) {
                wl_registry_destroy(registry_);
                registry_ = nullptr;
            }
            if (layerShell_) {
                zwlr_layer_shell_v1_destroy(layerShell_);
                layerShell_ = nullptr;
            }
            if (shm_) {
                wl_shm_destroy(shm_);
                shm_ = nullptr;
            }
            if (compositor_) {
                wl_compositor_destroy(compositor_);
                compositor_ = nullptr;
            }
            wl_display_flush(display_);
            wl_display_disconnect(display_);
            display_ = nullptr;
        }
        if (notifier_) notifier_->deleteLater();
        for (BufferSlot &slot : slots_) {
            if (slot.pixels) munmap(slot.pixels, slot.bytes);
            if (slot.fd >= 0) close(slot.fd);
            slot.pixels = nullptr;
            slot.fd = -1;
        }
    }

    QGuiApplication &app_;
    Clock &clock_;
    QQuickRenderControl renderControl_;
    QQuickWindow renderWindow_;
    QQmlApplicationEngine engine_;
    QQuickItem *scene_ = nullptr;
    QImage image_;
    QSocketNotifier *notifier_ = nullptr;
    wl_display *display_ = nullptr;
    wl_registry *registry_ = nullptr;
    wl_compositor *compositor_ = nullptr;
    wl_shm *shm_ = nullptr;
    wl_output *output_ = nullptr;
    zwlr_layer_shell_v1 *layerShell_ = nullptr;
    wl_surface *surface_ = nullptr;
    zwlr_layer_surface_v1 *layerSurface_ = nullptr;
    std::array<BufferSlot, kBufferCount> slots_;
    uint32_t width_ = 0, height_ = 0;
    bool configured_ = false, failed_ = false, started_ = true;
    bool pendingRender_ = false;
    qint64 lastMinute_ = -1;
};

} // namespace

int runLayerShellProof(QGuiApplication &app, Clock &clock) {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    LayerShellProof proof(app,clock);
    if (!proof.start()) return 2;
    return app.exec();
}
