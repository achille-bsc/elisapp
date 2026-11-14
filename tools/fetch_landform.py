#!/usr/bin/env python3
"""Build a landform layer (format v3) read by app/data/landform.

Two sources, mixed at run time by landform_add_layer (finest layer first):

MOLA MEGDR 32 px/deg (~1.85 km), the whole zone. Only the needed rows are
read (HTTP Range request or local file), never the whole 133 MB image:
  tools/fetch_landform.py                       # Jezero, 500 km, from PDS
  tools/fetch_landform.py --src my.img --lat 0 --lon 180 --radius-km 300

CTX DTM (~20 m) or any finer DTM tied to the MOLA areoid, as a raw float32
little endian grid on an equirectangular grid of whole micro-degrees.
From a USGS GeoTIFF, with GDAL:
  gdalwarp -t_srs "+proj=longlat +a=3396190 +b=3396190 +no_defs" \\
      -tr 0.000337 0.000337 -r bilinear ctx_dtm.tif ctx_ll.tif
  gdal_translate -of ENVI -ot Float32 ctx_ll.tif ctx.raw
  gdalinfo ctx_ll.tif     # origin (west, north) and size
  tools/fetch_landform.py --grid ctx.raw --width W --height H \\
      --west 77.2 --north 18.7 --step 0.000337 --out data/ctx_layer.bin

Output goes to data/ (git ignored).
"""
import argparse
import math
import os
import struct
import sys
import urllib.request
from array import array

PDS_URL = ("https://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/"
           "mgsl_300x/meg032/megt90n000fb.img")
PPD = 32                      # pixels per degree
WIDTH, HEIGHT = 360 * PPD, 180 * PPD
ROW_BYTES = WIDTH * 2         # int16, big endian, row 0 = 90N, col 0 = 0E
# MEGDR reference sphere (label A_AXIS_RADIUS): the pixel size in metres
MARS_RADIUS_KM = 3396.0
# keep in sync with landform.hpp
MAGIC, VERSION, MAX_SIDE = 0x414C4F4D, 3, 16384
NODATA_M = -32768             # LANDFORM_NODATA_M: hole, next layer is read
UDEG_PER_DEG = 1_000_000
STEP_UDEG = UDEG_PER_DEG // PPD
NORTH_POLE_UDEG = 90 * UDEG_PER_DEG
LON_MAX_UDEG = 360 * UDEG_PER_DEG
MIN_COS_LAT = 1e-3  # avoids dividing by 0 at the poles
HALF_DEG_PER_DEG = 2      # slope unit: 0.5 deg
MAX_SLOPE_HALF_DEG = 180  # 90 deg
# GDAL float32 nodata is often -3.4e38: anything below this is a hole
NODATA_FLOAT_BELOW = -1e30
INT16_MIN, INT16_MAX = -32768, 32767


def read_rows(src, first, last):
    """Bytes of rows [first, last) from a URL or a local file."""
    start, size = first * ROW_BYTES, (last - first) * ROW_BYTES
    if src.startswith("http"):
        req = urllib.request.Request(
            src, headers={"Range": f"bytes={start}-{start + size - 1}"})
        with urllib.request.urlopen(req, timeout=60) as resp:
            data = resp.read()
    else:
        with open(src, "rb") as f:
            f.seek(start)
            data = f.read(size)
    if len(data) != size:
        sys.exit(f"short read: got {len(data)} bytes, wanted {size}")
    return data


def mola_grid(args):
    """MOLA rows of the zone: (grid, west_udeg, north_udeg, step_udeg)."""
    dlat = args.radius_km / (MARS_RADIUS_KM * math.pi / 180)
    dlon = dlat / max(math.cos(math.radians(args.lat)), MIN_COS_LAT)
    row0 = max(math.floor((90 - (args.lat + dlat)) * PPD), 0)
    row1 = min(math.ceil((90 - (args.lat - dlat)) * PPD), HEIGHT)
    col0 = math.floor((args.lon - dlon) * PPD)
    col1 = math.ceil((args.lon + dlon) * PPD)
    # shortcut: zones crossing the 0/360 deg meridian are refused, stitch the
    # two halves if a landing site ever needs it
    if col0 < 0 or col1 > WIDTH:
        sys.exit("zone crosses the 0/360 deg meridian: not supported")
    rows = read_rows(args.src, row0, row1)
    grid = []                 # grid[r] = int16 row, host (little) endian
    for r in range(row1 - row0):
        line = array("h")
        line.frombytes(rows[r * ROW_BYTES + col0 * 2:r * ROW_BYTES + col1 * 2])
        line.byteswap()       # MSB file -> little endian
        grid.append(line)
    return grid, col0 * STEP_UDEG, NORTH_POLE_UDEG - row0 * STEP_UDEG, STEP_UDEG


def raw_grid(args):
    """Raw float32 DTM: (grid, west_udeg, north_udeg, step_udeg)."""
    step_udeg = round(args.step * UDEG_PER_DEG)
    # The run time maths is in whole micro-degrees: a rounded step would drift
    # by one cell every few thousand cells
    if step_udeg <= 0 or abs(args.step * UDEG_PER_DEG - step_udeg) > 1e-6:
        sys.exit("--step must be a whole number of micro-degrees: resample "
                 "with gdalwarp -tr (see --help)")
    values = array("f")
    with open(args.grid, "rb") as f:
        values.frombytes(f.read())
    if len(values) != args.width * args.height:
        sys.exit(f"{args.grid}: {len(values)} values, "
                 f"wanted {args.width}x{args.height}")
    grid = []
    for r in range(args.height):
        line = array("h")
        for v in values[r * args.width:(r + 1) * args.width]:
            if math.isnan(v) or v < NODATA_FLOAT_BELOW:
                line.append(NODATA_M)
            else:
                # NODATA_M itself is reserved for holes
                line.append(min(max(round(v), INT16_MIN + 1), INT16_MAX))
        grid.append(line)
    west = round(args.west * UDEG_PER_DEG) % LON_MAX_UDEG
    return grid, west, round(args.north * UDEG_PER_DEG), step_udeg


def slope_of(grid, r, c, dx_m, dy_m):
    """Slope in half degrees from the valid neighbours (holes are skipped)."""
    height, width = len(grid), len(grid[0])

    def gradient(a, b, span_m):
        return 0.0 if NODATA_M in (a, b) else (b - a) / span_m

    centre = grid[r][c]
    west = grid[r][c - 1] if c > 0 else NODATA_M
    east = grid[r][c + 1] if c + 1 < width else NODATA_M
    north = grid[r - 1][c] if r > 0 else NODATA_M
    south = grid[r + 1][c] if r + 1 < height else NODATA_M
    # Central difference when both sides exist, one sided otherwise
    if NODATA_M not in (west, east):
        dzdx = gradient(west, east, 2 * dx_m)
    elif west != NODATA_M:
        dzdx = gradient(west, centre, dx_m)
    else:
        dzdx = gradient(centre, east, dx_m)
    if NODATA_M not in (north, south):
        dzdy = gradient(south, north, 2 * dy_m)
    elif south != NODATA_M:
        dzdy = gradient(south, centre, dy_m)
    else:
        dzdy = gradient(centre, north, dy_m)
    half_deg = round(math.degrees(math.atan(math.hypot(dzdx, dzdy)))
                     * HALF_DEG_PER_DEG)
    return min(half_deg, MAX_SLOPE_HALF_DEG)


def write_layer(path, grid, west_udeg, north_udeg, step_udeg):
    height, width = len(grid), len(grid[0])
    if width > MAX_SIDE or height > MAX_SIDE:
        sys.exit(f"layer {width}x{height} exceeds {MAX_SIDE} cells per side")
    # Slope is computed here, once, so the target only reads one byte per cell
    # shortcut: pure python, ~1 min per 4096x4096 CTX tile; use numpy if the
    # layers get bigger
    dy_m = math.radians(step_udeg / UDEG_PER_DEG) * MARS_RADIUS_KM * 1000
    slopes = bytearray()
    for r in range(height):
        lat = (north_udeg - (r + 0.5) * step_udeg) / UDEG_PER_DEG
        dx_m = dy_m * max(math.cos(math.radians(lat)), MIN_COS_LAT)
        for c in range(width):
            slopes.append(0 if grid[r][c] == NODATA_M
                          else slope_of(grid, r, c, dx_m, dy_m))
    # Same layout as t_landform_header (24 bytes) in landform.hpp
    header = struct.pack("<IIiIHHHH", MAGIC, west_udeg, north_udeg, step_udeg,
                         width, height, VERSION, 0)
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "wb") as f:
        f.write(header)
        for line in grid:
            line.tofile(f)
        f.write(slopes)
    print(f"{path}: {width}x{height} cells of {step_udeg} udeg, "
          f"{len(header) + width * height * 3} bytes")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__.splitlines()[0],
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    ap.add_argument("--src", default=PDS_URL, help="MOLA: URL or path of .img")
    ap.add_argument("--lat", type=float, default=18.4445, help="deg north")
    ap.add_argument("--lon", type=float, default=77.4509, help="deg east")
    ap.add_argument("--radius-km", type=float, default=500.0)
    ap.add_argument("--grid", help="raw float32 DTM instead of MOLA")
    ap.add_argument("--width", type=int, help="--grid: columns")
    ap.add_argument("--height", type=int, help="--grid: rows")
    ap.add_argument("--west", type=float, help="--grid: west edge, deg east")
    ap.add_argument("--north", type=float, help="--grid: north edge, deg")
    ap.add_argument("--step", type=float, help="--grid: cell size, deg")
    ap.add_argument("--out", default="data/landform_zone.bin")
    args = ap.parse_args()

    if sys.byteorder != "little":
        sys.exit("big endian host: the layer file must be little endian")
    if args.grid:
        if None in (args.width, args.height, args.west, args.north, args.step):
            sys.exit("--grid needs --width --height --west --north --step")
        layer = raw_grid(args)
    else:
        layer = mola_grid(args)
    write_layer(args.out, *layer)


if __name__ == "__main__":
    main()
