"""Natural objects (trees, bushes, reeds, boulders) and resource overlays.

Trees/bushes/reeds/boulders are outlined; anchor = bottom-center
(ax = w // 2, ay = h - 1).  Resource overlays are 16x16, anchor (0, 0).
"""
import math

from palette import C
from primitives import rng, new, px, get, outline, ellipse_mask, line, hash2

LIGHT = (-0.7, -0.75)


def _norm_light():
    ll = math.hypot(*LIGHT)
    return LIGHT[0] / ll, LIGHT[1] / ll


def _mask_union(blobs):
    m = set()
    for (cx, cy, rx, ry) in blobs:
        m |= ellipse_mask(cx, cy, rx, ry)
    return m


def foliage(img, mask, ramp, key, clump_r=(1.3, 2.3), density=0.55, holes=0):
    """Leafy volume: the silhouette gets a dark base, then many small clumps
    are stamped back-to-front.  Each clump's brightness mixes the global
    pseudo-normal of the whole canopy (big-form light from the top-left)
    with its own local normal, which gives every clump a lit top-left rim
    and a shaded bottom-right -- the classic pixel-art leaf look."""
    if not mask:
        return
    r = rng(key)
    lx, ly = _norm_light()
    xs = [p[0] for p in mask]
    ys = [p[1] for p in mask]
    gcx = (min(xs) + max(xs) + 1) / 2.0
    gcy = (min(ys) + max(ys) + 1) / 2.0
    grx = max(1.0, (max(xs) - min(xs) + 1) / 2.0)
    gry = max(1.0, (max(ys) - min(ys) + 1) / 2.0)
    n = len(ramp)

    def level(x, y, lcx, lcy, lr):
        gx, gy = (x + 0.5 - gcx) / grx, (y + 0.5 - gcy) / gry
        g = -(gx * lx + gy * ly)
        sx, sy = (x + 0.5 - lcx) / lr, (y + 0.5 - lcy) / lr
        l = -(sx * lx + sy * ly)
        b = 0.42 + 0.38 * g + 0.30 * l
        return max(0, min(n - 1, int(b * n)))

    for (x, y) in mask:
        px(img, x, y, ramp[0])
    pts = sorted(mask)
    count = max(3, int(len(mask) * density / 4.0))
    clumps = []
    for _ in range(count):
        cx, cy = r.choice(pts)
        clumps.append((cx + r.uniform(0, 1), cy + r.uniform(0, 1), r.uniform(*clump_r)))
    for (cx, cy, cr) in sorted(clumps, key=lambda c: (c[1], c[0])):
        for (x, y) in ellipse_mask(cx, cy, cr, cr):
            if (x, y) in mask:
                px(img, x, y, ramp[level(x, y, cx, cy, cr)])
    for _ in range(holes):
        x, y = r.choice(pts)
        if all((x + dx, y + dy) in mask for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))):
            px(img, x, y, ramp[0])


def _finish(img, c=None):
    outline(img, c or C.outline)
    return img


def _bark(img, pts, light=None, dark=None):
    light = light or C.earth[2]
    dark = dark or C.earth[1]
    for (x, y, w) in pts:
        px(img, x, y, light)
        for i in range(1, w):
            px(img, x + i, y, dark)


def tree_umbrella_pine(key, big=True):
    """Stone pine: tall bare curving trunk, wide flat dark canopy."""
    w, h = (16, 22) if big else (14, 18)
    img = new(w, h)
    cx = w // 2
    top = 6 if big else 5
    pts = []
    for y in range(top, h - 1):
        t = (y - top) / float(h - 1 - top)
        x = cx - 1 + int(round(1.5 * (1 - t) * (1 - t)))
        pts.append((x, y, 2))
    _bark(img, pts)
    px(img, cx - 2, h - 2, C.earth[1])
    px(img, cx + 1, h - 2, C.earth[1])
    # forked branches into the canopy
    px(img, cx - 2, top, C.earth[2])
    px(img, cx + 2, top, C.earth[1])
    if big:
        m = _mask_union([(8, 4.2, 6.6, 2.8), (4.5, 5.2, 3.4, 2.0), (11.5, 5.2, 3.4, 2.0),
                         (7, 2.6, 3.6, 1.8)])
    else:
        m = _mask_union([(7, 3.8, 5.8, 2.6), (4.5, 4.6, 3, 1.8), (10, 4.6, 3, 1.8)])
    m = {(x, y) for (x, y) in m if 1 <= x < w - 1 and 1 <= y}
    ramp = (C.cypress[0], C.cypress[1], C.cypress[2], C.grass[0], C.grass[1])
    foliage(img, m, ramp, key, clump_r=(1.2, 2.0), density=0.7)
    return _finish(img)


def tree_cypress(key, tall=True):
    w, h = (8, 24) if tall else (8, 18)
    img = new(w, h)
    cx = w / 2.0
    top, bot = 1, h - 3
    m = set()
    for y in range(top, bot + 1):
        t = (y - top) / float(bot - top)
        half = 2.9 * math.sin(math.pi * min(1.0, t * 0.78 + 0.05)) ** 0.8
        for x in range(w):
            if abs(x + 0.5 - cx) <= half:
                m.add((x, y))
    ramp = (C.cypress[0], C.cypress[1], C.cypress[2], C.grass[0])
    foliage(img, m, ramp, key, clump_r=(1.0, 1.7), density=0.9)
    px(img, int(cx) - 1, h - 2, C.earth[2])
    px(img, int(cx), h - 2, C.earth[1])
    return _finish(img)


def tree_olive(key, variant=0):
    """Olive: short gnarled gray trunk, airy silvery-green crown with gaps."""
    w, h = 16, 17
    img = new(w, h)
    # twisted trunk splitting in two
    _bark(img, [(7, y, 2) for y in range(10, h - 1)], C.gray[2], C.earth[1])
    _bark(img, [(6, 9, 1), (5, 8, 1), (9, 9, 1), (10, 8, 1)], C.gray[2], C.earth[1])
    px(img, 6, h - 2, C.gray[1])
    px(img, 9, h - 2, C.earth[1])
    px(img, 8, 12, C.gray[1])                # knot
    if variant == 0:
        blobs = [(4.5, 5.5, 3.6, 3.0), (10.8, 5.2, 3.8, 3.2), (7.8, 3.4, 3.4, 2.6),
                 (7.5, 7.2, 2.6, 1.6)]
    else:
        blobs = [(5.2, 4.8, 4.0, 3.4), (10.6, 5.6, 3.6, 3.0), (8.5, 3.0, 2.8, 2.2)]
    m = _mask_union(blobs)
    m = {(x, y) for (x, y) in m if 1 <= x < w - 1 and 1 <= y < 9}
    ramp = (C.cypress[1], C.olive[0], C.olive[1], C.olive[2])
    foliage(img, m, ramp, key, clump_r=(1.0, 1.8), density=0.6, holes=4)
    return _finish(img)


def tree_oak(key):
    w, h = 16, 20
    img = new(w, h)
    _bark(img, [(7, y, 2) for y in range(10, h - 1)])
    px(img, 6, h - 2, C.earth[2])
    px(img, 9, h - 2, C.earth[1])
    m = _mask_union([(5, 6.5, 3.8, 3.4), (10.8, 6, 3.8, 3.6), (8, 4, 4.2, 3.2),
                     (7.8, 9.2, 4.8, 2.8)])
    m = {(x, y) for (x, y) in m if 1 <= x < w - 1 and 1 <= y < 13}
    ramp = (C.cypress[1], C.grass[0], C.grass[1], C.grass[2], C.grass[3])
    foliage(img, m, ramp, key, clump_r=(1.3, 2.3), density=0.6)
    return _finish(img)


def bush(i):
    key = "bush/%d" % i
    w, h = [(10, 8), (10, 7), (8, 7)][i]
    img = new(w, h)
    ramps = [(C.cypress[1], C.grass[0], C.grass[1], C.grass[2], C.grass[3]),   # laurel
             (C.olive[0], C.olive[1], C.olive[2], C.grass[3]),                   # rosemary
             (C.cypress[0], C.cypress[1], C.cypress[2], C.grass[1])]            # myrtle
    blobs = [[(3.5, 4, 2.6, 2.4), (6.5, 3.6, 2.8, 2.6), (5, 5.2, 3.8, 1.8)],
             [(3.5, 3.8, 2.6, 2.2), (6.8, 3.6, 2.6, 2.4), (5, 4.6, 3.8, 1.6)],
             [(4, 3.6, 3, 2.6), (4, 4.8, 3, 1.6)]][i]
    m = _mask_union(blobs)
    m = {(x, y) for (x, y) in m if 1 <= x < w - 1 and 1 <= y < h - 1}
    foliage(img, m, ramps[i], key, clump_r=(1.0, 1.6), density=0.8)
    if i == 0:   # oleander blossoms
        for (x, y) in ((3, 3), (6, 2), (7, 4)):
            if (x, y) in m:
                px(img, x, y, C.tyrian[1] if (x + y) % 2 else C.roman_red[2])
    return _finish(img)


def reeds(i):
    """Marsh reeds: thin stems, no outline (an outline would merge stems
    into a block).  Lit left side of each stem, dry ochre tips, brown
    cattail heads; a dark wet clump anchors them to the ground."""
    key = "reeds/%d" % i
    r = rng(key)
    w, h = [(11, 14), (10, 12), (8, 10)][i]
    img = new(w, h)
    xs = list(range(1, w - 1))
    r.shuffle(xs)
    stems = sorted(xs[: (7, 6, 4)[i]])
    for j, sx in enumerate(stems):
        top = r.randrange(0, h // 2)
        lean = r.choice((-1, 0, 0, 1))
        for y in range(top, h - 1):
            x = sx + (lean if y < top + 3 else 0)
            t = (y - top) / float(h - 1 - top)
            c = C.ochre[2] if t < 0.2 else (C.grass[3] if t < 0.55 else C.grass[2])
            px(img, x, y, c)
        if r.random() < 0.5:
            px(img, sx + lean, top + 1, C.earth[2])
            px(img, sx + lean, top + 2, C.earth[1])
    for x in range(1, w - 1):
        px(img, x, h - 1, C.cypress[0])
        if x % 2 == 0:
            px(img, x, h - 2, C.cypress[2])
    return img


def boulder(i):
    """Faceted limestone boulders: lit top facet, mid left, dark right."""
    key = "boulder/%d" % i
    r = rng(key)
    w, h = [(9, 7), (11, 8), (7, 6)][i]
    img = new(w, h)
    blobs = {0: [(4.5, 3.8, 3.4, 2.6)],
             1: [(4, 4.4, 3, 2.6), (7, 3.8, 3, 2.8)],
             2: [(3.5, 3.4, 2.5, 2.0)]}[i]
    m = _mask_union(blobs)
    m = {(x, y) for (x, y) in m if 1 <= x < w - 1 and 1 <= y < h - 1}
    xs = [p[0] for p in m]
    ys = [p[1] for p in m]
    cx = (min(xs) + max(xs) + 1) / 2.0
    top_y = min(ys)
    bot_y = max(ys)
    for (x, y) in m:
        rel = (y - top_y) / float(max(1, bot_y - top_y))
        side = (x + 0.5 - cx)
        if rel < 0.45:
            c = C.marble[0] if side < 0 else C.travertine[2]
            if r.random() < 0.15:
                c = C.travertine[1]
        elif side < -0.5:
            c = C.travertine[1]
        else:
            c = C.travertine[0] if side < 1.5 else C.gray[1]
        px(img, x, y, c)
    # facet line under the top face and a crack
    for (x, y) in m:
        rel = (y - top_y) / float(max(1, bot_y - top_y))
        if 0.45 <= rel < 0.45 + 1.0 / max(1, bot_y - top_y) and (x + y) % 3:
            px(img, x, y, C.travertine[0] if x < cx else C.gray[1])
    return _finish(img)


TREES = [
    lambda: tree_umbrella_pine("tree/0", True),
    lambda: tree_cypress("tree/1", True),
    lambda: tree_olive("tree/2", 0),
    lambda: tree_oak("tree/3"),
    lambda: tree_cypress("tree/4", False),
    lambda: tree_olive("tree/5", 1),
]


# ------------------------------------------------------------ resources
RESOURCES = ["clay", "stone", "iron", "silver", "marble", "limestone", "pozzolana", "fish"]


def _flecks(img, r, n, colors, shape="dot", margin=2):
    for _ in range(n):
        x, y = r.randrange(margin, 16 - margin - 1), r.randrange(margin, 16 - margin - 1)
        if shape == "chip":
            px(img, x, y, colors[0])
            px(img, x + 1, y, colors[1])
            px(img, x + 1, y + 1, colors[2])
        elif shape == "patch":
            px(img, x, y, colors[0])
            px(img, x + 1, y, colors[1])
            px(img, x, y + 1, colors[1])
            if r.random() < 0.5:
                px(img, x + 1, y + 1, colors[2])
        else:
            px(img, x, y, r.choice(colors))


def resource(name):
    img = new(16, 16)
    r = rng("res/" + name)
    if name == "clay":
        _flecks(img, r, 4, (C.roof_terracotta[2], C.roof_terracotta[1], C.roof_terracotta[0]), "patch")
    elif name == "stone":
        _flecks(img, r, 4, (C.gray[3], C.gray[2], C.gray[1]), "chip")
    elif name == "iron":
        _flecks(img, r, 4, (C.roof_terracotta[1], C.roman_red[0], C.gray[0]), "chip")
        _flecks(img, r, 3, (C.roman_red[1], C.earth[1]), "dot")
    elif name == "silver":
        _flecks(img, r, 4, (C.white, C.gray[3], C.gray[1]), "chip")
    elif name == "marble":
        _flecks(img, r, 5, (C.white, C.marble[1], C.marble[0]), "chip")
    elif name == "limestone":
        _flecks(img, r, 5, (C.travertine[2], C.travertine[1], C.travertine[0]), "chip")
    elif name == "pozzolana":
        _flecks(img, r, 4, (C.tyrian[1], C.gray[1], C.tyrian[0]), "patch")
        _flecks(img, r, 3, (C.gray[0], C.roman_red[0]), "dot")
    elif name == "fish":
        # two ripple arcs and a fish shadow
        for (cx, cy) in ((4, 4), (10, 10)):
            for dx in (-1, 0, 1):
                px(img, cx + dx, cy - 1, C.sea[4] if dx == 0 else C.sea[3])
            px(img, cx - 2, cy, C.sea[3])
            px(img, cx + 2, cy, C.sea[3])
        for x in range(9, 13):
            px(img, x, 4, C.sea[0])
        px(img, 8, 3, C.sea[0])
        px(img, 8, 5, C.sea[0])
    return img


def generate():
    out = []
    for i, fn in enumerate(TREES):
        img = fn()
        out.append(("obj/tree_%d" % i, img, (img.width // 2, img.height - 1)))
    for i in range(3):
        for name, fn in (("bush", bush), ("reeds", reeds), ("boulder", boulder)):
            img = fn(i)
            out.append(("obj/%s_%d" % (name, i), img, (img.width // 2, img.height - 1)))
    for name in RESOURCES:
        out.append(("obj/res_%s" % name, resource(name), None))
    return out
