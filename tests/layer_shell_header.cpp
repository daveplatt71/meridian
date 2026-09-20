#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#ifdef OMARIDIAN_WITH_FRACTIONAL_SCALE
#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#endif

int main() {
#ifdef OMARIDIAN_WITH_FRACTIONAL_SCALE
    if (!wp_fractional_scale_manager_v1_interface.name || !wp_viewporter_interface.name)
        return 1;
#endif
    return zwlr_layer_shell_v1_interface.name == nullptr;
}
