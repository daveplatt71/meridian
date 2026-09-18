#!/usr/bin/env bash
set -euo pipefail

# Build the committed terrain sidecar from Natural Earth's public-domain raster.
# The source is intentionally downloaded into a temporary directory and is not
# bundled with the project.

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
out_file="${root_dir}/assets/relief.png"
src_url='https://naturalearth.s3.amazonaws.com/50m_raster/SR_50M.zip'
src_sha256='ff810f5f2747463fd8ffa612b23e5f5e5d593a345218976872b6749610976ab7'
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/geochron-relief.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

curl --fail --location --retry 2 --output "${tmp_dir}/SR_50M.zip" "$src_url"
echo "${src_sha256}  ${tmp_dir}/SR_50M.zip" | sha256sum --check --strict
unzip -q "${tmp_dir}/SR_50M.zip" -d "${tmp_dir}/source"

# SR_50M.tif is already WGS84 geographic/equirectangular with a north-up
# world extent. Resize with explicit dimensions to retain the full world.
magick "${tmp_dir}/source/SR_50M.tif" \
  -resize 2048x1024! \
  -colorspace Gray \
  -strip \
  -define png:compression-level=9 \
  -define png:compression-filter=5 \
  "$out_file"

identify "$out_file"
test "$(stat -c '%s' "$out_file")" -le 409600
sha256sum "$out_file"
