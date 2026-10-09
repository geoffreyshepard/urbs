"""Painted buildings: per-tile ground sprites, farm huts and line structures.

  bld/<id>_tile_<0..3>   16x16 tiles drawn on every footprint tile of a
                         painted building.  Row / trellis / slab periods
                         divide 16 with a fixed phase, so any variant tiles
                         seamlessly against any other.
  bld/<farm>_hut         small farmhouse on a 1x1 footprint (16 x 28),
                         anchor bottom-left (0, 27)
  bld/<line>_c, _n/_e/_s/_w
                         4-direction autotiles.  Composite center first, then
                         one spoke per connected neighbour (spokes may be drawn
                         in any order).
      pipe, sewer, bridge: flat 16x16 overlays, anchor (0, 0)
      wall:     16 x 26 (10 px tall), anchor bottom-left (0, 25)
      aqueduct: 16 x 34 (18 px tall), anchor bottom-left (0, 33)
      Tall pieces: top surface shifted up by the structure height; the
      front (south) face shows under e/w spokes and under the center.  In a
      N-S run the center's face is covered by the s spoke and by the next
      tile south, so draw tiles north-to-south.
"""
import math

from palette import C, TRANSPARENT
from primitives import new, px, rect, hline, vline, rng, outline, ellipse_mask, roof, hash2
from buildings.placeholder import _wall, _window
from buildings.housing import thatch_roof, wattle_wall
import objects

T = 16
TILE_IDS = ["plaza", "garden", "stockpile", "wheat_farm", "vegetable_garden", "pig_farm",
            "olive_grove", "vineyard", "sheep_farm", "papyrus_marsh"]
FARMS = ["wheat_farm", "vegetable_garden", "pig_farm", "olive_grove", "vineyard", "sheep_farm"]
LINES = ["wall", "aqueduct", "pipe", "sewer", "bridge"]
SIDES = ["n", "e", "s", "w"]


def _noise(img, r, base, specks):
    """Opaque base fill with seeded speckles: specks = [(color, prob), ...]."""
    for y in range(T):
        for x in range(T):
            c = base
            q = r.random()
            acc = 0.0
            for col, p in specks:
                acc += p
                if q < acc:
                    c = col
                    break
            px(img, x, y, c)


def _outlined(layer, c=None):
    outline(layer, c or C.outline)
    return layer


def _paste(dst, src, x=0, y=0):
    dst.paste(src, (x, y), src)


# ================================================================ tiles
def plaza_tile(v):
    """Large square travertine slabs (8x8), rows staggered by 4 -- calmer and
    larger than road paving.  Thin joints on the shadow sides only."""
    r = rng("bld/plaza/%d" % v)
    img = new(T, T)
    faces = (C.travertine[2], C.marble[0], C.travertine[2], C.marble[0], C.marble[1])
    for row in range(2):
        off = 4 if row % 2 else 0
        for k in range(3):
            face = r.choice(faces)
            x0 = -off + k * 8
            for i in range(8):
                for j in range(8):
                    x, y = (x0 + i) % T, row * 8 + j
                    if i == 7 or j == 7:
                        c = C.travertine[1]                     # joint
                    elif j == 0 and i < 6:
                        c = C.marble[1] if face != C.marble[1] else C.white
                    else:
                        c = face
                        if r.random() < 0.05:
                            c = C.travertine[1]                 # wear / stains
                    px(img, x, y, c)
    return img


def _small_cypress(layer, cx, base_y, h, key):
    m = set()
    top = base_y - h
    for y in range(top, base_y - 1):
        t = (y - top) / float(h - 1)
        half = 2.2 * math.sin(math.pi * min(1.0, t * 0.8 + 0.06)) ** 0.8
        for x in range(cx - 3, cx + 4):
            if abs(x + 0.5 - (cx + 0.5)) <= half:
                m.add((x, y))
    objects.foliage(layer, m, (C.cypress[0], C.cypress[1], C.cypress[2], C.grass[0]), key,
                    clump_r=(0.9, 1.4), density=0.9)
    px(layer, cx, base_y - 1, C.earth[1])


def garden_tile(v):
    """Formal garden: gravel walks on the tile's north and west edges form a
    grid; each bed holds flowers, a clipped hedge, a cypress or a basin."""
    r = rng("bld/garden/%d" % v)
    img = new(T, T)
    _noise(img, r, C.grass[2], [(C.grass[1], 0.08), (C.grass[3], 0.08)])
    for y in range(T):
        for x in range(T):
            if x < 2 or y < 2:
                px(img, x, y, C.ochre[3] if hash2(x, y, v) % 4 else C.travertine[2])
    for x in range(2, T):                       # walk edging shadow
        px(img, x, 2, C.grass[1])
    for y in range(2, T):
        px(img, 2, y, C.grass[1])
    layer = new(T, T)
    if v == 0:
        # flower bed with a low box edging
        for y in range(4, 14):
            for x in range(4, 14):
                edge = x in (4, 13) or y in (4, 13)
                c = C.grass[0] if edge else C.earth[1]
                px(img, x, y, c)
        cols = (C.roman_red[2], C.tyrian[1], C.gold, C.white)
        for y in range(5, 13):
            for x in range(5, 13):
                if (x + 2 * y) % 3 == 0:
                    px(img, x, y, r.choice(cols))
                elif (x + y) % 2 == 0:
                    px(img, x, y, C.grass[1])
    elif v == 1:
        m = ellipse_mask(8.5, 8.0, 4.6, 4.0)
        objects.foliage(layer, m, (C.grass[0], C.grass[1], C.grass[2], C.grass[3]),
                        "garden/hedge", clump_r=(1.0, 1.6), density=0.9)
        _outlined(layer, C.cypress[0])
    elif v == 2:
        _small_cypress(layer, 8, 14, 12, "garden/cypress")
        _outlined(layer)
        for (x, y) in ((4, 12), (12, 13), (5, 5)):
            px(img, x, y, r.choice((C.roman_red[2], C.tyrian[1], C.white)))
    else:
        for (x, y) in ellipse_mask(9.0, 9.0, 4.5, 3.6):
            px(layer, x, y, C.marble[1])
        for (x, y) in ellipse_mask(9.0, 9.0, 3.2, 2.4):
            px(layer, x, y, C.sea[2])
        for (x, y) in ellipse_mask(9.0, 8.4, 3.0, 1.6):
            if (x, y) in ellipse_mask(9.0, 9.0, 3.2, 2.4):
                pass
        hline(layer, 7, 10, 7, C.sea[1])
        px(layer, 9, 9, C.white)
        px(layer, 9, 8, C.sea[4])
        _outlined(layer, C.gray[1])
    _paste(img, layer)
    return img


def _crate(layer, x, y, w=5, h=4):
    """Wooden crate: lit top face, front face with plank line."""
    hline(layer, x, x + w - 1, y, C.earth[3])
    hline(layer, x, x + w - 1, y + 1, C.ochre[1])
    for yy in range(y + 2, y + h):
        for xx in range(x, x + w):
            c = C.earth[2]
            if xx == x + w - 1:
                c = C.earth[1]
            if yy == y + 2:
                c = C.earth[1]
            px(layer, xx, yy, c)
    px(layer, x + w // 2, y + 3, C.earth[1])


def _amphora(layer, x, y):
    """2x5 amphora seen from the front-top."""
    px(layer, x, y, C.roof_terracotta[3])
    px(layer, x + 1, y, C.roof_terracotta[2])
    px(layer, x, y + 1, C.roof_terracotta[2])
    px(layer, x + 1, y + 1, C.roof_terracotta[1])
    px(layer, x, y + 2, C.roof_terracotta[2])
    px(layer, x + 1, y + 2, C.roof_terracotta[1])
    px(layer, x, y + 3, C.roof_terracotta[1])
    px(layer, x + 1, y + 3, C.roof_terracotta[0])


def _sack(layer, x, y):
    for (xx, yy) in ellipse_mask(x + 2.0, y + 2.0, 2.2, 1.8):
        c = C.travertine[2] if (xx - x) < 2 and (yy - y) < 2 else C.travertine[1]
        if (xx - x) >= 3 or (yy - y) >= 3:
            c = C.travertine[0]
        px(layer, xx, yy, c)
    px(layer, x + 2, y, C.earth[1])


def _barrel(layer, x, y):
    """Standing barrel (5x6): lit lid with rim, staves with iron hoops."""
    hline(layer, x + 1, x + 3, y, C.earth[3])
    hline(layer, x, x + 4, y + 1, C.ochre[1])
    px(layer, x, y + 1, C.earth[2])
    px(layer, x + 4, y + 1, C.earth[1])
    for yy in range(y + 2, y + 7):
        for xx in range(x, x + 5):
            c = C.earth[3] if xx == x else (C.earth[2] if xx < x + 3 else C.earth[1])
            if yy in (y + 3, y + 6):
                c = C.gray[2] if xx < x + 3 else C.gray[1]
            px(layer, xx, yy, c)


def stockpile_tile(v):
    r = rng("bld/stockpile/%d" % v)
    img = new(T, T)
    _noise(img, r, C.ochre[1], [(C.earth[3], 0.12), (C.ochre[2], 0.08), (C.ochre[0], 0.04)])
    layer = new(T, T)
    if v == 0:
        _crate(layer, 2, 3, 6, 5)
        _crate(layer, 8, 6, 6, 5)
        _crate(layer, 3, 9, 5, 4)
        _crate(layer, 9, 1, 4, 4)
    elif v == 1:
        for row, yy in enumerate((2, 8)):
            for k in range(5):
                _amphora(layer, 2 + k * 2 + (row % 2), yy + (k % 2))
    elif v == 2:
        for (x, y) in ((2, 6), (6, 7), (10, 6), (4, 3), (8, 3), (6, 10), (10, 10)):
            _sack(layer, x, y)
    else:
        _barrel(layer, 2, 3)
        _barrel(layer, 7, 4)
        # stacked logs, ends toward the viewer
        for (x, y) in ((4, 11), (7, 11), (10, 11), (5, 9), (8, 9), (11, 9)):
            px(layer, x, y, C.earth[3])
            px(layer, x + 1, y, C.ochre[1])
            px(layer, x, y + 1, C.ochre[1])
            px(layer, x + 1, y + 1, C.earth[2])
        _barrel(layer, 11, 2)
    _paste(img, _outlined(layer))
    return img


def wheat_tile(v):
    """Golden wheat in rows: 3 px of grain, 1 px furrow, period 4."""
    r = rng("bld/wheat/%d" % v)
    img = new(T, T)
    o = C.ochre
    for y in range(T):
        k = y % 4
        for x in range(T):
            if k == 3:
                c = C.earth[1] if (x + v) % 3 else C.earth[2]          # furrow
            elif k == 0:
                c = o[3] if (x % 2 == 0) else C.gold                   # lit ears
                if r.random() < 0.2:
                    c = o[2]
            elif k == 1:
                c = o[2] if r.random() < 0.8 else o[3]
            else:
                c = o[1] if (x % 2) else o[0]                          # stalks in shade
            px(img, x, y, c)
    for _ in range((0, 1, 2, 0)[v]):                                  # poppies
        x, y = r.randrange(1, T - 1), r.randrange(4) * 4 + 1
        px(img, x, y, C.roman_red[2])
    return img


def vegetable_tile(v):
    """Raised beds: 4 px rows (3 px planted ridge + 1 px dark furrow); plants
    3x2 every 4 px, alternate rows offset by 2 (period 16)."""
    r = rng("bld/veg/%d" % v)
    img = new(T, T)
    for y in range(T):
        for x in range(T):
            k = y % 4
            c = C.earth[2] if k in (0, 1) else C.earth[1]
            if k == 3:
                c = C.earth[0]
            elif r.random() < 0.08:
                c = C.earth[3]
            px(img, x, y, c)
    crops = [
        (C.grass[3], C.grass[2], C.grass[1], C.grass[0]),            # cabbage
        (C.grass[3], C.olive[2], C.grass[2], C.olive[1]),             # lettuce
        (C.grass[3], C.grass[2], C.grass[1], C.roof_terracotta[3]),   # carrots
        (C.olive[2], C.olive[1], C.olive[1], C.olive[0]),             # leeks
    ]
    hi, a, b, sh = crops[v]
    for row in range(4):
        y = row * 4
        for col in range(4):
            x = (col * 4 + (row % 2) * 2) % T
            if v == 3:                                   # upright leeks
                for dx in (0, 2):
                    px(img, (x + dx) % T, y, hi)
                    px(img, (x + dx) % T, y + 1, a)
                    px(img, (x + dx) % T, y + 2, sh)
                continue
            px(img, x, y, hi)
            px(img, (x + 1) % T, y, a)
            px(img, (x + 2) % T, y, a if v != 2 else hi)
            px(img, x, y + 1, b)
            px(img, (x + 1) % T, y + 1, b)
            px(img, (x + 2) % T, y + 1, sh)
            px(img, (x + 1) % T, y + 2, sh if v != 2 else C.roof_terracotta[2])
    return img


def olive_grove_tile(v):
    r = rng("bld/olivegrove/%d" % v)
    img = new(T, T)
    _noise(img, r, C.ochre[1], [(C.grass[1], 0.20), (C.olive[1], 0.12), (C.earth[3], 0.10),
                                (C.grass[2], 0.08)])
    # shade on the ground (bottom-right of the trunk)
    for (x, y) in ellipse_mask(9.5, 13.0, 4.0, 1.6):
        if (x + y) % 2 == 0 or (x, y) in ellipse_mask(9.5, 13.0, 3.0, 1.0):
            px(img, x, y, C.olive[0])
    layer = new(T, T)
    vline(layer, 7, 9, 13, C.gray[2])
    vline(layer, 8, 10, 13, C.earth[1])
    px(layer, 6, 9, C.gray[2])
    px(layer, 9, 9, C.earth[1])
    blobs = [[(5.5, 5.5, 3.2, 2.8), (10.2, 5.0, 3.2, 2.8), (8.0, 3.5, 2.6, 2.0)],
             [(6.0, 5.0, 3.4, 3.0), (10.0, 6.0, 3.0, 2.4)],
             [(5.0, 6.0, 2.8, 2.4), (8.5, 4.2, 3.4, 2.8), (11.0, 6.5, 2.4, 2.0)],
             [(7.8, 5.2, 4.4, 3.3)]][v]
    m = set()
    for b in blobs:
        m |= ellipse_mask(*b)
    m = {(x, y) for (x, y) in m if 1 <= x < T - 1 and 1 <= y < 9}
    objects.foliage(layer, m, (C.cypress[1], C.olive[0], C.olive[1], C.olive[2]),
                    "olivegrove/%d" % v, clump_r=(0.9, 1.6), density=0.7, holes=2)
    _paste(img, _outlined(layer))
    return img


def vineyard_tile(v):
    """Trellised vines: rows every 8 px (foliage band 4 px), posts every 8 px."""
    r = rng("bld/vineyard/%d" % v)
    img = new(T, T)
    _noise(img, r, C.earth[3], [(C.ochre[1], 0.15), (C.earth[2], 0.10)])
    for band in (0, 8):
        y0 = band + 2
        for x in range(T):
            for j in range(4):
                y = y0 + j
                q = r.random()
                if j == 0:
                    c = C.grass[3] if q < 0.5 else C.grass[2]
                elif j == 3:
                    c = C.grass[0] if q < 0.7 else C.olive[0]
                else:
                    c = C.grass[2] if q < 0.45 else (C.grass[1] if q < 0.85 else C.olive[1])
                px(img, x, y, c)
            # ragged top/bottom leaves
            if hash2(x, band, v) % 3 == 0:
                px(img, x, y0 - 1, C.grass[2])
            px(img, x, y0 + 4, C.earth[1] if (x + band) % 2 else C.earth[2])   # shadow
        # grape clusters
        for _ in range(3):
            x = r.randrange(0, T)
            if x % 8 == 0:
                continue
            px(img, x, y0 + 2, C.tyrian[1])
            px(img, x, y0 + 3, C.tyrian[0])
        # posts at fixed x (seamless), sticking up through the foliage
        for x in (0, 8):
            px(img, x, y0 - 1, C.earth[3])
            px(img, x, y0, C.earth[2])
            px(img, x, y0 + 3, C.earth[1])
            px(img, x, y0 + 4, C.earth[0])
    return img


def _sheep(layer, x, y, face_right=True, lying=False):
    body = 3 if lying else 3
    for yy in range(y, y + body):
        for xx in range(x, x + 5):
            c = C.marble[1]
            if yy == y:
                c = C.white
            if yy == y + body - 1 or xx == x + 4:
                c = C.marble[0]
            px(layer, xx, yy, c)
    hx = x + 5 if face_right else x - 2
    px(layer, hx, y, C.gray[0])
    px(layer, hx + 1, y, C.gray[0])
    px(layer, hx + (1 if face_right else 0), y + 1, C.gray[1])
    if not lying:
        for lx in (x + 1, x + 3):
            px(layer, lx, y + body, C.gray[0])


def sheep_tile(v):
    r = rng("bld/sheep/%d" % v)
    img = new(T, T)
    g = C.grass
    _noise(img, r, g[2], [(g[1], 0.08), (g[3], 0.09), (C.ochre[1], 0.03)])
    for _ in range(4):
        x, y = r.randrange(1, T - 2), r.randrange(1, T - 2)
        px(img, x, y, g[3])
        px(img, x, y + 1, g[1])
    layer = new(T, T)
    if v == 0:
        _sheep(layer, 4, 6, True)
    elif v == 1:
        _sheep(layer, 2, 3, True)
        _sheep(layer, 8, 10, False)
    elif v == 2:
        # hay rack
        for x in range(5, 11):
            px(layer, x, 8, C.ochre[3] if x % 2 else C.ochre[2])
            px(layer, x, 9, C.ochre[1])
            px(layer, x, 10, C.earth[2] if x in (5, 10) else C.earth[1])
        _sheep(layer, 6, 12, True, lying=True)
    else:
        # stone water trough and a lamb
        hline(layer, 3, 9, 3, C.marble[0])
        hline(layer, 3, 9, 4, C.sea[2])
        px(layer, 5, 4, C.sea[3])
        hline(layer, 3, 9, 5, C.travertine[0])
        _sheep(layer, 7, 10, False, lying=True)
    _paste(img, _outlined(layer))
    return img


def _pig(layer, x, y, face_right=True):
    for yy in range(y, y + 3):
        for xx in range(x, x + 5):
            c = C.skin[2]
            if yy == y + 2 or xx == x + 4:
                c = C.skin[1]
            px(layer, xx, yy, c)
    sx = x + 5 if face_right else x - 1
    px(layer, sx, y + 1, C.roof_terracotta[3])          # snout
    px(layer, x + (3 if face_right else 1), y, C.skin[1])   # ear
    for lx in (x, x + 3):
        px(layer, lx, y + 3, C.skin[0])


def pig_tile(v):
    """Mud yard divided into pens by a low rail fence on the tile's north and
    west edges (a grid of 16 px pens)."""
    r = rng("bld/pig/%d" % v)
    img = new(T, T)
    _noise(img, r, C.earth[2], [(C.earth[1], 0.18), (C.earth[3], 0.10), (C.ochre[0], 0.04)])
    # fence rails (lit top rail, shadow under)
    for x in range(T):
        px(img, x, 0, C.earth[3])
        px(img, x, 1, C.earth[0])
    for y in range(T):
        px(img, 0, y, C.earth[3])
        px(img, 1, y, C.earth[0])
    for (x, y) in ((0, 0), (0, 8), (8, 0)):
        px(img, x, y, C.ochre[2])
    layer = new(T, T)
    if v == 0:
        _pig(layer, 5, 6, True)
    elif v == 1:
        _pig(layer, 3, 4, True)
        _pig(layer, 8, 10, False)
    elif v == 2:
        # trough
        hline(layer, 4, 11, 4, C.earth[3])
        hline(layer, 4, 11, 5, C.sea[1])
        hline(layer, 4, 11, 6, C.earth[1])
        _pig(layer, 6, 9, False)
    else:
        for (x, y) in ellipse_mask(8.5, 9.5, 4.5, 2.6):
            px(img, x, y, C.earth[0] if (x + y) % 3 else C.earth[1])
        _pig(layer, 6, 8, True)
    _paste(img, _outlined(layer))
    return img


def _papyrus(layer, base_x, base_y, stems, r):
    """Papyrus clump: slender stems topped by wide fan-shaped umbels."""
    tops = []
    for i in range(stems):
        x = base_x + (i - stems // 2) * 2 + (i % 2)
        top = base_y - r.randrange(7, 11)
        lean = (x - base_x) // 2
        lean = max(-2, min(2, lean))
        for y in range(top + 1, base_y):
            t = (base_y - y) / float(base_y - top)
            xx = x + int(round(lean * t))
            px(layer, xx, y, C.grass[2] if y > top + 3 else C.grass[3])
        tops.append((x + lean, top))
    for (hx, top) in tops:
        # umbel: 5-wide fan, lit on top, darker underside
        for dx in (-2, -1, 0, 1, 2):
            px(layer, hx + dx, top - (1 if abs(dx) == 2 else 0), C.grass[3] if dx <= 0 else C.grass[2])
        px(layer, hx, top - 1, C.grass[3])
        px(layer, hx - 1, top + 1, C.olive[1])
        px(layer, hx + 1, top + 1, C.olive[1])
    for x in range(base_x - stems - 1, base_x + stems + 1):
        px(layer, x, base_y, C.cypress[1] if x % 2 else C.olive[0])


def papyrus_tile(v):
    r = rng("bld/papyrus/%d" % v)
    img = new(T, T)
    _noise(img, r, C.sea[2], [(C.sea[1], 0.12), (C.olive[0], 0.10), (C.sea[3], 0.04)])
    layer = new(T, T)
    clumps = [[(5, 14, 3), (11, 10, 2)], [(8, 14, 4)], [(4, 10, 2), (11, 13, 3)],
              [(10, 13, 3)]][v]
    for (cx, cy, n) in clumps:
        for (x, y) in ellipse_mask(cx + 0.5, cy - 0.5, n * 0.7 + 1, 1.6):
            if 0 <= x < T and 0 <= y < T:
                px(img, x, y, C.olive[0])
    for (cx, cy, n) in clumps:
        _papyrus(layer, cx, min(cy, T - 2), n, r)
    if v == 3:
        for (x, y) in ellipse_mask(4.0, 4.0, 1.6, 1.2):            # lily pad
            px(layer, x, y, C.grass[1])
        px(layer, 4, 3, C.white)
    # no outline: it would merge the thin stems into a block (like obj/reeds)
    _paste(img, layer)
    return img


TILE_FN = {
    "plaza": plaza_tile, "garden": garden_tile, "stockpile": stockpile_tile,
    "wheat_farm": wheat_tile, "vegetable_garden": vegetable_tile, "pig_farm": pig_tile,
    "olive_grove": olive_grove_tile, "vineyard": vineyard_tile, "sheep_farm": sheep_tile,
    "papyrus_marsh": papyrus_tile,
}


# ================================================================ farm huts
HUT_W, HUT_H = 16, 28


def farm_hut(fid):
    key = "bld/%s_hut" % fid
    s = new(HUT_W, HUT_H)
    wall_h = 8
    wall_top = HUT_H - 1 - wall_h
    thatched = fid in ("wheat_farm", "sheep_farm", "pig_farm")
    if fid == "pig_farm":
        # plank shed
        for y in range(wall_top, HUT_H - 1):
            for x in range(1, 15):
                c = C.earth[2] if x % 3 else C.earth[1]
                if x == 1:
                    c = C.earth[3]
                if y == wall_top:
                    c = C.earth[0]
                px(s, x, y, c)
    elif thatched:
        wattle_wall(s, 1, 14, wall_top, HUT_H - 2, key)
    else:
        _wall(s, 1, 14, wall_top, HUT_H - 2, key, 1, 1)
    if thatched:
        thatch_roof(s, 0, 3, 16, wall_top - 3, key)
    else:
        roof(s, 1, 3, 14, wall_top - 3, ridge="h", key=key)
        hline(s, 1, 14, wall_top - 1, C.roof_terracotta[0])
    # door
    rect(s, 6, HUT_H - 7, 3, 5, C.earth[0])
    px(s, 6, HUT_H - 7, C.outline)
    vline(s, 8, HUT_H - 6, HUT_H - 2, C.earth[1])
    # theme props at the front corners
    if fid == "wheat_farm":
        for x in (2, 4):                                   # sheaves
            vline(s, x, HUT_H - 6, HUT_H - 2, C.ochre[2])
            px(s, x, HUT_H - 7, C.ochre[3])
            px(s, x, HUT_H - 4, C.earth[1])
            px(s, x + 1, HUT_H - 5, C.ochre[1])
        for x in (11, 13):
            rect(s, x, HUT_H - 4, 2, 2, C.travertine[2])   # grain sack
            px(s, x + 1, HUT_H - 3, C.travertine[1])
    elif fid == "vegetable_garden":
        for x in (2, 11):                                  # baskets of greens
            hline(s, x, x + 2, HUT_H - 5, C.grass[3])
            hline(s, x, x + 2, HUT_H - 4, C.grass[1])
            hline(s, x, x + 2, HUT_H - 3, C.ochre[1])
            hline(s, x, x + 2, HUT_H - 2, C.ochre[0])
        px(s, 3, HUT_H - 5, C.roof_terracotta[3])
    elif fid == "pig_farm":
        hline(s, 9, 14, HUT_H - 4, C.earth[3])             # trough
        hline(s, 9, 14, HUT_H - 3, C.sea[1])
        hline(s, 9, 14, HUT_H - 2, C.earth[1])
    elif fid == "olive_grove":
        for x in (2, 4, 11, 13):                           # oil amphorae
            vline(s, x, HUT_H - 5, HUT_H - 2, C.roof_terracotta[2])
            px(s, x, HUT_H - 2, C.roof_terracotta[0])
            px(s, x, HUT_H - 5, C.roof_terracotta[3])
    elif fid == "vineyard":
        for x in (2, 11):                                  # barrels / grape baskets
            rect(s, x, HUT_H - 5, 3, 4, C.earth[2])
            hline(s, x, x + 2, HUT_H - 4, C.gray[1])
            hline(s, x, x + 2, HUT_H - 5, C.tyrian[1])
        px(s, 12, HUT_H - 6, C.tyrian[0])
    elif fid == "sheep_farm":
        for (x, y) in ((2, HUT_H - 5), (11, HUT_H - 5), (12, HUT_H - 7)):   # wool bales
            rect(s, x, y, 3, 3, C.marble[1])
            hline(s, x, x + 2, y, C.white)
            px(s, x + 2, y + 2, C.marble[0])
    return _outlined(s)


# ================================================================ flat lines
def _flat_piece(piece, texel):
    """Generic flat 4-dir autotile piece.  texel(x, y, piece) -> color/None.
    Center covers the 8x8 middle (4..11); spokes run from the tile edge to
    the center's boundary row/column inclusive, so they overwrite the
    center's rim where they join."""
    img = new(T, T)
    for y in range(T):
        for x in range(T):
            inside = False
            if piece == "c":
                inside = 4 <= x <= 11 and 4 <= y <= 11
            elif piece == "n":
                inside = 4 <= x <= 11 and y <= 4
            elif piece == "s":
                inside = 4 <= x <= 11 and y >= 11
            elif piece == "w":
                inside = 4 <= y <= 11 and x <= 4
            elif piece == "e":
                inside = 4 <= y <= 11 and x >= 11
            if inside:
                c = texel(x, y, piece)
                if c is not None:
                    px(img, x, y, c)
    return img


def _rim_side(x, y, piece):
    """Which side rim a pixel lies on for this piece, or None.
    Shared logic: corners of the center square are rims for everyone."""
    corner = x in (4, 11) and y in (4, 11)
    if corner:
        return "corner"
    if piece in ("n", "s"):
        if x == 4:
            return "w"
        if x == 11:
            return "e"
        return None
    if piece in ("e", "w"):
        if y == 4:
            return "n"
        if y == 11:
            return "s"
        return None
    # center: rims on all four sides
    if x == 4:
        return "w"
    if x == 11:
        return "e"
    if y == 4:
        return "n"
    if y == 11:
        return "s"
    return None


def pipe_texel(x, y, piece):
    """Lead water pipe (2 px) in a narrow stone-lined trench; junction box at
    the center."""
    if piece == "c":
        if 5 <= x <= 10 and 5 <= y <= 10:
            if x == 5 or y == 5:
                return C.travertine[2]
            if x == 10 or y == 10:
                return C.travertine[0]
            if 7 <= x <= 8 and 7 <= y <= 8:
                return C.gray[1]                          # lid ring
            return C.travertine[1]
        return None
    horiz = piece in ("e", "w")
    a = y if horiz else x                                  # across-axis coord
    if piece == "n" and y > 4:
        return None
    if piece == "s" and y < 11:
        return None
    if piece == "w" and x > 4:
        return None
    if piece == "e" and x < 11:
        return None
    if a == 6:
        return C.earth[1]                                  # trench edge
    if a == 7:
        return C.gray[3]                                   # lit lead pipe
    if a == 8:
        return C.gray[1]
    if a == 9:
        return C.earth[0] if (x + y) % 2 else C.earth[1]
    return None


def sewer_texel(x, y, piece):
    """Cloaca under stone cover slabs; grates every 8 px; a round manhole
    grate at the center."""
    side = _rim_side(x, y, piece)
    if side is not None:
        return C.gray[1] if side in ("e", "s", "corner") else C.gray[2]
    horiz = piece in ("e", "w")
    along = x if horiz else y
    across = y if horiz else x
    if piece == "c":
        if (x, y) in _SEWER_MANHOLE:
            ring = (x, y) not in _SEWER_MANHOLE_IN
            if ring:
                return C.gray[2]
            return C.outline if (x + y) % 2 else C.gray[1]
        return C.gray[3] if (x + y) % 5 else C.travertine[1]
    # slabs: joints every 4 px along the run
    if along % 4 == 3:
        return C.gray[2]
    if along % 8 in (0, 1) and 6 <= across <= 9:
        return C.outline if (across % 2 == 0) else C.gray[0]    # grate bars
    if across == 5:
        return C.marble[0]
    return C.gray[3] if (along + across) % 7 else C.travertine[1]


_SEWER_MANHOLE = ellipse_mask(8.0, 8.0, 3.0, 3.0)
_SEWER_MANHOLE_IN = ellipse_mask(8.0, 8.0, 2.0, 2.0)


def _bridge_piece(piece):
    """Stone bridge: deck 12 px wide (2..13) with parapets; shadow cast on the
    water to the right/below.  Pieces: center square 2..13; spokes from the
    edge to the center's parapet rows inclusive."""
    img = new(T, T)

    def region(x, y):
        if piece == "c":
            return 2 <= x <= 13 and 2 <= y <= 13
        if piece == "n":
            return 2 <= x <= 13 and y <= 3
        if piece == "s":
            return 2 <= x <= 13 and y >= 12
        if piece == "w":
            return 2 <= y <= 13 and x <= 3
        return 2 <= y <= 13 and x >= 12

    def color(x, y):
        cx = x in (2, 3, 12, 13)
        cy = y in (2, 3, 12, 13)
        if piece == "c":
            par_x, par_y = cx, cy
        elif piece in ("n", "s"):
            par_x, par_y = cx, cy and cx
        else:
            par_x, par_y = cx and cy, cy
        if (cx and cy):
            # parapet corner / pier block, identical for every piece
            return C.marble[1] if (x in (2, 3) and y in (2, 3)) else C.travertine[1]
        if par_x:
            return (C.marble[1] if x == 3 else C.gray[1]) if x in (2, 3) else \
                   (C.travertine[2] if x == 12 else C.gray[1])
        if par_y:
            return (C.marble[1] if y == 3 else C.gray[1]) if y in (2, 3) else \
                   (C.travertine[2] if y == 12 else C.gray[1])
        # deck: worn stone setts, periodic in 4
        if (x % 4 == 3) or (y % 4 == 3):
            return C.gray[2]
        return C.travertine[1] if hash2(x // 4, y // 4, 3) % 3 else C.gray[3]

    for y in range(T):
        for x in range(T):
            if region(x, y):
                px(img, x, y, color(x, y))
    # outer edge of parapets is the dark gray line already; add the cast
    # shadow on the water just outside the right / bottom parapet
    for y in range(T):
        for x in range(T):
            if img.getpixel((x, y))[3]:
                continue
            if piece in ("c", "n", "s") and x == 14 and region(13, y) and (y < 12 or piece != "c"):
                px(img, x, y, C.sea[0])
            if piece in ("c", "e", "w") and y == 14 and region(x, 13) and (x < 12 or piece != "c"):
                px(img, x, y, C.sea[0])
    if piece == "c":
        for i in range(3, 15):
            px(img, 14, i, C.sea[0])
            px(img, i, 14, C.sea[0])
    return img


# ================================================================ tall lines
WALL_HS = 10
AQ_HS = 18
BAND = (4, 11)          # top-surface band in footprint coords


def _piece_region(piece, x, y):
    """Top-surface region of a tall piece (footprint coords)."""
    a, b = BAND
    if piece == "c":
        return a <= x <= b and a <= y <= b
    if piece == "n":
        return a <= x <= b and y <= a + 1
    if piece == "s":
        return a <= x <= b and y >= b - 1
    if piece == "w":
        return a <= y <= b and x <= a + 1
    return a <= y <= b and x >= b - 1


def _face_cols(piece):
    """Columns of the front face per piece.  Center owns a..b; e/w own the
    columns outside the center so faces never overlap other pieces' tops."""
    a, b = BAND
    if piece == "c":
        return range(a, b + 1)
    if piece == "e":
        return range(b + 1, T)
    if piece == "w":
        return range(0, a)
    return range(0)


def wall_top(x, y, piece):
    """Walkway with merlons along the parapets."""
    a, b = BAND
    in_px = x in (a, a + 1, b - 1, b)
    in_py = y in (a, a + 1, b - 1, b)
    if piece in ("n", "s"):
        para_v, para_h = in_px, in_px and in_py
    elif piece in ("e", "w"):
        para_v, para_h = in_px and in_py, in_py
    else:
        para_v, para_h = in_px, in_py
    if para_v and para_h:
        return "merlon"
    if para_h:
        return "merlon" if (x // 2) % 2 == 0 else "crenel"
    if para_v:
        return "merlon" if (y // 2) % 2 == 0 else "crenel"
    return "walk"


def _wall_color(kind, x, y):
    if kind == "merlon":
        return C.marble[0] if (x + y) % 2 == 0 else C.travertine[2]
    if kind == "crenel":
        return C.travertine[0]
    return C.gray[2] if (x % 4 == 0 or y % 4 == 0) else C.gray[3]


def wall_face(x, r, Hs):
    """Ashlar courses of 3 px, blocks 4 wide, staggered (periodic in 8)."""
    if r == Hs - 1:
        return C.gray[1]                                  # footing in shadow
    if r == 0:
        return C.travertine[2]                            # lit coping
    course = (r - 1) // 3
    if (r - 1) % 3 == 2:
        return C.travertine[0]                            # bed joint
    if (x + (course % 2) * 2) % 4 == 3:
        return C.travertine[0]                            # head joint
    return C.travertine[1] if (x + course) % 5 else C.travertine[2]


def aq_top(x, y, piece):
    """Aqueduct top: masonry rims with the open water channel (specus)."""
    a, b = BAND
    rim_x = x in (a, a + 1, b - 1, b)
    rim_y = y in (a, a + 1, b - 1, b)
    if piece in ("n", "s"):
        rx, ry = rim_x, rim_x and rim_y
    elif piece in ("e", "w"):
        rx, ry = rim_x and rim_y, rim_y
    else:
        rx, ry = rim_x, rim_y
    if rx and ry:
        return C.travertine[2]
    if rx:
        return C.marble[0] if x in (a, b - 1) else C.travertine[0]
    if ry:
        return C.marble[0] if y in (a, b - 1) else C.travertine[0]
    # water
    if (x + y) % 5 == 0:
        return C.sea[3]
    return C.sea[2] if (y if piece in ("e", "w") else x) % 4 else C.sea[1]


AQ_PIERS = (2, 3, 4, 5)          # x % 8 for piers; openings at 6..9 (14..1)


def aq_face(x, r, Hs):
    """Arcade: cornice, attic band, string course, then 4px piers and 4px
    round-headed arches (transparent openings).  Periodic in 8."""
    k = x % 8
    if r == 0:
        return C.marble[0]
    if r in (1, 2, 3):
        if r == 3:
            return C.travertine[0] if k in (1, 5) else C.travertine[1]
        return C.travertine[1] if (x + r) % 4 else C.travertine[2]
    if r == 4:
        return C.travertine[2]                          # string course
    pier = k in AQ_PIERS
    arch_r = r - 5
    if not pier:
        # opening k in 6,7,0,1 -> position 0..3 within the arch
        pos = (k - 6) % 8
        if arch_r == 0 and pos in (0, 3):
            return C.travertine[0]                       # arch springing
        if arch_r == 0 and pos in (1, 2):
            return C.travertine[0]                       # keystone shadow
        if arch_r == 1 and pos in (0, 3):
            return C.travertine[0]
        return None
    if r == Hs - 1:
        return C.gray[1]
    pk = k - 2
    if pk == 0:
        return C.travertine[2]
    if pk == 3:
        return C.travertine[0]
    return C.travertine[1] if (r % 3) else C.travertine[0]


def _tall_piece(kind, piece):
    Hs = WALL_HS if kind == "wall" else AQ_HS
    H = T + Hs
    img = new(T, H)
    top_fn = wall_top if kind == "wall" else aq_top
    face_fn = wall_face if kind == "wall" else aq_face
    a, b = BAND
    # top surface (sprite y = footprint y, since extra == Hs)
    for y in range(T):
        for x in range(T):
            if _piece_region(piece, x, y):
                if kind == "wall":
                    c = _wall_color(top_fn(x, y, piece), x, y)
                else:
                    c = top_fn(x, y, piece)
                px(img, x, y, c)
    # front face under the band's south edge
    for x in _face_cols(piece):
        for rr in range(Hs):
            c = face_fn(x, rr, Hs)
            if c is not None:
                px(img, x, b + 1 + rr, c)
    # outline: outer silhouette only.  Each piece only outlines pixels no
    # other piece can paint, so the composite is draw-order independent.
    lay = img.copy()
    outline(lay, C.outline)
    for y in range(H):
        for x in range(T):
            if img.getpixel((x, y))[3] or not lay.getpixel((x, y))[3]:
                continue
            if kind == "aqueduct" and y >= b + 6 and (x % 8) not in AQ_PIERS:
                continue                       # inside / under an arch opening
            if piece == "c":
                keep = True
            elif piece == "n":
                keep = y <= a - 1 and x in (a - 1, b + 1)
            elif piece == "s":
                keep = b + 1 <= y <= T - 1 and x in (a - 1, b + 1)
            elif piece == "e":
                keep = x >= b + 1
            else:
                keep = x <= a - 1
            if keep:
                px(img, x, y, C.outline)
    return img


def generate():
    out = []
    for tid in TILE_IDS:
        for v in range(4):
            out.append(("bld/%s_tile_%d" % (tid, v), TILE_FN[tid](v), None))
    for fid in FARMS:
        img = farm_hut(fid)
        out.append(("bld/%s_hut" % fid, img, (0, img.height - 1)))
    for p in ["c"] + SIDES:
        out.append(("bld/pipe_%s" % p, _flat_piece(p, pipe_texel), None))
        out.append(("bld/sewer_%s" % p, _flat_piece(p, sewer_texel), None))
        out.append(("bld/bridge_%s" % p, _bridge_piece(p), None))
        for kind in ("wall", "aqueduct"):
            img = _tall_piece(kind, p)
            out.append(("bld/%s_%s" % (kind, p), img, (0, img.height - 1)))
    return out
