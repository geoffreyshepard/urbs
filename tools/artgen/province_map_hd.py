"""The campaign's province map, painted: assets/ui/province_map.png (2560x1440).

Same projection and coastlines as the pixel-art map in worldmap.py (equirectangular, lon -10..40,
lat 56..25), so the campaign's normalised mission positions land on the same places.  Instead of
palette pixels it builds a relief in numpy and shades it like a painted atlas plate:

- land mask rasterised at 2x from worldmap's polygons and box-filtered (soft, anti-aliased coasts);
- elevation = distance inland + fractal noise + the mountain ranges (blurred ridge lines times a
  ridged noise), hill-shaded with the light from the north-west, as in the game;
- land colour by climate (green Gaul and Italy, olive Iberia and Greece, ochre and sand Africa and
  Egypt; worldmap's aridity formula), dark forest masses in the temperate north, rock and snow on the
  high ranges, a green Nile valley;
- sea by depth (turquoise shallows to deep blue), a pale line of surf along the coast, faint wind
  streaks;
- rivers, Latin names for seas and lands (Cinzel, assets/fonts), a warm paper grain and a vignette.

Deterministic (fixed numpy seeds).  Run from the repo root:
    python3 tools/artgen/province_map_hd.py
Needs numpy and Pillow.  Not packed into the atlas: the game loads it as its own texture.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import worldmap as wm  # noqa: E402  (geography: SEAS, ISLANDS, ISLETS, MOUNTAINS, RIVERS, NILE)

W, H = 2560, 1440
SS = 2
OUT = os.path.join(ROOT, "assets", "ui", "province_map.png")
FONT = os.path.join(ROOT, "assets", "fonts", "Cinzel.ttf")


def proj(lon, lat, s=1.0):
    return ((lon - wm.LON0) / (wm.LON1 - wm.LON0) * W * s, (wm.LAT1 - lat) / (wm.LAT1 - wm.LAT0) * H * s)


# ------------------------------------------------------------------ numpy helpers
def box(a, r):
    """Box blur of radius r (separable, edge-clamped), via cumulative sums."""
    if r < 1:
        return a
    for axis in (0, 1):
        p = np.pad(a, [(r + 1, r) if k == axis else (0, 0) for k in (0, 1)], mode="edge")
        c = np.cumsum(p, axis=axis, dtype=np.float64)
        if axis == 0:
            a = (c[2 * r + 1:, :] - c[:-2 * r - 1, :]) / (2 * r + 1)
        else:
            a = (c[:, 2 * r + 1:] - c[:, :-2 * r - 1]) / (2 * r + 1)
    return a.astype(np.float32)


def blur(a, r):
    """Three box passes: close to a Gaussian of sigma ~ r."""
    r = max(1, int(round(r * 0.58)))
    return box(box(box(a, r), r), r)


def noise(seed, cell, aspect=1.0):
    """Value noise on a lattice of `cell` px (stretched `aspect` times along x), bicubic, 0..1."""
    rng = np.random.default_rng(seed)
    gw, gh = int(W / (cell * aspect)) + 3, int(H / cell) + 3
    g = (rng.random((gh, gw)) * 255).astype(np.uint8)
    im = Image.fromarray(g, "L").resize((int(gw * cell * aspect), gh * cell), Image.BICUBIC)
    return np.asarray(im, dtype=np.float32)[:H, :W] / 255.0


def fbm(seed, cell, octaves=5, aspect=1.0):
    acc, amp, tot = np.zeros((H, W), np.float32), 1.0, 0.0
    for o in range(octaves):
        c = max(2, cell >> o)
        acc += amp * noise(seed * 31 + o, c, aspect)
        tot += amp
        amp *= 0.55
    return acc / tot


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def mix(a, b, t):
    t = t[..., None] if np.ndim(t) == 2 else t
    return a * (1 - t) + b * t


def rgb(*c):
    return np.array(c, np.float32) / 255.0


# ------------------------------------------------------------------ geography rasters
def land_mask():
    big = Image.new("L", (W * SS, H * SS), 255)
    d = ImageDraw.Draw(big)
    for poly in wm.SEAS:
        d.polygon([proj(lo, la, SS) for lo, la in poly], fill=0)
    for poly in wm.ISLANDS.values():
        d.polygon([proj(lo, la, SS) for lo, la in poly], fill=255)
    for (lo, la, r) in wm.ISLETS:
        x0, y0 = proj(lo - r, la + r * 0.8, SS)
        x1, y1 = proj(lo + r, la - r * 0.8, SS)
        d.ellipse([x0, y0, x1, y1], fill=255)
    for seg in (((26.2, 40.05), (26.6, 40.35)), ((28.9, 41.0), (29.05, 41.2))):
        d.line([proj(*seg[0], SS), proj(*seg[1], SS)], fill=0, width=SS * 6)
    m = np.asarray(big.resize((W, H), Image.BOX), dtype=np.float32) / 255.0
    # Coastlines from polygons are straight between vertices: roughen them a little with noise.
    # Polygon coasts are straight between vertices: soften them, then let noise bite bays and capes.
    m = blur(m, 7.0)
    wobble = (fbm(5, 40, 4) - 0.5) * 0.9
    m = smooth(0.5 - 0.06, 0.5 + 0.06, m + wobble * (1 - np.abs(m - 0.5) * 2) * 0.5)
    return m.astype(np.float32)


def lines_raster(polylines, width, s=SS):
    im = Image.new("L", (W * s, H * s), 0)
    d = ImageDraw.Draw(im)
    for pts in polylines:
        xy = [proj(lo, la, s) for lo, la in pts]
        d.line(xy, fill=255, width=int(width * s), joint="curve")
        r = width * s / 2
        for (x, y) in (xy[0], xy[-1]):
            d.ellipse([x - r, y - r, x + r, y + r], fill=255)
    return np.asarray(im.resize((W, H), Image.BOX), dtype=np.float32) / 255.0


def lonlat_grids():
    xs = (np.arange(W, dtype=np.float32) + 0.5) / W
    ys = (np.arange(H, dtype=np.float32) + 0.5) / H
    lon = wm.LON0 + xs * (wm.LON1 - wm.LON0)
    lat = wm.LAT1 - ys * (wm.LAT1 - wm.LAT0)
    return np.meshgrid(lon, lat)


# ------------------------------------------------------------------ painting
def paint():
    land = land_mask()
    sea = 1.0 - land
    lon, lat = lonlat_grids()

    # --- elevation: gentle rolling ground, steeper ranges
    inland = 1.0 - blur(sea, 60)                       # 0 at the coast .. 1 deep inland
    hills = fbm(11, 220, 5)
    ridged = (1.0 - np.abs(fbm(12, 22, 4) * 2 - 1)) ** 2
    ridge = blur(lines_raster(wm.MOUNTAINS.values(), 30), 30)
    ridge = smooth(0.0, 0.8, ridge / max(1e-3, ridge.max()))
    core = blur(lines_raster(wm.MOUNTAINS.values(), 8), 12)
    core = core / max(1e-3, core.max())
    snowy = blur(lines_raster([wm.MOUNTAINS[k] for k in ("alps", "pyrenees", "taurus", "atlas")], 10), 8)
    snowy = snowy / max(1e-3, snowy.max())
    mountains = ridge * (0.15 + 0.55 * ridged) + 0.35 * core * (0.5 + 0.5 * ridged)
    rolling = smooth(0.35, 0.95, hills) * inland

    # --- hill shading, light from the north-west and above
    def hillshade(e, k):
        gy, gx = np.gradient(e * k)
        nx, ny = -gx, -gy
        norm = np.sqrt(nx * nx + ny * ny + 1.0)
        L = np.array([-0.6, -0.6, 0.53], np.float32)
        L /= np.linalg.norm(L)
        return (nx * L[0] + ny * L[1] + L[2]) / norm / L[2]   # 1 on flat ground
    # --- climate (worldmap's aridity, smooth)
    arid = (44.5 - lat) / 4.0 + np.maximum(0.0, lon - 33.0) / 6.0
    arid += np.maximum(0.0, 33.5 - lat) * 0.55
    arid += np.clip((37.6 - lat) / 1.8, 0, 1) * np.clip((lon - 34.0) / 1.5, 0, 1)
    arid += 0.4 * smooth(0.0, -2.5, lon) * smooth(36.5, 38.0, lat) * smooth(43.0, 41.5, lat)   # dry meseta
    arid += (fbm(21, 220, 4) - 0.5) * 1.1
    t = np.clip((0.6 + arid) / 6.0, 0.0, 1.0)

    nile = blur(lines_raster([wm.NILE] + wm.NILE_BRANCHES, 14), 10)
    nile = np.clip(nile / max(1e-3, nile.max()) * 1.6, 0, 1)
    t = t * (1 - 0.85 * nile)
    delta = np.clip(blur(lines_raster(wm.NILE_BRANCHES, 60), 30) * 2.2, 0, 1)
    t = t * (1 - 0.6 * delta)

    flat = 1.0 - 0.75 * smooth(0.6, 0.85, t)          # deserts are flat apart from the ranges
    elev = blur((0.10 * rolling * flat + 0.55 * mountains) * land, 1.5)
    shade = np.clip(hillshade(elev, 55.0), 0.5, 1.3)

    stops = [(0.00, rgb(78, 112, 52)), (0.16, rgb(104, 132, 62)), (0.30, rgb(132, 146, 74)),
             (0.44, rgb(160, 152, 88)), (0.58, rgb(192, 164, 104)), (0.74, rgb(218, 186, 128)),
             (1.00, rgb(232, 204, 150))]
    col = np.zeros((H, W, 3), np.float32)
    for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
        k = np.clip((t - t0) / (t1 - t0), 0, 1)
        inside = (t >= t0) & (t <= t1)
        col[inside] = (c0 * (1 - k[..., None]) + c1 * k[..., None])[inside]

    # brushy variation inside each climate band
    brush = fbm(31, 46, 4, aspect=1.8)
    col *= (0.92 + 0.16 * brush)[..., None]

    # forests: dark canopy masses in the temperate north and on the hills
    temperate = smooth(0.42, 0.18, t)
    fmask = smooth(0.52, 0.62, fbm(41, 90, 5) + 0.15 * ridge) * temperate
    canopy = 0.8 + 0.4 * noise(42, 3)
    forest = rgb(54, 86, 44) * canopy[..., None]
    col = mix(col, forest, fmask * 0.85)

    # dunes: soft ripples in the deep desert
    dunes = smooth(0.80, 0.92, t) * (np.sin((lon * 9 + fbm(51, 60, 3) * 12) + lat * 2) * 0.5 + 0.5)
    col = mix(col, rgb(238, 214, 164), dunes * 0.35)

    # rock and snow on the ranges
    rock = smooth(0.14, 0.40, elev)
    col = mix(col, rgb(150, 134, 110) * (0.9 + 0.2 * hills)[..., None], rock * 0.8)
    snow = smooth(0.36, 0.50, elev + 0.06 * noise(61, 6)) * snowy
    col = mix(col, rgb(246, 244, 236), snow * 0.85)

    col *= shade[..., None]
    # a pale beach rim along the coast
    rim = np.clip(blur(sea, 3) * 2.2, 0, 1) * land
    col = mix(col, rgb(222, 204, 160), rim * 0.55)

    # --- sea
    near = blur(land, 6)
    mid = blur(land, 30)
    far = blur(land, 120)
    depth = np.clip(1.0 - (near * 1.6 + mid * 1.4 + far * 1.3) + (fbm(73, 120, 4) - 0.5) * 0.25, 0.0, 1.0)
    sea_col = mix(rgb(104, 174, 176), rgb(54, 118, 152), smooth(0.0, 0.35, depth))
    sea_col = mix(sea_col, rgb(30, 70, 112), smooth(0.3, 1.0, depth))
    streaks = fbm(71, 30, 3, aspect=6.0)
    sea_col *= (0.95 + 0.1 * streaks)[..., None]
    # surf: a pale line just off the shore, broken by noise
    surf = smooth(0.08, 0.3, blur(land, 2.5)) * (1 - land) * (0.55 + 0.45 * noise(72, 8))
    sea_col = mix(sea_col, rgb(214, 232, 226), np.clip(surf, 0, 1) * 0.75)
    col = mix(sea_col, col, land)

    # --- rivers
    riv = lines_raster(wm.RIVERS, 4.2) * land
    niler = lines_raster([wm.NILE] + wm.NILE_BRANCHES, 3.8) * land
    col = mix(col, rgb(66, 124, 152), np.clip(riv + niler, 0, 1) * 0.9)

    # --- paper: warm toning, grain, vignette
    col = mix(col, rgb(236, 222, 190), np.full((H, W), 0.08, np.float32))
    grain = noise(81, 2) - 0.5
    col *= (1.0 + 0.05 * grain)[..., None]
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    vx, vy = (xx / W - 0.5) * 2, (yy / H - 0.5) * 2
    vig = np.clip(1.0 - 0.22 * (vx ** 2 + vy ** 2) ** 1.5, 0.6, 1.0)
    col *= vig[..., None]
    col = mix(col, rgb(92, 64, 40), (1 - vig) * 0.5)
    return np.clip(col, 0, 1), land


# ------------------------------------------------------------------ names
SEAS_NAMES = [  # (text, lon, lat, size, angle, letter spacing)
    ("MARE  INTERNVM", 17.2, 33.9, 46, 0, 0.42),
    ("OCEANVS", -5.6, 46.6, 32, 0, 0.35),
    ("ATLANTICVS", -5.6, 45.4, 32, 0, 0.35),
    ("PONTVS  EVXINVS", 34.6, 43.5, 30, 0, 0.35),
    ("MARE  GERMANICVM", 3.6, 55.0, 28, 0, 0.35),
    ("MARE  TYRRHENVM", 11.5, 39.6, 22, 0, 0.3),
    ("MARE  ADRIATICVM", 16.2, 42.9, 20, -36, 0.25),
    ("MARE  AEGAEVM", 25.0, 38.6, 18, -70, 0.2),
]
LAND_NAMES = [
    ("GALLIA", 2.4, 46.7, 40, 0, 0.5),
    ("HISPANIA", -4.3, 40.3, 40, 0, 0.5),
    ("GERMANIA", 11.6, 51.6, 36, 0, 0.5),
    ("BRITANNIA", -3.3, 53.6, 24, 0, 0.35),
    ("ITALIA", 11.9, 42.9, 22, -45, 0.35),
    ("AFRICA", 9.0, 30.9, 40, 0, 0.6),
    ("AEGYPTVS", 29.0, 27.8, 34, 0, 0.5),
    ("ASIA", 31.0, 39.3, 36, 0, 0.6),
    ("SYRIA", 38.0, 34.2, 28, 0, 0.45),
    ("DACIA", 24.5, 46.6, 30, 0, 0.5),
    ("PANNONIA", 18.0, 47.2, 26, 0, 0.4),
    ("MAVRETANIA", -4.8, 33.0, 26, 0, 0.4),
]


def draw_names(img):
    for names, fill in ((SEAS_NAMES, (222, 238, 238, 150)), (LAND_NAMES, (60, 38, 20, 150))):
        for (text, lo, la, size, angle, spacing) in names:
            font = ImageFont.truetype(FONT, size)
            widths = [font.getlength(ch) for ch in text]
            gap = size * spacing
            tw = sum(widths) + gap * (len(text) - 1)
            layer = Image.new("RGBA", (int(tw + size * 2), int(size * 2.2)), (0, 0, 0, 0))
            d = ImageDraw.Draw(layer)
            x = size
            for ch, w in zip(text, widths):
                d.text((x, size * 0.4), ch, font=font, fill=fill)
                x += w + gap
            if names is LAND_NAMES:   # a soft light halo so the names read over relief
                a = np.asarray(layer)[..., 3].astype(np.float32)
                halo = np.clip(np.asarray(Image.fromarray(a.astype(np.uint8)).filter(ImageFilter.GaussianBlur(size * 0.18)),
                                          np.float32) * 1.4, 0, 255)
                under = np.zeros(a.shape + (4,), np.uint8)
                under[..., :3] = (236, 222, 186)
                under[..., 3] = (halo * 0.55).astype(np.uint8)
                glow = Image.fromarray(under, "RGBA")
                glow.alpha_composite(layer)
                layer = glow
            if angle:
                layer = layer.rotate(angle, resample=Image.BICUBIC, expand=True)
            cx, cy = proj(lo, la)
            img.alpha_composite(layer, (int(cx - layer.width / 2), int(cy - layer.height / 2)))


def compass(img):
    """A small wind rose in the empty Atlantic corner."""
    cx, cy = proj(-6.6, 29.4)
    r = 70
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    ink, light = (64, 44, 28, 170), (236, 224, 196, 170)
    d.ellipse([cx - r * 0.62, cy - r * 0.62, cx + r * 0.62, cy + r * 0.62], outline=ink, width=2)
    for k in range(8):
        a = k * math.pi / 4
        ln = r if k % 2 == 0 else r * 0.55
        tip = (cx + math.sin(a) * ln, cy - math.cos(a) * ln)
        side = r * 0.12
        left = (cx + math.sin(a - math.pi / 2) * side, cy - math.cos(a - math.pi / 2) * side)
        right = (cx + math.sin(a + math.pi / 2) * side, cy - math.cos(a + math.pi / 2) * side)
        d.polygon([tip, left, (cx, cy)], fill=ink)
        d.polygon([tip, right, (cx, cy)], fill=light, outline=ink)
    font = ImageFont.truetype(FONT, 26)
    d.text((cx, cy - r - 22), "N", font=font, fill=ink, anchor="mm")
    img.alpha_composite(layer)


def main():
    col, _ = paint()
    img = Image.fromarray((col * 255 + 0.5).astype(np.uint8), "RGB").convert("RGBA")
    draw_names(img)
    compass(img)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.convert("RGB").save(OUT, optimize=True)
    print("wrote %s (%dx%d)" % (os.path.relpath(OUT, ROOT), W, H))


if __name__ == "__main__":
    main()
