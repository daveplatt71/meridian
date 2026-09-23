# Roadmap

## Supported product scope

- An offline Omaridian clock with a vintage picture-frame presentation,
  static map and relief resources, minute-rate solar shading, and a second-rate
  clock.
- Manual preview launch for inspecting the presentation.
- Opt-in Omarchy screensaver integration through the user-owned launcher
  adapter. The stock launcher remains available as a fallback, and Omarchy's
  idle and lock timing stays unchanged.

## Remaining validation

- Review the manual preview's appearance and refine composition.
- Validate fullscreen rendering, display scaling, resize, input dismissal,
  suspend/resume, and monitor hotplug on real Wayland hardware and GPU drivers.
- Validate the opt-in Omarchy adapter on multi-monitor systems, including
  idle/lock handoff, preserving existing timing and handling renderer failure.
- Record real Wayland GPU/CPU use, memory, startup time, and package size.

The optional layer-shell renderer and theme assets remain in the repository
for reference. Neither is part of the supported screensaver package or its
release milestones.
