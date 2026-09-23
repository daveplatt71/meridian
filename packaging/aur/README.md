# AUR packaging

This directory contains the source-build recipe for the Arch User Repository.
After publication as `omaridian`, Omarchy users can install it with:

```sh
omarchy pkg aur add omaridian
```

Then run `omaridian --fullscreen` to preview it. To opt in to Omarchy's idle
screensaver, run `/usr/share/omaridian/omarchy/install.sh`. Package installation
does not change the user's idle or lock settings.

The AUR recipe downloads the tagged source archive, verifies its SHA-256 hash,
builds the application, and runs its tests. Before submitting a new version,
update `pkgver`, the archive checksum, and `.SRCINFO`; build from a clean
directory with `makepkg -s`.

The first AUR package uses release number `3` so it upgrades the downloadable
`omaridian-0.1.0-2` preview package cleanly.

The existing `packaging/PKGBUILD` builds directly from a local checkout and is
used for the downloadable binary release. This recipe is self-contained so
that AUR helpers can fetch and build it.
