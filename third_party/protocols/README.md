# Vendored Wayland protocols

This directory contains the pinned protocol description used by the optional
Omaridian layer-shell generation target.

- Protocol: `wlr-layer-shell-unstable-v1.xml`
- Upstream: `https://github.com/swaywm/wlroots`
- Upstream path: `protocol/wlr-layer-shell-unstable-v1.xml`
- Pinned revision: `b7dc4f2990d1e6cdba38a7e9d2d286e48dd1a3eb`
- Local SHA-256: `2a8031a9572931810b73eccfe0575f4e9f57c48775386f32246828ceebb4c9d9`

The XML retains its upstream `<copyright>` notice and license text. Omaridian
uses the source-compatible argument name `name_space` for one XML argument
because `namespace` is a C++ keyword; this does not change the Wayland wire
protocol. The protocol is not used by the default build; enable generation
with `-DOMARIDIAN_WITH_LAYER_SHELL=ON`.

The optional generator also uses `stable/xdg-shell/xdg-shell.xml` from the
installed `wayland-protocols` package because layer-shell references
`xdg_popup_interface`; the default build does not require this generator.
