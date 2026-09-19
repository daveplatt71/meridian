# Roadmap

## v0.1 — public standalone preview (done)

- Native offline Meridian preview with a vintage picture-frame presentation.
- Static map and relief resources with minute-rate solar shading and a
  second-rate clock.
- Public standalone launch path; no Omarchy idle, lock, or screensaver
  integration.

## Next milestone — Omarchy theme package (static preview published)

- Add the `theme/meridian/` package with a walnut/brass/parchment/deep-ocean
  palette and standard/ultrawide static wallpaper assets.
- Document the static fallback wallpaper and keep live behavior out of the
  theme until the renderer and lifecycle contracts exist.
- Make activation opt-in, retain a static fallback if live mode fails, clean
  up on theme switch/uninstall, and record CPU/GPU/memory budgets.
- Prepare the package for an Omarchy suggestion/PR after local review.
- Keep the installable theme at the root of the companion
  `daveplatt71/meridian-theme` repository.

## Renderer and desktop modes

- Share one renderer between `--wallpaper` and `--screensaver` modes. (renderer
  preview implemented; compositor/lifecycle integration remains)
- Pin and CI-test optional `wlr-layer-shell` protocol generation. (done)
- Add one-output layer-shell proof with empty input and safe `wl_shm`
  lifecycle. (implemented; initial and minute-based Meridian scene renders
  work, bounded frame allocation, protocol-version checks, and double-buffer
  coalescing included;
  multi-output lifecycle still pending)
- Update solar shading at minute rate while keeping the clock responsive.
- Add a Wayland background layer for wallpaper mode.
- Support multiple monitors and per-output scale factors.
- Handle suspend/resume and display hotplug without stale surfaces.
- Define idle/lock handoff so Meridian never changes normal policy and exits
  cleanly when the lock screen takes over.

## Validation and release

- Add clean-install tests for the package and theme discovery path.
- Test on real Wayland hardware across supported GPU drivers, monitor layouts,
  fractional scaling, suspend/resume, idle, and lock transitions.
- Submit an Omarchy suggestion/PR once the package and lifecycle behavior are
  reviewable.
- Announce the release on X after the clean-install and real-hardware checks
  pass.
