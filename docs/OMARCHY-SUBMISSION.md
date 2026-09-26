# Omarchy submission draft

## Suggested discussion title

Omaridian: an opt-in vintage Geochron-inspired screensaver for Omarchy

## Suggested discussion text

I have built Omaridian, a small native Qt6 application inspired by the
1950s-era Geochron boardroom clocks. It renders an offline world map with a
live solar/daylight display and is designed for Omarchy first, with ultrawide
and per-output sizing as primary use cases.

The project is currently an external companion rather than an Omarchy core
change. Its Omarchy integration is deliberately opt-in and user-owned:

- it does not modify `/usr/share/omarchy`;
- it does not change Omarchy's idle or lock policy;
- it preserves `/usr/bin/omarchy-launch-screensaver` as a fallback;
- it refuses to overwrite unrelated launchers or customized configuration;
- it installs only user-owned adapter files and removes only files it created;
- the static theme remains data-only and does not launch background processes.

The application remains usable on its own with no Omarchy integration. The
repository includes a reproducible Arch package recipe, automated tests for
the launcher/fallback/install/uninstall behavior, optional Wayland
layer-shell rendering, and optional fractional-scale support.

Repository: https://github.com/daveplatt71/meridian

I would appreciate feedback on whether this belongs as an external companion,
an Omarchy theme/integration package, or a different contribution format.

## Before publishing a release

- Test installation on a second Omarchy machine with a clean user account.
- Verify screensaver dismissal, lock takeover, fallback, uninstall, and
  multi-monitor behavior on real Hyprland.
- Publish a versioned GitHub release archive.
- Replace the local-tree `source=()` in `packaging/PKGBUILD` with the release
  archive URL and SHA-256 checksum before proposing an AUR-style package.
- Attach a short ultrawide preview and the exact Omarchy/system test details.
