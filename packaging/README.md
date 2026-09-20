# Arch package

Build the local package from a checked-out Omaridian tree:

```sh
cd packaging
makepkg -si
omaridian --fullscreen
```

The package enables the tested Wayland layer-shell renderer, so the installed
binary also supports `omaridian --wallpaper-layer` on compositors that provide
layer-shell v4. The normal preview and screensaver modes remain available.

The recipe lives outside the application's tracked `src/` directory. It builds
from the parent checkout and uses makepkg's disposable `packaging/src/`
directory, so cleanup cannot remove application source files.

The package ships the opt-in Omarchy adapter under
`/usr/share/omaridian/omarchy/`; it does not install a PATH override
automatically. Run that directory's `install.sh` to copy the adapter into the
user-owned `~/.local/bin` and restore the stock launcher with `uninstall.sh`.

The first public GitHub release should replace the local-tree recipe's empty
`source=()` with a versioned GitHub release archive and its SHA-256 checksum.
That is the form intended for an external package repository or AUR submission.
