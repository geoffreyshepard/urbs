"""Effects and construction overlays.

  fx/scaffold_<0..3>   16x16 timber scaffolding / stacked materials (per tile)
  fx/foundation        16x16 dug earth with a stone outline
  fx/fire_<0..3>       16x24 flames, anchor bottom-left (0, 23)
  fx/smoke_<0..3>      16x16 dithered smoke puff (translucency by dithering)
  fx/rubble_<0..3>     16x16 ruin tile
  fx/plague_mark       8x8 red cross badge
  fx/alert             8x8 exclamation badge
  fx/cart_<n|e|s|w>    16x12 mule cart, anchor bottom-center (8, 11)
  fx/ship_<n|e|s|w>    24x24 merchant ship, anchor bottom-center (12, 23)
"""
import math

from palette import C
from primitives import (rng, new, px, get, rect, hline, vline, line, outline,
                        ellipse_mask, hash2)

T = 16


# ------------------------------------------------------------ construction
def _pole(img, x, y0, y1):
    """Vertical timber pole, lit left pixel + shaded right pixel."""
    vline(img, x, y0, y1, C.earth[3])
    vline(img, x + 1, y0, y1, C.earth[1])


def _plank(img, x0, x1, y):
    hline(img, x0, x1, y, C.ochre[1])
    hline(img, x0, x1, y + 1, C.earth[1])


def _block(img, x, y, w, h, lit, mid, dark):
    rect(img, x, y, w, h, mid)
    hline(img, x, x + w - 1, y, lit)
    vline(img, x + w - 1, y, y + h - 1, dark)
    hline(img, x, x + w - 1, y + h - 1, dark)


def _outlined(layer):
    outline(layer, C.outline)
    return layer


def scaffold(v):
    img = new(T, T)
    r = rng("fx/scaffold/%d" % v)
    if v in (0, 1):
        # scaffold frame: two poles, two plank levels, a diagonal brace
        _pole(img, 1, 1, 14)
        _pole(img, 12, 1, 14)
        _plank(img, 0, 15, 4)
        _plank(img, 0, 15, 10)
        line(img, 3, 9, 11, 5, C.earth[2])
        if v == 1:
            # bucket + rope hanging from the top plank
            vline(img, 7, 6, 7, C.travertine[2])
            rect(img, 6, 8, 3, 2, C.earth[2])
            px(img, 6, 8, C.earth[3])
    if v == 2:
        # stacked cut stone blocks + one pole
        _pole(img, 13, 0, 14)
        layer = new(T, T)
        for (x, y, w, h) in ((1, 10, 5, 4), (6, 10, 5, 4), (3, 6, 5, 4)):
            _block(layer, x, y, w, h, C.marble[0], C.travertine[2], C.travertine[0])
            vline(layer, x + w - 1, y, y + h - 1, C.travertine[0])
        _outlined(layer)
        img.paste(layer, (0, 0), layer)
        _plank(img, 9, 15, 4)
    if v == 3:
        # brick pile and stacked lumber
        layer = new(T, T)
        for row in range(3):
            y = 11 - row * 2
            for k in range(3 - row):
                x = 1 + row + k * 3
                hline(layer, x, x + 2, y, C.roof_terracotta[2])
                hline(layer, x, x + 2, y + 1, C.roof_terracotta[1])
                px(layer, x + 2, y, C.roof_terracotta[1])
        for j in range(3):
            y = 12 - j * 2
            hline(layer, 10, 14, y, C.earth[3])
            hline(layer, 10, 14, y + 1, C.earth[2])
            px(layer, 10, y, C.ochre[2])
            px(layer, 10, y + 1, C.ochre[1])
        _outlined(layer)
        img.paste(layer, (0, 0), layer)
        _pole(img, 6, 0, 6)
        _plank(img, 0, 9, 1)
    return img


def foundation():
    img = new(T, T)
    r = rng("fx/foundation")
    # dug earth floor, darker toward the north (shadow of the trench wall)
    for y in range(T):
        for x in range(T):
            c = C.earth[2]
            if y <= 3:
                c = C.earth[1]
            q = r.random()
            if q < 0.12:
                c = C.earth[3]
            elif q < 0.2:
                c = C.earth[1]
            px(img, x, y, c)
    # stone outline: a ring of footing blocks, lit top-left
    for i in range(T):
        for (x, y) in ((i, 0), (i, T - 1), (0, i), (T - 1, i)):
            c = C.travertine[2] if (x + y) % 4 else C.gray[2]
            if y == T - 1 or x == T - 1:
                c = C.travertine[1] if (x + y) % 4 else C.gray[1]
            px(img, x, y, c)
    hline(img, 1, T - 2, 1, C.earth[0])            # trench shadow under north stones
    # surveyor's stakes and a few loose stones
    for (x, y) in ((4, 5), (11, 9)):
        px(img, x, y, C.earth[3])
        px(img, x, y + 1, C.earth[0])
    for (x, y) in ((7, 11), (9, 4)):
        px(img, x, y, C.travertine[2])
        px(img, x + 1, y, C.travertine[0])
    return img


# ------------------------------------------------------------ fire & smoke
FIRE_RAMP = (C.roman_red[0], C.roman_red[1], C.roman_red[2], C.roof_terracotta[3],
             C.gold, C.ochre[3], C.white)


def fire(f):
    """Flames rising from the bottom of a 16x24 tile.  The column heights are
    a sum of sines whose phase moves each frame, so the cycle loops."""
    W, H = 16, 24
    img = new(W, H)
    ph = f * math.pi / 2
    n = len(FIRE_RAMP)
    heights = []
    for x in range(W):
        env = math.sin(math.pi * (x + 0.5) / W) ** 0.6
        h = 6 + 11 * env + 2.2 * math.sin(x * 1.3 + ph) + 1.6 * math.sin(x * 0.55 - ph * 2 + 1.0)
        heights.append(max(2, h))
    for x in range(W):
        hx = heights[x]
        for y in range(H):
            d = H - 1 - y                         # height above the base
            if d > hx:
                continue
            t = d / hx                            # 0 base .. 1 tip
            core = 1 - abs(x + 0.5 - W / 2) / (W / 2)
            b = (1 - t) * 0.55 + core * 0.55 - 0.1
            if hash2(x, y + f * 7, 3) % 5 == 0:
                b -= 0.15
            i = max(0, min(n - 1, int(b * n)))
            if d >= hx - 1:
                i = min(i, 1)                     # dark red tongue tips
            px(img, x, y, FIRE_RAMP[i])
    # embers drifting up
    r = rng("fx/fire/embers")
    for k in range(5):
        x = r.randrange(2, W - 2)
        y0 = r.randrange(0, 8)
        y = (y0 - f * 2) % 9
        px(img, x + (f + k) % 2, y, C.gold if k % 2 else C.roof_terracotta[3])
    # charred base line
    for x in range(W):
        if get(img, x, H - 1)[3]:
            px(img, x, H - 1, C.roman_red[0] if x % 3 else C.outline)
    return img


def smoke(f):
    """A rising, growing puff.  Body solid gray, rim dithered so it reads
    as translucent; lit top-left.  Later frames thin out (more dither)."""
    img = new(T, T)
    puffs = [(5.5, 11.0, 3.6), (10.0, 10.0, 3.4), (7.8, 6.8, 3.2), (4.5, 7.5, 2.2)]
    grow = 1.0 + 0.12 * f
    rise = 1.4 * f
    drift = 0.4 * f
    for (cx, cy, rr) in puffs:
        cx2, cy2, r2 = cx + drift, cy - rise, rr * grow
        for (x, y) in sorted(ellipse_mask(cx2, cy2, r2, r2 * 0.9)):
            if not (0 <= x < T and 0 <= y < T):
                continue
            dx, dy = (x + 0.5 - cx2) / r2, (y + 0.5 - cy2) / (r2 * 0.9)
            dist = math.hypot(dx, dy)
            if dist > 0.8 - 0.06 * f and (x + y) % 2:
                continue
            if f >= 2 and (x + 2 * y) % (6 - f) == 0:
                continue
            lit = -(dx * -0.7 + dy * -0.7)
            c = C.gray[3] if lit > 0.2 else (C.gray[2] if lit > -0.4 else C.gray[1])
            if lit > 0.6:
                c = C.marble[0]
            px(img, x, y, c)
    return img


# ------------------------------------------------------------ rubble
def rubble(v):
    img = new(T, T)
    r = rng("fx/rubble/%d" % v)
    # ragged dusty ground patch
    for (x, y) in ellipse_mask(8, 8.5, 7.6, 7.0):
        if 0 <= x < T and 0 <= y < T:
            edge = math.hypot(x + 0.5 - 8, (y + 0.5 - 8.5) * 1.08)
            if edge > 6.4 and r.random() < 0.5:
                continue
            q = r.random()
            c = C.earth[3] if q < 0.55 else (C.travertine[0] if q < 0.8 else C.earth[2])
            px(img, x, y, c)
    layer = new(T, T)
    # broken stone blocks
    for _ in range(3 + v % 2):
        w, h = r.randrange(2, 5), r.randrange(2, 4)
        x, y = r.randrange(1, T - w - 1), r.randrange(2, T - h - 1)
        _block(layer, x, y, w, h, C.marble[0], C.travertine[1], C.travertine[0])
    # roof tile shards
    for _ in range(4):
        x, y = r.randrange(2, T - 3), r.randrange(2, T - 3)
        px(layer, x, y, C.roof_terracotta[2])
        px(layer, x + 1, y, C.roof_terracotta[1])
    # a charred beam
    if v % 2 == 0:
        x0, y0 = r.randrange(1, 5), r.randrange(4, 12)
        line(layer, x0, y0, x0 + 7, y0 - 3, C.earth[1])
        line(layer, x0 + 1, y0, x0 + 8, y0 - 3, C.earth[0])
    if v == 3:
        # a toppled column drum
        hline(layer, 8, 12, 11, C.marble[1])
        hline(layer, 8, 12, 12, C.marble[0])
        px(layer, 12, 11, C.travertine[1])
        px(layer, 12, 12, C.travertine[1])
    outline(layer, C.outline)
    img.paste(layer, (0, 0), layer)
    return img


# ------------------------------------------------------------ badges
def plague_mark():
    """Red disc with a pale cross."""
    img = new(8, 8)
    for (x, y) in ellipse_mask(4, 4, 3.1, 3.1):
        px(img, x, y, C.roman_red[1])
    px(img, 2, 2, C.roman_red[2])
    px(img, 3, 1, C.roman_red[2])
    px(img, 1, 3, C.roman_red[2])
    hline(img, 2, 5, 4, C.marble[1])
    vline(img, 4, 2, 5, C.marble[1])
    px(img, 4, 4, C.white)
    outline(img, C.outline)
    return img


def alert():
    """Gold disc with a dark exclamation mark."""
    img = new(8, 8)
    for (x, y) in ellipse_mask(4, 4, 3.1, 3.1):
        px(img, x, y, C.gold)
    px(img, 2, 2, C.ochre[3])
    px(img, 3, 1, C.ochre[3])
    px(img, 5, 6, C.ochre[0])
    px(img, 6, 5, C.ochre[0])
    vline(img, 4, 1, 4, C.outline)
    px(img, 4, 6, C.outline)
    outline(img, C.outline)
    return img


# ------------------------------------------------------------ cart
def _wheel_side(img, cx, cy):
    """Side-view spoked wheel, 5x5."""
    for (x, y) in ellipse_mask(cx + 0.5, cy + 0.5, 2.6, 2.6):
        px(img, x, y, C.earth[1])
    for (x, y) in ellipse_mask(cx + 0.5, cy + 0.5, 1.6, 1.6):
        px(img, x, y, C.earth[2])
    px(img, cx, cy, C.gray[2])
    px(img, cx - 1, cy - 1, C.earth[1])
    px(img, cx + 1, cy + 1, C.earth[1])


MULE = (C.gray[1], C.gray[2], C.gray[3])


def _mule_side(img, x, y, frame=0):
    """Mule facing east; (x, y) = top-left of its 7x9 box."""
    rows = ["....L.L",   # long ears
            "....HHH",
            "....HHM",   # head, muzzle
            "TTTTT..",   # neck joins at the right of the back
            "TTTTT..",
            "BBBBB..",
            "D...D..",
            "D...D..",
            "F...F.."]
    cols = {"T": MULE[1], "B": MULE[0], "D": MULE[0], "F": C.outline,
            "L": MULE[0], "H": MULE[1], "M": C.gray[0]}
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch != ".":
                px(img, x + i, y + j, cols[ch])
    vline(img, x + 4, y + 2, y + 3, MULE[1])      # neck
    hline(img, x, x + 4, y + 3, MULE[2])          # lit back
    px(img, x + 4, y + 1, MULE[2])
    px(img, x + 5, y + 1, C.outline)              # eye
    px(img, x, y + 4, C.gray[0])                  # tail
    px(img, x - 1, y + 5, C.gray[0])


def cart(d):
    W, H = 16, 12
    img = new(W, H)
    if d in ("e", "w"):
        # cart bed with a load of sacks; big spoked wheel; mule ahead (east)
        rect(img, 0, 4, 9, 3, C.earth[2])
        hline(img, 0, 8, 4, C.earth[3])
        hline(img, 0, 8, 6, C.earth[1])
        for x in (1, 4):
            rect(img, x, 2, 3, 2, C.ochre[1])
            px(img, x, 2, C.ochre[2])
            px(img, x + 2, 3, C.ochre[0])
        hline(img, 8, 10, 6, C.earth[1])          # shafts
        _wheel_side(img, 4, 8)
        _mule_side(img, 9, 2)
    else:
        # end-on: the cart behind, the mule toward the facing direction
        cy0, my = (0, 5) if d == "s" else (5, 0)
        rect(img, 3, cy0 + 1, 10, 5, C.earth[2])
        hline(img, 3, 12, cy0 + 1, C.earth[3])
        vline(img, 12, cy0 + 1, cy0 + 5, C.earth[1])
        hline(img, 3, 12, cy0 + 5, C.earth[1])
        rect(img, 4, cy0 + 2, 3, 2, C.ochre[1])
        rect(img, 8, cy0 + 2, 3, 2, C.ochre[2])
        px(img, 4, cy0 + 2, C.ochre[2])
        px(img, 10, cy0 + 3, C.ochre[0])
        for wx in (1, 13):                        # wheels edge-on
            vline(img, wx, cy0 + 1, cy0 + 6, C.earth[1])
            vline(img, wx + 1, cy0 + 1, cy0 + 6, C.earth[0])
            px(img, wx, cy0 + 1, C.earth[3])
        # mule: shaded body block, lighter head toward the facing side
        rect(img, 6, my + 1, 4, 5, MULE[0])
        if d == "s":
            rect(img, 7, my + 2, 2, 5, MULE[1])   # long head toward the viewer
            vline(img, 7, my + 2, my + 5, MULE[2])
            hline(img, 7, 8, my + 6, C.gray[0])   # muzzle
            px(img, 6, my + 1, C.gray[0])
            px(img, 9, my + 1, C.gray[0])         # ears
            px(img, 7, my + 3, C.outline)
            px(img, 8, my + 3, C.outline)         # eyes
        else:
            rect(img, 7, my, 2, 2, MULE[1])       # head beyond the shoulders
            px(img, 6, my, C.gray[0])
            px(img, 9, my, C.gray[0])
            hline(img, 6, 9, my + 2, C.roman_red[1])   # harness collar
            vline(img, 6, my + 3, my + 5, MULE[1])
            px(img, 7, my + 5, MULE[2])
            px(img, 8, my + 6, C.gray[0])         # tail
    if d == "w":
        img = img.transpose(0)                     # FLIP_LEFT_RIGHT
    outline(img, C.outline)
    return img


# ------------------------------------------------------------ ship
def _ship_side(img):
    """Merchant ship (corbita) facing east in 3/4 side view: round-bellied
    hull with both ends sweeping up, deck strip visible, square sail."""
    x0, x1 = 1, 22
    for x in range(x0, x1 + 1):
        t = (x - x0) / float(x1 - x0)
        sheer = 2.6 * (2 * t - 1) ** 4 + (1.2 if t < 0.5 else 0.0) * (1 - 2 * t) ** 2
        top = int(round(16 - sheer * 1.6))
        belly = math.sin(math.pi * t) ** 0.5
        bottom = int(round(16 + 5.5 * belly))
        if bottom < top + 1:
            bottom = top + 1
        for y in range(top, bottom + 1):
            if y == top:
                c = C.earth[3]                     # gunwale, lit
            elif y == top + 1:
                c = C.earth[1]
            elif y >= bottom - 1:
                c = C.earth[0]                     # wet, shaded belly
            elif (y - top) % 3 == 0:
                c = C.earth[1]                     # strakes
            else:
                c = C.earth[2]
            px(img, x, y, c)
    # goose-neck stern ornament curling over the west end
    px(img, 1, 11, C.earth[3])
    px(img, 2, 10, C.earth[3])
    px(img, 3, 10, C.earth[2])
    # deck strip seen from above
    hline(img, 4, 19, 15, C.ochre[1])
    hline(img, 5, 18, 14, C.ochre[0])
    # steering oar at the stern quarter
    line(img, 4, 16, 3, 21, C.earth[3])
    # mast, yard and square sail with a red band
    vline(img, 12, 1, 14, C.earth[1])
    hline(img, 6, 18, 1, C.earth[2])
    for y in range(2, 12):
        bulge = 1 if 4 <= y <= 9 else 0
        for x in range(7, 18 + bulge):
            c = C.marble[1] if x < 12 else C.marble[0]
            if y == 2:
                c = C.white if x < 12 else C.marble[1]
            if x >= 17 + bulge - 0 or y == 11:
                c = C.travertine[1]
            if y in (6, 7):
                c = C.roman_red[2] if x < 12 else C.roman_red[1]
            px(img, x, y, c)
    # cargo amphorae on deck
    for x in (6, 8, 16, 18):
        px(img, x, 13, C.roof_terracotta[2] if x % 4 else C.roof_terracotta[1])


def _ship_top(img, south):
    """Ship heading north, seen from above; the sail yard lies across."""
    for y in range(1, 23):
        t = (y - 1) / 21.0
        # pointed bow at the top, fuller rounded stern at the bottom
        half = 5.2 * (math.sin(math.pi * min(1.0, 0.05 + t * 0.9)) ** (0.9 if t < 0.5 else 0.35))
        for x in range(24):
            dx = x + 0.5 - 12
            if abs(dx) <= half:
                if abs(dx) > half - 1:
                    c = C.earth[3] if dx < 0 else C.earth[1]   # gunwales, lit left
                elif y % 3 == 0:
                    c = C.ochre[0]                              # deck seams
                else:
                    c = C.ochre[1]
                px(img, x, y, c)
    # sail: yard across, canvas bellying toward the bow
    yard = 9
    hline(img, 4, 19, yard + 2, C.earth[1])
    for x in range(5, 19):
        bel = 2 - int(round(2 * abs(x + 0.5 - 12) / 7.0))
        for y in range(yard + 1 - bel, yard + 2):
            c = C.marble[1] if x < 12 else C.marble[0]
            if y == yard + 1 - bel:
                c = C.white if x < 12 else C.marble[1]
            px(img, x, y, c)
        px(img, x, yard + 1, C.roman_red[2] if x < 12 else C.roman_red[1])
    px(img, 12, yard + 2, C.earth[0])                           # mast
    for (x, y) in ((10, 16), (13, 16), (11, 18), (14, 18), (12, 5)):
        px(img, x, y, C.roof_terracotta[2])                     # amphorae
    for (x, y) in ((8, 20), (9, 21)):
        px(img, x, y, C.earth[3])                               # steering oar
    if south:
        return img.transpose(1)                                 # FLIP_TOP_BOTTOM
    return img


def ship(d):
    img = new(24, 24)
    if d in ("e", "w"):
        _ship_side(img)
        if d == "w":
            img = img.transpose(0)
    else:
        img = _ship_top(img, d == "s")
    # keep the outline inside the canvas
    outline(img, C.outline)
    return img


# ------------------------------------------------------------ export
def generate():
    out = []
    for v in range(4):
        out.append(("fx/scaffold_%d" % v, scaffold(v), None))
    out.append(("fx/foundation", foundation(), None))
    for f in range(4):
        out.append(("fx/fire_%d" % f, fire(f), (0, 23)))
        out.append(("fx/smoke_%d" % f, smoke(f), None))
    for v in range(4):
        out.append(("fx/rubble_%d" % v, rubble(v), None))
    out.append(("fx/plague_mark", plague_mark(), None))
    out.append(("fx/alert", alert(), None))
    for d in ("n", "e", "s", "w"):
        out.append(("fx/cart_%s" % d, cart(d), (8, 11)))
        out.append(("fx/ship_%s" % d, ship(d), (12, 23)))
    return out
