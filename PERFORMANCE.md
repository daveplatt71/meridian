# Performance checkpoint

Measured locally on 2026-09-17 at 5120×1440 with Qt's offscreen software
backend, fixed time 2026-06-21T08:24:00Z, and PNG snapshot output. Each row is
one process run. Elapsed time includes the preview's deliberate 600 ms capture
delay and PNG encoding; it is not time to first visible frame.

| Build | Executable bytes | Elapsed seconds | Process CPU seconds | Peak RSS KiB |
| --- | ---: | ---: | ---: | ---: |
| Previous, no build type | 1,529,040 | 1.100 | 0.797 | 262,176 |
| Release, cached terrain clipping | 1,399,976 | 1.079 | 0.738 | 264,856 |

The executable includes map resources and excludes shared Qt dependencies.
The two captures have zero differing pixels. The small timing difference in
single samples does not establish a performance improvement. Cached clipping
avoids assembling the combined polygon path on each map paint; the map itself
still renders when its minute, size, or projection changes.

The 2048×1024 relief PNG is 401,068 bytes. Source terrain downloads, captures,
test binaries, and build tools must stay out of application packages.

Real Wayland frame timing, minute-update costs, GPU memory and power remain
unmeasured. The software snapshot's peak memory is not an idle-memory result.
These measurements are a baseline, not final performance acceptance.
