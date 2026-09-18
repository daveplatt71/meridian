#include "layer_shell.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QSocketNotifier>
#include <QTimer>
#include <QtGlobal>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

class LayerShellProof {
public:
    explicit LayerShellProof(QCoreApplication &app) : app_(app) {}
    ~LayerShellProof() { cleanup(); }

    bool start() {
        display_ = wl_display_connect(nullptr);
        if (!display_) return fail("could not connect to WAYLAND_DISPLAY");

        registry_ = wl_display_get_registry(display_);
        static const wl_registry_listener listener = {
            [](void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
                auto *self = static_cast<LayerShellProof *>(data);
                if (std::strcmp(interface, wl_compositor_interface.name) == 0)
                    self->compositor_ = static_cast<wl_compositor *>(wl_registry_bind(registry, name, &wl_compositor_interface, qMin(version, 4u)));
                else if (std::strcmp(interface, wl_shm_interface.name) == 0)
                    self->shm_ = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
                else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0)
                    self->layerShell_ = static_cast<zwlr_layer_shell_v1 *>(wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, qMin(version, 4u)));
            },
            [](void *, wl_registry *, uint32_t) {}
        };
        wl_registry_add_listener(registry_, &listener, this);
        if (wl_display_roundtrip(display_) < 0)
            return fail("Wayland registry roundtrip failed");
        if (!compositor_ || !shm_ || !layerShell_)
            return fail("compositor lacks wl_compositor, wl_shm, or wlr-layer-shell");

        surface_ = wl_compositor_create_surface(compositor_);
        if (!surface_) return fail("could not create Wayland surface");
        layerSurface_ = zwlr_layer_shell_v1_get_layer_surface(
            layerShell_, surface_, nullptr, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "meridian-wallpaper-proof");
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
        if (!configured_) return fail("layer-shell surface did not configure");

        notifier_ = new QSocketNotifier(wl_display_get_fd(display_), QSocketNotifier::Read, &app_);
        QObject::connect(notifier_, &QSocketNotifier::activated, &app_, [this] {
            if (!display_ || wl_display_dispatch(display_) < 0) {
                fail("Wayland connection closed while dispatching events");
                app_.exit(1);
            }
            wl_display_flush(display_);
        });
        if (wl_display_flush(display_) < 0)
            return fail("could not flush Wayland requests");
        return true;
    }

private:
    bool fail(const char *message) {
        std::fprintf(stderr, "Meridian layer-shell proof: %s\n", message);
        std::fflush(stderr);
        failed_ = true;
        if (started_) app_.exit(1);
        return false;
    }

    void configure(zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height) {
        zwlr_layer_surface_v1_ack_configure(surface, serial);
        if (width == 0 || height == 0 || width > 16384 || height > 16384) {
            fail("compositor supplied an invalid layer size");
            return;
        }
        if (configured_) return;
        width_ = width;
        height_ = height;
        if (!allocateBuffer()) {
            fail("could not allocate wl_shm buffer");
            return;
        }
        wl_surface_attach(surface_, buffer_, 0, 0);
        wl_surface_damage_buffer(surface_, 0, 0, width_, height_);
        wl_surface_commit(surface_);
        configured_ = true;
    }

    bool allocateBuffer() {
        const uint64_t stride = uint64_t(width_) * 4;
        const uint64_t bytes = stride * uint64_t(height_);
        if (stride > INT32_MAX || bytes == 0 || bytes > SIZE_MAX) return false;
        char name[] = "/meridian-layer-XXXXXX";
        fd_ = memfd_create(name, MFD_CLOEXEC);
        if (fd_ < 0 || ftruncate(fd_, static_cast<off_t>(bytes)) < 0) return false;
        pixels_ = static_cast<uint32_t *>(mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
        if (pixels_ == MAP_FAILED) return false;
        for (uint32_t y = 0; y < height_; ++y)
            for (uint32_t x = 0; x < width_; ++x)
                pixels_[y * width_ + x] = 0xff15231f; // diagnostic deep-ocean green
        wl_shm_pool *pool = wl_shm_create_pool(shm_, fd_, static_cast<int32_t>(bytes));
        if (!pool) return false;
        buffer_ = wl_shm_pool_create_buffer(pool, 0, width_, height_, static_cast<int32_t>(stride), WL_SHM_FORMAT_XRGB8888);
        wl_shm_pool_destroy(pool);
        if (!buffer_) return false;
        static const wl_buffer_listener bufferListener = {
            [](void *data, wl_buffer *) {
                static_cast<LayerShellProof *>(data)->bufferReleased_ = true;
            }
        };
        wl_buffer_add_listener(buffer_, &bufferListener, this);
        return true;
    }

    void cleanup() {
        if (notifier_) notifier_->setEnabled(false);
        // Disconnect before releasing shared memory so the compositor cannot
        // retain a submitted wl_shm buffer while its mapping is unmapped.
        if (display_) {
            wl_display_flush(display_);
            wl_display_disconnect(display_);
            display_ = nullptr;
        }
        if (notifier_) notifier_->deleteLater();
        if (pixels_ && pixels_ != MAP_FAILED) munmap(pixels_, size_t(width_) * size_t(height_) * 4);
        if (fd_ >= 0) close(fd_);
    }

    QCoreApplication &app_;
    QSocketNotifier *notifier_ = nullptr;
    wl_display *display_ = nullptr;
    wl_registry *registry_ = nullptr;
    wl_compositor *compositor_ = nullptr;
    wl_shm *shm_ = nullptr;
    zwlr_layer_shell_v1 *layerShell_ = nullptr;
    wl_surface *surface_ = nullptr;
    zwlr_layer_surface_v1 *layerSurface_ = nullptr;
    wl_buffer *buffer_ = nullptr;
    uint32_t *pixels_ = nullptr;
    int fd_ = -1;
    uint32_t width_ = 0, height_ = 0;
    bool configured_ = false, failed_ = false, started_ = true;
    bool bufferReleased_ = false;
};

} // namespace

int runLayerShellProof(QGuiApplication &app) {
    LayerShellProof proof(app);
    if (!proof.start()) return 2;
    return app.exec();
}
