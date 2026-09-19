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
#include <algorithm>
#include <memory>
#include <vector>

namespace {

constexpr uint64_t kMaxFrameBytes = 256ull * 1024ull * 1024ull;

// Borrowed connection services. LayerShellProof owns these proxies and keeps
// them alive until its OutputSurface and all callback sources are destroyed.
struct WaylandState {
    QGuiApplication &app;
    wl_display *display = nullptr;
    wl_compositor *compositor = nullptr;
    wl_shm *shm = nullptr;
    zwlr_layer_shell_v1 *layerShell = nullptr;
    bool failed = false;

    bool fail(const char *message) {
        std::fprintf(stderr, "Meridian layer-shell proof: %s\n", message);
        std::fflush(stderr);
        failed = true;
        app.exit(1);
        return false;
    }

    bool flush(const char *message = "could not flush Wayland requests") {
        if (wl_display_flush(display) < 0) return fail(message);
        return true;
    }
};

// One output's surface, render scene, and two stable buffer slots. Neither this
// object nor its slots may move while Wayland listeners hold their addresses.
class OutputSurface {
public:
    OutputSurface(WaylandState &wayland, Clock &clock, wl_output *output)
        : wayland_(wayland), clock_(clock), output_(output), renderWindow_(&renderControl_) {}
    ~OutputSurface() { cleanup(); }
    OutputSurface(const OutputSurface &) = delete;
    OutputSurface &operator=(const OutputSurface &) = delete;

    bool start() {
        surface_ = wl_compositor_create_surface(wayland_.compositor);
        if (!surface_) return fail("could not create Wayland surface");
        layerSurface_ = zwlr_layer_shell_v1_get_layer_surface(
            wayland_.layerShell, surface_, output_, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "meridian-wallpaper-proof");
        if (!layerSurface_) return fail("could not create layer-shell surface");

        static const zwlr_layer_surface_v1_listener layerListener = {
            [](void *data, zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height) {
                auto *self = static_cast<OutputSurface *>(data);
                self->configure(surface, serial, width, height);
            },
            [](void *data, zwlr_layer_surface_v1 *) {
                auto *self = static_cast<OutputSurface *>(data);
                // The controller destroys this owner after dispatch returns.
                // A close commonly accompanies output removal, in either order.
                self->retire();
            }
        };
        zwlr_layer_surface_v1_add_listener(layerSurface_, &layerListener, this);
        zwlr_layer_surface_v1_set_anchor(layerSurface_,
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_exclusive_zone(layerSurface_, -1);
        zwlr_layer_surface_v1_set_keyboard_interactivity(layerSurface_, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
        wl_region *empty = wl_compositor_create_region(wayland_.compositor);
        if (!empty) return fail("could not create empty input region");
        wl_surface_set_input_region(surface_, empty);
        wl_region_destroy(empty);
        // Required initial commit: no buffer may be attached before configure.
        wl_surface_commit(surface_);
        return true;
    }

    bool configured() const { return configured_; }
    bool retired() const { return retired_; }
    void retire() { retired_ = true; pendingRender_ = false; }

    void minuteChanged(qint64 minute) {
        if (minute == lastMinute_ || retired_ || wayland_.failed) return;
        lastMinute_ = minute;
        pendingRender_ = true;
        renderWhenReleased();
    }

private:
    static constexpr size_t kBufferCount = 2;

    struct BufferSlot {
        OutputSurface *owner = nullptr;
        wl_buffer *buffer = nullptr;
        uint32_t *pixels = nullptr;
        size_t bytes = 0;
        int fd = -1;
        bool released = true;
    };

    bool fail(const char *message) {
        return wayland_.fail(message);
    }

    void configure(zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height) {
        if (retired_ || wayland_.failed) return;
        zwlr_layer_surface_v1_ack_configure(surface, serial);
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
            wl_shm_pool *pool = wl_shm_create_pool(wayland_.shm, slot.fd, static_cast<int32_t>(bytes));
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
        if (!pendingRender_ || !configured_ || retired_ || wayland_.failed) return;

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
        wayland_.flush("could not flush redraw request");
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
        renderControl_.invalidate();
        renderWindow_.setRenderTarget(QQuickRenderTarget());
        if (layerSurface_) zwlr_layer_surface_v1_destroy(layerSurface_);
        if (surface_) wl_surface_destroy(surface_);
        for (BufferSlot &slot : slots_) {
            if (slot.buffer) wl_buffer_destroy(slot.buffer);
            // The compositor owns its own mapping/FD for any submitted buffer.
            if (slot.pixels) munmap(slot.pixels, slot.bytes);
            if (slot.fd >= 0) close(slot.fd);
        }
    }

    WaylandState &wayland_;
    Clock &clock_;
    wl_output *output_; // Borrowed; the controller destroys it after this object.
    QQuickRenderControl renderControl_;
    QImage image_; // Must outlive the render window's paint-device target.
    QQuickWindow renderWindow_;
    QQmlApplicationEngine engine_;
    QQuickItem *scene_ = nullptr;
    wl_surface *surface_ = nullptr;
    zwlr_layer_surface_v1 *layerSurface_ = nullptr;
    std::array<BufferSlot, kBufferCount> slots_;
    uint32_t width_ = 0, height_ = 0;
    bool configured_ = false;
    bool retired_ = false;
    bool pendingRender_ = false;
    qint64 lastMinute_ = -1;
};

class LayerShellProof : public QObject {
public:
    LayerShellProof(QGuiApplication &app, Clock &clock) : wayland_{app}, clock_(clock) {}
    ~LayerShellProof() override {
        // Stop callback delivery before destroying per-output owners.
        QObject::disconnect(clockConnection_);
        notifier_.reset();
        for (auto &output : outputs_) output->surface.reset();
        outputs_.clear();
        if (registry_) wl_registry_destroy(registry_);
        if (wayland_.layerShell) zwlr_layer_shell_v1_destroy(wayland_.layerShell);
        if (wayland_.shm) wl_shm_destroy(wayland_.shm);
        if (wayland_.compositor) wl_compositor_destroy(wayland_.compositor);
        if (wayland_.display) {
            wl_display_flush(wayland_.display);
            wl_display_disconnect(wayland_.display);
        }
    }

    bool start() {
        wayland_.display = wl_display_connect(nullptr);
        if (!wayland_.display) return wayland_.fail("could not connect to WAYLAND_DISPLAY");
        registry_ = wl_display_get_registry(wayland_.display);
        if (!registry_) return wayland_.fail("could not get Wayland registry");
        static const wl_registry_listener listener = {
            [](void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
                auto *self = static_cast<LayerShellProof *>(data);
                auto &wayland = self->wayland_;
                if (std::strcmp(interface, wl_compositor_interface.name) == 0 && version >= 4)
                    wayland.compositor = static_cast<wl_compositor *>(wl_registry_bind(registry, name, &wl_compositor_interface, 4));
                else if (std::strcmp(interface, wl_shm_interface.name) == 0)
                    wayland.shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
                else if (std::strcmp(interface, wl_output_interface.name) == 0)
                    self->pendingOutputs_.push_back({name, qMin(version, 4u)});
                else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0 && version >= 4)
                    wayland.layerShell = static_cast<zwlr_layer_shell_v1 *>(wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 4));
            },
            [](void *data, wl_registry *, uint32_t name) {
                static_cast<LayerShellProof *>(data)->outputRemoved(name);
            }
        };
        wl_registry_add_listener(registry_, &listener, this);
        if (wl_display_roundtrip(wayland_.display) < 0)
            return wayland_.fail("Wayland registry roundtrip failed");
        if (!wayland_.compositor || !wayland_.shm || !wayland_.layerShell)
            return wayland_.fail("compositor lacks wl_compositor, wl_shm, wl_output, or wlr-layer-shell v4");

        if (!applyOutputChanges()) return false;
        if (outputs_.empty()) return wayland_.fail("compositor advertises no wl_output");
        if (wl_display_roundtrip(wayland_.display) < 0)
            return wayland_.fail("Wayland configure roundtrip failed");
        if (wayland_.failed) return false;
        for (const auto &output : outputs_)
            if (!output->surface->retired() && !output->surface->configured())
                return wayland_.fail("layer-shell surface did not configure");
        // Changes arriving during the configure roundtrip follow the same path
        // as runtime hotplug. New surfaces configure asynchronously below.
        if (!applyOutputChanges()) return false;

        notifier_ = std::make_unique<QSocketNotifier>(wl_display_get_fd(wayland_.display), QSocketNotifier::Read);
        QObject::connect(notifier_.get(), &QSocketNotifier::activated, this, [this] {
            if (wl_display_dispatch(wayland_.display) < 0) {
                wayland_.fail("Wayland connection closed while dispatching events");
                notifier_->setEnabled(false);
                return;
            }
            if (!applyOutputChanges() || !wayland_.flush())
                notifier_->setEnabled(false);
        });
        clockConnection_ = QObject::connect(&clock_, &Clock::changed, this, [this] {
            const qint64 minute = clock_.utc().toSecsSinceEpoch() / 60;
            for (const auto &output : outputs_)
                if (output->surface) output->surface->minuteChanged(minute);
        });
        return wayland_.flush();
    }

private:
    struct Output {
        uint32_t globalName;
        wl_output *proxy;
        std::unique_ptr<OutputSurface> surface;

        ~Output() {
            surface.reset(); // The surface borrows proxy; destroy it first.
            if (wl_output_get_version(proxy) >= WL_OUTPUT_RELEASE_SINCE_VERSION)
                wl_output_release(proxy);
            else
                wl_output_destroy(proxy);
        }
    };

    struct OutputChange {
        uint32_t globalName;
        uint32_t version; // Zero means removal; wl_output versions start at one.
    };

    void outputRemoved(uint32_t name) {
        // Cancel an advertisement withdrawn in this dispatch before binding it.
        pendingOutputs_.erase(std::remove_if(pendingOutputs_.begin(), pendingOutputs_.end(),
            [name](const OutputChange &change) {
                return change.globalName == name && change.version != 0;
            }), pendingOutputs_.end());
        pendingOutputs_.push_back({name, 0});
        for (const auto &output : outputs_)
            if (output->globalName == name && output->surface) output->surface->retire();
    }

    bool applyOutputChanges() {
        if (wayland_.failed) return false;
        // Only called after a roundtrip/dispatch returns, never from a Wayland
        // callback or the clock's surface iteration. Listener addresses remain
        // valid throughout delivery of close/configure/buffer-release events.
        for (auto &output : outputs_)
            if (output->surface && output->surface->retired()) output->surface.reset();

        std::vector<OutputChange> changes;
        changes.swap(pendingOutputs_);
        for (const OutputChange &change : changes) {
            auto found = std::find_if(outputs_.begin(), outputs_.end(),
                [&change](const auto &output) { return output->globalName == change.globalName; });
            if (change.version == 0) {
                if (found != outputs_.end()) outputs_.erase(found);
                continue;
            }
            if (found != outputs_.end()) continue;
            auto *proxy = static_cast<wl_output *>(wl_registry_bind(
                registry_, change.globalName, &wl_output_interface, change.version));
            if (!proxy) return wayland_.fail("could not bind advertised wl_output");
            auto output = std::make_unique<Output>();
            output->globalName = change.globalName;
            output->proxy = proxy;
            output->surface = std::make_unique<OutputSurface>(wayland_, clock_, proxy);
            outputs_.push_back(std::move(output));
            if (!outputs_.back()->surface->start()) return false;
        }
        // With no outputs, retain only the connection/clock and wait for a new
        // advertisement. No buffers or render scenes remain after unplugging.
        return true;
    }

    WaylandState wayland_;
    Clock &clock_;
    wl_registry *registry_ = nullptr;
    std::vector<std::unique_ptr<Output>> outputs_;
    std::vector<OutputChange> pendingOutputs_;
    std::unique_ptr<QSocketNotifier> notifier_;
    QMetaObject::Connection clockConnection_;
};

} // namespace

int runLayerShellProof(QGuiApplication &app, Clock &clock) {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    LayerShellProof proof(app, clock);
    if (!proof.start()) return 2;
    return app.exec();
}
