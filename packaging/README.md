# Arch package

## Install the x86_64 prerelease

The [v0.1.0 prerelease](https://github.com/daveplatt71/meridian/releases/tag/v0.1.0)
provides `omaridian-0.1.0-2-x86_64.pkg.tar.zst` and `SHA256SUMS`.

Download the package and checksum manifest directly:

```sh
asset=omaridian-0.1.0-2-x86_64.pkg.tar.zst
base=https://github.com/daveplatt71/meridian/releases/download/v0.1.0
curl -fL -O "$base/$asset"
curl -fL -o SHA256SUMS "$base/SHA256SUMS"
sha256sum --check SHA256SUMS && sudo pacman -U "$asset"
```

Alternatively, use GitHub CLI to fetch both release files:

```sh
gh release download v0.1.0 -R daveplatt71/meridian \
  -p 'omaridian-0.1.0-2-x86_64.pkg.tar.zst' -p SHA256SUMS
sha256sum --check SHA256SUMS && sudo pacman -U omaridian-0.1.0-2-x86_64.pkg.tar.zst
```

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
