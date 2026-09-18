# Arch package

Build the local package from a checked-out Meridian tree:

```sh
cd packaging
makepkg -si
meridian --fullscreen
```

The recipe lives outside the application's tracked `src/` directory. It builds
from the parent checkout and uses makepkg's disposable `packaging/src/`
directory, so cleanup cannot remove application source files.

The first public GitHub release should replace the local-tree recipe's empty
`source=()` with a versioned GitHub release archive and its SHA-256 checksum.
That is the form intended for an external package repository or AUR submission.
