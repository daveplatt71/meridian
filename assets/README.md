# Map provenance

`land.geojson`: Natural Earth 1:50m land polygons, version 5.1.2.
Source: https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/ne_50m_land.geojson
Terms: https://www.naturalearthdata.com/about/terms-of-use/ (public domain).
The palette, labels, frame, and application graphics are original project work.
This project is inspired by mechanical world clocks, not affiliated with Geochron.

`countries.geojson`: Natural Earth 1:50m admin-0 country polygons, version 5.1.2.
Only the geometry and small set of display properties needed by Omaridian are
retained. MultiPolygon members are rendered separately so island countries and
overseas territories are not silently dropped.
Source: https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/ne_50m_admin_0_countries.geojson
Terms: https://www.naturalearthdata.com/about/terms-of-use/ (public domain).

`relief.png` is documented separately in [RELIEF.md](RELIEF.md). It is bundled
as a small grayscale raster and clipped to country shapes at runtime.
