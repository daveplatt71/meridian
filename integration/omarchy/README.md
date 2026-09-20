# Omarchy screensaver integration

This integration uses a user-owned PATH override for
`omarchy-launch-screensaver`:

```sh
./integration/omarchy/install.sh
```

The installer creates `~/.local/bin/omarchy-launch-screensaver`, adds a
managed PATH block to `~/.bashrc` before its non-interactive early return, and,
if it is missing, creates `~/.config/omaridian/omarchy-screensaver.conf`. It
refuses to replace an unrelated existing launcher or customized config. Remove
it with:

```sh
./integration/omarchy/uninstall.sh
```

The adapter does not edit `~/.config/omarchy/shell.json`, so Omarchy retains the
existing `idle.screensaver` and `idle.lock` values. It also never edits
`/usr/share/omarchy`.

## Runtime contract

The adapter preserves Omarchy's `force` and `screensaver-off` behavior, opens
one direct Omaridian process per Hyprland monitor, focuses each monitor before
launch, and waits up to `OMARIDIAN_SCREENSAVER_DEADLINE` seconds for an
`org.omarchy.screensaver` open-window event. On an unavailable binary or a
failed launch before any Omaridian window maps it delegates to
`/usr/bin/omarchy-launch-screensaver`. After a partial launch it leaves the
visible Omaridian window in place so Omarchy does not mistake recovery for user
dismissal and cancel the pending lock.

Each direct process runs `omaridian --screensaver` with both the Wayland app id
and the argv marker `org.omarchy.screensaver`. This is intentional: Hyprland
idle sees the expected class/app-id, and Omarchy's existing lock command can
reach direct Omaridian processes with its package-owned `pkill -f` handoff.

Omaridian dismisses all same-identity screensaver processes on keyboard or
pointer input. That closes every monitor's window, allowing Omarchy's idle
service to classify the event as `screensaver-dismissed` and cancel the lock
countdown. When `omarchy-system-lock` runs, its existing identity-based kill
path terminates Omaridian before the lock screen takes over.

Set `OMARIDIAN_OMARCHY_ENABLED=0` in the config to use the stock launcher
without uninstalling the adapter. `--wallpaper-layer` is unrelated and is not
started by this integration.

This is a normal fullscreen Qt window per monitor, not a layer-shell surface;
it is deliberately scoped to the screensaver lifecycle and does not change
desktop wallpaper behavior.
