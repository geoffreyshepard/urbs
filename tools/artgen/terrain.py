"""Terrain tiles, autotile transitions, cliffs and water shimmer.

Rules
- 16x16 tiles, no outlines on terrain.
- Base tiles are opaque.  Variants share base color and feature density and
  keep multi-pixel features 1px away from tile borders, so any variant tiles
  seamlessly against any other.
- Edges/corners: the fringe depth is pinned to EDGE_END at both tile ends so
  neighbouring edge tiles and corner pieces connect.  Dither parity is taken
  from tile-local (x + y), consistent across tiles because 16 is even.
"""
import math

from palette import C
from primitives import rng, new, px, rect, get, hash2

T = 16
TERRAINS = ["grass", "fertile", "sand", "rock", "forest", "marsh", "shallow", "deep"]
PRIORITY = ["deep", "shallow", "marsh", "sand", "rock", "grass", "fertile", "forest"]
SIDES = ["n", "e", "s", "w"]
CORNERS = ["ne", "se", "sw", "nw"]
EDGE_END = 3          # fringe depth at the two ends of every edge piece
WATER = {"shallow", "deep"}


# ------------------------------------------------------------ base textures
def _in(x, y, m=1):
    return m <= x < T - m and m <= y < T - m


def _grass_like(img, r, base, dark, mid_light, light, extra=(), tuft_n=6, dry=None):
    rect(img, 0, 0, T, T, base)
    for y in range(T):
        for x in range(T):
            v = r.random()
            if v < 0.07:
                px(img, x, y, dark)
            elif v < 0.16:
                px(img, x, y, mid_light)
    # tufts: lit blade over a shadow pixel (light from top-left)
    for _ in range(tuft_n):
        x, y = r.randrange(1, T - 2), r.randrange(1, T - 2)
        px(img, x, y, light)
        px(img, x + 1, y, mid_light)
        px(img, x, y + 1, dark)
        px(img, x + 1, y + 1, dark)
    for color, n in extra:
        for _ in range(n):
            px(img, r.randrange(1, T - 1), r.randrange(1, T - 1), color)


def tex_grass(v, key=None):
    img = new(T, T)
    r = rng(key or "terrain/grass/%d" % v)
    g = C.grass
    extra = [(C.ochre[1], 2)]
    if v == 2:
        extra.append((C.marble[1], 1))       # a tiny flower
    if v == 3:
        extra.append((C.gold, 1))
    _grass_like(img, r, g[2], g[1], g[3], g[3], extra, tuft_n=5)
    return img


def tex_fertile(v, key=None):
    img = new(T, T)
    r = rng(key or "terrain/fertile/%d" % v)
    g = C.grass
    _grass_like(img, r, g[1], g[0], g[2], g[3], [(C.earth[2], 3), (C.earth[1], 2)], tuft_n=6)
    return img


def tex_forest(v, key=None):
    img = new(T, T)
    r = rng(key or "terrain/forest/%d" % v)
    cy = C.cypress
    rect(img, 0, 0, T, T, cy[2])
    for y in range(T):
        for x in range(T):
            q = r.random()
            if q < 0.22:
                px(img, x, y, cy[1])
            elif q < 0.28:
                px(img, x, y, C.grass[0])
            elif q < 0.31:
                px(img, x, y, C.earth[1])
    for _ in range(4):                       # fallen needles / leaf litter
        x, y = r.randrange(1, T - 2), r.randrange(1, T - 1)
        px(img, x, y, C.olive[0])
        px(img, x + 1, y, C.earth[1])
    for _ in range(3):                       # moss highlight
        px(img, r.randrange(1, T - 1), r.randrange(1, T - 1), C.grass[1])
    return img


def tex_sand(v, key=None):
    img = new(T, T)
    r = rng(key or "terrain/sand/%d" % v)
    o = C.ochre
    rect(img, 0, 0, T, T, o[2])
    for y in range(T):
        for x in range(T):
            q = r.random()
            if q < 0.12:
                px(img, x, y, o[3])
            elif q < 0.20:
                px(img, x, y, o[1])
    for _ in range(2):                       # wind ripple: light over dark
        x, y = r.randrange(1, T - 5), r.randrange(1, T - 2)
        for i in range(3 + r.randrange(2)):
            px(img, x + i, y, o[3])
            px(img, x + i + 1, y + 1, o[1])
    if v in (1, 3):
        x, y = r.randrange(1, T - 2), r.randrange(1, T - 2)
        px(img, x, y, C.travertine[2])
        px(img, x + 1, y + 1, C.travertine[0])
    return img


def tex_rock(v, key=None):
    """Barren limestone ground: gravel speckle plus a few flat bedrock plates
    lit from the top-left (light upper rim, dark lower rim)."""
    img = new(T, T)
    r = rng(key or "terrain/rock/%d" % v)
    tv = C.travertine
    rect(img, 0, 0, T, T, tv[0])
    for y in range(T):
        for x in range(T):
            q = r.random()
            if q < 0.10:
                px(img, x, y, C.gray[2])
            elif q < 0.20:
                px(img, x, y, C.earth[3])
            elif q < 0.24:
                px(img, x, y, C.earth[2])
    for _ in range(2 + v % 2):
        w, h = r.randrange(3, 6), r.randrange(2, 4)
        x, y = r.randrange(1, T - w - 1), r.randrange(1, T - h - 1)
        for yy in range(y, y + h + 1):
            for xx in range(x, x + w + 1):
                if (xx in (x, x + w)) and (yy in (y, y + h)):
                    continue                      # rounded corners
                if yy == y + h:
                    c = C.earth[1]                # shadow lip
                elif yy == y or xx == x:
                    c = tv[2]                     # lit rim
                elif xx == x + w:
                    c = C.gray[1]
                else:
                    c = tv[1]
                px(img, xx, yy, c)
    return img


def tex_marsh(v, key=None):
    img = new(T, T)
    r = rng(key or "terrain/marsh/%d" % v)
    ol = C.olive
    rect(img, 0, 0, T, T, ol[0])
    for y in range(T):
        for x in range(T):
            q = r.random()
            if q < 0.10:
                px(img, x, y, ol[1])
            elif q < 0.15:
                px(img, x, y, C.earth[1])
    # puddles: irregular pools; dark bank on the north rim (shadow), a lit
    # south rim, deep center and one glint
    from primitives import ellipse_mask
    for _ in range((1, 2, 1, 0)[v]):
        rx, ry = r.uniform(2.8, 4.2), r.uniform(1.6, 2.3)
        cx, cy = r.uniform(rx + 1.5, T - rx - 1.5), r.uniform(ry + 1.5, T - ry - 1.5)
        m = ellipse_mask(cx, cy, rx, ry) | ellipse_mask(cx + r.uniform(-1.5, 1.5), cy + 0.6, rx * 0.7, ry)
        m = {(x, y) for (x, y) in m if _in(x, y, 1)}
        for (x, y) in m:
            px(img, x, y, C.sea[2] if (x, y - 1) in m else C.sea[1])
        for (x, y) in m:
            if (x, y - 1) not in m:
                px(img, x, y - 1, C.earth[0])
            if (x, y + 1) not in m and _in(x, y + 1, 1):
                px(img, x, y + 1, C.olive[1])
        gx, gy = sorted(m)[len(m) // 2]
        px(img, gx, gy, C.sea[3])
    for _ in range(4):                       # sedge tufts
        x, y = r.randrange(1, T - 1), r.randrange(1, T - 2)
        px(img, x, y, C.grass[2])
        px(img, x, y + 1, C.grass[0])
    return img


def _water(img, r, base, dark, light, dashes, glints=0):
    rect(img, 0, 0, T, T, base)
    for y in range(T):
        for x in range(T):
            if r.random() < 0.06:
                px(img, x, y, dark)
    for _ in range(dashes):                  # gentle wave dashes
        x, y = r.randrange(1, T - 4), r.randrange(1, T - 1)
        n = r.randrange(2, 4)
        for i in range(n):
            px(img, x + i, y, light)
    for _ in range(glints):
        px(img, r.randrange(1, T - 1), r.randrange(1, T - 1), C.sea[4])


def tex_shallow(v, key=None):
    img = new(T, T)
    r = rng(key or "terrain/shallow/%d" % v)
    _water(img, r, C.sea[2], C.sea[1], C.sea[3], 3, glints=0)
    return img


def tex_deep(v, key=None):
    img = new(T, T)
    r = rng(key or "terrain/deep/%d" % v)
    _water(img, r, C.sea[0], C.sea[0], C.sea[1], 4)
    return img


TEXTURES = {
    "grass": tex_grass, "fertile": tex_fertile, "sand": tex_sand, "rock": tex_rock,
    "forest": tex_forest, "marsh": tex_marsh, "shallow": tex_shallow, "deep": tex_deep,
}

# fringe treatment per terrain when it bleeds onto lower-priority neighbours:
#   rim  = color of the outermost solid fringe pixels (wet lip / shadow lip)
#   foam = color of sparse pixels just outside the fringe (only for terrains
#          whose lower neighbours are always water/wetland: sand, marsh)
FRINGE = {
    "forest":  dict(rim=C.cypress[0], foam=None),
    "fertile": dict(rim=C.grass[0], foam=None),
    "grass":   dict(rim=C.grass[0], foam=None),
    "rock":    dict(rim=C.earth[1], foam=None),
    "sand":    dict(rim=C.ochre[1], foam=C.sea[4]),
    "marsh":   dict(rim=C.earth[0], foam=C.sea[3]),
    "shallow": dict(rim=C.sea[3], foam=None),
    "deep":    dict(rim=None, foam=None),
}


def base_tile(t, v):
    return TEXTURES[t](v)


# ------------------------------------------------------------ edges/corners
def _profile(key):
    """Fringe depth per position 0..15, pinned to EDGE_END at both ends;
    a smooth wave (sum of two sines) so blends read as organic shapes."""
    r = rng(key)
    p1, p2 = r.uniform(0, 6.283), r.uniform(0, 6.283)
    a1, a2 = r.uniform(0.7, 1.2), r.uniform(0.4, 0.7)
    d = []
    for a in range(T):
        env = math.sin(math.pi * (a + 0.5) / T)          # 0 at the ends
        wave = a1 * math.sin(2 * math.pi * a / T * 1.0 + p1) + a2 * math.sin(2 * math.pi * a / T * 3.0 + p2)
        d.append(int(round(EDGE_END + 0.6 + env * wave)))
    d[0] = d[-1] = EDGE_END
    return [max(2, min(6, v)) for v in d]


def _side_coords(side, x, y):
    """(along, depth) of pixel for a fringe entering from `side`."""
    if side == "n":
        return x, y
    if side == "s":
        return x, T - 1 - y
    if side == "w":
        return y, x
    return y, T - 1 - x        # e


def _paint_fringe(img, t, inside_fn, key):
    """inside_fn(x, y) -> 'solid' | 'rim' | 'dither' | 'foam' | None."""
    src = TEXTURES[t](0, key=key + "/tex")
    fr = FRINGE[t]
    for y in range(T):
        for x in range(T):
            k = inside_fn(x, y)
            if k is None:
                continue
            chk = (x + y) % 2 == 0
            if k == "solid":
                px(img, x, y, src.getpixel((x, y)))
            elif k == "rim":
                px(img, x, y, fr["rim"] or src.getpixel((x, y)))
            elif k == "dither":
                if chk:
                    px(img, x, y, fr["rim"] or src.getpixel((x, y)))
            elif k == "foam":
                if fr["foam"] is not None and chk and hash2(x, y, 7) % 3 == 0:
                    px(img, x, y, fr["foam"])
    return img


def edge(t, side):
    prof = _profile("edge/%s/%s" % (t, side))

    def inside(x, y):
        a, d = _side_coords(side, x, y)
        lim = prof[a]
        if d < lim - 1:
            return "solid"
        if d == lim - 1:
            return "rim" if t not in WATER else "solid"
        if d == lim:
            return "dither"
        if d == lim + 1:
            return "foam"
        return None

    return _paint_fringe(new(T, T), t, inside, "edge/%s/%s" % (t, side))


def corner(t, c):
    # distance measured from the corner point
    def inside(x, y):
        dx = (T - 1 - x) if "e" in c else x
        dy = y if "n" in c else (T - 1 - y)
        dist = math.hypot(dx + 0.5, dy + 0.5)
        if dist < EDGE_END - 0.6:
            return "solid"
        if dist < EDGE_END + 0.5:
            return "rim" if t not in WATER else "solid"
        if dist < EDGE_END + 1.5:
            return "dither"
        if dist < EDGE_END + 2.5:
            return "foam"
        return None

    return _paint_fringe(new(T, T), t, inside, "corner/%s/%s" % (t, c))


# ------------------------------------------------------------ cliffs
def cliff(side):
    img = new(T, T)
    r = rng("cliff/" + side)
    e = C.earth
    if side == "s":
        # irregular top of the face; pinned at ends for seamless runs
        top = [11] * T
        for x in range(2, T - 2):
            top[x] = 11 + r.choice((-1, 0, 0, 0, 1))
        for x in range(T):
            t0 = top[x]
            px(img, x, t0 - 1, C.travertine[1] if (x % 3) else C.earth[3])   # lit lip
            for y in range(t0, T):
                depth = y - t0
                if y == T - 1:
                    c = e[0]
                elif depth == 0:
                    c = e[3]
                else:
                    c = e[2] if (x + (y // 2)) % 4 else e[1]   # strata
                    if r.random() < 0.15:
                        c = e[1]
                px(img, x, y, c)
            if x % 5 == 2:                              # vertical fissure
                for y in range(t0 + 1, T - 1):
                    px(img, x, y, e[1])
        # small boulder detail
        bx = r.randrange(3, T - 4)
        px(img, bx, T - 3, C.travertine[1])
        px(img, bx + 1, T - 3, C.travertine[0])
        px(img, bx + 1, T - 2, e[0])
    else:
        # e: face looks away from the light -> dark; w: faces the light
        for y in range(T):
            jag = 1 if (hash2(y, 3, 11) % 4 == 0 and 1 < y < T - 2) else 0
            if side == "e":
                cols = [(T - 1, e[0]), (T - 2, e[1]), (T - 3 - jag, e[1] if jag else None)]
                lip = T - 3 - jag if not jag else T - 4
                for x, c in cols:
                    if c is not None:
                        px(img, x, y, c)
                if y % 2 == 0:
                    px(img, lip, y, C.earth[2])
            else:
                px(img, 0, y, e[1])
                px(img, 1, y, e[3])
                px(img, 2 + jag, y, C.travertine[1] if y % 2 else e[3])
    return img


# ------------------------------------------------------------ water shimmer


def shimmer(f):
    img = new(T, T)
    r = rng("shimmer")
    glints = []
    for i in range(3):
        glints.append((r.randrange(1, T - 4), r.randrange(1, T - 1), i))
    for (x, y, ph) in glints:
        p = (f + ph) % 4
        if p == 1:
            px(img, x + 1, y, C.sea[3])
        elif p == 2:
            px(img, x, y, C.sea[3])
            px(img, x + 1, y, C.sea[4])
            px(img, x + 2, y, C.sea[3])
        elif p == 3:
            px(img, x + 2, y, C.sea[3])
    return img


# ------------------------------------------------------------ export
def generate():
    """-> list of (name, image, anchor or None)"""
    out = []
    for t in TERRAINS:
        for v in range(4):
            out.append(("terrain/%s_%d" % (t, v), base_tile(t, v), None))
    for t in TERRAINS:
        for s in SIDES:
            out.append(("terrain/edge_%s_%s" % (t, s), edge(t, s), None))
        for c in CORNERS:
            out.append(("terrain/corner_%s_%s" % (t, c), corner(t, c), None))
    for s in ("s", "e", "w"):
        out.append(("terrain/cliff_%s" % s, cliff(s), None))
    for f in range(4):
        out.append(("terrain/water_shimmer_%d" % f, shimmer(f), None))
    return out
