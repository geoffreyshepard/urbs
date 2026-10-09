"""Building kit: projection helpers, walls, roofs, colonnades, domes, props.

Coordinates
- Ground coords (gx, gy) are pixels inside the footprint (0..w*16-1,
  0..h*16-1).  A point at height z appears at sprite (gx, gy + E - z),
  where E = extra height above the footprint (8 + 4*min(w, h)).
- A "box" with ground rect (x, gy, w, d) and height z shows its top (roof)
  as the ground rect shifted up by z, and its south face (z rows) below.

Parts are drawn into layers (same size as the sprite).  `Bld.put(layer)`
outlines the layer with the dark outline color and composites it, so every
solid part gets a crisp silhouette while ground surfaces (ol=False) do not.
Draw back (north) to front (south).
"""
import math

from palette import C
from primitives import (new, px, rect, hline, vline, rng, outline, ellipse_mask, line,
                        get, hash2)
from primitives import roof as _roof

TILE = 16


def extra_height(w, h):
    return 8 + 4 * min(w, h)


# ------------------------------------------------------------ materials
MAT = {
    "stucco": dict(base=C.marble[0], lit=C.marble[1], shade=C.travertine[1], eave=C.travertine[0], joint=None),
    "ochre": dict(base=C.ochre[2], lit=C.ochre[3], shade=C.ochre[1], eave=C.ochre[0], joint=None),
    "red": dict(base=C.roman_red[2], lit=C.roof_terracotta[3], shade=C.roman_red[1], eave=C.roman_red[0], joint=None),
    "stone": dict(base=C.gray[3], lit=C.marble[0], shade=C.gray[2], eave=C.gray[1], joint=C.gray[2]),
    "travertine": dict(base=C.travertine[2], lit=C.marble[0], shade=C.travertine[1], eave=C.travertine[0], joint=C.travertine[1]),
    "marble": dict(base=C.marble[1], lit=C.white, shade=C.marble[0], eave=C.gray[3], joint=C.marble[0]),
    "brick": dict(base=C.roof_terracotta[2], lit=C.roof_terracotta[3], shade=C.roof_terracotta[1], eave=C.roof_terracotta[0], joint=C.roof_terracotta[1]),
    "wood": dict(base=C.earth[2], lit=C.earth[3], shade=C.earth[1], eave=C.earth[0], joint=C.earth[1]),
    "wattle": dict(base=C.ochre[1], lit=C.ochre[2], shade=C.ochre[0], eave=C.earth[1], joint=C.earth[2]),
}
ROOF = {
    "terracotta": C.roof_terracotta,
    "lead": (C.gray[0], C.gray[1], C.gray[2], C.gray[3]),
    "thatch": (C.earth[1], C.ochre[0], C.ochre[1], C.ochre[2]),
    "wood": (C.earth[0], C.earth[1], C.earth[2], C.earth[3]),
    "old": (C.earth[1], C.roof_terracotta[0], C.roof_terracotta[1], C.roof_terracotta[2]),
    "bronze": (C.earth[1], C.ochre[0], C.gold, C.ochre[3]),
    "canvas": (C.travertine[0], C.travertine[1], C.marble[0], C.marble[1]),
}


class Bld:
    def __init__(self, bid, w, h, extra=None):
        self.id = bid
        self.w, self.h = w, h
        self.W, self.D = w * TILE, h * TILE
        self.E = extra_height(w, h) if extra is None else extra
        self.H = self.D + self.E
        self.img = new(self.W, self.H)
        self.r = rng("bld/" + bid)

    def sy(self, gy, z=0):
        return gy + self.E - z

    def layer(self):
        return new(self.W, self.H)

    def put(self, L, ol=True):
        if ol:
            outline(L, C.outline)
        self.img.alpha_composite(L)

    def result(self):
        return ("bld/" + self.id, self.img, (0, self.H - 1))


# ------------------------------------------------------------ ground
def _paving_color(x, y, kind):
    if kind == "paving":          # travertine slabs 6x4, staggered rows
        row = y // 4
        xo = x + (3 if row % 2 else 0)
        if y % 4 == 3 or xo % 6 == 5:
            return C.travertine[1]
        if y % 4 == 0 or xo % 6 == 0:
            return C.marble[0]
        return C.travertine[2] if hash2(xo // 6, row, 3) % 4 else C.marble[0]
    if kind == "marble":          # big marble slabs
        if y % 6 == 5 or (x + (4 if (y // 6) % 2 else 0)) % 8 == 7:
            return C.marble[0]
        return C.marble[1] if hash2(x, y, 4) % 11 else C.white
    if kind == "dirt":
        h = hash2(x, y, 5) % 100
        return C.earth[2] if h < 12 else (C.ochre[0] if h < 22 else C.earth[3])
    if kind == "sand":
        h = hash2(x, y, 6) % 100
        return C.ochre[1] if h < 15 else (C.ochre[3] if h < 25 else C.ochre[2])
    if kind == "grass":
        h = hash2(x, y, 7) % 100
        return C.grass[1] if h < 12 else (C.grass[3] if h < 20 else C.grass[2])
    if kind == "lawn":
        h = hash2(x, y, 8) % 100
        return C.grass[0] if h < 10 else (C.grass[2] if h < 20 else C.grass[1])
    if kind == "water":
        if hash2(x // 3, y, 9) % 9 == 0:
            return C.sea[3]
        return C.sea[2] if hash2(x, y, 10) % 13 else C.sea[1]
    if kind == "cobble":
        from roads import TEX
        return TEX["paved"][y % 16][x % 16]
    if kind == "mud":
        h = hash2(x, y, 11) % 100
        return C.earth[1] if h < 20 else (C.earth[0] if h < 28 else C.earth[2])
    if kind == "wood":            # deck planks (run east-west)
        if y % 3 == 2:
            return C.earth[1]
        return C.earth[3] if (x + 5 * (y // 3)) % 11 else C.earth[2]
    if kind == "gravel":
        h = hash2(x, y, 12) % 100
        return C.gray[2] if h < 20 else (C.travertine[1] if h < 35 else C.travertine[0])
    raise KeyError(kind)


def ground(b, L, x, gy, w, d, kind, z=0, border=None):
    y0 = b.sy(gy, z)
    for yy in range(d):
        for xx in range(w):
            px(L, x + xx, y0 + yy, _paving_color(x + xx, gy + yy, kind))
    if border:
        hline(L, x, x + w - 1, y0, border[0])
        vline(L, x, y0, y0 + d - 1, border[0])
        hline(L, x, x + w - 1, y0 + d - 1, border[1])
        vline(L, x + w - 1, y0, y0 + d - 1, border[1])


def ground_ellipse(b, L, cx, cgy, rx, ry, kind, z=0):
    m = ellipse_mask(cx, b.sy(cgy, z), rx, ry)
    for (x, y) in m:
        px(L, x, y, _paving_color(x, y - b.E + z, kind))
    return m


# ------------------------------------------------------------ faces
def face(L, x, y0, w, h, mat="stucco", key="f", plinth=True, courses=True):
    """South wall face covering sprite rows y0..y0+h-1."""
    m = MAT[mat]
    r = rng(key)
    for yy in range(h):
        for xx in range(w):
            c = m["base"]
            if m["joint"] is not None and courses:
                if yy % 3 == 2:
                    c = m["joint"]
                elif (xx + (3 if (yy // 3) % 2 else 0)) % 6 == 5:
                    c = m["joint"]
            elif r.random() < 0.05:
                c = m["shade"]
            if xx == 0:
                c = m["lit"]
            elif xx == w - 1:
                c = m["shade"]
            if yy == 0:
                c = m["eave"]
            px(L, x + xx, y0 + yy, c)
    if plinth and h >= 5:
        hline(L, x, x + w - 1, y0 + h - 1, C.gray[2])
        hline(L, x, x + w - 1, y0 + h - 2, C.gray[3])


def roof_top(L, x, y, w, d, kind="hip", ramp="terracotta", key="r"):
    rp = ROOF[ramp] if isinstance(ramp, str) else ramp
    if kind == "hip":
        _roof(L, x, y, w, d, ramp=rp, ridge="h", key=key)
    elif kind == "gable":                     # ridge N-S
        _roof(L, x, y, w, d, ramp=rp, ridge="v", key=key)
    elif kind == "shed":                      # lean-to sloping down to the south
        for yy in range(d):
            for xx in range(w):
                c = rp[2] if (x + xx) % 3 != 2 else rp[1]
                if yy < 1:
                    c = rp[3]
                px(L, x + xx, y + yy, c)
        hline(L, x, x + w - 1, y + d - 1, rp[0])
    elif kind == "flat":                      # terrace with parapet
        rect(L, x, y, w, d, C.travertine[1])
        for yy in range(1, d - 1):
            for xx in range(1, w - 1):
                if hash2(x + xx, y + yy, 13) % 9 == 0:
                    px(L, x + xx, y + yy, C.travertine[0])
        hline(L, x, x + w - 1, y, C.marble[0])
        vline(L, x, y, y + d - 1, C.marble[0])
        hline(L, x, x + w - 1, y + d - 1, C.travertine[0])
        vline(L, x + w - 1, y, y + d - 1, C.travertine[0])
    elif kind == "tent":                      # ridge E-W canvas tent
        mid = y + d // 2
        for yy in range(d):
            for xx in range(w):
                c = rp[3] if y + yy < mid else rp[2]
                if y + yy == mid:
                    c = rp[1]
                if xx == w - 1:
                    c = rp[1]
                px(L, x + xx, y + yy, c)
    else:
        raise KeyError(kind)


def box(b, L, x, gy, w, d, z, mat="stucco", roof="hip", ramp="terracotta", z0=0, plinth=True,
        key=None):
    """Building block; returns (face_top_row, face_bottom_row)."""
    key = key or "%s/%d/%d" % (b.id, x, gy)
    top = b.sy(gy, z)
    roof_top(L, x, top, w, d, roof, ramp, key)
    f0 = top + d
    f1 = b.sy(gy + d - 1, z0)
    if f1 >= f0:
        face(L, x, f0, w, f1 - f0 + 1, mat, key, plinth=plinth and z0 == 0)
    return f0, f1


def door(L, cx, bottom, h=6, w=4, kind="arch", leaf=None):
    x = cx - w // 2
    top = bottom - h + 1
    leaf = leaf or C.earth[1]
    for yy in range(top, bottom + 1):
        for xx in range(x, x + w):
            c = leaf if (xx - x) % 2 == 0 else C.earth[0]
            if xx == x + w - 1:
                c = C.outline
            px(L, xx, yy, c)
    hline(L, x, x + w - 1, top, C.outline)
    if kind == "arch":
        px(L, x, top, C.travertine[1])
        px(L, x + w - 1, top, C.travertine[1])
        hline(L, x - 1, x + w, top - 1, C.travertine[1])
    elif kind == "dark":
        rect(L, x, top + 1, w, h - 1, C.outline)


def window(L, x, y, w=2, h=2, sill=True, shutters=False):
    rect(L, x, y, w, h, C.outline)
    if h > 1:
        hline(L, x, x + w - 1, y + h - 1, C.earth[0])
    if sill:
        hline(L, x, x + w - 1, y + h, C.marble[1])
    if shutters:
        vline(L, x - 1, y, y + h - 1, C.sea[1])
        vline(L, x + w, y, y + h - 1, C.sea[1])


def windows(L, x0, x1, y, step=6, w=2, h=2, skip=None, **kw):
    for x in range(x0, x1 - w + 2, step):
        if skip and skip[0] <= x + w and x <= skip[1]:
            continue
        window(L, x, y, w, h, **kw)


def colonnade(L, x0, x1, top, bottom, mat="marble", step=4, back=None, entab=2, base=1):
    """Columns 2px wide every `step` px between sprite rows top..bottom with an
    entablature on top and a stylobate at the bottom; gaps show shaded
    interior (back color)."""
    m = MAT[mat]
    back = back or C.travertine[0]
    for y in range(top, bottom + 1):
        for x in range(x0, x1 + 1):
            px(L, x, y, back)
    for x in range(x0 + 1, x1, step):
        for y in range(top + entab, bottom - base + 1):
            px(L, x, y, m["lit"])
            if x + 1 <= x1:
                px(L, x + 1, y, m["shade"])
        px(L, x - 1 if x > x0 else x, top + entab, m["base"])     # capital
        px(L, x + 2 if x + 2 <= x1 else x + 1, top + entab, m["shade"])
    for i in range(entab):
        hline(L, x0, x1, top + i, m["lit"] if i == 0 else m["base"])
    for i in range(base):
        hline(L, x0, x1, bottom - i, m["shade"] if i == 0 else m["base"])
    # shadow under entablature in the gaps
    for x in range(x0, x1 + 1):
        if get(L, x, top + entab) == back:
            px(L, x, top + entab, C.outline if mat != "wood" else C.earth[0])


def pediment(L, x0, x1, base_row, ph=None, mat="marble", oculus=False):
    """Triangular gable end; base on sprite row base_row."""
    m = MAT[mat]
    w = x1 - x0 + 1
    ph = ph or max(3, w // 5)
    cx = (x0 + x1 + 1) / 2.0
    for k in range(ph + 1):
        yrow = base_row - k
        half = (w / 2.0) * (1 - k / float(ph + 1))
        xa, xb = int(round(cx - half)), int(round(cx + half)) - 1
        if xb < xa:
            continue
        for x in range(xa, xb + 1):
            px(L, x, yrow, m["shade"] if x >= cx else m["base"])
        px(L, xa, yrow, m["lit"])
        px(L, xb, yrow, m["eave"])
    hline(L, x0, x1, base_row, m["lit"])
    if oculus and ph >= 5:
        px(L, int(cx) - 1, base_row - ph // 2, C.outline)
        px(L, int(cx), base_row - ph // 2, C.outline)
    return ph


def steps(L, x0, x1, top, bottom, mat="marble"):
    m = MAT[mat]
    for y in range(top, bottom + 1):
        c = m["lit"] if (y - top) % 2 == 0 else m["shade"]
        hline(L, x0, x1, y, c)
        px(L, x0, y, m["base"])
        px(L, x1, y, m["eave"])


def arcade(L, x0, x1, top, bottom, mat="travertine", period=8, opening=None, tiers=1, pier=2):
    """Wall face with arched openings (opening=None -> transparent)."""
    m = MAT[mat]
    th = (bottom - top + 1) / float(tiers)
    for t in range(tiers):
        ty0 = int(round(top + t * th))
        ty1 = int(round(top + (t + 1) * th)) - 1
        for y in range(ty0, ty1 + 1):
            for x in range(x0, x1 + 1):
                p = (x - x0) % period
                ow = period - pier
                inner = p >= pier
                arch_top = ty0 + 2
                if inner:
                    dx = (p - pier + 0.5) - ow / 2.0
                    rad = ow / 2.0
                    ay = arch_top + rad - math.sqrt(max(0.0, rad * rad - dx * dx))
                    inside = y >= ay and y < ty1
                else:
                    inside = False
                if inside:
                    if opening is None:
                        px(L, x, y, (0, 0, 0, 0))
                    else:
                        px(L, x, y, opening)
                else:
                    c = m["base"]
                    if y == ty0:
                        c = m["lit"]
                    elif y == ty1:
                        c = m["eave"]
                    elif p == 0:
                        c = m["lit"]
                    elif p == pier - 1:
                        c = m["shade"]
                    px(L, x, y, c)


# ------------------------------------------------------------ round things
def cylinder(b, L, cx, cgy, rx, ry, z0, z1, mat="travertine", arches=0, top=None):
    """Vertical cylinder (drum): ellipse footprint (rx, ry); face from z0 to
    z1 visible on the south half; returns the top ellipse mask."""
    m = MAT[mat]
    cy_top = b.sy(cgy, z1)
    cy_bot = b.sy(cgy, z0)
    for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
        dx = (x + 0.5 - cx) / rx
        if abs(dx) > 1:
            continue
        dy = ry * math.sqrt(1 - dx * dx)
        y_a = int(round(cy_top + dy))
        y_b = int(round(cy_bot + dy))
        shade = m["lit"] if dx < -0.6 else (m["base"] if dx < 0.35 else m["shade"])
        for y in range(y_a, y_b + 1):
            c = shade
            if m["joint"] is not None and (y - y_a) % 3 == 2:
                c = m["joint"] if dx > -0.6 else m["base"]
            if arches and y_b - y_a > 5:
                ang = math.asin(max(-1, min(1, dx)))
                if int((ang + 2) * arches) % 2 == 1 and y_a + 2 < y < y_b - 1:
                    c = C.outline if dx > -0.2 else C.travertine[0]
            px(L, x, y, c)
    tm = ellipse_mask(cx, cy_top, rx, ry)
    if top is not None:
        for (x, y) in tm:
            px(L, x, y, top)
    return tm


def dome(b, L, cx, cgy, rx, ry, rz, z0, ramp=None, oculus=False, ribs=0, noise=0.0):
    """Hemispherical dome on top of height z0: base ellipse (rx, ry),
    rising rz px; lit from the top-left."""
    ramp = ramp or (C.gray[1], C.gray[2], C.gray[3], C.marble[0])
    base_cy = b.sy(cgy, z0)
    pts = {}
    for y in range(int(base_cy - rz - ry) - 1, int(base_cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            nx = (x + 0.5 - cx) / rx
            # solve for height: point on the dome surface projecting here
            for hgt in range(rz, -1, -1):
                k = hgt / float(rz)
                rr = math.sqrt(max(0.0, 1 - k * k))
                ny = (y + 0.5 - (base_cy - hgt)) / ry
                if nx * nx + ny * ny <= rr * rr and rr > 0:
                    pts[(x, y)] = (nx, ny, k)
                    break
    lx, ly, lz = -0.48, -0.42, 0.77
    n_r = len(ramp)
    for (x, y), (ux, uy, k) in pts.items():
        d = ux * lx + uy * ly + k * lz
        if noise:
            d += ((hash2(x // 2, y, 77) % 100) / 100.0 - 0.5) * noise
        i = max(0, min(n_r - 1, int((d * 0.5 + 0.5) * n_r * 1.05)))
        c = ramp[i]
        if ribs and k < 0.92:
            ang = math.atan2(uy, ux)
            if int((ang + 4) * ribs / 3.1416) % 2 == 0 and hash2(x, y, 1) % 3 == 0:
                c = ramp[max(0, i - 1)]
        px(L, x, y, c)
    if oculus:
        ty = int(base_cy - rz)
        for dx in (-1, 0):
            px(L, int(cx) + dx, ty + 1, C.outline)
            px(L, int(cx) + dx, ty + 2, C.outline)
    return pts


# ------------------------------------------------------------ props
def amphora(L, x, y, c=None):
    """3x5, (x, y) = bottom-left."""
    c = c or C.roof_terracotta[2]
    d = C.roof_terracotta[1] if c == C.roof_terracotta[2] else C.earth[1]
    px(L, x + 1, y - 4, C.roof_terracotta[3] if c == C.roof_terracotta[2] else c)
    hline(L, x, x + 2, y - 3, c)
    px(L, x + 2, y - 3, d)
    hline(L, x, x + 2, y - 2, c)
    px(L, x + 2, y - 2, d)
    px(L, x + 1, y - 1, d)
    px(L, x + 1, y, d)


def amphorae(L, x, y, n=3, c=None):
    for i in range(n):
        amphora(L, x + i * 3, y, c)


def crate(L, x, y, w=4, h=4):
    """(x, y) bottom-left; lit top, planked front."""
    rect(L, x, y - h + 1, w, h, C.earth[2])
    hline(L, x, x + w - 1, y - h + 1, C.ochre[1])
    hline(L, x, x + w - 1, y - h + 2, C.earth[3])
    vline(L, x + w - 1, y - h + 2, y, C.earth[1])
    if h >= 4:
        hline(L, x, x + w - 2, y - 1, C.earth[1])


def sack(L, x, y, c=None):
    c = c or C.travertine[2]
    hline(L, x + 1, x + 2, y - 3, c)
    hline(L, x, x + 3, y - 2, c)
    hline(L, x, x + 3, y - 1, c)
    px(L, x + 3, y - 2, C.travertine[1])
    px(L, x + 3, y - 1, C.travertine[1])
    hline(L, x, x + 3, y, C.travertine[0])
    px(L, x + 1, y - 4, C.earth[2])


def barrel(L, x, y):
    for yy in range(y - 4, y + 1):
        hline(L, x, x + 2, yy, C.earth[2])
        px(L, x + 2, yy, C.earth[1])
    hline(L, x, x + 2, y - 4, C.ochre[1])
    hline(L, x, x + 2, y - 2, C.gray[1])


def logs(L, x, y, w=8, rows=2):
    """Stack of logs lying east-west; ends visible on the left."""
    for r_ in range(rows):
        yy = y - r_ * 2
        off = r_
        hline(L, x + off, x + off + w - 1, yy - 1, C.earth[3])
        hline(L, x + off, x + off + w - 1, yy, C.earth[1])
        px(L, x + off, yy - 1, C.ochre[2])
        px(L, x + off, yy, C.ochre[1])


def planks(L, x, y, w=8, rows=3):
    for r_ in range(rows):
        yy = y - r_
        hline(L, x, x + w - 1, yy, C.ochre[2] if r_ % 2 == 0 else C.ochre[1])
    px(L, x + w - 1, y, C.earth[2])


def blocks(L, x, y, n=3, mat="stone", size=3):
    m = MAT[mat]
    for i in range(n):
        bx = x + i * (size + 1)
        rect(L, bx, y - size + 1, size, size, m["base"])
        hline(L, bx, bx + size - 1, y - size + 1, m["lit"])
        vline(L, bx + size - 1, y - size + 2, y, m["shade"])


def pile(L, cx, by, rx, h, ramp):
    """Conical heap: base at row by, centre cx, radius rx, height h."""
    for y in range(by - h, by + 1):
        t = (by - y) / float(max(1, h))
        half = rx * (1 - t) + 0.5
        for x in range(int(cx - half), int(cx + half) + 1):
            side = (x + 0.5 - cx) / max(0.5, half)
            i = 1 if side > 0.3 else (len(ramp) - 1 if side < -0.3 and t > 0.2 else 2 if len(ramp) > 2 else 1)
            if hash2(x, y, 31) % 7 == 0:
                i = max(0, i - 1)
            px(L, x, y, ramp[min(i, len(ramp) - 1)])
        if y == by:
            hline(L, int(cx - half), int(cx + half), y, ramp[0])


def vat(L, cx, cy, r, liquid, rim=None):
    rim = rim or C.travertine[2]
    outer = ellipse_mask(cx, cy, r, r * 0.8)
    inner = ellipse_mask(cx, cy + 0.3, r - 1, r * 0.8 - 1)
    for (x, y) in outer:
        px(L, x, y, rim)
    for (x, y) in inner:
        px(L, x, y, liquid)
    if inner:
        ys = min(y for (_, y) in inner)
        for (x, y) in inner:
            if y == ys:
                px(L, x, y, C.outline)


def awning(L, x, y, w, d, colors=None, posts=2):
    """Market stall: striped canvas top (sprite rows y..y+d-1) + posts."""
    colors = colors or (C.roman_red[2], C.marble[1])
    for yy in range(d):
        for xx in range(w):
            c = colors[(xx // 2) % 2]
            if yy == d - 1:
                c = C.roman_red[0] if c == colors[0] else C.gray[3]
            px(L, x + xx, y + yy, c)
    for yy in range(y + d, y + d + posts):
        px(L, x, yy, C.earth[1])
        px(L, x + w - 1, yy, C.earth[1])


def fence(L, x0, y0, x1, y1, post=3, c=None):
    c = c or C.earth[2]
    if y0 == y1:
        for x in range(x0, x1 + 1):
            px(L, x, y0, c if (x - x0) % post else C.earth[1])
            if (x - x0) % post == 0:
                px(L, x, y0 - 1, C.earth[3])
    else:
        for y in range(y0, y1 + 1):
            px(L, x0, y, c if (y - y0) % post else C.earth[1])


def chimney(L, x, top, h=4, smoke=False):
    for y in range(top, top + h):
        px(L, x, y, C.roof_terracotta[2])
        px(L, x + 1, y, C.roof_terracotta[1])
    hline(L, x, x + 1, top, C.outline)
    if smoke:
        for (dx, dy) in ((0, -2), (1, -3), (-1, -4), (0, -5)):
            px(L, x + dx, top + dy, C.marble[0] if dy % 2 else C.gray[3])


def glow(L, x, y, w=2, h=2):
    for yy in range(h):
        for xx in range(w):
            px(L, x + xx, y + yy, C.gold if (xx + yy) % 2 == 0 else C.roof_terracotta[3])
    if h > 1:
        hline(L, x, x + w - 1, y + h - 1, C.roman_red[2])


def millstone(L, x, y):
    """Hourglass donkey mill (meta + catillus), 4x6, (x, y) bottom-left."""
    shape = ["1221", ".22.", ".21.", "1221", "1221", "1111"]
    cols = {"1": C.gray[2], "2": C.gray[3]}
    for j, row in enumerate(shape):
        for i, ch in enumerate(row):
            if ch != ".":
                px(L, x + i, y - 5 + j, cols[ch])
    px(L, x, y - 5, C.earth[2])       # wooden frame arm
    px(L, x - 1, y - 4, C.earth[2])


def statue(L, cx, base, h=7, mat="marble", ped=True):
    """Statue on a pedestal; bottom at row base."""
    m = MAT[mat] if mat in MAT else MAT["marble"]
    if ped:
        rect(L, cx - 2, base - 2, 5, 3, C.travertine[2])
        hline(L, cx - 2, cx + 2, base - 2, C.marble[0])
        vline(L, cx + 2, base - 1, base, C.travertine[0])
        base -= 3
    if mat == "bronze":
        lit, mid, sh = C.ochre[2], C.ochre[0], C.earth[1]
    elif mat == "gold":
        lit, mid, sh = C.ochre[3], C.gold, C.ochre[0]
    else:
        lit, mid, sh = m["lit"], m["base"], m["shade"]
    px(L, cx, base - h + 1, lit)                       # head
    for y in range(base - h + 2, base + 1):
        px(L, cx - 1, y, lit)
        px(L, cx, y, mid)
        px(L, cx + 1, y, sh)
    px(L, cx + 2, base - h + 3, mid)                   # raised arm
    px(L, cx + 2, base - h + 2, mid)


def tree(b, x, base_y, kind="cypress", ol=False):
    """Paste an existing outlined object sprite with its bottom-centre at (x, base_y)."""
    import objects
    fn = {"cypress": lambda: objects.tree_cypress("bt/c", False),
          "cypress_tall": lambda: objects.tree_cypress("bt/ct", True),
          "pine": lambda: objects.tree_umbrella_pine("bt/p", False),
          "olive": lambda: objects.tree_olive("bt/o", 1),
          "oak": lambda: objects.tree_oak("bt/k"),
          "bush": lambda: objects.bush(0),
          "bush2": lambda: objects.bush(2)}[kind]
    im = fn()
    L = b.layer()
    L.alpha_composite(im, (x - im.width // 2, base_y - im.height + 1))
    b.put(L, ol=False)


def crane(L, x, base, h=12):
    """Wooden derrick (A-frame with boom and rope)."""
    line(L, x, base, x + 2, base - h, C.earth[2])
    line(L, x + 4, base, x + 2, base - h, C.earth[1])
    line(L, x + 2, base - h, x + 7, base - h + 3, C.earth[2])
    vline(L, x + 7, base - h + 4, base - 4, C.travertine[0])
    rect(L, x + 6, base - 3, 3, 2, C.gray[3])


def cart(L, x, y, load=None):
    """Small two-wheeled cart seen from the side, 8x5, (x, y) bottom-left."""
    rect(L, x, y - 3, 7, 2, C.earth[2])
    hline(L, x, x + 6, y - 3, C.earth[3])
    hline(L, x + 7, x + 9, y - 2, C.earth[1])          # shaft
    for (dx, dy) in ((2, -1), (1, 0), (2, 0), (3, 0), (2, 1)):
        pass
    rect(L, x + 2, y - 1, 3, 2, C.earth[0])
    px(L, x + 3, y - 1, C.earth[3])
    if load:
        hline(L, x + 1, x + 5, y - 4, load)
        hline(L, x + 2, x + 4, y - 5, load)


def banner(L, x, top, c=None, h=8):
    c = c or C.roman_red[2]
    vline(L, x, top, top + h, C.earth[1])
    rect(L, x + 1, top + 1, 3, 4, c)
    px(L, x + 3, top + 4, C.gold)
    hline(L, x - 1, x + 4, top, C.gold)


def smoke(L, x, y, n=4, light=True):
    for i in range(n):
        xx = x + (i % 2) - (i // 3)
        yy = y - i * 2
        px(L, xx, yy, C.marble[1] if light and i % 2 else C.gray[3])
        if i % 2 == 0:
            px(L, xx + 1, yy - 1, C.marble[0])


def water_wheel(L, cx, cy, r=5):
    for a in range(0, 360, 15):
        t = math.radians(a)
        px(L, int(round(cx + r * math.cos(t))), int(round(cy + r * math.sin(t))), C.earth[2])
    for a in range(0, 180, 45):
        t = math.radians(a)
        line(L, int(round(cx - r * math.cos(t))), int(round(cy - r * math.sin(t))),
             int(round(cx + r * math.cos(t))), int(round(cy + r * math.sin(t))), C.earth[1])
    px(L, cx, cy, C.gray[1])


def pit(b, L, x, gy, w, d, ramp, levels=3, water=False, cut=True):
    """Stepped excavation seen in 3/4 view.  Each terrace has a floor, a
    sunlit cut face on its north side (with vertical tool marks / block
    joints), a shaded west wall and a lit east lip.  ramp: dark -> light."""
    y0 = b.sy(gy)
    n = len(ramp)
    for lv in range(levels):
        ix, iy = lv * 4, lv * 4
        ww, dd = w - 2 * ix, d - iy - lv * 2
        if ww < 6 or dd < 6:
            break
        xa, ya = x + ix, y0 + iy
        floor = ramp[max(0, n - 2 - lv)] if lv else ramp[n - 2]
        rect(L, xa, ya, ww, dd, floor)
        # cut face (north wall) 3px tall, lit, with joints
        for yy in range(3):
            for xx in range(ww):
                c = ramp[n - 1] if yy == 0 else ramp[max(0, n - 2 - lv)]
                if yy > 0 and cut and (xx + lv * 2) % 4 == 3:
                    c = ramp[0]
                px(L, xa + xx, ya + yy, c)
        hline(L, xa, xa + ww - 1, ya + 3, ramp[0])          # shadow at the foot
        vline(L, xa, ya, ya + dd - 1, ramp[0])               # west wall in shadow
        vline(L, xa + ww - 1, ya, ya + dd - 1, ramp[n - 1])  # east lip lit
        hline(L, xa, xa + ww - 1, ya + dd - 1, ramp[n - 1])  # south lip lit
        # loose cut blocks on the terrace floor
        for k in range(ww // 9):
            bx = xa + 3 + k * 9 + (lv * 3) % 5
            by = ya + dd - 3
            if bx + 2 < xa + ww - 1:
                hline(L, bx, bx + 2, by - 1, ramp[n - 1])
                hline(L, bx, bx + 2, by, ramp[max(0, n - 3)])
    if water:
        ix = levels * 4
        ww, dd = w - 2 * ix, d - levels * 6
        if ww > 2 and dd > 2:
            rect(L, x + ix, y0 + levels * 4, ww, dd, C.sea[2])
            hline(L, x + ix, x + ix + ww - 1, y0 + levels * 4, C.sea[1])
            px(L, x + ix + 2, y0 + levels * 4 + 2, C.sea[3])


def ladder(L, x, top, bottom):
    vline(L, x, top, bottom, C.earth[2])
    vline(L, x + 2, top, bottom, C.earth[1])
    for y in range(top + 1, bottom, 2):
        px(L, x + 1, y, C.earth[3])


def mound(b, L, cx, cgy, rx, ry, h, ramp):
    """Earth/rock hill (for mines): dome-ish mound."""
    return dome(b, L, cx, cgy, rx, ry, h, 0, ramp=ramp, noise=0.5)
