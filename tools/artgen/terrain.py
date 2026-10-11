"""Terrain tiles, autotile transitions, cliffs and water shimmer.

Rules
- 16x16 tiles, no outlines on terrain.
- Base tiles are opaque.  Variants share base color and feature density and
  keep multi-pixel features 1px away from tile borders, so any variant tiles
  seamlessly against any other.
- Transitions are dual-grid overlays (see "dual-grid blends" below).  Dither
  parity is taken from cell-local (x + y), consistent across cells because 16
  is even.
"""
import math

from palette import C
from primitives import rng, new, px, rect, get, hash2

T = 16
TERRAINS = ["grass", "fertile", "sand", "rock", "forest", "marsh", "shallow", "deep"]
PRIORITY = ["deep", "shallow", "marsh", "sand", "rock", "grass", "fertile", "forest"]
SIDES = ["n", "e", "s", "w"]
CORNERS = ["ne", "se", "sw", "nw"]
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
    # close to grass (fertility is read from the overlay): a little denser, with tilled-earth flecks
    _grass_like(img, r, g[2], g[1], g[1], g[3], [(C.earth[2], 4), (C.earth[1], 3), (C.grass[0], 6)], tuft_n=7)
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


# ------------------------------------------------------------ dual-grid blends
# Terrain transitions use a dual grid: a 16x16 cell is drawn centred on every tile corner, so its
# four corners sit on the centres of the four tiles around that point.  The cell takes the lowest-
# priority terrain of the four as a full base tile, then, for each higher terrain L present, the
# overlay terrain/dg_<L>_<mask>, where mask has bit NW=1, NE=2, SW=4, SE=8 set for every corner
# whose terrain has priority >= L.  See dual_layers() (mirrored by render/terrain.cpp).
#
# An overlay's shape is the 0.5 contour of the bilinear blend of its corner bits, pushed about by
# a small noise that is periodic over 16 px, so shapes agree along every shared cell edge: straight
# runs wobble, outer corners round off and diagonal coasts become smooth 45-degree lines.
DUAL_TERRAINS = [t for t in PRIORITY if t != PRIORITY[0]]   # the lowest never overlays


def _paint_fringe(img, t, inside_fn, key):
    """inside_fn(x, y) -> 'solid' | 'rim' | 'dither' | 'foam' | None.  The terrain texture is
    read half a tile off ((x + 8, y + 8) mod 16), matching full tiles drawn on dual cells."""
    src = TEXTURES[t](0, key=key + "/tex")
    fr = FRINGE[t]
    for y in range(T):
        for x in range(T):
            k = inside_fn(x, y)
            if k is None:
                continue
            chk = (x + y) % 2 == 0
            tex = src.getpixel(((x + 8) % T, (y + 8) % T))
            if k == "solid":
                px(img, x, y, tex)
            elif k == "rim":
                px(img, x, y, fr["rim"] or tex)
            elif k == "dither":
                if chk:
                    px(img, x, y, fr["rim"] or tex)
            elif k == "foam":
                if fr["foam"] is not None and chk and hash2(x, y, 7) % 3 == 0:
                    px(img, x, y, fr["foam"])
    return img


def _dual_noise(x, y):
    """Smooth noise with period 16 in x and y (so it matches across cell edges)."""
    u, v = 2 * math.pi * (x + 0.5) / T, 2 * math.pi * (y + 0.5) / T
    return (0.075 * math.sin(u + 2 * v + 0.7) + 0.06 * math.sin(3 * u - v + 2.1) +
            0.045 * math.sin(2 * u + 3 * v + 4.0))


def _dual_field(mask, x, y):
    """(value, gradient length per px) of the corner blend at pixel (x, y)."""
    nw, ne, sw, se = ((mask >> i) & 1 for i in range(4))
    u, v = (x + 0.5) / T, (y + 0.5) / T
    top, bot = nw + (ne - nw) * u, sw + (se - sw) * u
    f = top + (bot - top) * v
    fu = (ne - nw) * (1 - v) + (se - sw) * v
    fv = bot - top
    g = math.hypot(fu, fv) / T
    return f + _dual_noise(x, y), max(g, 0.02)


def dual(t, mask):
    def inside(x, y):
        f, g = _dual_field(mask, x, y)
        d = (f - 0.5) / g                  # signed distance to the contour, in px (inside > 0)
        if d >= 1.0:
            return "solid"
        if d >= 0.0:
            return "rim" if t not in WATER else "solid"
        if d >= -1.0:
            return "dither"
        if d >= -2.0:
            return "foam"
        return None

    return _paint_fringe(new(T, T), t, inside, "dual/%s/%d" % (t, mask))


def dual_layers(terrain_at, x, y, variant=0):
    """Sprite names for the dual cell whose corners are the centres of tiles (x-1, y-1) .. (x, y).
    terrain_at(x, y) must clamp to the map.  Drawn at pixel (x*16 - 8, y*16 - 8)."""
    corners = [terrain_at(x - 1, y - 1), terrain_at(x, y - 1), terrain_at(x - 1, y), terrain_at(x, y)]
    prio = [PRIORITY.index(c) for c in corners]
    low = min(prio)
    names = ["terrain/%s_%d" % (PRIORITY[low], variant % 4)]
    for p in sorted(set(prio)):
        if p == low:
            continue
        mask = sum(1 << i for i in range(4) if prio[i] >= p)
        names.append("terrain/dg_%s_%d" % (PRIORITY[p], mask))
    return names


# ------------------------------------------------------------ cliffs
# Height steps use the dual grid too: terrain/hc_<mask> (mask bits as for dg_, set for corners on
# the higher level) is drawn along the same noisy contour as the terrain overlays, so cliffs follow
# coastlines.  Where the high ground lies to the north the step shows a face of earth strata below
# the contour (lit when it turns west, dark when it turns east).  Other steps draw nothing: the
# ground shader's smooth height shading shows them.
def height_cliff(mask):
    img = new(T, T)
    e = C.earth
    for y in range(T):
        for x in range(T):
            f, g = _dual_field(mask, x, y)
            d = (f - 0.5) / g                      # px from the contour, + on the high side
            nw, ne, sw, se = ((mask >> i) & 1 for i in range(4))
            u, v = (x + 0.5) / T, (y + 0.5) / T
            gx = (ne - nw) * (1 - v) + (se - sw) * v
            gy = (sw - nw) * (1 - u) + (se - ne) * u
            n = math.hypot(gx, gy) or 1.0
            gx, gy = gx / n, gy / n                # unit vector toward the high side
            face = max(0.0, -gy)                   # high ground to the north: the face is visible
            depth = 1.0 + 3.2 * face               # face height in px
            if -depth <= d < 0 and face > 0.25:
                k = -d                             # 0 at the top of the face
                if k < 1.0:
                    c = e[3]                       # sunlit top edge
                elif d < -depth + 1.0:
                    c = e[0]                       # foot of the face
                else:
                    c = e[2] if (int(y) // 2 + x) % 4 else e[1]   # strata
                    if gx > 0.45:
                        c = e[1]                   # turning east, away from the light
                    elif gx < -0.45:
                        c = e[3] if (x + y) % 2 else e[2]
                px(img, x, y, c)
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
    for t in DUAL_TERRAINS:
        for m in range(1, 15):
            out.append(("terrain/dg_%s_%d" % (t, m), dual(t, m), None))
    for m in range(1, 15):
        out.append(("terrain/hc_%d" % m, height_cliff(m), None))
    for f in range(4):
        out.append(("terrain/water_shimmer_%d" % f, shimmer(f), None))
    return out
