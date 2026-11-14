#!/usr/bin/env python3
"""Extract a zone of MOLA MEGDR 32 px/deg into the file read by landform.cpp.

Only the needed rows are read (HTTP Range request or local file), never the
whole 133 MB image. Output goes to data/ (git ignored).

  tools/fetch_landform.py                       # Jezero, 500 km, from PDS
  tools/fetch_landform.py --src my.img --lat 0 --lon 180 --radius-km 300
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
MARS_RADIUS_KM = 3396.0
STEP_UDEG = UDEG_PER_DEG // PPD
MAGIC, VERSION, MAX_SIDE = 0x414C4F4D, 2, 4096  # keep in sync with landform.hpp
UDEG_PER_DEG = 1_000_000
NORTH_POLE_UDEG = 90 * UDEG_PER_DEG
MIN_COS_LAT = 1e-3  # avoids dividing by 0 at the poles
HALF_DEG_PER_DEG = 2      # slope unit: 0.5 deg
MAX_SLOPE_HALF_DEG = 255  # uint8 layer, saturates


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


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--src", default=PDS_URL, help="URL or path of the .img")
    ap.add_argument("--lat", type=float, default=18.4445, help="deg north")
    ap.add_argument("--lon", type=float, default=77.4509, help="deg east")
    ap.add_argument("--radius-km", type=float, default=500.0)
    ap.add_argument("--out", default="data/landform_zone.bin")
    args = ap.parse_args()

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

    if sys.byteorder != "little":
        sys.exit("big endian host: the zone file must be little endian")
    width, height = col1 - col0, row1 - row0
    if width > MAX_SIDE or height > MAX_SIDE:
        sys.exit(f"zone {width}x{height} exceeds {MAX_SIDE} cells per side")

    rows = read_rows(args.src, row0, row1)
    grid = []                 # grid[r] = int16 row, host (little) endian
    for r in range(height):
        line = array("h")
        line.frombytes(rows[r * ROW_BYTES + col0 * 2:r * ROW_BYTES + col1 * 2])
        line.byteswap()       # MSB file -> little endian
        grid.append(line)

    # Slope is computed here, once, so the chip only reads one byte per cell
    dy_m = math.radians(1 / PPD) * MARS_RADIUS_KM * 1000
    slopes = bytearray()
    for r in range(height):
        lat = 90 - (row0 + r + 0.5) / PPD
        dx_m = dy_m * max(math.cos(math.radians(lat)), MIN_COS_LAT)
        up, down = grid[max(r - 1, 0)], grid[min(r + 1, height - 1)]
        span_y = max(min(r + 1, height - 1) - max(r - 1, 0), 1)
        line = grid[r]
        for c in range(width):
            c0, c1 = max(c - 1, 0), min(c + 1, width - 1)
            dzdx = (line[c1] - line[c0]) / (dx_m * max(c1 - c0, 1))
            dzdy = (up[c] - down[c]) / (dy_m * span_y)  # rows grow southward
            half_deg = round(math.degrees(math.atan(math.hypot(dzdx, dzdy)))
                            * HALF_DEG_PER_DEG)
            slopes.append(min(half_deg, MAX_SLOPE_HALF_DEG))

    # Same layout as t_landform_header (20 bytes) in landform.hpp
    header = struct.pack("<IIiHHHH", MAGIC, col0 * STEP_UDEG,
                         NORTH_POLE_UDEG - row0 * STEP_UDEG, width, height,
                         VERSION, 0)
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(header)
        for line in grid:
            line.tofile(f)
        f.write(slopes)
    print(f"{args.out}: {width}x{height} cells, "
          f"{len(header) + width * height * 3} bytes")


if __name__ == "__main__":
    main()
