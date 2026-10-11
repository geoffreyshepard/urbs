"""Option A: the current top-down 3/4 pixel style, redesigned for readability.

Draws the six buildings of tools/arttest/brief.md with the game's own art kit
(tools/artgen: palette, primitives, buildings/kit.py) so the only variable is
the design: category accent colours, function props on the lot, category
ground, and a size/silhouette hierarchy that keeps houses quiet.

    python3 tools/arttest/pixel/gen.py     -> tools/arttest/pixel/sprites/<id>.png

Sprite convention matches the game: width = w*16, bottom row = south edge of
the footprint, extra height above the footprint for walls and roofs.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ARTGEN = os.path.join(HERE, "..", "..", "artgen")
sys.path.insert(0, ARTGEN)

from palette import C, check_image  # noqa: E402
from primitives import px, rect, hline, vline, outline, hash2, ellipse_mask  # noqa: E402
from buildings.kit import (Bld, ground, box, face, roof_top, door, window, windows,  # noqa: E402
                           colonnade, pediment, steps, dome, glow, millstone, sack,
                           amphorae, barrel, cart, tree, chimney, fence, MAT, ROOF)
import buildings.kit as kit  # noqa: E402

OUT = os.path.join(HERE, "sprites")

# Category accents (brief): no other category uses these on roofs/awnings.
FADED = (C.earth[1], C.skin[0], C.skin[1], C.skin[2])          # housing roofs
SLATE = (C.outline, C.gray[0], C.gray[1], C.gray[2])                     # industry
TIMBER = (C.earth[0], C.earth[1], C.earth[2], C.earth[3])                # storage
MARBLE_ROOF = (C.gray[3], C.marble[0], C.marble[1], C.white)             # religion
SAFFRON = (C.gold, C.ochre[3])                                           # food awning stripes
HOUSE_OL = C.earth[1]   # softer outline: housing is the background

kit.MAT["plaster"] = dict(base=C.travertine[2], lit=C.marble[0], shade=C.travertine[1],
                          eave=C.travertine[0], joint=None)
kit.MAT["tan"] = dict(base=C.ochre[2], lit=C.ochre[3], shade=C.ochre[1], eave=C.earth[1],
                      joint=C.ochre[1])


def put(b, L, ol=True, c=None):
    if ol:
        outline(L, c or C.outline)
    b.img.alpha_composite(L)


def lot(b, kind, inset=0, border=None):
    L = b.layer()
    ground(b, L, inset, inset, b.W - 2 * inset, b.D - 2 * inset, kind, border=border)
    put(b, L, ol=False)


def soot_ground(b):
    L = b.layer()
    for gy in range(b.D):
        for x in range(b.W):
            h = hash2(x, gy, 41) % 100
            c = C.earth[1] if h < 50 else (C.gray[1] if h < 72 else (C.earth[0] if h < 88 else C.earth[2]))
            px(L, x, b.sy(gy), c)
    put(b, L, ol=False)


def smoke_plume(L, x, y, dark=False, n=6):
    """Puffs rising from (x, y); light for ovens, dark for forges."""
    cols = (C.gray[1], C.gray[2]) if dark else (C.marble[0], C.gray[3])
    for i in range(n):
        cx = x + (i % 3) - 1 + i // 3
        cy = y - i * 2
        r = 1 if i < 2 else 2
        for (mx, my) in ellipse_mask(cx, cy, r, r * 0.8):
            px(L, mx, my, cols[0] if (mx + my + i) % 3 else cols[1])


def awning_shed(L, x, y, w, d, stripes, shade_row=True):
    """Striped canvas awning seen from above, sloping south; posts at the eave."""
    for yy in range(d):
        for xx in range(w):
            c = stripes[(xx // 3) % 2]
            if yy == d - 1:
                c = C.ochre[0] if c == stripes[0] else C.ochre[1]
            px(L, x + xx, y + yy, c)
    hline(L, x, x + w - 1, y, C.ochre[3])


# ------------------------------------------------------------ housing
def house_small():
    b = Bld("house_small", 2, 2, extra=14)
    # garden lot: lawn + fenced vegetable patch (east side)
    L = b.layer()
    ground(b, L, 0, 0, 32, 32, "lawn")
    put(b, L, ol=False)
    L = b.layer()
    for gy in range(17, 29):
        for x in range(21, 30):
            row = (gy - 17) % 3
            c = C.earth[1] if row == 2 else (C.grass[2] if (x + gy) % 2 else C.olive[1])
            px(L, x, b.sy(gy), c)
    fence(L, 20, b.sy(16), 30, b.sy(16), post=5)
    fence(L, 20, b.sy(29), 30, b.sy(29), post=5)
    put(b, L, ol=False)
    # low one-storey house, faded roof
    L = b.layer()
    f0, f1 = box(b, L, 3, 4, 16, 16, 6, mat="plaster", roof="hip", ramp=FADED, key="hs")
    door(L, 9, f1, h=4, w=3, kind="plain")
    window(L, 14, f0 + 1, 2, 1, sill=False)
    put(b, L, c=HOUSE_OL)
    L = b.layer()
    # washing line / bench by the door
    hline(L, 4, 7, b.sy(23), C.earth[2])
    px(L, 13, b.sy(23), C.roof_terracotta[2])
    px(L, 13, b.sy(23) - 1, C.olive[2])
    put(b, L, ol=False)
    return b


def house_large():
    b = Bld("house_large", 2, 2, extra=18)
    L = b.layer()
    ground(b, L, 0, 0, 32, 32, "lawn")
    ground(b, L, 13, 26, 6, 6, "gravel")
    put(b, L, ol=False)
    L = b.layer()
    f0, f1 = box(b, L, 2, 3, 28, 21, 12, mat="plaster", roof="hip", ramp=FADED, key="hl")
    # compluvium: opening in the roof onto the atrium with a blue impluvium
    top = b.sy(3, 12)
    ax, ay, aw, ad = 11, top + 6, 10, 8
    rect(L, ax, ay, aw, ad, C.earth[0])
    rect(L, ax + 1, ay + 2, aw - 2, ad - 3, C.travertine[1])
    rect(L, ax + 3, ay + 3, aw - 6, ad - 5, C.sea[2])
    hline(L, ax + 3, ax + aw - 4, ay + 3, C.sea[1])
    hline(L, ax, ax + aw - 1, ay + ad - 1, C.skin[2])
    # red base band (painted socle) and two storeys of windows
    for y in range(f1 - 3, f1 + 1):
        hline(L, 2, 29, y, C.roman_red[1] if y < f1 else C.roman_red[0])
    hline(L, 2, 29, f0 + 6, C.travertine[1])
    windows(L, 5, 27, f0 + 2, step=5, w=2, h=2, shutters=False)
    door(L, 16, f1, h=5, w=4, kind="plain")
    put(b, L, c=HOUSE_OL)
    L = b.layer()
    for x in (6, 10, 23, 27):                  # potted plants along the front
        px(L, x, b.sy(27), C.roof_terracotta[2])
        px(L, x, b.sy(27) - 1, C.grass[3])
        px(L, x - 1, b.sy(27) - 1, C.grass[1])
        px(L, x, b.sy(27) - 2, C.grass[2])
    put(b, L, ol=False)
    return b


# ------------------------------------------------------------ food
def bakery():
    b = Bld("bakery", 2, 2, extra=16)
    lot(b, "dirt", inset=1, border=(C.earth[3], C.earth[1]))
    # small bakehouse at the back-left, under a big saffron awning
    L = b.layer()
    f0, f1 = box(b, L, 2, 2, 19, 9, 9, mat="plaster", roof="shed", ramp=(C.ochre[0], C.ochre[1], C.gold, C.ochre[3]), key="bk")
    door(L, 11, f1, h=5, w=3, kind="dark")
    put(b, L)
    L = b.layer()
    awning_shed(L, 1, f1 - 3, 21, 8, SAFFRON)
    for x in (1, 21):
        vline(L, x, f1 + 5, f1 + 7, C.earth[1])
    put(b, L)
    L = b.layer()
    # counter of loaves under the awning
    rect(L, 4, f1 + 6, 14, 2, C.travertine[2])
    hline(L, 4, 17, f1 + 6, C.marble[1])
    for x in range(5, 17, 3):
        px(L, x, f1 + 5, C.ochre[1])
        px(L, x + 1, f1 + 5, C.earth[3])
    put(b, L)
    # big domed brick oven in the yard (east), glowing mouth, white smoke
    L = b.layer()
    dome(b, L, 26, 16, 5, 4, 7, 0, ramp=(C.roof_terracotta[0], C.roof_terracotta[1],
                                         C.roof_terracotta[2], C.roof_terracotta[3]))
    rect(L, 25, b.sy(19, 3), 3, 3, C.outline)
    glow(L, 25, b.sy(19, 2), 3, 2)
    put(b, L)
    L = b.layer()
    smoke_plume(L, 27, b.sy(16, 9), n=7)
    put(b, L, ol=False)
    # donkey mill + flour sacks at the front
    L = b.layer()
    millstone(L, 6, b.sy(28))
    rect(L, 10, b.sy(28) - 3, 4, 2, C.gray[2])          # donkey
    px(L, 14, b.sy(28) - 4, C.gray[2])
    px(L, 10, b.sy(28) - 1, C.gray[1])
    px(L, 13, b.sy(28) - 1, C.gray[1])
    sack(L, 18, b.sy(28), C.marble[1])
    sack(L, 22, b.sy(29), C.marble[1])
    put(b, L)
    return b


# ------------------------------------------------------------ industry
def blacksmith():
    b = Bld("blacksmith", 2, 2, extra=22)
    soot_ground(b)
    # open-sided forge shed: slate roof on posts, dark interior, glowing hearth
    z = 9
    L = b.layer()
    top = b.sy(2, z)
    roof_top(L, 1, top, 24, 12, "hip", SLATE, key="bs")
    f0 = top + 12
    f1 = b.sy(13)
    rect(L, 1, f0, 24, f1 - f0 + 1, C.outline)
    hline(L, 1, 24, f0, C.gray[1])
    for x in (1, 12, 24):                       # posts
        vline(L, x, f0, f1, C.earth[2])
    rect(L, 4, f1 - 3, 6, 3, C.roof_terracotta[1])      # hearth with fire
    glow(L, 5, f1 - 4, 4, 2)
    px(L, 6, f1 - 5, C.gold)
    rect(L, 15, f1 - 4, 6, 4, C.gray[1])               # bellows / workbench
    hline(L, 15, 20, f1 - 4, C.gray[2])
    put(b, L)
    L = b.layer()                               # tall dark chimney + soot smoke
    for y in range(top - 6, top + 4):
        px(L, 6, y, C.gray[2])
        px(L, 7, y, C.gray[1])
        px(L, 8, y, C.gray[0])
    put(b, L)
    L = b.layer()
    smoke_plume(L, 8, top - 8, dark=True, n=5)
    put(b, L, ol=False)
    # anvil on a stump, orange hot iron, iron bar stack, quench barrel
    L = b.layer()
    ay = b.sy(23)
    rect(L, 6, ay - 1, 3, 3, C.earth[1])
    rect(L, 4, ay - 3, 7, 2, C.gray[1])
    hline(L, 4, 10, ay - 3, C.gray[3])
    px(L, 11, ay - 3, C.gray[1])
    hline(L, 7, 8, ay - 4, C.roof_terracotta[3])
    px(L, 9, ay - 4, C.gold)
    for i in range(4):                          # bar stack
        hline(L, 17, 27, b.sy(26) - i, C.gray[3] if i % 2 else C.gray[2])
        px(L, 17 + i % 2, b.sy(26) - i, C.gray[3])
    vline(L, 18, b.sy(26) - 4, b.sy(26), C.earth[1])
    vline(L, 26, b.sy(26) - 4, b.sy(26), C.earth[1])
    barrel(L, 27, b.sy(18))
    px(L, 28, b.sy(18) - 4, C.sea[0])
    put(b, L)
    return b


# ------------------------------------------------------------ storage
def granary():
    b = Bld("granary", 3, 3, extra=24)
    lot(b, "paving", inset=0, border=(C.marble[0], C.travertine[0]))
    L = b.layer()
    f0, f1 = box(b, L, 2, 2, 44, 28, 16, mat="tan", roof="hip", ramp=TIMBER, plinth=False, key="gr")
    # raised floor: vents between stone piers
    for x in range(2, 46):
        for y in range(f1 - 2, f1 + 1):
            px(L, x, y, C.outline if (x - 2) % 4 >= 2 else C.travertine[0])
    # buttresses
    for x in range(4, 45, 7):
        vline(L, x, f0 + 1, f1 - 3, C.ochre[3])
        vline(L, x + 1, f0 + 1, f1 - 3, C.ochre[1])
        vline(L, x + 2, f0 + 2, f1 - 3, C.earth[1])
    # big double doors (dark timber) with a ramp
    rect(L, 19, f1 - 12, 10, 10, C.earth[0])
    vline(L, 23, f1 - 12, f1 - 3, C.outline)
    vline(L, 24, f1 - 12, f1 - 3, C.earth[1])
    hline(L, 18, 29, f1 - 13, C.earth[2])
    for y in (f1 - 9, f1 - 6):
        hline(L, 19, 28, y, C.earth[1])
    put(b, L)
    L = b.layer()
    rect(L, 19, f1 + 1, 10, 3, C.earth[2])           # timber loading ramp
    for y in range(f1 + 1, f1 + 4):
        hline(L, 19, 28, y, C.earth[3] if y % 2 else C.earth[2])
    put(b, L)
    # loading yard: sacks, amphorae, ox cart
    L = b.layer()
    for i, (x, gy) in enumerate(((4, 40), (8, 41), (6, 43), (33, 40), (37, 41), (41, 40), (39, 43))):
        sack(L, x, b.sy(gy), C.ochre[3] if i % 2 else C.travertine[2])
    cart(L, 12, b.sy(44), load=C.ochre[3])
    rect(L, 23, b.sy(44) - 3, 4, 2, C.earth[2])      # ox
    px(L, 22, b.sy(44) - 4, C.earth[2])
    put(b, L)
    return b


# ------------------------------------------------------------ religion
def temple():
    b = Bld("temple", 3, 3, extra=30)
    lot(b, "marble", inset=0, border=(C.white, C.marble[0]))
    L = b.layer()
    x, gy, w, d = 7, 2, 34, 36
    zpod, zcol = 6, 13
    sd = 9
    bd = d - sd
    zt = zpod + zcol + 3
    top = b.sy(gy, zt)
    roof_top(L, x, top, w, bd, "gable", MARBLE_ROOF, key="tp")
    # gold edging along the roof
    vline(L, x, top, top + bd - 1, C.gold)
    vline(L, x + w - 1, top, top + bd - 1, C.ochre[0])
    vline(L, x + w // 2, top, top + bd - 1, C.gold)
    f0 = top + bd
    colonnade(L, x, x + w - 1, f0, f0 + 2 + zcol, mat="marble", step=6, back=C.gray[2], entab=3)
    hline(L, x, x + w - 1, f0 + 1, C.gold)
    face(L, x, f0 + 3 + zcol, w, zpod, "marble", key="tp/pod", plinth=False)
    ph = pediment(L, x, x + w - 1, f0, ph=7, mat="marble")
    cx = x + w // 2
    for k in range(ph + 1):                     # gilded raking cornice
        half = (w / 2.0) * (1 - k / float(ph + 1))
        px(L, int(round(cx - half)), f0 - k, C.gold)
        px(L, int(round(cx + half)) - 1, f0 - k, C.ochre[0])
    rect(L, cx - 1, f0 - ph - 2, 2, 2, C.gold)  # acroteria
    px(L, x, f0 - 2, C.gold)
    px(L, x + w - 1, f0 - 2, C.gold)
    put(b, L)
    L = b.layer()
    steps(L, x + 3, x + w - 4, b.sy(gy + bd, zpod), b.sy(gy + d - 1), mat="marble")
    put(b, L)
    # altar with fire in front
    L = b.layer()
    box(b, L, 21, 41, 6, 3, 3, mat="marble", roof="flat", key="tp/al")
    glow(L, 23, b.sy(41, 5), 2, 2)
    put(b, L)
    tree(b, 3, b.sy(44), "cypress")
    tree(b, 45, b.sy(44), "cypress")
    return b


BUILDERS = [house_small, house_large, bakery, blacksmith, granary, temple]


def main():
    os.makedirs(OUT, exist_ok=True)
    for fn in BUILDERS:
        b = fn()
        check_image(b.img, b.id)
        b.img.save(os.path.join(OUT, b.id + ".png"))
        print("%-12s %dx%d" % (b.id, b.img.width, b.img.height))


if __name__ == "__main__":
    main()
