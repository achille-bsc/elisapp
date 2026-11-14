#!/usr/bin/env python3
"""Synthetic DTMs for the end to end test of tools/fetch_landform.py.

MOLA image: elevation = (col % 300) * 100 + row % 100 (global indices), big
endian int16, same size and layout as megt90n000fb.img.
CTX grid (--ctx): raw float32 little endian, 400 x 300 cells of 0.000337 deg
from (77.4 E, 18.5 N), elevation = 1000 + col % 50 * 2, with a NaN hole on
rows and cols 100..149. tests/data_test.cpp checks both.

  python3 tests/make_synthetic_mola.py /tmp/synthetic.img
  python3 tests/make_synthetic_mola.py --ctx /tmp/ctx.raw
  python3 tools/fetch_landform.py --src /tmp/synthetic.img --out /tmp/zone.bin
  python3 tools/fetch_landform.py --grid /tmp/ctx.raw --width 400 \\
      --height 300 --west 77.4 --north 18.5 --step 0.000337 --out /tmp/ctx.bin
  make test ZONE=/tmp/zone.bin ZONE_FINE=/tmp/ctx.bin
"""
import sys
from array import array

WIDTH, HEIGHT = 360 * 32, 180 * 32  # keep in sync with tools/fetch_landform.py
COL_PERIOD, COL_STEP_M, ROW_PERIOD = 300, 100, 100  # keep in sync with the test
CTX_WIDTH, CTX_HEIGHT = 400, 300                     # keep in sync with the test
CTX_BASE_M, CTX_COL_PERIOD, CTX_COL_STEP_M = 1000, 50, 2
CTX_HOLE_FIRST, CTX_HOLE_END = 100, 150


def write_mola(path):
    base = [(col % COL_PERIOD) * COL_STEP_M for col in range(WIDTH)]
    rows = []  # only ROW_PERIOD distinct rows: build them once
    for r in range(ROW_PERIOD):
        line = array("h", (b + r for b in base))
        line.byteswap()  # file is big endian
        rows.append(line.tobytes())
    with open(path, "wb") as f:
        for row in range(HEIGHT):
            f.write(rows[row % ROW_PERIOD])


def write_ctx(path):
    grid = array("f")
    for r in range(CTX_HEIGHT):
        for c in range(CTX_WIDTH):
            hole = (CTX_HOLE_FIRST <= r < CTX_HOLE_END and
                    CTX_HOLE_FIRST <= c < CTX_HOLE_END)
            grid.append(float("nan") if hole else
                        CTX_BASE_M + (c % CTX_COL_PERIOD) * CTX_COL_STEP_M)
    with open(path, "wb") as f:
        grid.tofile(f)


def main():
    if len(sys.argv) == 2:
        write_mola(sys.argv[1])
    elif len(sys.argv) == 3 and sys.argv[1] == "--ctx":
        write_ctx(sys.argv[2])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
