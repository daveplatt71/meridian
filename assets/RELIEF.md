# Terrain relief sidecar

`relief.png` is a low-resolution, grayscale world hillshade blended at 22%
opacity over the country colors. It is deliberately neutral: bright low-relief plains
and darker shaded terrain remain legible without introducing land-cover colors;
the renderer clips it to land.

## Source and license

- Source: Natural Earth **1:50m Shaded Relief Basic**, described as grayscale
  shaded relief of land derived from downsampled SRTM Plus elevation data,
  clipped to the Natural Earth 50m coastline, with flat gray water.
- Documentation: https://www.naturalearthdata.com/downloads/50m-shaded-relief/50m-shaded-relief-basic/
- Maintained download mirror used by the build:
  https://naturalearth.s3.amazonaws.com/50m_raster/SR_50M.zip
- Natural Earth page release label: version 3.2.0. The downloaded archive's
  embedded `SR_50M.VERSION.txt` reports `2.0.0`; both are recorded here to
  avoid claiming a version not present in the reproducible input archive.
- Archive SHA-256:
  `ff810f5f2747463fd8ffa612b23e5f5e5d593a345218976872b6749610976ab7`
- License: Natural Earth data are free for use in any type of project; see the
  project's terms: https://www.naturalearthdata.com/about/terms-of-use/

The source is not bundled. `scripts/build-relief.sh` downloads it into a
temporary directory, verifies the archive checksum, extracts only the GeoTIFF,
and converts it with ImageMagick.

## Projection and conversion

The source GeoTIFF is `SR_50M.tif`, 10,800×5,400, 8-bit grayscale. Its WGS 84
`.prj` is geographic (longitude/latitude), and its world file records:

```text
pixel size:  0.03333333333333 degrees
top-left:   -179.98333333333333, 89.98333333333333
y step:     -0.03333333333333 degrees
```

Thus the source is north-up equirectangular/world geographic: the first pixel
is near 180°W, 90°N, longitude increases to the right, and latitude decreases
downward. The build preserves this full-world orientation and resizes to
2048×1024 with `magick -resize 2048x1024!`, converts to grayscale, strips
metadata, and writes maximum PNG compression.

To reproduce:

```sh
bash scripts/build-relief.sh
```
