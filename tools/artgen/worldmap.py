"""ui/province_map: 512x288 pixel-art map of the Roman Mediterranean world.

Equirectangular: lon -10..40 -> x 0..512, lat 56..25 -> y 0..288.
The world starts as land; seas are carved from coastline polygons
(lon, lat), islands are added back on top.  Polygons are rasterised at 4x
and reduced by majority, so coastlines stay clean.  Colours come only from
the palette; sea depth, land climate and relief use ordered dithering.

Also exposes PLACES (normalised x, y of named places, snapped onto land) for
assets/sprites/province_places.json.
"""
import math

from PIL import Image, ImageDraw

from palette import C
from primitives import new, px, hash2

W, H = 512, 288
LON0, LON1, LAT0, LAT1 = -10.0, 40.0, 25.0, 56.0
SS = 4                                   # supersampling for rasterisation

BAYER = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))


def proj(lon, lat, s=1):
    return ((lon - LON0) / (LON1 - LON0) * W * s, (LAT1 - lat) / (LAT1 - LAT0) * H * s)


# ------------------------------------------------------------ geography
ATLANTIC = [
    (-5.6, 36.0), (-6.3, 36.5), (-7.4, 37.2), (-8.9, 37.0), (-8.8, 38.5), (-9.5, 38.7),
    (-8.9, 40.2), (-8.7, 41.2), (-8.9, 42.4), (-9.3, 43.0), (-8.0, 43.7), (-5.8, 43.6),
    (-3.8, 43.45), (-1.8, 43.4), (-1.3, 44.5), (-1.2, 45.6), (-1.4, 46.3), (-2.2, 47.1),
    (-3.2, 47.6), (-4.4, 47.8), (-4.7, 48.4), (-3.5, 48.8), (-2.0, 48.6), (-1.6, 48.7),
    (-1.9, 49.7), (-1.2, 49.4), (0.2, 49.45), (1.0, 49.9), (1.6, 50.3), (1.9, 51.0),
    (3.2, 51.35), (4.0, 51.9), (4.7, 52.9), (5.6, 53.3), (6.9, 53.45), (8.0, 53.6),
    (8.6, 53.9), (8.9, 54.4), (8.6, 55.0), (8.2, 55.6), (8.1, 56.1),
    (-10.5, 56.1), (-10.5, 28.5), (-9.8, 29.5), (-9.6, 30.5), (-9.8, 31.4), (-9.3, 32.5),
    (-8.5, 33.3), (-7.6, 33.6), (-6.8, 34.05), (-6.2, 35.0), (-5.9, 35.8),
]

MEDITERRANEAN = [
    (-5.6, 36.0), (-5.35, 36.15), (-4.4, 36.7), (-3.0, 36.75), (-2.4, 36.8), (-2.0, 36.7),
    (-1.3, 37.5), (-0.7, 37.6), (-0.5, 38.3), (0.2, 38.75), (-0.2, 39.3), (-0.3, 39.5),
    (0.1, 40.1), (0.9, 40.7), (1.2, 41.1), (2.2, 41.4), (3.2, 41.9), (3.3, 42.3),
    (3.05, 42.8), (3.1, 43.1), (3.8, 43.45), (4.6, 43.4), (5.0, 43.35), (5.4, 43.3),
    (5.9, 43.1), (6.6, 43.2), (7.3, 43.7), (8.2, 43.95), (8.9, 44.4), (9.8, 44.1),
    (10.3, 43.6), (10.5, 42.95), (11.1, 42.45), (11.8, 42.1), (12.3, 41.7), (12.65, 41.45),
    (13.05, 41.25), (13.6, 41.2), (14.0, 40.95), (14.25, 40.83), (14.4, 40.6), (14.75, 40.65),
    (15.0, 40.35), (15.6, 40.05), (15.8, 39.65), (16.1, 38.9), (15.65, 38.25), (15.75, 37.95),
    (16.1, 38.0), (16.6, 38.45), (17.15, 39.0), (17.0, 39.4), (16.55, 39.7), (17.0, 40.45),
    (17.6, 40.3), (18.0, 40.1), (18.4, 39.8), (18.5, 40.15), (17.95, 40.65), (16.9, 41.15),
    (16.1, 41.45), (16.15, 41.9), (15.7, 41.95), (15.0, 42.0), (14.2, 42.45), (13.6, 43.5),
    (12.6, 44.05), (12.3, 44.45), (12.55, 44.95), (12.3, 45.4), (12.9, 45.65), (13.4, 45.72),
    (13.75, 45.62), (13.55, 45.15), (13.85, 44.85), (14.3, 45.3), (14.9, 45.05), (15.2, 44.15),
    (15.95, 43.6), (16.4, 43.5), (17.4, 43.05), (18.1, 42.65), (18.7, 42.35), (19.4, 41.9),
    (19.45, 41.3), (19.4, 40.5), (19.95, 39.95), (20.2, 39.5), (20.7, 39.0), (21.1, 38.4),
    (21.6, 38.4), (22.3, 38.45), (22.95, 38.12), (22.9, 37.97), (22.1, 38.22), (21.75, 38.22),
    (21.4, 38.2),
    (21.3, 37.65), (21.6, 37.1), (21.7, 36.8), (22.1, 37.0), (22.4, 36.4), (22.6, 36.8),
    (23.05, 36.45), (23.1, 36.85), (22.75, 37.5), (23.2, 37.35), (23.4, 37.6), (23.1, 37.9),
    (23.7, 37.95), (24.0, 37.65), (24.1, 38.1), (24.55, 38.0), (24.2, 38.65), (23.4, 39.0),
    (22.9, 39.35), (23.3, 39.25), (22.6, 40.0), (22.9, 40.55), (23.4, 40.25), (23.7, 39.95),
    (23.9, 40.35), (24.4, 40.9), (25.3, 40.9), (26.0, 40.75), (26.2, 40.4), (26.2, 40.05),
    (26.25, 39.95), (26.1, 39.5), (26.8, 39.3), (26.85, 38.8), (27.1, 38.45), (26.5, 38.35),
    (27.25, 37.95), (27.3, 37.5), (27.45, 37.0), (28.0, 36.8), (28.3, 36.65), (29.1, 36.6),
    (29.7, 36.15), (30.5, 36.35), (30.65, 36.85), (31.5, 36.7), (32.3, 36.2), (33.0, 36.1),
    (34.0, 36.3), (34.6, 36.8), (35.5, 36.6), (36.15, 36.85), (35.95, 36.15), (35.8, 35.5),
    (35.85, 34.9), (35.85, 34.4), (35.5, 33.9), (35.2, 33.25), (34.95, 32.8), (34.75, 32.1),
    (34.5, 31.6), (34.2, 31.3), (33.0, 31.1), (32.3, 31.25), (31.9, 31.5), (31.0, 31.6),
    (30.4, 31.5), (29.9, 31.2), (29.0, 30.9), (28.0, 31.1), (27.2, 31.35), (25.1, 31.6),
    (24.0, 32.0), (23.0, 32.6), (22.5, 32.85), (21.7, 32.95), (20.6, 32.6), (20.05, 32.1),
    (20.0, 31.3), (19.6, 30.5), (18.5, 30.7), (17.5, 30.9), (16.6, 31.2), (15.4, 31.9),
    (15.2, 32.35), (14.2, 32.65), (13.2, 32.9), (12.0, 32.9), (11.1, 33.2), (10.8, 33.6),
    (10.1, 33.85), (10.4, 34.3), (10.75, 34.7), (11.1, 35.25), (10.75, 35.75), (10.55, 36.35),
    (11.05, 36.9), (10.7, 36.9), (10.3, 36.75), (10.25, 37.2), (9.85, 37.3), (9.0, 37.15),
    (8.6, 36.95), (7.8, 36.95), (7.2, 37.05), (6.9, 36.9), (5.6, 36.8), (5.1, 36.75),
    (3.8, 36.9), (3.05, 36.75), (2.0, 36.55), (1.3, 36.5), (0.1, 35.9), (-0.65, 35.75),
    (-1.3, 35.3), (-2.0, 35.1), (-2.95, 35.3), (-4.3, 35.2), (-5.3, 35.9), (-5.8, 35.8),
]

BLACK_SEA = [
    (29.0, 41.15), (30.2, 41.2), (30.6, 41.1), (31.5, 41.35), (32.4, 41.75), (33.3, 42.0),
    (34.0, 41.98), (35.15, 42.05), (35.9, 41.7), (36.35, 41.3), (37.0, 41.15), (37.9, 41.0),
    (39.7, 41.0), (40.5, 41.1), (40.5, 43.3), (39.7, 43.6), (38.3, 44.4), (37.8, 44.7),
    (36.8, 45.15), (36.6, 45.35), (37.0, 45.6), (38.2, 46.25), (39.3, 47.2), (38.0, 47.1),
    (36.8, 46.7), (35.5, 46.3), (35.1, 45.65), (35.4, 45.1), (34.4, 44.5), (33.5, 44.55),
    (33.6, 45.15), (32.5, 45.4), (33.6, 46.0), (32.0, 46.5), (31.0, 46.6), (30.7, 46.5),
    (30.1, 45.8), (29.7, 45.2), (28.65, 44.2), (27.9, 43.2), (27.5, 42.5), (28.0, 41.9),
    (28.9, 41.3),
]

MARMARA = [
    (26.6, 40.35), (27.5, 40.95), (28.6, 41.0), (29.0, 40.95), (29.4, 40.75), (29.0, 40.4),
    (28.0, 40.35), (27.0, 40.35),
]

BALTIC = [
    (10.0, 56.1), (10.0, 54.6), (11.0, 54.1), (12.5, 54.4), (14.2, 54.0), (16.5, 54.5),
    (18.6, 54.4), (19.8, 54.5), (21.1, 55.3), (21.0, 56.1), (16.0, 56.1), (14.3, 55.5),
    (13.0, 55.4), (12.6, 56.1),
]

RED_SEA = [
    (32.55, 29.95), (32.7, 29.4), (33.2, 28.4), (33.6, 27.6), (33.95, 26.9), (34.3, 26.1),
    (34.9, 24.5), (37.4, 24.5), (36.6, 26.0), (35.7, 27.35), (35.0, 28.1), (34.95, 29.5),
    (34.65, 28.9), (34.3, 27.75), (33.6, 28.3), (33.0, 29.1),
]

SEAS = [ATLANTIC, MEDITERRANEAN, BLACK_SEA, MARMARA, BALTIC, RED_SEA]

ISLANDS = {
    "britain": [(-5.7, 50.05), (-4.2, 50.35), (-3.4, 50.6), (-1.9, 50.6), (-0.8, 50.75),
                (1.0, 50.9), (1.4, 51.35), (0.6, 51.5), (1.6, 52.1), (1.75, 52.6), (1.0, 52.95),
                (0.3, 52.85), (0.3, 53.4), (-0.1, 53.6), (-0.4, 54.3), (-1.2, 54.65),
                (-1.4, 55.0), (-1.6, 55.6), (-2.1, 55.9), (-2.5, 56.2), (-5.7, 56.2),
                (-5.4, 55.6), (-5.0, 55.7), (-4.9, 55.0), (-4.4, 54.7), (-3.4, 54.9),
                (-3.45, 54.45), (-3.0, 53.9), (-3.0, 53.4), (-4.0, 53.3), (-4.6, 53.3),
                (-4.7, 52.8), (-4.1, 52.4), (-5.25, 51.75), (-4.2, 51.6), (-3.2, 51.45),
                (-3.0, 51.25), (-4.2, 51.2), (-5.0, 50.6)],
    "ireland": [(-6.4, 52.2), (-6.0, 52.95), (-6.15, 53.35), (-6.2, 54.0), (-5.5, 54.4),
                (-5.9, 55.0), (-6.2, 55.25), (-7.3, 55.35), (-8.3, 55.1), (-8.5, 54.3),
                (-9.9, 54.2), (-10.1, 53.5), (-9.0, 53.2), (-9.9, 52.5), (-10.2, 52.1),
                (-9.8, 51.6), (-8.5, 51.6), (-7.5, 52.0)],
    "corsica": [(9.4, 43.0), (9.55, 42.6), (9.45, 41.9), (9.3, 41.6), (9.2, 41.37), (8.8, 41.6),
                (8.6, 41.9), (8.65, 42.3), (8.95, 42.6), (9.3, 42.7)],
    "sardinia": [(9.2, 41.25), (9.8, 40.9), (9.65, 40.3), (9.65, 39.6), (9.55, 39.15), (9.1, 39.2),
                 (8.4, 38.9), (8.4, 39.4), (8.5, 39.9), (8.35, 40.55), (8.2, 40.9), (8.8, 40.95)],
    "sicily": [(12.4, 37.8), (12.6, 38.1), (13.35, 38.2), (14.3, 38.05), (15.65, 38.27),
               (15.25, 37.5), (15.3, 37.0), (15.1, 36.65), (14.5, 36.75), (13.6, 37.15),
               (12.6, 37.6)],
    "crete": [(23.55, 35.55), (24.1, 35.55), (25.1, 35.35), (26.3, 35.3), (26.25, 35.0),
              (25.5, 34.98), (24.7, 34.93), (23.6, 35.2)],
    "cyprus": [(32.3, 35.05), (32.9, 35.4), (33.6, 35.35), (34.55, 35.68), (34.0, 35.0),
               (33.9, 34.95), (33.0, 34.6), (32.4, 34.75)],
    "mallorca": [(2.35, 39.6), (2.9, 39.95), (3.45, 39.75), (3.2, 39.35), (2.7, 39.5)],
    "rhodes": [(27.75, 36.4), (28.25, 36.45), (28.2, 36.1), (27.75, 35.9)],
    "lesbos": [(25.85, 39.3), (26.4, 39.35), (26.6, 39.05), (26.1, 39.0)],
    "euboea_n": [(23.3, 38.95), (23.6, 38.9), (23.4, 38.7)],
}

# small islands (Aegean and others): centre lon/lat, radius in degrees
ISLETS = [(25.4, 37.05, 0.18), (25.15, 37.07, 0.12), (24.85, 37.85, 0.14), (25.15, 37.6, 0.1),
          (25.35, 37.45, 0.08), (25.42, 36.4, 0.1), (26.0, 38.38, 0.2), (26.8, 37.73, 0.17),
          (27.1, 36.85, 0.13), (25.2, 39.9, 0.17), (24.7, 40.68, 0.13), (25.55, 40.48, 0.08),
          (24.4, 37.4, 0.08), (24.05, 38.15, 0.1), (20.0, 39.6, 0.12), (20.6, 38.3, 0.15),
          (14.4, 35.9, 0.09), (10.3, 42.78, 0.1), (1.4, 38.95, 0.1), (4.1, 39.95, 0.12),
          (14.2, 40.55, 0.06), (13.95, 40.73, 0.06)]

MOUNTAINS = {
    "alps": [(5.6, 44.0), (6.8, 44.9), (7.0, 45.9), (8.5, 46.4), (10.0, 46.45), (11.5, 46.75),
             (13.0, 46.5), (14.5, 46.4), (15.8, 47.0)],
    "pyrenees": [(-1.8, 43.1), (0.0, 42.75), (1.5, 42.6), (2.9, 42.45)],
    "apennines": [(8.6, 44.45), (10.0, 44.25), (11.5, 43.8), (12.8, 43.0), (13.5, 42.3),
                  (14.3, 41.6), (15.3, 40.8), (15.9, 40.1), (16.2, 39.2)],
    "atlas": [(-9.0, 30.9), (-7.5, 31.3), (-5.5, 32.3), (-4.0, 33.0), (-2.5, 33.6),
              (-0.5, 34.3), (1.5, 35.2), (3.0, 35.8)],
    "tell_atlas": [(3.5, 36.3), (5.5, 36.2), (7.5, 35.9), (9.0, 35.6)],
    "taurus": [(29.8, 37.3), (31.5, 37.4), (33.0, 37.2), (34.8, 37.4), (36.3, 38.0), (38.0, 38.3)],
    "dinarides": [(15.3, 45.0), (16.6, 44.2), (18.0, 43.5), (19.4, 42.6), (20.6, 41.5),
                  (21.2, 40.3), (21.6, 39.3)],
    "carpathians": [(18.0, 49.4), (20.5, 49.3), (22.5, 48.8), (24.6, 47.8), (25.8, 46.4),
                    (26.0, 45.5), (24.5, 45.4), (22.8, 45.3)],
    "lebanon": [(35.9, 34.6), (35.8, 33.6)],
}

NILE = [(32.75, 24.5), (32.65, 25.7), (32.7, 26.15), (31.75, 26.55), (31.2, 27.2),
        (30.75, 28.1), (31.1, 29.0), (31.25, 30.05)]
NILE_BRANCHES = [[(31.25, 30.05), (30.85, 30.7), (30.4, 31.45)],
                 [(31.25, 30.05), (31.45, 30.8), (31.8, 31.45)]]
RIVERS = [
    [(9.3, 47.6), (8.2, 47.6), (7.6, 47.6), (8.2, 49.0), (7.6, 50.3), (7.0, 51.0), (6.3, 51.8),
     (5.0, 51.9)],                                                                   # Rhine
    [(8.5, 48.0), (11.0, 48.7), (13.4, 48.6), (16.4, 48.2), (18.9, 47.9), (18.9, 46.0),
     (20.5, 44.8), (22.5, 44.6), (24.0, 43.7), (26.0, 43.9), (28.0, 44.1), (28.8, 45.2),
     (29.6, 45.2)],                                                                  # Danube
    [(4.8, 45.8), (4.8, 44.5), (4.7, 43.7)],                                         # Rhone
    [(12.5, 41.9), (12.25, 41.75)],                                                  # Tiber
]

PLACES_LL = {
    "Rome": (12.5, 41.9),
    "Ostia": (12.29, 41.75),
    "Beneventum": (14.78, 41.13),
    "Capua": (14.21, 41.1),
    "Syracuse": (15.29, 37.08),
    "Carthago Nova": (-0.98, 37.6),
    "Aquileia": (13.37, 45.77),
    "Narbo": (3.0, 43.18),
    "Campania": (14.49, 40.75),
    "Corinth": (22.93, 37.9),
    "Lugdunum": (4.83, 45.76),
    "Alexandria": (29.92, 31.2),
    "Colonia Agrippina": (6.96, 50.94),
    "Londinium": (-0.13, 51.51),
    "Antioch": (36.16, 36.2),
}


# ------------------------------------------------------------ rasterising
def _mask():
    big = Image.new("L", (W * SS, H * SS), 255)          # 255 = land
    d = ImageDraw.Draw(big)
    for poly in SEAS:
        d.polygon([proj(lo, la, SS) for lo, la in poly], fill=0)
    for name, poly in ISLANDS.items():
        d.polygon([proj(lo, la, SS) for lo, la in poly], fill=255)
    for (lo, la, r) in ISLETS:
        x0, y0 = proj(lo - r, la + r * 0.8, SS)
        x1, y1 = proj(lo + r, la - r * 0.8, SS)
        d.ellipse([x0, y0, x1, y1], fill=255)
    # straits: Dardanelles and Bosporus
    for seg in (((26.2, 40.05), (26.6, 40.35)), ((28.9, 41.0), (29.05, 41.2))):
        d.line([proj(*seg[0], SS), proj(*seg[1], SS)], fill=0, width=SS + 2)
    small = big.resize((W, H), Image.BOX)
    return [[small.getpixel((x, y)) >= 128 for x in range(W)] for y in range(H)]


def _line_px(pts, width=1):
    """Pixels of a polyline in map space (1x)."""
    im = Image.new("L", (W, H), 0)
    ImageDraw.Draw(im).line([proj(lo, la) for lo, la in pts], fill=255, width=width)
    return {(x, y) for y in range(H) for x in range(W) if im.getpixel((x, y))}


def _dist_to(mask, value, maxd=8):
    """Chebyshev-ish BFS distance to the nearest pixel with mask == value."""
    INF = 99
    dist = [[INF] * W for _ in range(H)]
    frontier = []
    for y in range(H):
        for x in range(W):
            if mask[y][x] == value:
                dist[y][x] = 0
                frontier.append((x, y))
    d = 0
    while frontier and d < maxd:
        d += 1
        nxt = []
        for (x, y) in frontier:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                xx, yy = x + dx, y + dy
                if 0 <= xx < W and 0 <= yy < H and dist[yy][xx] == INF:
                    dist[yy][xx] = d
                    nxt.append((xx, yy))
        frontier = nxt
    return dist


def _dither(level, x, y, ramp):
    """Ordered dither: level in 0..len(ramp)-1 (float)."""
    level = max(0.0, min(len(ramp) - 1.0, level))
    i = int(level)
    f = level - i
    if i >= len(ramp) - 1:
        return ramp[-1]
    return ramp[i + 1] if BAYER[y % 4][x % 4] < f * 16 else ramp[i]


LAND_RAMP = (C.grass[1], C.olive[1], C.grass[2], C.olive[2], C.ochre[1], C.ochre[2], C.ochre[3])


def province_map():
    land = _mask()
    img = new(W, H)
    d_land = _dist_to(land, True, maxd=10)      # for sea pixels: distance to land
    nile = _line_px(NILE) | set().union(*[_line_px(b) for b in NILE_BRANCHES])
    nile_valley = set()
    for (x, y) in nile:
        for dx in (-2, -1, 0, 1, 2):
            nile_valley.add((x + dx, y))
    rivers = set().union(*[_line_px(r) for r in RIVERS])
    ridge_all = set().union(*[_line_px(pts, width=5) for pts in MOUNTAINS.values()])

    for y in range(H):
        lat = LAT1 - (y + 0.5) / H * (LAT1 - LAT0)
        for x in range(W):
            lon = LON0 + (x + 0.5) / W * (LON1 - LON0)
            if land[y][x]:
                # climate: greener north and west, desert south and east
                arid = (44.5 - lat) / 4.0 + max(0.0, lon - 33.0) / 6.0
                if lat < 33.5:
                    arid += (33.5 - lat) * 0.55
                arid += (max(0.0, min(1.0, (37.6 - lat) / 1.8)) *
                         max(0.0, min(1.0, (lon - 34.0) / 1.5)) * 1.0)     # Syrian steppe
                if lon < -1 and 37 < lat < 42.5:            # dry Iberian meseta
                    arid += 0.4
                arid += ((hash2(x // 6, y // 6, 3) % 100) / 100.0 - 0.5) * 0.6
                level = max(0.0, min(6.0, 0.6 + arid))
                c = _dither(level, x, y, LAND_RAMP)
                if (x, y) in nile_valley:
                    c = _dither(1.5, x, y, LAND_RAMP)
                # forests: darker clumps in the temperate north
                fn = _noise(x, y, 7) - (arid * 0.12)
                if fn > 0.56:
                    dens = min(1.0, (fn - 0.56) / 0.08)
                    if BAYER[y % 4][x % 4] < dens * 12:
                        c = C.grass[0] if (x + y) % 3 else C.cypress[2]
                # dunes in the deserts
                if level > 5.4 and hash2(x // 4, y // 3, 4) % 23 == 0 and y % 3 == 0:
                    c = C.ochre[1] if x % 4 else C.ochre[2]
                if (x, y) in ridge_all:
                    c = _dither(max(0.0, level - 1.6), x, y, LAND_RAMP)   # foothills
                if (x, y) in rivers or (x, y) in nile:
                    c = C.sea[2]
                px(img, x, y, c)
            else:
                dl = d_land[y][x]
                if dl == 1:
                    c = C.sea[3]                             # 1px light shore line
                elif dl <= 3:
                    c = _dither(1.6 - (dl - 2) * 0.6, x, y, (C.sea[1], C.sea[2]))
                else:
                    c = _dither(0.55 if dl < 8 else 0.25, x, y, (C.sea[0], C.sea[1]))
                    if hash2(x // 5, y, 9) % 97 == 0:
                        c = C.sea[2]                         # faint wave flecks
                px(img, x, y, c)
    _mountains(img, land)
    return img, land


def _noise(x, y, salt):
    """Smooth value noise in 0..1 (bilinear over an 8px lattice)."""
    gx, gy = x / 8.0, y / 8.0
    x0, y0 = int(gx), int(gy)
    fx, fy = gx - x0, gy - y0
    v = [[(hash2(x0 + i, y0 + j, salt) % 1000) / 1000.0 for i in (0, 1)] for j in (0, 1)]
    a = v[0][0] * (1 - fx) + v[0][1] * fx
    b = v[1][0] * (1 - fx) + v[1][1] * fx
    return a * (1 - fy) + b * fy


def _peak(img, x, y, h, snow):
    """Small mountain glyph: apex at (x, y - h + 1), base on row y; lit west
    slope, shaded east slope."""
    for k in range(h):
        yy = y - h + 1 + k
        for dx in range(-k, k + 1):
            if dx < 0:
                c = C.travertine[2] if snow and k < 2 else C.travertine[0]
            elif dx == 0:
                c = C.white if snow and k == 0 else (C.travertine[1] if k < 2 else C.earth[2])
            else:
                c = C.earth[1] if dx < k else C.earth[0]
            px(img, x + dx, yy, c)


def _mountains(img, land):
    snowy = {"alps", "pyrenees", "atlas", "taurus"}
    for name, pts in MOUNTAINS.items():
        ppts = [proj(lo, la) for lo, la in pts]
        acc = 0.0
        stamps = []
        for (x0, y0), (x1, y1) in zip(ppts, ppts[1:]):
            seg = math.hypot(x1 - x0, y1 - y0)
            t = 0.0
            while t <= seg:
                if acc <= 0:
                    f = t / seg if seg else 0
                    stamps.append((x0 + (x1 - x0) * f, y0 + (y1 - y0) * f))
                    acc = 4.0
                step = 1.0
                t += step
                acc -= step
        for i, (x, y) in enumerate(sorted(stamps, key=lambda p: p[1])):
            xi, yi = int(x), int(y)
            if 0 <= yi < H and 0 <= xi < W and land[yi][xi]:
                h = 4 + (hash2(xi, yi, 21) % 2)
                _peak(img, xi + (1 if i % 2 else -1), yi + 1, h, name in snowy)


def places(land):
    """Normalised positions; points falling in water are pulled to the
    nearest land pixel (searching outward)."""
    out = {}
    for name, (lo, la) in PLACES_LL.items():
        fx, fy = proj(lo, la)
        x, y = int(fx), int(fy)
        if not land[y][x]:
            best = None
            for r in range(1, 12):
                for dy in range(-r, r + 1):
                    for dx in range(-r, r + 1):
                        xx, yy = x + dx, y + dy
                        if 0 <= xx < W and 0 <= yy < H and land[yy][xx]:
                            dd = dx * dx + dy * dy
                            if best is None or dd < best[0]:
                                best = (dd, xx, yy)
                if best:
                    break
            _, x, y = best
            fx, fy = x + 0.5, y + 0.5
        out[name] = {"x": round(fx / W, 4), "y": round(fy / H, 4)}
    return out


_CACHE = {}


def build():
    if "map" not in _CACHE:
        img, land = province_map()
        _CACHE["map"] = (img, places(land))
    return _CACHE["map"]


def generate():
    img, _ = build()
    return [("ui/province_map", img, None)]


def extra_files():
    """Non-image outputs written next to the sprites by build_all.py."""
    _, pl = build()
    return {"province_places.json": pl}
