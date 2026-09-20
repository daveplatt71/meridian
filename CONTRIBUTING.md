# Contributing to Omaridian

Omaridian is an early standalone preview for an Omarchy screensaver. Small,
focused changes are easiest to review.

Before submitting a change:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

For visual changes, also create a 5120×1440 snapshot and inspect it. Keep map
data offline and document its source and license under `assets/`. Do not add
runtime downloads, root-only setup, or edits to `/usr/share/omarchy`.

The application must remain usable as a standalone preview until an Omarchy
integration has been reviewed separately. Changes affecting idle, dismissal,
locking, or monitor handling need tests and a short manual test description.
