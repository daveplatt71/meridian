# Omaridian

A native, offline vintage world clock for Omarchy / Wayland, inspired by
mechanical boardroom solar clocks. Supported use is an opt-in Omarchy
screensaver and a preview launched manually.

![Omaridian ultrawide preview](docs/preview-ultrawide.png)

## Status

This is an early public-preview candidate. Omaridian can be launched manually
as a windowed preview or fullscreen screensaver. Its optional Omarchy
integration uses a user-owned PATH adapter and direct per-monitor fullscreen
windows. Installing the adapter is explicit; without it, Omarchy keeps its
stock screensaver launcher. Real-hardware, suspend/resume, and monitor-change
validation remain outstanding.

## Design priority: speed and size

The user's guiding requirement is that Omaridian stay fast and small, in keeping
with Omarchy. Treat performance and footprint as acceptance criteria for new
features, not a cleanup step at the end.

- Prefer compact vector/coordinate data and procedural drawing.
- Reuse system Qt libraries for the Omarchy package; report dependencies
  separately from the application payload.
- Keep screenshots, development tools and test binaries out of release packages.
- Keep raster layers deliberately small and preprocess them offline; do not
  download map data at runtime.
- Cache static layers, limit redraws to what changes, and avoid continuous
  animation or background work without a concrete need.
- Measure startup time, package size, memory and idle CPU/GPU use when adding
  significant layers. Record before/after results and resolve regressions before
  accepting the feature. Establish numerical budgets from the measured baseline.

The current Release executable is approximately 1.34 MiB including the
compressed country and relief resources (excluding shared Qt dependencies); this
is not an installer size. The bundled source assets are retained at 1:50m
resolution and downsampled where raster data is used.
Measurement conditions and outstanding checks are in [PERFORMANCE.md](PERFORMANCE.md).

## Build and run

Requires CMake, Ninja, a C++17 compiler, Qt 6.11 or newer (Base, Declarative
and Wayland), and Noto fonts. A fully updated Arch or Omarchy system is the
supported runtime baseline.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
./build/omaridian
./build/omaridian --fullscreen
./build/omaridian --screensaver
```

Escape closes the preview or screensaver. The fullscreen view uses the display's current
logical dimensions; Qt handles the display scale. A thin walnut/brass picture
frame replaces the former side panels. Ultrawide displays use a Lambert
cylindrical equal-area map with mild horizontal expansion; standard displays
retain the 2:1 equirectangular map. This uses more ultrawide canvas while
keeping the complete world visible. Equal-area projection compresses the polar
regions rather than stretching an existing image vertically. The projection
changes automatically on resize.
The screensaver adapter and its installer are documented in
[integration/omarchy/README.md](integration/omarchy/README.md).

## Arch and Omarchy installation

The x86_64 Arch package is available as a
[GitHub prerelease](https://github.com/daveplatt71/meridian/releases/tag/v0.1.1).
Verify the immutable release and downloaded package with GitHub CLI before
installing the local file:

```sh
gh release verify v0.1.1 -R daveplatt71/meridian
gh release download v0.1.1 -R daveplatt71/meridian \
  -p 'omaridian-0.1.1-1-x86_64.pkg.tar.zst'
gh release verify-asset v0.1.1 ./omaridian-0.1.1-1-x86_64.pkg.tar.zst \
  -R daveplatt71/meridian &&
  sudo pacman -U ./omaridian-0.1.1-1-x86_64.pkg.tar.zst
```

Use a recent GitHub CLI with `release verify` and `release verify-asset`.
The release also provides `SHA256SUMS`, but a checksum downloaded from the
same release is only an integrity check, not independent authentication.
Pacman requires a trusted signature for remote `-U` URLs, so use the verified
local file. Do not weaken pacman's signature policy to install Omaridian.

The package requires a fully updated Arch/Omarchy system with Qt 6.11 or
newer (`qt6-base`, `qt6-declarative`, and `qt6-wayland`), plus Wayland and
Noto fonts. Preview the installed app with `omaridian --fullscreen`; use
`omaridian --screensaver` to test its fullscreen screensaver mode (press Escape
to close either one).

The adapter is opt-in. To connect it to Omarchy's idle screensaver, run:

```sh
/usr/share/omaridian/omarchy/install.sh
```

This enables the user-owned launcher in `~/.local/bin`; it does not change
Omarchy's idle or lock timings. To uninstall the package, first run
`/usr/share/omaridian/omarchy/uninstall.sh`, then remove the package:

```sh
sudo pacman -R omaridian
```

Before upgrading with another `pacman -U`, run the installed
`/usr/share/omaridian/omarchy/uninstall.sh`. After the upgrade, run the new
`/usr/share/omaridian/omarchy/install.sh` to refresh the copied launcher if you
want to keep the Omarchy integration enabled. Package removal alone does not
remove user-owned files from your home directory.

For a clean source build, follow the CMake commands above. Package and CI
builds use Release mode and run the full test suite before installation.

```sh
./build/omaridian --at 2026-06-21T08:24:00Z
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME=basic \
  ./build/omaridian --size 5120x1440 --at 2026-09-16T18:00:00Z --snapshot /tmp/omaridian.png
```

`--at` requires an explicit timezone. Snapshots are static renders, not
verification of Wayland fullscreen, input, power use or lock behavior.

## The clock

UTC drives the solar calculation; system timezone drives the local clock.
The map band reports **apparent solar hours** by longitude, not civil timezones.
Night begins at the geometric horizon, with a six-degree civil-twilight fade.
The solar coordinates use NOAA/Meeus equations; atmospheric refraction is not
included. The map updates each minute; the text clock updates each second.

Reference: https://gml.noaa.gov/grad/solcalc/calcdetails.html
Map provenance and public-domain terms: [assets/README.md](assets/README.md).
The display includes compact, preprocessed country polygons and a 2048×1024
Natural Earth shaded-relief layer. Both are static; only the solar/night layer
changes with time.

## Remaining validation

1. Review the preview's appearance and refine composition.
2. Profile GPU/CPU on real hardware, including ultrawide and fractional-scale
   displays; check suspend/resume, resize, and input dismissal.
3. Validate the user-owned Omarchy integration on multi-monitor hardware;
   confirm idle/lock timing is preserved and renderer failure is distinguished
   from normal dismissal.

## Known limitations

- The Omarchy adapter is opt-in; its installer activates the user-owned
  launcher path, while an uninstalled system keeps the stock launcher.
- Omaridian screensaver windows are fullscreen Qt windows, not layer-shell
  surfaces. Wallpaper rendering and layer-shell support are experimental and
  outside the supported product scope.
- It has not yet been validated across AMD, Intel, and NVIDIA hardware or
  across suspend/resume and monitor hotplug events.
- The current performance record uses Qt's offscreen software backend; real
  Wayland GPU and power measurements are still required.

Please report failures with the Omarchy version, CPU, GPU, display layout and
the command that was run. Include terminal output and a screenshot when
possible.

Preview windows use `org.omaridian.preview`. Screensaver windows use
`org.omarchy.screensaver`; the Omarchy adapter also passes that identity in
argv for the existing lock handoff. No desktop configuration or security
policy is changed.

## Experimental layer-shell development status

The repository contains an optional Wayland layer-shell renderer for
development; it is disabled by default and is not part of the supported
screensaver or manual-preview experience. When enabled,
`--wallpaper-layer` creates a real background surface with an empty input
region and renders the Omaridian map into it. It binds every initially
advertised `wl_output`, creates one surface per output, handles runtime output
add/remove, and redraws each output when the UTC minute changes. Integer output
scales are applied to physical buffers while QML remains logical. An optional
fractional-scale build uses `wp_viewporter` when the compositor advertises it;
otherwise it falls back to integer scaling. Layer-surface close and logical
resize recovery recreate only the affected output. Suspend/resume remains
pending. It
requires layer-shell v4 and `wl_compositor` v4, and bounds shared-memory frame
allocation. The
layer-shell proof also uses two bounded shared-memory buffers and coalesces
updates while the compositor holds both. The existing
`--wallpaper` mode remains a normal fullscreen renderer preview.

To validate the optional developer feature locally, install
`wayland-scanner`, `wayland-client`, and `wayland-protocols`, then run:

```sh
cmake -S . -B build-layer-shell -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOMARIDIAN_WITH_LAYER_SHELL=ON
cmake --build build-layer-shell
ctest --test-dir build-layer-shell --output-on-failure
```

To exercise the optional fractional-scale bindings as well, add
`-DOMARIDIAN_WITH_FRACTIONAL_SCALE=ON` to the configure command. That requires
the installed staging `fractional-scale-v1` and stable `viewporter` protocol
XML files.

The generated protocol bindings and proof are not included in the default
package. The proof requires a live Wayland session and exits safely if the
compositor lacks layer-shell support.
The implementation plan and safety requirements are in
[docs/LAYER-SHELL.md](docs/LAYER-SHELL.md).

## Agent workflow

Luna implemented the QML presentation under a fixed interface. Astra reviewed
it and the clock backend. Reviews found and prompted fixes for keyboard focus,
ultrawide layout, the solar-hour overlay, and historical-date shading. Final
acceptance also requires real rendered output and passing geometry tests.
