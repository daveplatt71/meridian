# Omarchy package repository proposal

These files were submitted as
[`omacom/omarchy-pkgs` pull request #603](https://github.com/omacom/omarchy-pkgs/pull/603).
They have not been accepted by Omarchy. Only a merged and published package in
Omarchy's signed repository would enable `omarchy pkg add omaridian`.

The recipe builds Omaridian from the immutable `v0.1.1` source tag, checks its
pinned SHA-256 digest, runs the project tests, and packages only the supported
screensaver and opt-in adapter. The `.omarchy/package.json` file configures
direct tag-based upstream updates using Omarchy's package-repository format.

The submitted upstream files are `PKGBUILD` and `.omarchy/package.json` under
`pkgbuilds/omaridian/`. This README remains in the Omaridian repository and is
not part of the upstream package directory.
