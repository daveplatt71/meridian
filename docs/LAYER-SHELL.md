# Real Hyprland wallpaper adapter

## Decision

The recommended prototype path is an optional raw Wayland client for
`zwlr_layer_shell_v1`, with the existing Qt Quick/QML scene rendered through
the public `QQuickRenderControl` API. Keep this behind an explicit
`--wallpaper-layer` mode and an optional build feature. The current Qt
`--wallpaper` mode remains a normal renderer preview.

Do not use Hyprland's internal headers as an application API, and do not make
GTK a second rendering host just to obtain layer-shell support. Hyprland
window rules are useful for a temporary fallback, but cannot turn a normal
Qt toplevel into a real background layer.

## Local audit

The checkout already has the renderer boundary needed for this split:

- `src/atlas.*` owns map data and painting; `qml/Main.qml` owns composition;
  `src/main.cpp` owns the current window and command-line lifecycle.
- Qt 6.11.2 Quick, QML, WaylandClient, and the public
  `QQuickRenderControl`/`QQuickRenderTarget` headers are installed.
- `wayland-client` 1.26.0, `wayland-protocols` 1.49, CMake 4.4.3, Ninja and
  `wayland-scanner` are installed.
- `gtk4` 4.22.4 and `gtk4-layer-shell` 1.3.0 are installed, including
  `gtk4-layer-shell-0.pc` and `gtk4-layer-shell.h`.
- No `wlr-protocols.pc` or standalone wlr-layer-shell XML is installed.
  `/usr/include/hyprland/protocols/wlr-layer-shell-unstable-v1.hpp` is a
  Hyprland-generated server-side C++ header, not a stable client dependency.
- Omarchy's current screensaver launcher creates one terminal window per
  monitor with app identity `org.omarchy.screensaver`; its lock command kills
  those processes. This project must not replace that path implicitly.

The current `--wallpaper` implementation calls `showFullScreen()` on a
`QQuickWindow`. It is therefore useful for visual and renderer testing, but it
has no compositor layer role, output binding, or guaranteed click-through.

## Options considered

### 1. Raw Wayland layer-shell client plus `QQuickRenderControl`

This is the recommended architecture. A small adapter would own the Wayland
display connection, registry, outputs, layer-shell globals, layer surfaces and
buffer commits. A separate renderer object would own the QML engine and a
render-control scene. The adapter submits each rendered frame to the matching
output's layer surface.

Advantages:

- Correct background ordering, no focus, no keyboard interaction, and an
  explicit empty input region.
- Keeps the map and solar logic in Qt Quick rather than rewriting it in GTK or
  Cairo.
- No GTK runtime dependency; generated protocol code can be built into the
  executable.
- Output binding, scale, configure/ack, close, and hotplug behavior are under
  our control.

Costs and risks:

- `QQuickRenderControl` is a rendering API, not a layer-shell integration. We
  must provide a render target and transfer its pixels or GPU image into a
  Wayland buffer. A CPU `QImage`/`wl_shm` proof is easiest, but adds a copy and
  may not meet the final ultrawide power budget. DMA-BUF/GPU interop should be
  a measured follow-up, not assumed.
- The layer-shell client protocol must be generated from a pinned XML file or
  obtained from a future distro package. Do not include a dependency on
  Hyprland's private/server headers.
- Render-control lifecycle is more complex than the current `QQuickWindow`:
  initialize, polish, sync, render, commit, and invalidate must be ordered on
  the correct thread, with a defined behavior for scene-graph failure.
- QML key and mouse dismissal must remain disabled in wallpaper mode; input
  handling belongs to the eventual screensaver/lifecycle adapter.

### 2. GTK4-layer-shell host and embedding

`gtk4-layer-shell` already exposes the right policy controls:
background/bottom/top/overlay layer, output selection, anchors, margins,
exclusive zone, keyboard mode `NONE`, support detection, and surface-close
handling. It is a good reference for the protocol state machine and could be
used for a small experimental host.

It is not the preferred production host for Meridian. Embedding the existing
QML scene in GTK would require either a renderer rewrite or a
`QQuickRenderControl` scene rendered into a GTK/GDK texture. That introduces
two UI toolkits, another frame/synchronization boundary, and likely a CPU or
GPU copy. It also adds GTK4, GLib, GDK, Pango, Cairo and related runtime
dependencies to an application whose current package only needs Qt.

Use this option only if a raw client proves unreliable on a target compositor
or if a future GTK-based shell integration is deliberately desired. It should
be a separate experimental target, not a hidden runtime dependency of the
small Qt package.

### 3. Hyprland window rules

This is the lowest-effort compatibility path: keep a normal Qt toplevel,
assign a stable app identity, and use user-owned Hyprland rules for monitor,
fullscreen, focus and stacking behavior.

It is not a real wallpaper implementation. Rules cannot give an xdg-toplevel
the `zwlr_layer_surface_v1` background role. The window can cover or be
covered by other windows, can be affected by focus/animation policy, and its
input behavior is compositor/window dependent. Rule syntax is also version
sensitive. Keep it as a developer fallback or manual preview only; never
advertise it as click-through background layering.

## Recommended phased architecture

### Phase 0: preserve the current renderer

Keep `AtlasMap`, the QML scene, snapshot mode, ordinary preview, and the
current terminal-based Omarchy screensaver unchanged. Extract the reusable
visual composition into an `Item` component separate from the visible
`Window`: render-control must be associated with an unshown `QQuickWindow`
before the scene is loaded. Add an internal renderer contract conceptually
shaped like:

```text
RendererScene(size, scale, clock) -> render(target)
LayerSurface(output, size, scale) -> configure/commit/close
WallpaperController -> enumerate outputs, connect scenes to surfaces
```

The contract must allow the renderer to fail without taking down the preview
or changing Omarchy settings.

### Phase 1: raw layer-shell proof of life

Add a build option such as `MERIDIAN_WITH_LAYER_SHELL`, disabled when the
optional protocol/toolchain pieces are unavailable. Pin and license the
wlr-layer-shell protocol XML, generate client bindings with `wayland-scanner`,
and use only public Wayland and Qt APIs.

Implement one output first, using a CPU `QImage` plus `wl_shm` buffer. Select
Qt Quick's software backend before creating the scene and use a persistent
image target with the single-threaded `polishItems()` → `sync()` → `render()`
sequence. Do not call the GPU-only initialize/beginFrame/endFrame sequence
for this software target. Request:

- background layer;
- all four anchors, zero margins and exclusive zone `-1`;
- keyboard interactivity `NONE`;
- an explicitly empty `wl_region`, so the surface does not intercept pointer
  input;
- a stable namespace such as `meridian-wallpaper`.

The initial layer-surface commit must attach no buffer. Set size to `0,0`,
anchor all four edges, commit once, then wait for configure, acknowledge the
serial, attach the first buffer, damage and commit. Wayland buffers are
immutable while submitted: use a bounded pool and only rewrite storage after
`wl_buffer.release`; coalesce minute updates while all buffers are busy.
Integrate the Wayland fd with Qt's event loop and handle prepare-read,
dispatch, flush, writable notifications and disconnect without blocking the
GUI thread. Do not use
`showFullScreen()` for this mode. Keep `--wallpaper` as the preview and give
the real adapter a distinct explicit command until it passes compositor tests.

### Phase 2: output manager and measured rendering

Create one layer surface per `wl_output`. Track output identity, logical size,
scale and removal. Each output gets the correct projection and device-pixel
buffer; the 5120x1440 display is simply the first important case, not a
special hard-coded size. Treat logical-size and preferred-scale changes as
independent events and coalesce them before rendering; do not assume a scale
change always arrives with another layer configure.

Share immutable parsed atlas geometry and relief data between scenes. Avoid a
second `AtlasMap` per mode and avoid rendering continuously: redraw on initial
configure, resize/scale, and the minute solar change. Measure the `wl_shm`
copy path before considering a DMA-BUF path. If GPU interop is added, keep it
behind the same renderer interface and benchmark startup, RSS, frame time and
idle CPU/GPU/power.

On output removal, destroy only that surface. On scale or geometry changes,
resize and rerender after the new configure. On suspend/resume, invalidate and
recreate buffers as needed; never reuse a stale `wl_buffer` or surface role.

### Phase 3: lifecycle integration, separately

Treat wallpaper and screensaver as different policies:

- wallpaper: background layer, no focus, no input, runs while the desktop is
  usable;
- screensaver: a separate, dismissible surface or future session-lock-aware
  implementation, with explicit ownership of idle and input transitions.

An eventual user-owned Omarchy hook or service may start/stop the adapter, but
must preserve Omarchy's normal `idle.screensaver` and `idle.lock` behavior.
The wallpaper adapter must not edit `/usr/share/omarchy` or silently change
`~/.config`.

## Failure behavior

Failure must be safe and boring:

1. If Wayland, layer-shell, required outputs, render initialization, or buffer
   allocation is unavailable, print one actionable diagnostic and exit
   nonzero. Do not fall back automatically to a fullscreen window.
2. If a layer surface is closed or an output disappears, tear down that output
   cleanly. If no outputs remain, exit without changing desktop configuration.
3. If one output fails, keep healthy outputs running only if the controller can
   prove that cleanup is independent; otherwise stop all Meridian surfaces and
   leave the existing static wallpaper intact.
4. Never claim success until the first configured frame has been submitted.
   A separate launcher can then choose the static theme wallpaper as fallback.

This avoids the dangerous failure mode where a “wallpaper” becomes a focused,
fullscreen application or blocks pointer input.

## Multi-monitor and scale strategy

Use the Wayland output list as the source of truth for layer surfaces. Bind one
surface to each output, anchor it to all edges, and render using that output's
logical dimensions and scale. Rebuild on output add/remove, mode/scale
changes, and compositor reconfiguration. Do not infer monitor placement from a
single `QScreen` or from the 5120x1440 ultrawide assumption.

The visual policy should be per-output: equal-area projection for very wide
outputs, standard projection for ordinary ratios, and a shared UTC instant so
all monitors show the same solar state. A single process is preferable for
shared clock/data caches; each output still needs its own scene target and
surface lifecycle.

## Test plan before enabling it by default

- Keep the current offscreen Qt tests and package smoke test passing.
- Add a protocol-disabled negative test: no layer-shell global must produce a
  clean nonzero exit and no ordinary fullscreen fallback.
- Add a small layer-surface integration test under a nested Wayland test
  compositor that supports wlr-layer-shell; verify configure/ack, background
  ordering, empty input region, first-frame commit, and clean close.
- On Hyprland, manually verify `hyprctl layers -j` and visible behavior on one
  output, then two outputs; test 5120x1440, mixed aspect ratios, fractional
  scale, hotplug, suspend/resume and compositor restart.
- Confirm ordinary windows remain focusable and clickable through the wallpaper,
  and that starting/stopping it does not alter Omarchy idle or lock behavior.
- Record startup latency, package size, RSS, minute-update CPU/GPU time and
  power impact for the `wl_shm` prototype. Set a release budget from those
  measurements before accepting DMA-BUF complexity.

The implementation should remain opt-in until all of these pass on the
supported Omarchy baseline.
