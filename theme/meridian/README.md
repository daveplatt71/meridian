# Meridian Omarchy theme

This is the static theme package for Meridian’s next roadmap milestone. It
uses Omarchy’s `colors.toml` palette interface and the installed `Yaru-wartybrown`
icon variant: dark deep-ocean surfaces, walnut-brown shadows, aged brass
accents, and parchment text.

The `backgrounds/` directory contains pre-rendered standard and ultrawide
wallpapers, and `preview.png` is composed for the theme picker. The wallpaper
artwork contains no clock/date header or dismissal instruction, so it does not
pretend to be live. Lock-screen artwork is deferred until the lock-screen
milestone.

## Wallpaper boundary

The theme is intentionally data-only at this stage. A static Meridian render
is the fallback wallpaper concept: it can be exported from the standalone
preview and selected like any other background, with no process, timer, or
Omarchy hook running behind it. The repository’s preview image is a visual
reference, not an executable integration or a live background provider.

Future work may add a shared renderer mode for a minute-rate live wallpaper
and a separate screensaver mode. Those modes will need explicit Wayland layer,
multi-monitor, scale, suspend/resume, and idle/lock handoff work before this
theme package should attempt to launch them. This milestone does not modify
`~/.config`, `/usr/share/omarchy`, idle policy, lock behavior, or shell startup.

`shell.toml` is intentionally absent: the installed local themes use
`shell.lock.toml` for the optional shell override, and Meridian does not need
to lock shell colors yet.
