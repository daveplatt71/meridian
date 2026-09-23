# Omarchy package repository proposal

These files are ready to copy to `pkgbuilds/omaridian/` in
[`omacom/omarchy-pkgs`](https://github.com/omacom/omarchy-pkgs). They have not
been submitted or accepted by Omarchy. Only a merged and published package in
Omarchy's signed repository would enable `omarchy pkg add omaridian`.

The recipe builds Omaridian from the immutable `v0.1.1` source tag, checks its
pinned SHA-256 digest, runs the project tests, and packages only the supported
screensaver and opt-in adapter. The `.omarchy/package.json` file configures
direct tag-based upstream updates using Omarchy's package-repository format.

To prepare an upstream proposal, copy `PKGBUILD` and `.omarchy/package.json`
into that repository's `pkgbuilds/omaridian/` directory. Keep this README in
the Omaridian repository; it is not part of the upstream package directory.
