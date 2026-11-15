#!/usr/bin/env python3
"""Download the real data of the site of assets/mission_site.json into data/.

  relief_{mola,hrsc,ctx,hirise}.bin  relief layers, 1.85 km down to 2 m
  imagery_{themis,ctx}.bin           orbital images, 100 m and 6 m
  ground_ti.bin                      thermal inertia (soft, dusty ground)
  weather_mcd.bin                    Mars Climate Database, 24 h, 1 deg cells
  points_of_interest.json, missions.json, photos/   places of the zone

Only the needed bytes of the global mosaics are read (HTTP Range); Jezero
products are cached in data/cache/. Run through `make data`.
"""
import argparse
import concurrent.futures
import http.client
import json
import math
import os
import re
import struct
import sys
import threading
import time
import urllib.parse
import urllib.request
import warnings

import numpy as np

# Mars reference sphere of the USGS and PDS products (IAU 2000)
MARS_RADIUS_M = 3396190.0
M_PER_DEG = MARS_RADIUS_M * math.pi / 180
UDEG_PER_DEG = 1_000_000
LON_MAX_UDEG = 360 * UDEG_PER_DEG

# keep in sync with app/data/landform/landform.hpp
LANDFORM_MAGIC, LANDFORM_VERSION, MAX_SIDE = 0x414C4F4D, 5, 16384
CLEARANCE_M = 50.0
# Slopes over a few strides, not a 2 m pixel (boulders, DTM noise)
SLOPE_BASELINE_M = 10.0
RAW_SPAN = 65000          # int16 values used around the base, NODATA apart
MAX_UNIT_MM, MAX_BASE_M = 1000, 32767  # LANDFORM_MAX_UNIT_MM, LANDFORM_MAX_BASE_M
NODATA_M = -32768
MAX_SLOPE_HALF_DEG = 180
# keep in sync with app/data/imagery/imagery.hpp
IMAGERY_MAGIC, IMAGERY_VERSION = 0x52474D49, 1  # "IMGR"
# keep in sync with app/data/ground/ground.hpp
GROUND_MAGIC, GROUND_VERSION = 0x524E4954, 1  # "TINR"
GROUND_TIU_PER_STEP = 8   # stored value * 8 = thermal inertia (tiu)
# shortcut: TI = TI_TES * (DN / local mean DN) ** EXPONENT, to calibrate on
# the THEMIS quantitative inertia mosaic
GROUND_DOWNSCALE_EXPONENT = 1.0
GROUND_BOX_M = 3000.0
PHOTO_MAGIC = 0x544F4850  # "PHOT"
PHOTO_W, PHOTO_H = 360, 240
IMAGERY_NODATA = 0
# keep in sync with app/data/sim/weather_mcd.hpp
WEATHER_MAGIC, WEATHER_VERSION = 0x5744434D, 2  # "MCDW", v2: wind direction
HOURS_PER_SOL = 24
SCALE_HEIGHT_M = 11100.0  # MARS_SCALE_HEIGHT_M, app/data/config.hpp

S3 = "https://asc-pds-services.s3.us-west-2.amazonaws.com/"
PDS_MOLA = ("https://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/"
            "mgsl_300x/meg032/megt90n000fb.img")
HRSC = "mosaic/Mars/HRSC_MOLA_Blend/Mars_HRSC_MOLA_BlendDEM_Global_200mp_v2"
THEMIS = ("mosaic/Mars/THEMIS_controlled_mosaics/AA_mosaics/"
          "THEMIS_DayIR_ControlledMosaics_100m_v2_oct2018")
TRN = "mosaic/mars2020_trn/"
THEMIS_NIGHT = ("mosaic/Mars/THEMIS_controlled_mosaics/AA_mosaics/"
                "THEMIS_NightIR_ControlledMosaics_100m_v2_oct2018")
TES_TI = ("https://pds-geosciences.wustl.edu/mgs/mgs-m-tes-5-timap-v1/"
          "mgst_9001/data/global_ti_night_2007.img")
NASA_IMAGE = "https://images-assets.nasa.gov/image/{0}/{0}~medium.jpg"
# Real photographs of the places, NASA Photojournal (all NASA/JPL-Caltech)
NASA_PHOTOS = {
    "Ingenuity helicopter": ("PIA26236", "Perseverance spots Ingenuity at its final airfield (Mastcam-Z, 2024)"),
    "Perseverance rover": ("PIA26344", "Perseverance's selfie with 'Cheyava Falls' (WATSON, 2024)"),
    "Three Forks sample depot": ("PIA25681", "Perseverance's Three Forks sample depot selfie (2023)"),
    "Wright Brothers Field": ("PIA24542", "Perseverance's selfie with Ingenuity at Wright Brothers Field (2021)"),
    "Octavia E. Butler Landing": ("PIA24483", "Welcome to Octavia E. Butler Landing (2021)"),
    "Jezero delta front": ("PIA24815", "The scarps of Jezero crater's delta (2021)"),
    "Mars 2020 descent stage": ("PIA24335", "The descent stage on the surface, seen by HiRISE (2021)"),
}
CTX_DTM = TRN + ("CTX/JEZ_ctx_B_soc_008_DTM_MOLAtopography_DeltaGeoid_20m_"
                 "Eqc_latTs0_lon0.tif")
CTX_ORTHO = TRN + "CTX/JEZ_ctx_B_soc_008_orthoMosaic_6m_Eqc_latTs0_lon0.tif"
HIRISE_PREFIX = TRN + "HiRISE/DTM_MOLAtopography_DeltaGeoid_Jezero_"
MCD = "https://www-mars.lmd.jussieu.fr/mcd_python/"
GAZETTEER = ("https://planetarynames.wr.usgs.gov/SearchResults?"
             "Target=20_Mars&Feature%20Type=&System=")
MMGIS = "https://mars.nasa.gov/mmgis-maps/M20/Layers/json/"

# Missions: a walk there and back plus the work must fit one EVA (xEVA 8 h)
EVA_S = 8 * 3600
WALK_SPEED_M_S = 1.0       # WALK_SPEED_MM_S, app/data/config.hpp
ROUTE_DETOUR = 1.3         # planned routes are longer than the straight line
ROVER_SPEED_M_S = 2.778    # kRoverSpeedMmPerSecond, app/integration/Tuning.hpp
WORK_DAY_S = 10 * 3600     # a rover traverse and its EVA within one sol
WORK_MINUTES = {"hardware": 90, "feature": 120, "site": 60}

HIRISE_STEP_M = 2.0       # the 2 m relief asked for the crew
HIRISE_BLOCK = 2          # 1 m posts averaged 2 x 2
FLOAT_NODATA_BELOW = -1e30
THREADS = 8
MCD_THREADS = 4           # a public service: stay polite
TIMEOUT_S = 120
STRETCH_LOW, STRETCH_HIGH = 1.0, 99.0  # image contrast, percentiles
USER_AGENT = "elisapp-fetch-site/1 (NASA Space Apps 2026 student project)"


# Site //

def load_site(path):
    with open(path, encoding="utf-8") as f:
        site = json.load(f)["mission_site"]
    for key in ("latitude", "longitude"):
        if not isinstance(site.get(key), (int, float)):
            sys.exit(f"{path}: mission_site.{key} must be a number")
    if not -90 <= site["latitude"] <= 90:
        sys.exit(f"{path}: latitude out of [-90, 90]")
    site["longitude"] %= 360
    return site


class Zone:
    """Equirectangular target grid around the site, whole micro-degrees."""

    def __init__(self, site, radius_km, step_m):
        self.step_udeg = max(1, round(step_m / M_PER_DEG * UDEG_PER_DEG))
        step_deg = self.step_udeg / UDEG_PER_DEG
        dlat = radius_km * 1000 / M_PER_DEG
        dlon = dlat / max(math.cos(math.radians(site["latitude"])), 1e-3)
        self.height = min(MAX_SIDE, math.ceil(2 * dlat / step_deg))
        self.width = min(MAX_SIDE, math.ceil(2 * dlon / step_deg))
        self.north_udeg = round((site["latitude"] + dlat) * UDEG_PER_DEG)
        self.north_udeg = min(self.north_udeg, 90 * UDEG_PER_DEG)
        self.west_udeg = round((site["longitude"] - dlon) * UDEG_PER_DEG)

    def centres(self):
        """Latitudes (rows) and east longitudes (cols) of the cell centres."""
        s = self.step_udeg / UDEG_PER_DEG
        lat = self.north_udeg / UDEG_PER_DEG - (np.arange(self.height) + 0.5) * s
        lon = self.west_udeg / UDEG_PER_DEG + (np.arange(self.width) + 0.5) * s
        return lat, lon

    def header(self, magic, version, unit_mm=0, base_m=0):
        # Same layout as t_landform_header / t_imagery_header (28 bytes)
        return struct.pack("<IIiIHHHHi", magic, self.west_udeg % LON_MAX_UDEG,
                           self.north_udeg, self.step_udeg, self.width,
                           self.height, version, unit_mm, base_m)

    def clip(self, west, east, south, north):
        """Shrinks the zone to a footprint (deg). False if they do not meet."""
        s = self.step_udeg / UDEG_PER_DEG
        w0 = self.west_udeg / UDEG_PER_DEG
        if w0 >= 180 and west < 0:  # footprint in -180..180, zone east
            west, east = west + 360, east + 360
        n0 = self.north_udeg / UDEG_PER_DEG
        c0 = max(0, math.floor((west - w0) / s))
        c1 = min(self.width, math.ceil((east - w0) / s))
        r0 = max(0, math.floor((n0 - north) / s))
        r1 = min(self.height, math.ceil((n0 - south) / s))
        if c1 <= c0 or r1 <= r0:
            return False
        self.west_udeg += c0 * self.step_udeg
        self.north_udeg -= r0 * self.step_udeg
        self.width, self.height = c1 - c0, r1 - r0
        return True


# HTTP //

_local = threading.local()


def http_range(url, start, size):
    """`size` bytes of `url` from `start`, one keep-alive connection a thread."""
    parts = urllib.parse.urlsplit(url)
    for attempt in range(3):
        conn = getattr(_local, "conns", {}).get(parts.netloc)
        if conn is None:
            conn = http.client.HTTPSConnection(parts.netloc, timeout=TIMEOUT_S)
            _local.conns = getattr(_local, "conns", {})
            _local.conns[parts.netloc] = conn
        try:
            conn.request("GET", parts.path, headers={
                "Range": f"bytes={start}-{start + size - 1}",
                "User-Agent": USER_AGENT})
            resp = conn.getresponse()
            data = resp.read()
            if resp.status == 206 and len(data) == size:
                return data
        except (OSError, http.client.HTTPException):
            pass
        conn.close()
        _local.conns.pop(parts.netloc, None)
    sys.exit(f"cannot read {size} bytes at {start} of {url}")


def download(url, path):
    """Whole file into the cache, once."""
    if os.path.exists(path):
        return path
    os.makedirs(os.path.dirname(path), exist_ok=True)
    print(f"  downloading {url.rsplit('/', 1)[-1]}", flush=True)
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=TIMEOUT_S) as resp, \
            open(path + ".part", "wb") as f:
        while chunk := resp.read(1 << 20):
            f.write(chunk)
    os.replace(path + ".part", path)
    return path


def http_text(url, attempts=4):
    """Text of `url`; a dropped connection is retried with a growing pause."""
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    for attempt in range(attempts):
        try:
            with urllib.request.urlopen(req, timeout=TIMEOUT_S) as resp:
                return resp.read().decode("utf-8", "replace")
        except (OSError, http.client.HTTPException):
            if attempt + 1 == attempts:
                raise
            time.sleep(2 ** attempt)


# Sources //

class Raster:
    """A simple cylindrical raster in metres (Eqc, lon 0, true scale at 0)."""

    # West end of the product's longitudes: -180 (USGS) or 0 (MOLA)
    lon_west = -180.0

    def __init__(self, x0, y0, res, width, height):
        self.x0, self.y0, self.res = x0, y0, res
        self.width, self.height = width, height

    def window(self, r0, r1, c0, c1):
        raise NotImplementedError

    def footprint(self):
        """(west, east, south, north) in degrees, east longitude in the
        product's domain (lon_west .. lon_west + 360)."""
        return (self.x0 / M_PER_DEG,
                (self.x0 + self.width * self.res) / M_PER_DEG,
                (self.y0 - self.height * self.res) / M_PER_DEG,
                self.y0 / M_PER_DEG)

    def sample(self, zone, out):
        """Bilinear values at the zone cell centres into `out` where it is NaN."""
        lat, lon = zone.centres()
        lon = (lon - self.lon_west) % 360 + self.lon_west
        fc = (lon * M_PER_DEG - self.x0) / self.res - 0.5
        fr = (self.y0 - lat * M_PER_DEG) / self.res - 0.5
        c0 = max(0, int(math.floor(fc.min())))
        c1 = min(self.width, int(math.floor(fc.max())) + 2)
        r0 = max(0, int(math.floor(fr.min())))
        r1 = min(self.height, int(math.floor(fr.max())) + 2)
        if c1 - c0 < 2 or r1 - r0 < 2:
            return
        win = self.window(r0, r1, c0, c1)
        x = np.clip(fc - c0, 0, c1 - c0 - 1.000001)
        y = np.clip(fr - r0, 0, r1 - r0 - 1.000001)
        inside_c = (fc >= -0.5) & (fc <= self.width - 0.5)
        inside_r = (fr >= -0.5) & (fr <= self.height - 0.5)
        ix, iy = x.astype(np.int64), y.astype(np.int64)
        tx, ty = (x - ix)[None, :], (y - iy)[:, None]
        a = win[iy][:, ix]
        b = win[iy][:, ix + 1]
        c = win[iy + 1][:, ix]
        d = win[iy + 1][:, ix + 1]
        v = (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty
        # A hole among the four posts: nearest post, or a hole
        nearest = win[np.rint(y).astype(np.int64)][:, np.rint(x).astype(np.int64)]
        v = np.where(np.isnan(v), nearest, v)
        v[~inside_r, :] = np.nan
        v[:, ~inside_c] = np.nan
        todo = np.isnan(out)
        out[todo] = v[todo]


class CubeRaster(Raster):
    """Uncompressed band sequential raster read row by row (HTTP Range)."""

    def __init__(self, url, label, dtype, nodata, grid=None):
        """`label`: an ISIS label, or None with `grid` = (x0, y0, res,
        width, height, start byte) for a plain PDS image."""
        if label is None:
            x0, y0, res, width, height, start = grid
        else:
            def value(key):
                return float(re.search(rf"\b{key}\s*=\s*([-0-9.eE]+)", label)[1])
            x0, y0 = value("UpperLeftCornerX"), value("UpperLeftCornerY")
            res, width = value("PixelResolution"), int(value("Samples"))
            height, start = int(value("Lines")), int(value("StartByte")) - 1
        super().__init__(x0, y0, res, width, height)
        self.url, self.dtype, self.nodata = url, np.dtype(dtype), nodata
        self.start = start

    def window(self, r0, r1, c0, c1):
        item = self.dtype.itemsize
        row_bytes = self.width * item

        def row(r):
            return http_range(self.url, self.start + r * row_bytes + c0 * item,
                              (c1 - c0) * item)
        with concurrent.futures.ThreadPoolExecutor(THREADS) as pool:
            rows = list(pool.map(row, range(r0, r1)))
        win = np.frombuffer(b"".join(rows), self.dtype).reshape(r1 - r0, c1 - c0)
        win = win.astype(np.float32)
        win[win == self.nodata] = np.nan
        return win


class ArrayRaster(Raster):
    def __init__(self, data, x0, y0, res):
        super().__init__(x0, y0, res, data.shape[1], data.shape[0])
        self.data = data

    def window(self, r0, r1, c0, c1):
        return self.data[r0:r1, c0:c1]


def tiff_raster(path, block=1, nodata_zero=False):
    """A GeoTIFF of the Mars 2020 TRN set, averaged `block` x `block`."""
    import tifffile
    with tifffile.TiffFile(path) as tif:
        page = tif.pages[0]
        scale = page.tags["ModelPixelScaleTag"].value
        tie = page.tags["ModelTiepointTag"].value
        data = page.asarray().astype(np.float32)
    # GDAL -3.4e38 (CTX) or -32768 (HiRISE tiles): holes
    data[(data < FLOAT_NODATA_BELOW) | (data <= NODATA_M) | ~np.isfinite(data)] = np.nan
    if nodata_zero:
        data[data == 0] = np.nan
    res = float(scale[0])
    if block > 1:
        h, w = data.shape[0] // block * block, data.shape[1] // block * block
        with warnings.catch_warnings():  # all-hole blocks stay holes
            warnings.simplefilter("ignore", RuntimeWarning)
            data = np.nanmean(data[:h, :w].reshape(h // block, block,
                                                   w // block, block),
                              axis=(1, 3))
        res *= block
    # Tie point: pixel (I, J) corner at model (X, Y)
    x0 = tie[3] - tie[0] * float(scale[0])
    y0 = tie[4] + tie[1] * float(scale[1])
    return ArrayRaster(data, x0, y0, res)


def mola_raster():
    """MOLA MEGDR 32 px/deg: big endian int16, row 0 = 90 N, col 0 = 0 E."""
    ppd = 32
    width, height = 360 * ppd, 180 * ppd
    res = M_PER_DEG / ppd

    class Mola(Raster):
        lon_west = 0.0  # columns run 0..360 E

        def window(self, r0, r1, c0, c1):
            def row(r):
                return http_range(PDS_MOLA, (r * width + c0) * 2, (c1 - c0) * 2)
            with concurrent.futures.ThreadPoolExecutor(THREADS) as pool:
                rows = list(pool.map(row, range(r0, r1)))
            return np.frombuffer(b"".join(rows), ">i2").reshape(
                r1 - r0, c1 - c0).astype(np.float32)

    return Mola(0.0, 90 * M_PER_DEG, res, width, height)


# Writers //

def walking_scale(z, zone):
    """Elevation averaged over SLOPE_BASELINE_M: the slope a crew walks on,
    not the one of a single 2 m pixel (a boulder, or DTM noise)."""
    cell = zone.step_udeg / UDEG_PER_DEG * M_PER_DEG
    half = int(SLOPE_BASELINE_M / cell / 2)
    if half < 1:
        return z
    valid = ~np.isnan(z)
    filled = np.where(valid, z, 0.0).astype(np.float64)
    def box(a):
        p = np.pad(a, half, mode="edge")
        k = 2 * half + 1
        v = np.lib.stride_tricks.sliding_window_view(p, k, axis=0).sum(-1)
        return np.lib.stride_tricks.sliding_window_view(v, k, axis=1).sum(-1)
    mean = box(filled) / np.maximum(box(valid.astype(np.float64)), 1)
    return np.where(valid, mean, np.nan).astype(np.float32)


def slopes_half_deg(z, zone):
    """Slope of each cell in 0.5 deg steps, over SLOPE_BASELINE_M; holes
    borrow the centre value."""
    z = walking_scale(z, zone)
    lat = np.radians(zone.centres()[0])[:, None]
    dy = zone.step_udeg / UDEG_PER_DEG * M_PER_DEG
    dx = dy * np.maximum(np.cos(lat), 1e-3)
    p = np.pad(z, 1, mode="edge")
    centre = p[1:-1, 1:-1]

    def side(v):
        return np.where(np.isnan(v), centre, v)
    dzdx = (side(p[1:-1, 2:]) - side(p[1:-1, :-2])) / (2 * dx)
    dzdy = (side(p[:-2, 1:-1]) - side(p[2:, 1:-1])) / (2 * dy)
    slope = np.degrees(np.arctan(np.hypot(dzdx, dzdy))) * 2
    slope = np.nan_to_num(slope, nan=0.0)
    return np.clip(np.rint(slope), 0, MAX_SLOPE_HALF_DEG).astype(np.uint8)


def running_max(a, half, axis):
    """Max over a window of 2 * half + 1 along `axis` (edges clipped)."""
    if half <= 0:
        return a
    pad = [(0, 0), (0, 0)]
    pad[axis] = (half, half)
    p = np.pad(a, pad, mode="edge")
    view = np.lib.stride_tricks.sliding_window_view(p, 2 * half + 1, axis=axis)
    return view.max(axis=-1)


def steepest_around(slope, zone):
    """Per cell, the steepest slope within CLEARANCE_M (a square of that
    half side, on the safe side of the disc): what the planner checks so
    that no dangerous ground lies within 50 m of the path."""
    dy = zone.step_udeg / UDEG_PER_DEG * M_PER_DEG
    # The poleward row sets the columns: 50 m hold everywhere
    pole = float(np.abs(zone.centres()[0]).max())
    dx = dy * max(math.cos(math.radians(pole)), 1e-3)
    rows, cols = math.ceil(CLEARANCE_M / dy), math.ceil(CLEARANCE_M / dx)
    # Coarser than the clearance: a cell already covers it
    if dy >= CLEARANCE_M:
        return slope
    return running_max(running_max(slope, rows, 0), cols, 1).astype(np.uint8)


def write_relief(path, zone, z):
    valid = ~np.isnan(z)
    if not valid.any():
        print(f"{path}: no data in the zone, skipped")
        return
    # Finest unit spanning the zone's relief
    low, high = float(np.nanmin(z)), float(np.nanmax(z))
    base_m = round((low + high) / 2)
    # + 1 m: base_m is rounded
    unit_mm = max(1, math.ceil((high - low + 1) * 1000 / RAW_SPAN))
    if unit_mm > MAX_UNIT_MM or abs(base_m) > MAX_BASE_M:
        sys.exit(f"{path}: relief {low:.0f}..{high:.0f} m does not fit the layer format")
    raw = np.rint((np.nan_to_num(z) - base_m) * 1000 / unit_mm)
    elev = np.where(valid, np.clip(raw, NODATA_M + 1, 32767),
                    NODATA_M).astype("<i2")
    slope = slopes_half_deg(z, zone)
    near = steepest_around(slope, zone)
    with open(path, "wb") as f:
        f.write(zone.header(LANDFORM_MAGIC, LANDFORM_VERSION, unit_mm, base_m))
        f.write(elev.tobytes())
        f.write(slope.tobytes())
        f.write(near.tobytes())
    print(f"{path}: {zone.width}x{zone.height} cells of {zone.step_udeg} udeg,"
          f" {100 * valid.mean():.0f} % with data, "
          f"{os.path.getsize(path) / 1e6:.1f} MB")


def stretch(v):
    """8-bit grey, 1..255 (0 = no data), 1-99 % percentiles stretched."""
    good = v[~np.isnan(v)]
    lo, hi = np.percentile(good, [STRETCH_LOW, STRETCH_HIGH])
    g = np.clip((v - lo) / max(hi - lo, 1e-6), 0, 1)
    out = np.where(np.isnan(v), IMAGERY_NODATA, 1 + np.rint(g * 254))
    return out.astype(np.uint8)


def match(grey, zone, reference, ref_zone):
    """`grey` linearly matched to `reference` (mean, spread) where both have
    data: the seam between two images does not show as a frame."""
    lat, lon = zone.centres()
    s = ref_zone.step_udeg / UDEG_PER_DEG
    r = np.floor((ref_zone.north_udeg / UDEG_PER_DEG - lat) / s).astype(np.int64)
    c = np.floor((lon - ref_zone.west_udeg / UDEG_PER_DEG) / s).astype(np.int64)
    rows, cols = (r >= 0) & (r < ref_zone.height), (c >= 0) & (c < ref_zone.width)
    if not rows.any() or not cols.any():
        return grey
    ref = np.zeros_like(grey)
    ref[np.ix_(rows, cols)] = reference[np.ix_(r[rows], c[cols])]
    both = (grey != IMAGERY_NODATA) & (ref != IMAGERY_NODATA)
    if both.sum() < 100:
        return grey
    a, b = grey[both].astype(np.float64), ref[both].astype(np.float64)
    gain = b.std() / max(a.std(), 1e-6)
    v = (grey.astype(np.float64) - a.mean()) * gain + b.mean()
    out = np.where(grey == IMAGERY_NODATA, IMAGERY_NODATA, np.clip(np.rint(v), 1, 255))
    return out.astype(np.uint8)


def write_imagery(path, zone, grey):
    with open(path, "wb") as f:
        f.write(zone.header(IMAGERY_MAGIC, IMAGERY_VERSION))
        f.write(grey.tobytes())
        f.write(b"\0" * (-f.tell() % 4))  # the next blob stays 4 bytes aligned
    print(f"{path}: {zone.width}x{zone.height} pixels of {zone.step_udeg} "
          f"udeg, {os.path.getsize(path) / 1e6:.1f} MB")


# Products //

def make_relief(site, out, cache):
    print("relief: MOLA 32 px/deg (context)")
    zone = Zone(site, site.get("context_radius_km", 500), M_PER_DEG / 32)
    z = np.full((zone.height, zone.width), np.nan, np.float32)
    mola_raster().sample(zone, z)
    write_relief(os.path.join(out, "relief_mola.bin"), zone, z)

    print("relief: HRSC-MOLA blend 200 m")
    label = http_text(S3 + HRSC + ".lbl")
    zone = Zone(site, site.get("zone_radius_km", 60), 200.0)
    z = np.full((zone.height, zone.width), np.nan, np.float32)
    CubeRaster(S3 + HRSC + ".tif", label, "<i2", NODATA_M).sample(zone, z)
    write_relief(os.path.join(out, "relief_hrsc.bin"), zone, z)

    print("relief: CTX DTM 20 m (Mars 2020 TRN)")
    zone = Zone(site, site.get("zone_radius_km", 60), 20.0)
    ctx = tiff_raster(download(S3 + CTX_DTM, os.path.join(cache, "ctx_dtm.tif")))
    if zone.clip(*ctx.footprint()):
        z = np.full((zone.height, zone.width), np.nan, np.float32)
        ctx.sample(zone, z)
        write_relief(os.path.join(out, "relief_ctx.bin"), zone, z)
    else:
        print("  the site is outside the CTX DTM: skipped")
    del ctx

    print(f"relief: HiRISE DTM 1 m averaged to {HIRISE_STEP_M:g} m")
    zone = Zone(site, site.get("fine_radius_km", 5), HIRISE_STEP_M)
    z = np.full((zone.height, zone.width), np.nan, np.float32)
    listing = http_text(S3 + "?list-type=2&prefix=" + HIRISE_PREFIX)
    for key in re.findall(r"<Key>([^<]+_1m_[^<]+\.tif)</Key>", listing):
        meta = http_text(S3 + key[:-4] + ".xml")
        bc = {k: float(re.search(rf"<{k}>([-0-9.]+)</{k}>", meta)[1])
              for k in ("westbc", "eastbc", "southbc", "northbc")}
        probe = Zone(site, site.get("fine_radius_km", 5), HIRISE_STEP_M)
        if not probe.clip(bc["westbc"], bc["eastbc"], bc["southbc"],
                          bc["northbc"]):
            continue
        name = key.rsplit("/", 1)[-1]
        tile = tiff_raster(download(S3 + key, os.path.join(cache, name)),
                           block=HIRISE_BLOCK)
        tile.sample(zone, z)
        del tile
    write_relief(os.path.join(out, "relief_hirise.bin"), zone, z)


def box_mean(a, half):
    """Mean over a (2 half + 1)^2 box, holes (NaN) left out."""
    valid = ~np.isnan(a)
    v = np.where(valid, a, 0.0).astype(np.float64)
    def box(x):
        c = np.pad(x, half + 1, mode="edge").cumsum(0).cumsum(1)
        k = 2 * half + 1
        return c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]
    total, count = box(v), box(valid.astype(np.float64))
    h, w = a.shape
    return (total / np.maximum(count, 1))[:h, :w]


def make_ground(site, out):
    """Thermal inertia: loose, dusty or soft ground has a low one."""
    print("ground: TES thermal inertia (Putzig & Mellon 2007) refined by "
          "THEMIS night IR 100 m")
    zone = Zone(site, site.get("zone_radius_km", 60), 100.0)
    tes = CubeRaster(TES_TI, None, ">i2", 0,
                     (-180 * M_PER_DEG, 90 * M_PER_DEG, M_PER_DEG / 20, 7200,
                      3600, 0))
    ti = np.full((zone.height, zone.width), np.nan, np.float32)
    tes.sample(zone, ti)
    ti[ti <= 0] = np.nan
    label = http_text(S3 + THEMIS_NIGHT + ".lbl")
    night = np.full((zone.height, zone.width), np.nan, np.float32)
    CubeRaster(S3 + THEMIS_NIGHT + ".tif", label, "u1", 0).sample(zone, night)
    half = max(1, round(GROUND_BOX_M / 2 / 100.0))
    ratio = night / box_mean(night, half)
    fine = ti * np.where(np.isnan(ratio), 1.0, ratio) ** GROUND_DOWNSCALE_EXPONENT
    value = np.clip(np.rint(fine / GROUND_TIU_PER_STEP), 1, 255)
    grey = np.where(np.isnan(fine), 0, value).astype(np.uint8)
    path = os.path.join(out, "ground_ti.bin")
    with open(path, "wb") as f:
        f.write(zone.header(GROUND_MAGIC, GROUND_VERSION))
        f.write(grey.tobytes())
        f.write(b"\0" * (-f.tell() % 4))
    good = fine[~np.isnan(fine)]
    print(f"{path}: {zone.width}x{zone.height} pixels, inertia "
          f"{np.percentile(good, 1):.0f}..{np.percentile(good, 99):.0f} tiu "
          f"(1-99 %), {os.path.getsize(path) / 1e6:.1f} MB")


def make_imagery(site, out, cache):
    print("imagery: CTX orthomosaic 6 m (Mars 2020 TRN)")
    zone = Zone(site, site.get("imagery_radius_km", 15), 6.0)
    ctx = tiff_raster(download(S3 + CTX_ORTHO, os.path.join(cache, "ctx_ortho.tif")),
                      nodata_zero=True)
    reference = None
    if zone.clip(*ctx.footprint()):
        v = np.full((zone.height, zone.width), np.nan, np.float32)
        ctx.sample(zone, v)
        reference = (stretch(v), zone)
        write_imagery(os.path.join(out, "imagery_ctx.bin"), zone, reference[0])
    del ctx

    print("imagery: THEMIS Day IR 100 m")
    label = http_text(S3 + THEMIS + ".lbl")
    zone = Zone(site, site.get("zone_radius_km", 60), 100.0)
    v = np.full((zone.height, zone.width), np.nan, np.float32)
    CubeRaster(S3 + THEMIS + ".tif", label, "u1", 0).sample(zone, v)
    grey = stretch(v)
    if reference is not None:
        grey = match(grey, zone, *reference)
    write_imagery(os.path.join(out, "imagery_themis.bin"), zone, grey)


def mcd_query(site, lat, lon, variables):
    """24 hourly values of up to 4 MCD variables, 2 m above the ground."""
    names = list(variables) + ["none"] * (4 - len(variables))
    query = {
        "var1": names[0], "var2": names[1], "var3": names[2], "var4": names[3],
        "datekeyhtml": "1", "ls": f"{site.get('solar_longitude_deg', 0):g}",
        "localtime": "all", "latitude": f"{lat:g}", "longitude": f"{lon:g}",
        "altitude": "2", "zkey": "3", "spacecraft": "none", "isfixedlt": "off",
        "dust": "1", "hrkey": "1", "zonmean": "off", "diumean": "off",
        "averaging": "off", "dpi": "80", "islog": "off", "colorm": "jet",
        "proj": "cyl", "iswind": "off",
    }
    for key in ("year", "month", "day", "hours", "minutes", "seconds",
                "julian", "martianyear", "sol", "minval", "maxval", "plat",
                "plon", "trans", "latpoint", "lonpoint"):
        query[key] = ""
    page = http_text(MCD + "cgi-bin/mcdcgi.py?" + urllib.parse.urlencode(query))
    txt = re.search(r"txt/([0-9a-f]+)\.txt", page)
    if not txt:
        raise RuntimeError(f"MCD gave no data at {lat}, {lon}")
    text = http_text(MCD + "txt/" + txt[1] + ".txt")
    blocks = []
    for block in re.split(r"\n#{20,}\n(?:###.*\n)+#{20,}\n", "\n" + text):
        rows = [line.split() for line in block.strip().splitlines()
                if line.strip() and not line.startswith("#")]
        if rows:
            blocks.append({round(float(h)): float(v) for h, v in rows})
    if len(blocks) != len(variables):
        raise RuntimeError(f"MCD answer at {lat}, {lon}: {len(blocks)} blocks")
    return [[blocks[k][h] for k in range(len(variables))]
            for h in range(HOURS_PER_SOL)]


def mcd_cell(site, lat, lon):
    """24 hourly (T K, ps Pa, wind m/s, wind from deg, tau) at 2 m."""
    air = mcd_query(site, lat, lon, ("t", "ps", "u", "v"))
    dust = mcd_query(site, lat, lon, ("tauref",))
    hours = []
    for (t, ps, u, v), (tau,) in zip(air, dust):
        # WMO: where the wind blows from, clockwise from true north
        from_deg = math.degrees(math.atan2(-u, -v)) % 360
        hours.append((t, ps, math.hypot(u, v), from_deg, tau))
    return hours


def make_weather(site, out):
    print(f"weather: Mars Climate Database v6.2, Ls "
          f"{site.get('solar_longitude_deg', 0):g} deg")
    radius_deg = site.get("weather_radius_km", 300) * 1000 / M_PER_DEG
    # Protocol cells: 1 deg, row 0 = 90 S, col 0 = 0 E
    row0 = max(0, math.floor(site["latitude"] - radius_deg) + 90)
    row1 = min(180, math.floor(site["latitude"] + radius_deg) + 91)
    col0 = math.floor(site["longitude"] - radius_deg)
    col1 = math.floor(site["longitude"] + radius_deg) + 1
    if col0 < 0 or col1 > 360:
        sys.exit("weather zone crosses 0 E: not supported")
    cells = [(r, c) for r in range(row0, row1) for c in range(col0, col1)]
    # MOLA elevation of each cell centre, to reduce ps to the datum
    zone = Zone({"latitude": site["latitude"], "longitude": site["longitude"]},
                radius_deg * M_PER_DEG / 1000 + 120, M_PER_DEG / 32)
    elev = np.full((zone.height, zone.width), np.nan, np.float32)
    mola_raster().sample(zone, elev)
    s = zone.step_udeg / UDEG_PER_DEG

    def elevation(lat, lon):
        r = int((zone.north_udeg / UDEG_PER_DEG - lat) / s)
        c = int((lon - zone.west_udeg / UDEG_PER_DEG) / s)
        if 0 <= r < zone.height and 0 <= c < zone.width:
            return float(np.nan_to_num(elev[r, c]))
        return 0.0

    def fetch(cell):
        r, c = cell
        lat, lon = r - 90 + 0.5, c + 0.5
        # The MCD takes east longitudes in -180..180
        return cell, mcd_cell(site, lat, (lon + 180) % 360 - 180), elevation(lat, lon)

    rows, cols = row1 - row0, col1 - col0
    data = np.zeros((HOURS_PER_SOL, rows, cols, 5), np.float64)
    valid = np.zeros((rows, cols), bool)
    with concurrent.futures.ThreadPoolExecutor(MCD_THREADS) as pool:
        for done, ((r, c), hours, z) in enumerate(pool.map(fetch, cells), 1):
            for h, (t, ps, wind, from_deg, tau) in enumerate(hours):
                datum = ps * math.exp(z / SCALE_HEIGHT_M)
                data[h, r - row0, c - col0] = (t, datum, wind, from_deg, tau)
            valid[r - row0, c - col0] = True
            print(f"\r  {done}/{len(cells)} cells", end="", flush=True)
    print()
    path = os.path.join(out, "weather_mcd.bin")
    with open(path, "wb") as f:
        f.write(struct.pack("<IHBBHHHHHH", WEATHER_MAGIC, WEATHER_VERSION,
                            HOURS_PER_SOL, 0, col0, row0, cols, rows,
                            round(site.get("solar_longitude_deg", 0) * 10), 0))
        for h in range(HOURS_PER_SOL):
            for r in range(rows):
                for c in range(cols):
                    t, p, w, from_deg, tau = data[h, r, c]
                    # t_weather_mcd_cell (10 bytes); direction in 1/256 turn
                    f.write(struct.pack(
                        "<HHHBBBB", min(65535, round(t * 10)),
                        min(65535, round(p * 10)), min(65535, round(w * 100)),
                        min(255, round(tau * 40)),
                        round(from_deg * 256 / 360) % 256, int(valid[r, c]), 0))
    site_r = math.floor(site["latitude"]) + 90 - row0
    site_c = math.floor(site["longitude"]) - col0
    t = data[:, site_r, site_c, 0]
    print(f"{path}: {cols}x{rows} cells x {HOURS_PER_SOL} h, "
          f"{os.path.getsize(path)} bytes; at the site T {t.min():.0f} K "
          f"(h {t.argmin()}) .. {t.max():.0f} K (h {t.argmax()})")


# Places //

def distance_m(lat1, lon1, lat2, lon2):
    """Great circle distance on the Mars sphere."""
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp, dl = p2 - p1, math.radians(lon2 - lon1)
    h = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * MARS_RADIUS_M * math.asin(min(1.0, math.sqrt(h)))


def gazetteer_places(site, radius_km):
    """IAU named features whose centre lies within `radius_km`."""
    page = http_text(GAZETTEER)
    places = []
    for row in re.findall(r"<tr[^>]*>(.*?)</tr>", page, re.S):
        def cells(name):
            return [re.sub(r"<[^>]+>|\s+", " ", c).strip() for c in
                    re.findall(rf'class="{name}[^"]*"[^>]*>(.*?)</td>', row, re.S)]
        name, centre = cells("cleanFeatureNameColumn"), cells("centerLatLonColumn")
        if not name or len(centre) < 2:
            continue
        try:
            lat, lon = float(centre[0]), float(centre[1]) % 360
            diameter = float((cells("diameterColumn") or ["0"])[0] or 0)
        except ValueError:
            continue
        if distance_m(site["latitude"], site["longitude"], lat, lon) > radius_km * 1000:
            continue
        kind = (cells("featureTypeColumn") or [""])[0].split(",")[0]
        origin = (cells("originColumn") or [""])[0].rstrip(".")
        what = f"{kind} about {diameter:.0f} km across" if diameter else kind
        places.append({
            "name": name[0], "category": "feature",
            "description": f"{what} (IAU name; named after: {origin}). "
                           "Centre position from the USGS Gazetteer.",
            "latitude": round(lat, 5), "longitude": round(lon, 5)})
    return places


def mmgis_places():
    """Perseverance and Ingenuity, from NASA's Mars 2020 MMGIS map."""
    rover = json.loads(http_text(MMGIS + "M20_waypoints.json"))["features"]
    heli = json.loads(http_text(MMGIS + "m20_heli_flight_path.json"))["features"]
    first, last = rover[0]["properties"], rover[-1]["properties"]
    flights = sorted(heli, key=lambda f: f["properties"]["Flight"])
    lon0, lat0 = flights[0]["geometry"]["coordinates"][0][:2]
    lon1, lat1 = flights[-1]["geometry"]["coordinates"][-1][:2]
    return [
        {"name": "Octavia E. Butler Landing", "category": "site",
         "description": f"Perseverance landed here on 18 February 2021 "
                        f"(sol {first['sol']} localisation).",
         "latitude": round(first["lat"], 6), "longitude": round(first["lon"], 6)},
        {"name": "Perseverance rover", "category": "hardware",
         "description": f"Last position published by NASA: sol {last['sol']}, "
                        f"after {last['dist_total_m'] / 1000:.1f} km of driving.",
         "latitude": round(last["lat"], 6), "longitude": round(last["lon"], 6)},
        {"name": "Wright Brothers Field", "category": "site",
         "description": "Take-off point of Ingenuity's first flight, the first "
                        "powered flight on another planet (19 April 2021).",
         "latitude": round(lat0, 6), "longitude": round(lon0, 6)},
        {"name": "Ingenuity helicopter", "category": "hardware",
         "description": f"Resting place after its {len(flights)}th and last "
                        "flight (January 2024), a rotor blade broken. "
                        "End of the flight path published by NASA.",
         "latitude": round(lat1, 6), "longitude": round(lon1, 6)},
    ]


MISSION_TEXT = {
    "hardware": ("Heritage", "Survey {name} and document its state without "
                 "touching it: photographs from every side, dust cover, "
                 "damage. {about}"),
    "feature": ("Geology", "Reach {name} and sample the rocks at its edge; "
                "describe the layering. {about}"),
    "site": ("Heritage", "Visit {name}, photograph the site and check the "
             "hardware left around. {about}"),
}


def write_photo(path, rgb):
    """RGB (h, w, 3) uint8 -> "PHOT" blob: magic, width, height, RGB565 LE."""
    r, g, b = (rgb[..., k].astype(np.uint16) for k in range(3))
    px = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    with open(path, "wb") as f:
        f.write(struct.pack("<IHH", PHOTO_MAGIC, rgb.shape[1], rgb.shape[0]))
        f.write(px.astype("<u2").tobytes())


def cover(image):
    """Pillow image cropped to the photo's aspect ratio and resized."""
    from PIL import Image
    w, h = image.size
    target = PHOTO_W / PHOTO_H
    if w / h > target:
        nw = round(h * target)
        image = image.crop(((w - nw) // 2, 0, (w - nw) // 2 + nw, h))
    else:
        nh = round(w / target)
        image = image.crop((0, (h - nh) // 2, w, (h - nh) // 2 + nh))
    return image.convert("RGB").resize((PHOTO_W, PHOTO_H), Image.LANCZOS)


def orbital_crop(out, lat, lon, span_m):
    """Grey orbital view centred on a place from data/imagery_*.bin, or None."""
    for name, credit in (("imagery_ctx.bin", "NASA/JPL-Caltech/MSSS (CTX)"),
                         ("imagery_themis.bin", "NASA/JPL-Caltech/ASU (THEMIS)")):
        path = os.path.join(out, name)
        if not os.path.exists(path):
            continue
        head = open(path, "rb").read(28)
        _, west, north, step, w, h, _, _, _ = struct.unpack("<IIiIHHHHi", head)
        grey = np.memmap(path, np.uint8, "r", 28, (h, w))
        s = step / UDEG_PER_DEG
        dy = span_m / M_PER_DEG / 2
        dx = dy * PHOTO_W / PHOTO_H / max(math.cos(math.radians(lat)), 1e-3)
        # floor, not a cast: -0.x must stay out of the image
        rows = np.floor((north / UDEG_PER_DEG - (lat + dy - np.arange(PHOTO_H) * 2 * dy / PHOTO_H)) / s).astype(int)
        cols = np.floor(((lon - dx + np.arange(PHOTO_W) * 2 * dx / PHOTO_W) - west / UDEG_PER_DEG) / s).astype(int)
        if rows.min() < 0 or cols.min() < 0 or rows.max() >= h or cols.max() >= w:
            continue
        crop = np.asarray(grey[np.ix_(rows, cols)])
        if (crop == 0).mean() > 0.2:
            continue
        return np.repeat(crop[..., None], 3, axis=2), credit
    return None


def make_photos(places, out, cache):
    """A photo per place: a NASA photograph when there is one, else the
    orbital image around it. Paths and credits go into the place."""
    from PIL import Image
    folder = os.path.join(out, "photos")
    os.makedirs(folder, exist_ok=True)
    for p in places:
        slug = re.sub(r"[^a-z0-9]+", "_", p["name"].lower()).strip("_")
        path = os.path.join(folder, slug + ".bin")
        jpg = None
        if p["name"] in NASA_PHOTOS:
            pia, title = NASA_PHOTOS[p["name"]]
            try:
                jpg = download(NASA_IMAGE.format(pia), os.path.join(cache, pia + ".jpg"))
            except OSError as error:  # refused or offline: the orbital view
                print(f"  {pia}: {error}; orbital view instead")
        if jpg is not None:
            write_photo(path, np.asarray(cover(Image.open(jpg))))
            p["photo_credit"] = f"{title}. NASA/JPL-Caltech, {pia}."
        else:
            span = 3000.0
            m = re.search(r"about (\d+) km across", p["description"])
            if m:
                span = min(60000.0, max(3000.0, 1500.0 * float(m[1])))
            crop = orbital_crop(out, p["latitude"], p["longitude"], span)
            if crop is None:
                continue
            write_photo(path, crop[0])
            p["photo_credit"] = f"Orbital view, {span / 1000:.0f} km wide. {crop[1]}."
        p["photo"] = path
    print(f"{folder}: {sum('photo' in p for p in places)} photos")


def make_places(site, out):
    print("places: USGS Gazetteer, NASA MMGIS, curated places")
    radius_km = site.get("zone_radius_km", 60)
    places = mmgis_places() + gazetteer_places(site, radius_km)
    # Curated places of assets/ that neither source has
    with open("assets/points_of_interest.json", encoding="utf-8") as f:
        curated = json.load(f)["points_of_interest"]
    with open("assets/missions.json", encoding="utf-8") as f:
        curated += [{"name": m["name"], "description": m["description"],
                     "latitude": m["latitude"], "longitude": m["longitude"],
                     "category": "feature" if m["type"] == "Geology" else "hardware",
                     "mission_type": m["type"]}
                    for m in json.load(f)["missions"]]
    known = {p["name"].lower() for p in places}
    # "Ingenuity at Valinor Hills" is superseded by NASA's flight log
    known_words = {p["name"].split()[0].lower() for p in places
                   if p["category"] != "feature"}
    for p in curated:
        near = any(distance_m(p["latitude"], p["longitude"], q["latitude"],
                              q["longitude"]) < 300 for q in places)
        inside = distance_m(site["latitude"], site["longitude"], p["latitude"],
                            p["longitude"]) <= radius_km * 1000
        first = p["name"].split()[0].lower()
        if inside and not near and p["name"].lower() not in known and \
                first not in known_words:
            places.append(dict(p, category=p.get("category", "feature")))
    make_photos(places, out, os.path.join(out, "cache"))
    with open(os.path.join(out, "points_of_interest.json"), "w",
              encoding="utf-8") as f:
        json.dump({"points_of_interest": [
            {k: v for k, v in p.items() if k != "mission_type"} for p in places]},
            f, indent=2, ensure_ascii=False)

    missions = []
    for p in places:
        d = distance_m(site["latitude"], site["longitude"], p["latitude"],
                       p["longitude"])
        if d < 200:
            continue  # the base itself
        # A curated place may bring a category of its own
        category = p["category"] if p["category"] in MISSION_TEXT else "feature"
        work_min = WORK_MINUTES[category]
        walk_s = 2 * d * ROUTE_DETOUR / WALK_SPEED_M_S
        drive_s = 2 * d * ROUTE_DETOUR / ROVER_SPEED_M_S
        on_foot = walk_s + work_min * 60 <= EVA_S
        if not on_foot and drive_s + work_min * 60 > WORK_DAY_S:
            continue
        kind, text = MISSION_TEXT[category]
        if not on_foot:
            text += " Too far for one EVA on foot: pressurised rover traverse."
        kind = p.get("mission_type", kind)
        share = (walk_s + work_min * 60) / EVA_S if on_foot else \
            (drive_s + work_min * 60) / WORK_DAY_S
        missions.append({
            "name": p["name"], "type": kind,
            "description": text.format(name=p["name"], about=p["description"]),
            "latitude": p["latitude"], "longitude": p["longitude"],
            "duration_minutes": work_min,
            "difficulty_percent": min(95, round(20 + 70 * share))})
    missions.sort(key=lambda m: distance_m(site["latitude"], site["longitude"],
                                           m["latitude"], m["longitude"]))
    with open(os.path.join(out, "missions.json"), "w", encoding="utf-8") as f:
        json.dump({"missions": missions}, f, indent=2, ensure_ascii=False)
    print(f"{out}/points_of_interest.json: {len(places)} places; "
          f"{out}/missions.json: {len(missions)} missions within one sol")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__.splitlines()[0],
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    ap.add_argument("--site", default="assets/mission_site.json")
    ap.add_argument("--out", default="data")
    ap.add_argument("--only", default="relief,imagery,ground,weather,places",
                    help="comma separated subset of relief,imagery,ground,"
                         "weather,places")
    args = ap.parse_args()
    if sys.byteorder != "little":
        sys.exit("big endian host: the data files must be little endian")
    site = load_site(args.site)
    cache = os.path.join(args.out, "cache")
    os.makedirs(cache, exist_ok=True)
    only = set(args.only.split(","))
    print(f"site: {site.get('name', '?')} {site['latitude']:.4f} N "
          f"{site['longitude']:.4f} E")
    if "relief" in only:
        make_relief(site, args.out, cache)
    if "imagery" in only:
        make_imagery(site, args.out, cache)
    if "ground" in only:
        make_ground(site, args.out)
    if "weather" in only:
        make_weather(site, args.out)
    if "places" in only:
        make_places(site, args.out)


if __name__ == "__main__":
    main()
