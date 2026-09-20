# Omarchy integration and submission notes

Omaridian is currently a standalone Qt application with an optional,
user-owned Omarchy screensaver adapter. It is not part of Omarchy itself and
must remain safe to install, disable, and remove without changing Omarchy's
stock files.

## Compatibility rules

- Never modify `/usr/share/omarchy` or files owned by the Omarchy package.
- Keep the integration opt-in and preserve `/usr/bin/omarchy-launch-screensaver`
  as the fallback for disabled, missing, or failed Omaridian launches.
- Do not change `idle.screensaver`, `idle.lock`, lock policy, or shell layout.
- Install only into user-owned paths: `~/.local/bin`, `~/.config/omaridian`, and
  a managed block in the user's shell startup file.
- Refuse to overwrite unrelated launchers or customized configuration.
- Make uninstall conservative: remove only files and managed lines created by
  Omaridian, never recursively delete user data.
- Keep the static theme data-only. It must not start a process, download data,
  or alter the lock screen.

The adapter is intentionally a screensaver integration rather than a
Quickshell bar plugin. Omarchy's plugin directory is for shell plugins and
widgets; idle screensaver replacement has a separate launcher contract.

## Review checklist

Before proposing an Omarchy integration or packaging change:

1. Run the default CTest suite and the layer-shell suite.
2. Run `tests/test-omarchy-integration.sh` and inspect the generated install
   and uninstall behavior.
3. Test missing binary, stock fallback, keyboard dismissal, lock takeover,
   one monitor, and multiple monitors on a real Hyprland session.
4. Capture a 5120×1440 visual preview for visual changes.
5. Keep the proposal focused and route feature ideas to Omarchy Discussions;
   only verified Omarchy bugs belong in the issue tracker.

The current integration should be presented as an external, opt-in companion
project until Omarchy maintainers request a different upstream shape.
