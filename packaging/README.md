# Arch package

## Install the x86_64 prerelease

The [v0.1.1 prerelease](https://github.com/daveplatt71/meridian/releases/tag/v0.1.1)
provides `omaridian-0.1.1-1-x86_64.pkg.tar.zst`. Use a recent GitHub CLI to
verify the immutable release and the downloaded asset:

```sh
gh release verify v0.1.1 -R daveplatt71/meridian
gh release download v0.1.1 -R daveplatt71/meridian \
  -p 'omaridian-0.1.1-1-x86_64.pkg.tar.zst'
gh release verify-asset v0.1.1 ./omaridian-0.1.1-1-x86_64.pkg.tar.zst \
  -R daveplatt71/meridian &&
  sudo pacman -U ./omaridian-0.1.1-1-x86_64.pkg.tar.zst
```

The release's `SHA256SUMS` detects transfer errors but does not independently
authenticate an asset from the same release. Installing a remote URL directly
with `pacman -U` requires a trusted package signature and is not supported by
this release. Keep pacman's signature settings intact.

If GitHub CLI release verification is unavailable, compare the downloaded
package against this pinned v0.1.1 digest before installing. The digest was
checked against the immutable release attestation:

```sh
curl -fLO https://github.com/daveplatt71/meridian/releases/download/v0.1.1/omaridian-0.1.1-1-x86_64.pkg.tar.zst
printf '%s  %s\n' \
  '3f1c57aea5badf46d1323d22fb375f553061837df2175b0bd7eb411c7e21ca9b' \
  'omaridian-0.1.1-1-x86_64.pkg.tar.zst' | sha256sum --check - &&
  sudo pacman -U ./omaridian-0.1.1-1-x86_64.pkg.tar.zst
```

The AUR source recipe in [`aur/`](aur/) is ready for submission. It is not
published to the AUR yet, so `omarchy pkg aur add omaridian` is not available.

Use a fully updated x86_64 Arch or Omarchy system with Qt 6.11 or newer
(`qt6-base`, `qt6-declarative`, and `qt6-wayland`), Wayland, and Noto fonts.
Test the installed preview with `omaridian --fullscreen`; test fullscreen
screensaver mode with `omaridian --screensaver`. Press Escape to close either.

## Enable the Omarchy adapter (optional)

The package installs adapter files in
`/usr/share/omaridian/omarchy/`, but does not activate them automatically. To
opt in, run:

```sh
/usr/share/omaridian/omarchy/install.sh
```

This installs the user-owned launcher in `~/.local/bin` and adds its PATH
activation to `~/.bashrc`. Omarchy's stock idle and lock timings remain
unchanged.

## Upgrade or uninstall

Before upgrading the package, remove the copied adapter while the installed
scripts are available:

```sh
/usr/share/omaridian/omarchy/uninstall.sh
```

Then download, verify, and install the newer package with `sudo pacman -U` as
above. Run the new package's `install.sh` afterward if you want the adapter
enabled again. To uninstall, run `uninstall.sh` first, then remove the package:

```sh
sudo pacman -R omaridian
```

Package removal alone does not remove the user-owned launcher or PATH entry.

## Build from a checkout

For local development, build and install the package from the repository:

```sh
cd packaging
makepkg -si
```

The recipe builds the preview and screensaver from the parent checkout. It
keeps makepkg's disposable `packaging/src/` directory separate from the
project's tracked `src/` tree. The experimental Wayland wallpaper renderer is
not included in the package.
