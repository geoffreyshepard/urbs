"""Fixed buildings: water, storage, trade, food, industry, housing extras.

Every recipe draws into a kit.Bld (footprint w x h tiles, extra height
8 + 4*min(w, h)) back-to-front.  Families share a layout: a roofed workshop
block at the back (north) and a yard at the front showing what the
building produces, so function reads at a glance.
"""
from palette import C
from primitives import px, rect, hline, vline, line, ellipse_mask, hash2
from .kit import (ground, ground_ellipse, box, face, door, window, windows, colonnade,
                  pediment, steps, arcade, cylinder, dome, amphora, amphorae, crate, sack,
                  barrel, logs, planks, blocks, pile, vat, awning, fence, chimney, glow,
                  millstone, statue, tree, crane, cart, banner, water_wheel, pit, ladder,
                  roof_top, MAT, ROOF)

RECIPES = {}


def recipe(fn):
    RECIPES[fn.__name__] = fn
    return fn


# ------------------------------------------------------------ shared layouts
def block(b, x, gy, w, d, z, mat="stucco", roof="hip", ramp="terracotta", door_x=None,
          door_kind="arch", win=True, win_step=6, door_h=None, door_w=4):
    L = b.layer()
    f0, f1 = box(b, L, x, gy, w, d, z, mat=mat, roof=roof, ramp=ramp)
    if roof == "gable":
        pediment(L, x, x + w - 1, f0, ph=max(3, min(z, w // 4)), mat=mat if mat in ("marble", "travertine", "stone") else "stucco")
    if door_x is not False:
        dx = x + w // 2 if door_x is None else door_x
        dh = door_h or max(3, min(z - 3, 7))
        door(L, dx, f1 - (1 if z >= 6 else 0), h=dh, w=door_w, kind=door_kind)
        skip = (dx - door_w // 2 - 2, dx + door_w // 2 + 1)
    else:
        skip = None
    if win and z >= 7:
        windows(L, x + 3, x + w - 4, f0 + 2, step=win_step, skip=skip)
    b.put(L)
    return f0, f1


def yard(b, x, gy, w, d, kind="dirt"):
    L = b.layer()
    ground(b, L, x, gy, w, d, kind)
    b.put(L, ol=False)


def low_wall(b, x, gy, w, z=2, mat="stucco", gap=None):
    """Low front wall/fence along ground row gy (with an optional gate gap)."""
    L = b.layer()
    for xx in range(x, x + w):
        if gap and gap[0] <= xx <= gap[1]:
            continue
        top = b.sy(gy, z)
        px(L, xx, top, MAT[mat]["lit"])
        for yy in range(top + 1, b.sy(gy) + 1):
            px(L, xx, yy, MAT[mat]["base"] if (xx + yy) % 5 else MAT[mat]["shade"])
    b.put(L)


def workshop(b, mat="stucco", roof="hip", ramp="terracotta", yard_kind="dirt", frac=0.5,
             z=None, wall=True, door_kind="arch", block_w=None, x=None):
    """Block at the back, yard in front.  Returns the yard rect (x, gy, w, d)
    in ground coords and the block face rows."""
    W, D = b.W, b.D
    bd = max(8, int(D * frac))
    z = z or (7 + 2 * min(b.w, b.h))
    yard(b, 1, 1, W - 2, D - 2, yard_kind)
    bw = block_w or (W - 2)
    bx = 1 if x is None else x
    f = block(b, bx, 1, bw, bd, z, mat=mat, roof=roof, ramp=ramp, door_kind=door_kind)
    yr = (2, bd + 2, W - 4, D - bd - 4)
    return yr, f


def finish_front(b, gap_w=6, mat="stucco"):
    W = b.W
    low_wall(b, 1, b.D - 2, W - 2, z=2, mat=mat, gap=(W // 2 - gap_w // 2, W // 2 + gap_w // 2 - 1))


def at(b, gx, gy):
    """Sprite coords of a ground point."""
    return gx, b.sy(gy)


# ------------------------------------------------------------ water
@recipe
def well(b):
    L = b.layer()
    ground(b, L, 2, 3, 12, 11, "paving")
    b.put(L, ol=False)
    L = b.layer()
    # posts and crossbeam behind the well-head
    for x in (3, 12):
        vline(L, x, b.sy(7, 11), b.sy(7), C.earth[2])
    hline(L, 3, 12, b.sy(7, 11), C.earth[1])
    hline(L, 3, 12, b.sy(7, 12), C.earth[3])
    b.put(L)
    L = b.layer()
    top = cylinder(b, L, 8, 8, 4.5, 3.2, 0, 4, mat="stone")
    inner = ellipse_mask(8, b.sy(8, 4) + 0.3, 2.6, 1.6)
    for (x, y) in inner:
        px(L, x, y, C.sea[0])
    b.put(L)
    L = b.layer()
    vline(L, 8, b.sy(7, 11) + 1, b.sy(8, 6), C.travertine[0])     # rope
    rect(L, 7, b.sy(8, 6), 3, 2, C.earth[2])                       # bucket
    px(L, 7, b.sy(8, 6), C.earth[3])
    hline(L, 7, 9, b.sy(8, 6) + 1, C.gray[1])
    b.put(L)


@recipe
def fountain(b):
    L = b.layer()
    ground(b, L, 1, 2, 14, 13, "paving")
    b.put(L, ol=False)
    L = b.layer()
    box(b, L, 2, 4, 12, 10, 3, mat="stone", roof="flat")
    ground(b, L, 3, 5, 10, 8, "water", z=3)
    b.put(L)
    L = b.layer()
    # central pillar with a lion-head spout, water jets
    rect(L, 7, b.sy(8, 10), 2, 8, C.marble[0])
    vline(L, 7, b.sy(8, 10), b.sy(8, 3), C.marble[1])
    statue(L, 8, b.sy(8, 10), h=4, mat="bronze", ped=False)
    b.put(L)
    L = b.layer()
    for (x, y) in ((6, b.sy(8, 7)), (5, b.sy(8, 6)), (10, b.sy(8, 7)), (11, b.sy(8, 6)),
                   (4, b.sy(9, 4)), (12, b.sy(9, 4))):
        px(L, x, y, C.sea[4])
    b.put(L, ol=False)


@recipe
def reservoir(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "gravel")
    L = b.layer()
    box(b, L, 3, 8, W - 6, D - 12, 5, mat="stone", roof="flat")
    ground(b, L, 5, 10, W - 10, D - 16, "water", z=5)
    b.put(L)
    # castellum (distribution house) at the back
    block(b, 14, 1, 20, 9, 11, mat="travertine", roof="hip", door_kind="dark", win=False)
    L = b.layer()
    for x in range(8, W - 8, 8):        # outlet channels on the front wall
        rect(L, x, b.sy(D - 5, 2), 2, 2, C.sea[1])
    b.put(L)


@recipe
def latrina(b):
    yr, f = workshop(b, mat="stucco", roof="gable", frac=0.62)
    L = b.layer()
    # flushing water channel along the front
    hline(L, 2, b.W - 3, b.sy(b.D - 6), C.sea[2])
    hline(L, 2, b.W - 3, b.sy(b.D - 5), C.sea[1])
    b.put(L, ol=False)
    L = b.layer()
    amphora(L, 3, b.sy(b.D - 3))
    b.put(L)
    finish_front(b)


@recipe
def engineers_post(b):
    yard(b, 1, 1, 14, 14, "dirt")
    block(b, 1, 1, 12, 8, 8, mat="stucco", roof="hip", win=False)
    L = b.layer()
    # groma (surveying cross) on a staff
    vline(L, 12, b.sy(13, 10), b.sy(13), C.earth[1])
    hline(L, 10, 14, b.sy(13, 10), C.earth[2])
    px(L, 10, b.sy(13, 9), C.gray[1])
    px(L, 14, b.sy(13, 9), C.gray[1])
    planks(L, 2, b.sy(13), w=6, rows=2)
    b.put(L)


# ------------------------------------------------------------ storage / trade
@recipe
def warehouse(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    L = b.layer()
    f0, f1 = box(b, L, 2, 2, W - 4, 32, 14, mat="travertine", roof="hip")
    # barn doors and high slit windows
    for dx in (W // 2,):
        rect(L, dx - 5, f1 - 8, 10, 9, C.earth[2])
        for x in range(dx - 5, dx + 5):
            if x % 2:
                vline(L, x, f1 - 7, f1, C.earth[1])
        vline(L, dx, f1 - 8, f1, C.outline)
        hline(L, dx - 6, dx + 5, f1 - 9, C.travertine[0])
    for x in range(6, W - 6, 6):
        if abs(x - W // 2) > 8:
            rect(L, x, f0 + 3, 1, 3, C.outline)
    b.put(L)
    L = b.layer()
    crate(L, 3, b.sy(D - 3), 5, 4)
    crate(L, 8, b.sy(D - 3), 4, 3)
    barrel(L, 34, b.sy(D - 3))
    barrel(L, 38, b.sy(D - 3))
    sack(L, 42, b.sy(D - 3))
    b.put(L)


@recipe
def granary(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    L = b.layer()
    f0, f1 = box(b, L, 2, 2, W - 4, 30, 15, mat="ochre", roof="hip", plinth=False)
    # raised floor on piers: dark ventilation gaps at the base
    for x in range(2, W - 2):
        for y in range(f1 - 3, f1 + 1):
            if (x - 2) % 5 >= 2:
                px(L, x, y, C.outline if y > f1 - 3 else C.earth[0])
            else:
                px(L, x, y, C.travertine[1] if (x - 2) % 5 == 0 else C.travertine[0])
    # vertical vent slits
    for x in range(5, W - 5, 4):
        vline(L, x, f0 + 3, f0 + 7, C.outline)
    # raised entrance with steps
    door(L, W // 2, f1 - 4, h=5, w=4)
    b.put(L)
    L = b.layer()
    steps(L, W // 2 - 3, W // 2 + 2, f1 - 3, b.sy(D - 3), mat="travertine")
    for i, x in enumerate((4, 8, 33, 37, 41)):
        sack(L, x, b.sy(D - 3 - (i % 2)), C.ochre[2])
    b.put(L)


@recipe
def market(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    # back portico (shops) with columns
    L = b.layer()
    f0, f1 = box(b, L, 1, 1, W - 2, 10, 8, mat="stucco", roof="shed")
    colonnade(L, 1, W - 2, f0, f1, mat="marble", step=5, back=C.earth[0])
    b.put(L)
    # side shop rows
    for x in (1, W - 8):
        L = b.layer()
        box(b, L, x, 11, 7, D - 13, 7, mat="stucco", roof="gable")
        b.put(L)
    # stalls with awnings
    L = b.layer()
    awning(L, 10, b.sy(30, 5), 7, 3, (C.roman_red[2], C.marble[1]))
    awning(L, 31, b.sy(30, 5), 7, 3, (C.ochre[2], C.marble[1]))
    for (x, c) in ((11, C.grass[2]), (13, C.roman_red[2]), (32, C.ochre[1]), (35, C.tyrian[1])):
        px(L, x, b.sy(31), c)
        px(L, x + 1, b.sy(31), c)
    b.put(L)
    # central tholos with conical roof
    L = b.layer()
    cylinder(b, L, 24, 25, 6, 4.5, 0, 7, mat="marble", arches=3)
    b.put(L)
    L = b.layer()
    dome(b, L, 24, 25, 7, 5, 5, 7, ramp=ROOF["terracotta"])
    b.put(L)
    L = b.layer()
    amphorae(L, 9, b.sy(41), 2)
    crate(L, 35, b.sy(42), 4, 3)
    b.put(L)
    low_wall(b, 1, D - 2, W - 2, z=3, gap=(W // 2 - 4, W // 2 + 3))


@recipe
def trade_post(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    block(b, 1, 1, W - 2, 16, 11, mat="ochre", roof="hip")
    L = b.layer()
    box(b, L, 1, 17, 12, 22, 9, mat="ochre", roof="gable")
    b.put(L)
    L = b.layer()
    awning(L, 18, b.sy(24, 5), 10, 3, (C.sea[2], C.marble[1]))
    amphorae(L, 18, b.sy(30), 3)
    crate(L, 30, b.sy(31), 5, 4)
    crate(L, 35, b.sy(32), 4, 3)
    sack(L, 30, b.sy(40), C.marble[0])
    sack(L, 34, b.sy(41), C.ochre[2])
    cart(L, 17, b.sy(42), load=C.travertine[2])
    banner(L, 42, b.sy(20, 12), C.sea[2])
    b.put(L)
    finish_front(b, mat="ochre")


@recipe
def cart_depot(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    block(b, 1, 1, W - 2, 14, 10, mat="wood", roof="shed", ramp="wood", door_x=False, win=False)
    L = b.layer()
    f0 = b.sy(15, 10)
    for x in range(4, W - 4, 10):
        rect(L, x, b.sy(14, 7), 7, 7, C.earth[0])           # open stalls
    b.put(L, ol=False)
    L = b.layer()
    cart(L, 4, b.sy(28), load=C.ochre[2])
    cart(L, 17, b.sy(31))
    cart(L, 30, b.sy(27), load=C.travertine[1])
    pile(L, 40, b.sy(40), 4, 4, (C.ochre[0], C.ochre[1], C.ochre[2]))
    b.put(L)
    L = b.layer()
    fence(L, 2, b.sy(D - 2), W - 3, b.sy(D - 2))
    b.put(L)


# ------------------------------------------------------------ waterfront
def _pier(b, L, x, gy0, w, gy1):
    ground(b, L, x, gy0, w, gy1 - gy0 + 1, "wood", z=1)
    y1 = b.sy(gy1, 1)
    hline(L, x, x + w - 1, y1 + 1, C.earth[0])
    for xx in range(x, x + w, 4):
        px(L, xx, y1 + 2, C.earth[0])            # piles below the deck
    for gy in range(gy0 + 2, gy1, 6):
        px(L, x, b.sy(gy, 3), C.earth[1])        # mooring posts
        px(L, x, b.sy(gy, 2), C.earth[1])
        px(L, x + w - 1, b.sy(gy, 3), C.earth[1])
        px(L, x + w - 1, b.sy(gy, 2), C.earth[1])


def _boat(L, x, y, w=12):
    """Small moored boat seen from above, (x, y) top-left."""
    for yy in range(4):
        inset = 2 if yy in (0, 3) else 0
        hline(L, x + inset, x + w - 1 - inset // 2, y + yy, C.earth[2] if yy < 2 else C.earth[1])
    hline(L, x + 3, x + w - 3, y + 1, C.earth[3])
    vline(L, x + w // 2, y - 6, y + 1, C.earth[1])               # mast
    hline(L, x + w // 2 - 3, x + w // 2 + 3, y - 5, C.marble[1])  # furled sail


@recipe
def harbor(b):
    W, D = b.W, b.D
    L = b.layer()
    ground(b, L, 1, 15, W - 2, 8, "paving")                       # stone quay
    b.put(L, ol=False)
    block(b, 2, 1, W - 4, 14, 12, mat="travertine", roof="hip", door_kind="dark")
    L = b.layer()
    _pier(b, L, 6, 23, 10, D - 3)
    _pier(b, L, 40, 23, 10, D - 3)
    b.put(L)
    L = b.layer()
    _boat(L, 20, b.sy(32), 14)
    b.put(L)
    L = b.layer()
    crane(L, 52, b.sy(21), 13)
    amphorae(L, 18, b.sy(21), 4)
    crate(L, 32, b.sy(21), 5, 4)
    crate(L, 37, b.sy(22), 4, 3)
    b.put(L)


@recipe
def fishing_wharf(b):
    W, D = b.W, b.D
    L = b.layer()
    ground(b, L, 1, 1, W - 2, 15, "sand")
    b.put(L, ol=False)
    block(b, 1, 1, 15, 10, 8, mat="wood", roof="gable", ramp="thatch", win=False)
    L = b.layer()
    _pier(b, L, 19, 6, 8, D - 2)
    b.put(L)
    L = b.layer()
    # drying net on a frame
    for x in range(2, 15):
        for y in range(b.sy(15, 6), b.sy(15, 1)):
            if (x + y) % 2 == 0:
                px(L, x, y, C.travertine[0])
    vline(L, 2, b.sy(15, 7), b.sy(15), C.earth[1])
    vline(L, 15, b.sy(15, 7), b.sy(15), C.earth[1])
    hline(L, 2, 15, b.sy(15, 7), C.earth[2])
    b.put(L)
    L = b.layer()
    _boat(L, 4, b.sy(22), 11)
    for x in (20, 23):
        px(L, x, b.sy(12, 2), C.gray[3])     # fish on the pier
        px(L, x + 1, b.sy(12, 2), C.gray[2])
    b.put(L)


@recipe
def salt_works(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "sand")
    block(b, 1, 1, 14, 10, 8, mat="wood", roof="shed", ramp="wood", win=False)
    L = b.layer()
    for i, (x, gy) in enumerate(((17, 2), (32, 2), (17, 15), (32, 15), (2, 15))):
        ground(b, L, x, gy, 13, 11, "water")
        for k in range(14):
            xx = x + 1 + hash2(i, k, 2) % 11
            yy = b.sy(gy + 1 + hash2(k, i, 3) % 9)
            px(L, xx, yy, C.marble[1] if i % 2 else C.white)
        for xx in range(x, x + 13):
            px(L, xx, b.sy(gy), C.ochre[1])
        vline(L, x, b.sy(gy), b.sy(gy + 10), C.ochre[1])
    b.put(L, ol=False)
    L = b.layer()
    pile(L, 8, b.sy(29), 3, 4, (C.marble[0], C.marble[1], C.white))
    pile(L, 26, b.sy(29), 3, 3, (C.marble[0], C.marble[1], C.white))
    b.put(L)


# ------------------------------------------------------------ food
@recipe
def mill(b):
    yr, f = workshop(b, mat="stucco", roof="hip", yard_kind="paving", frac=0.45)
    L = b.layer()
    for (cx, gy) in ((9, 22), (22, 22)):
        ring = ellipse_mask(cx, b.sy(gy), 6, 3.5)
        inner = ellipse_mask(cx, b.sy(gy), 4, 2)
        for (x, y) in ring - inner:
            px(L, x, y, C.travertine[1])
    b.put(L, ol=False)
    L = b.layer()
    millstone(L, 7, b.sy(23))
    millstone(L, 20, b.sy(23))
    sack(L, 13, b.sy(29), C.marble[1])
    sack(L, 26, b.sy(29), C.marble[1])
    b.put(L)
    finish_front(b)


@recipe
def bakery(b):
    yr, f = workshop(b, mat="stucco", roof="hip", yard_kind="paving", frac=0.5)
    L = b.layer()
    chimney(L, 22, b.sy(4, 15), 5)
    b.put(L)
    # domed brick oven with glowing mouth
    L = b.layer()
    dome(b, L, 10, 24, 6, 4, 6, 0, ramp=(C.roof_terracotta[0], C.roof_terracotta[1],
                                         C.roof_terracotta[2], C.roof_terracotta[3]))
    rect(L, 8, b.sy(27, 3), 4, 3, C.outline)
    glow(L, 9, b.sy(27, 2), 2, 2)
    b.put(L)
    L = b.layer()
    rect(L, 19, b.sy(25, 3), 9, 3, C.travertine[2])     # counter with loaves
    hline(L, 19, 27, b.sy(25, 3), C.marble[0])
    for x in (20, 23, 26):
        px(L, x, b.sy(25, 4), C.ochre[1])
        px(L, x + 1, b.sy(25, 4), C.ochre[0])
    b.put(L)
    finish_front(b)


@recipe
def olive_press(b):
    yr, f = workshop(b, mat="stucco", roof="hip", yard_kind="paving", frac=0.45)
    L = b.layer()
    # trapetum: round crushing basin with two upright millstones
    cylinder(b, L, 9, 24, 6, 4, 0, 3, mat="stone", top=C.gray[2])
    b.put(L)
    L = b.layer()
    rect(L, 5, b.sy(24, 7), 3, 4, C.gray[3])
    rect(L, 11, b.sy(24, 7), 3, 4, C.gray[3])
    hline(L, 4, 14, b.sy(24, 6), C.earth[2])
    b.put(L)
    L = b.layer()
    # lever press beam
    line(L, 17, b.sy(19, 6), 29, b.sy(25, 3), C.earth[2])
    rect(L, 26, b.sy(26, 4), 3, 4, C.gray[2])
    amphorae(L, 18, b.sy(29), 2, C.olive[2])
    b.put(L)
    finish_front(b)


@recipe
def winery(b):
    yr, f = workshop(b, mat="stucco", roof="hip", yard_kind="dirt", frac=0.5)
    L = b.layer()
    for i in range(5):                      # half-buried dolia
        cx = 6 + i * 8
        e = ellipse_mask(cx, b.sy(22), 3, 2)
        for (x, y) in e:
            px(L, x, y, C.roof_terracotta[2])
        px(L, cx - 1, b.sy(22), C.outline)
        px(L, cx, b.sy(22), C.outline)
    b.put(L)
    L = b.layer()
    for x in (6, 12):                       # grape baskets
        rect(L, x, b.sy(28, 2), 4, 2, C.earth[2])
        hline(L, x, x + 3, b.sy(28, 3), C.tyrian[1])
        px(L, x + 1, b.sy(28, 4), C.tyrian[0])
    amphorae(L, 32, b.sy(29), 4, C.roman_red[1])
    b.put(L)
    finish_front(b)


@recipe
def garum_workshop(b):
    yr, f = workshop(b, mat="stucco", roof="shed", yard_kind="paving", frac=0.42)
    L = b.layer()
    for x in (3, 16):                       # salting vats
        box(b, L, x, 16, 12, 8, 2, mat="stone", roof="flat")
        ground(b, L, x + 1, 17, 10, 6, "water", z=2)
        for xx in range(x + 1, x + 11):
            for yy in range(b.sy(17, 2), b.sy(22, 2) + 1):
                if hash2(xx, yy, 40) % 3:
                    px(L, xx, yy, C.roman_red[0] if hash2(xx, yy, 41) % 2 else C.roof_terracotta[0])
    b.put(L)
    L = b.layer()
    amphorae(L, 4, b.sy(29), 3, C.ochre[1])
    for x in (18, 22, 26):
        hline(L, x, x + 2, b.sy(28, 1), C.gray[3])     # fish
        px(L, x + 3, b.sy(28, 1), C.gray[1])
    b.put(L)
    finish_front(b)


# ------------------------------------------------------------ industry
@recipe
def lumber_camp(b):
    yard(b, 1, 1, b.W - 2, b.D - 2, "dirt")
    block(b, 1, 1, 14, 11, 8, mat="wood", roof="shed", ramp="wood", win=False)
    tree(b, 25, b.sy(12), "pine")
    L = b.layer()
    logs(L, 3, b.sy(22), w=10, rows=3)
    logs(L, 17, b.sy(28), w=9, rows=2)
    for (x, gy) in ((5, 28), (14, 17)):                 # stumps
        rect(L, x, b.sy(gy, 2), 3, 2, C.earth[2])
        hline(L, x, x + 2, b.sy(gy, 2), C.ochre[2])
    vline(L, 7, b.sy(26, 4), b.sy(26, 1), C.earth[1])   # axe
    px(L, 8, b.sy(26, 4), C.gray[3])
    b.put(L)


@recipe
def sawmill(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    L = b.layer()
    ground(b, L, 1, 1, 6, D - 2, "water")              # mill race
    b.put(L, ol=False)
    block(b, 7, 1, 26, 15, 10, mat="wood", roof="gable", ramp="wood", win=False, door_kind="dark")
    L = b.layer()
    water_wheel(L, 6, b.sy(12, 6), 5)
    b.put(L)
    L = b.layer()
    logs(L, 35, b.sy(10), w=10, rows=3)
    planks(L, 10, b.sy(25), w=10, rows=3)
    planks(L, 22, b.sy(27), w=9, rows=2)
    for x in (36, 43):                                 # saw horse
        line(L, x, b.sy(25), x + 2, b.sy(25, 4), C.earth[1])
    hline(L, 36, 45, b.sy(25, 4), C.earth[3])
    b.put(L)


def _quarry(b, ramp, block_mat, extra_pile=None, water=False):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "gravel")
    L = b.layer()
    pit(b, L, 3, 3, W - 6, D - 18, ramp, levels=3, water=water)
    b.put(L, ol=False)
    L = b.layer()
    ladder(L, 8, b.sy(5), b.sy(14))
    b.put(L)
    L = b.layer()
    blocks(L, 4, b.sy(D - 6), 3, mat=block_mat)
    blocks(L, 6, b.sy(D - 3), 2, mat=block_mat)
    if extra_pile:
        pile(L, W - 18, b.sy(D - 4), 4, 4, extra_pile)
    b.put(L)
    L = b.layer()
    crane(L, W - 12, b.sy(D - 4), 14)
    b.put(L)


@recipe
def quarry(b):
    _quarry(b, (C.gray[0], C.gray[1], C.gray[2], C.gray[3]), "stone",
            (C.gray[1], C.gray[2], C.gray[3]))


@recipe
def limestone_quarry(b):
    _quarry(b, (C.travertine[0], C.travertine[1], C.travertine[2], C.marble[0]), "travertine",
            (C.travertine[0], C.travertine[1], C.travertine[2]))


@recipe
def marble_quarry(b):
    _quarry(b, (C.gray[2], C.marble[0], C.marble[1], C.white), "marble",
            (C.marble[0], C.marble[1], C.white))


def _small_pit(b, ramp, piles, water=False, shed=True):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    L = b.layer()
    pit(b, L, 2, 2, W - 4, D - 13, ramp, levels=2, water=water)
    b.put(L, ol=False)
    L = b.layer()
    ladder(L, 5, b.sy(3), b.sy(11))
    b.put(L)
    L = b.layer()
    for i, (x, h) in enumerate(((7, 4), (17, 3))):
        pile(L, x, b.sy(D - 4), 4, h, piles)
    rect(L, 23, b.sy(D - 4, 2), 5, 2, C.earth[2])      # basket / barrow
    hline(L, 23, 27, b.sy(D - 4, 3), piles[-1])
    b.put(L)


@recipe
def clay_pit(b):
    _small_pit(b, (C.earth[0], C.earth[1], C.roof_terracotta[0], C.roof_terracotta[1]),
               (C.roof_terracotta[0], C.roof_terracotta[1], C.roof_terracotta[2]), water=True)


@recipe
def pozzolana_pit(b):
    _small_pit(b, (C.gray[0], C.tyrian[0], C.gray[1], C.tyrian[1]),
               (C.gray[0], C.tyrian[0], C.tyrian[1]))


@recipe
def sand_pit(b):
    _small_pit(b, (C.ochre[0], C.ochre[1], C.ochre[2], C.ochre[3]),
               (C.ochre[0], C.ochre[1], C.ochre[2]))


def _kiln_round(b, L, cx, gy, r, z, mat="brick"):
    cylinder(b, L, cx, gy, r, r * 0.7, 0, z, mat=mat)
    top = ellipse_mask(cx, b.sy(gy, z), r - 1.2, r * 0.7 - 1)
    for (x, y) in top:
        px(L, x, y, C.outline)
    for (x, y) in ellipse_mask(cx, b.sy(gy, z) + 0.6, r - 2.5, r * 0.7 - 2):
        px(L, x, y, C.roof_terracotta[3] if (x + y) % 2 else C.gold)


@recipe
def brick_kiln(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    L = b.layer()
    f0, f1 = box(b, L, 3, 3, 20, 14, 10, mat="brick", roof="flat")
    rect(L, 10, f1 - 4, 6, 5, C.outline)
    glow(L, 11, f1 - 2, 4, 2)
    hline(L, 9, 16, f1 - 5, C.roof_terracotta[3])
    for x in range(5, 21, 4):
        px(L, x, b.sy(9, 10), C.outline)               # vents on top
    b.put(L)
    L = b.layer()
    for k in range(3):                                 # stacked bricks
        blocks(L, 4 + k * 9, b.sy(D - 3), 2, mat="brick", size=3)
        blocks(L, 6 + k * 9, b.sy(D - 6), 1, mat="brick", size=3)
    logs(L, 24, b.sy(10), w=6, rows=2)
    b.put(L)


@recipe
def lime_kiln(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "gravel")
    L = b.layer()
    _kiln_round(b, L, 12, 14, 8, 14, mat="stone")
    rect(L, 10, b.sy(19, 4), 4, 4, C.outline)
    glow(L, 11, b.sy(19, 2), 2, 2)
    b.put(L)
    L = b.layer()
    pile(L, 25, b.sy(D - 4), 4, 5, (C.marble[0], C.marble[1], C.white))
    pile(L, 6, b.sy(D - 3), 3, 3, (C.marble[0], C.marble[1], C.white))
    logs(L, 21, b.sy(12), w=8, rows=2)
    b.put(L)


@recipe
def concrete_works(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "gravel")
    block(b, 1, 1, 20, 12, 9, mat="wood", roof="shed", ramp="wood", win=False)
    L = b.layer()
    for (x, gy) in ((5, 20), (21, 22)):              # mixing pits
        ground(b, L, x, gy, 12, 8, "gravel")
        rect(L, x + 2, b.sy(gy + 2), 8, 4, C.gray[2])
        hline(L, x + 2, x + 9, b.sy(gy + 2), C.gray[1])
    b.put(L, ol=False)
    L = b.layer()
    pile(L, 30, b.sy(12), 5, 6, (C.gray[0], C.tyrian[0], C.tyrian[1]))
    pile(L, 41, b.sy(13), 4, 5, (C.marble[0], C.marble[1], C.white))
    pile(L, 39, b.sy(D - 4), 4, 4, (C.gray[1], C.gray[2], C.gray[3]))
    amphorae(L, 4, b.sy(D - 4), 3)
    b.put(L)


@recipe
def stonemason(b):
    yr, f = workshop(b, mat="stone", roof="hip", yard_kind="gravel", frac=0.45)
    L = b.layer()
    blocks(L, 3, b.sy(23), 3, mat="stone", size=4)
    blocks(L, 5, b.sy(28), 2, mat="travertine", size=3)
    cylinder(b, L, 24, 23, 3, 2, 0, 3, mat="marble", top=C.marble[1])     # column drums
    b.put(L)
    L = b.layer()
    cylinder(b, L, 21, 28, 3, 2, 0, 2, mat="marble", top=C.marble[1])
    b.put(L)
    finish_front(b, mat="stone")


@recipe
def sculptor(b):
    yr, f = workshop(b, mat="stucco", roof="hip", yard_kind="paving", frac=0.45)
    L = b.layer()
    statue(L, 7, b.sy(26), h=7)
    statue(L, 16, b.sy(27), h=6)
    blocks(L, 21, b.sy(27), 2, mat="marble", size=4)
    b.put(L)
    finish_front(b)


def _mine(b, ore_ramp, hill_ramp):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "gravel")
    L = b.layer()
    dome(b, L, W // 2, 13, W // 2 - 2, 11, 9, 0, ramp=hill_ramp, noise=0.55)
    for (x, y) in [(10, b.sy(8, 8)), (30, b.sy(6, 9)), (22, b.sy(12, 8))]:
        blocks(L, x, y, 1, mat='stone', size=2)
    b.put(L)
    L = b.layer()
    # timbered adit facing south
    cx = W // 2
    base = b.sy(24)
    rect(L, cx - 4, base - 8, 8, 8, C.earth[1])
    rect(L, cx - 3, base - 7, 6, 7, C.outline)
    hline(L, cx - 5, cx + 4, base - 8, C.earth[3])
    b.put(L)
    L = b.layer()
    for gy in range(25, D - 2):                        # rails
        px(L, cx - 2, b.sy(gy), C.gray[1])
        px(L, cx + 1, b.sy(gy), C.gray[1])
        if gy % 2 == 0:
            hline(L, cx - 3, cx + 2, b.sy(gy), C.earth[1])
    b.put(L, ol=False)
    L = b.layer()
    pile(L, 10, b.sy(D - 4), 6, 6, ore_ramp)
    pile(L, W - 10, b.sy(D - 5), 5, 5, ore_ramp)
    cart(L, cx - 4, b.sy(D - 6), load=ore_ramp[1])
    b.put(L)
    L = b.layer()
    # headframe over a shaft on the hill
    x = W - 14
    line(L, x, b.sy(10, 8), x + 3, b.sy(10, 18), C.earth[2])
    line(L, x + 6, b.sy(10, 8), x + 3, b.sy(10, 18), C.earth[1])
    for (dx, dy) in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)):
        px(L, x + 3 + dx, b.sy(10, 18) + dy, C.gray[1])
    b.put(L)


@recipe
def iron_mine(b):
    _mine(b, (C.roman_red[0], C.roof_terracotta[0], C.roof_terracotta[1]),
          (C.earth[0], C.earth[1], C.earth[2], C.roof_terracotta[1]))


@recipe
def silver_mine(b):
    _mine(b, (C.gray[1], C.gray[3], C.white),
          (C.gray[0], C.gray[1], C.travertine[0], C.gray[2]))


def _smelter(b, ore, ingot):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    block(b, 1, 1, 18, 12, 9, mat="stone", roof="shed", ramp="old", win=False, door_kind="dark")
    for cx in (25, 38):
        L = b.layer()
        cylinder(b, L, cx, 11, 5, 3.5, 0, 13, mat="brick")
        for (x, y) in ellipse_mask(cx, b.sy(11, 13), 3, 2):
            px(L, x, y, C.outline)
        rect(L, cx - 1, b.sy(14, 3), 3, 3, C.outline)
        glow(L, cx - 1, b.sy(14, 2), 2, 2)
        b.put(L)
    L = b.layer()
    pile(L, 7, b.sy(D - 4), 5, 5, ore)
    pile(L, 18, b.sy(D - 4), 4, 4, (C.outline, C.gray[0], C.gray[1]))    # charcoal
    for k in range(3):                                                  # ingot stack
        hline(L, 30 + k, 36 + k, b.sy(D - 3) - k, ingot[0] if k % 2 else ingot[1])
    b.put(L)


@recipe
def smelter(b):
    _smelter(b, (C.roman_red[0], C.roof_terracotta[0], C.roof_terracotta[1]), (C.gray[1], C.gray[2]))


@recipe
def silver_smelter(b):
    _smelter(b, (C.gray[1], C.gray[3], C.white), (C.gray[3], C.white))


@recipe
def blacksmith(b):
    yr, f = workshop(b, mat="stone", roof="hip", ramp="old", yard_kind="dirt", frac=0.5)
    L = b.layer()
    box(b, L, 3, 18, 10, 7, 4, mat="brick", roof="flat")             # forge hearth
    glow(L, 5, b.sy(21, 4), 6, 2)
    b.put(L)
    L = b.layer()
    chimney(L, 10, b.sy(18, 12), 7)
    rect(L, 19, b.sy(24, 3), 5, 2, C.gray[1])                        # anvil
    hline(L, 18, 24, b.sy(24, 3), C.gray[2])
    vline(L, 21, b.sy(24, 1), b.sy(24), C.gray[0])
    for x in (26, 28):                                               # tool rack
        vline(L, x, b.sy(22, 6), b.sy(22), C.earth[1])
    px(L, 26, b.sy(22, 6), C.gray[3])
    b.put(L)
    finish_front(b, mat="stone")


@recipe
def armory(b):
    yr, f = workshop(b, mat="stone", roof="hip", yard_kind="dirt", frac=0.5)
    L = b.layer()
    hline(L, 3, 20, b.sy(24, 7), C.earth[2])                         # spear rack
    for x in range(4, 20, 2):
        vline(L, x, b.sy(24, 9), b.sy(24), C.earth[1])
        px(L, x, b.sy(24, 10), C.gray[3])
    for x in (24, 31, 38):                                           # shields
        rect(L, x, b.sy(27, 6), 5, 6, C.roman_red[1])
        vline(L, x, b.sy(27, 6), b.sy(27, 1), C.roman_red[2])
        px(L, x + 2, b.sy(27, 4), C.gold)
        px(L, x + 2, b.sy(27, 3), C.gold)
    b.put(L)
    L = b.layer()
    chimney(L, 40, b.sy(3, 16), 5)
    b.put(L)
    finish_front(b, mat="stone")


@recipe
def potter(b):
    yr, f = workshop(b, mat="stucco", roof="hip", yard_kind="dirt", frac=0.45)
    L = b.layer()
    dome(b, L, 8, 22, 5, 3.5, 5, 0, ramp=(C.roof_terracotta[0], C.roof_terracotta[1],
                                        C.roof_terracotta[2], C.roof_terracotta[3]))
    rect(L, 7, b.sy(25, 2), 3, 2, C.outline)
    px(L, 8, b.sy(25, 1), C.gold)
    b.put(L)
    L = b.layer()
    for i, x in enumerate(range(16, 29, 4)):
        amphora(L, x, b.sy(24 + (i % 2) * 3), C.roof_terracotta[2])
    rect(L, 3, b.sy(29, 2), 4, 2, C.earth[2])                        # bowls
    hline(L, 3, 6, b.sy(29, 2), C.roof_terracotta[3])
    b.put(L)
    finish_front(b)


@recipe
def weaver(b):
    yr, f = workshop(b, mat="stucco", roof="hip", yard_kind="dirt", frac=0.45)
    L = b.layer()
    # upright loom with coloured warp
    vline(L, 4, b.sy(25, 10), b.sy(25), C.earth[1])
    vline(L, 15, b.sy(25, 10), b.sy(25), C.earth[1])
    hline(L, 4, 15, b.sy(25, 10), C.earth[2])
    for x in range(5, 15):
        for y in range(b.sy(25, 9), b.sy(25, 3)):
            px(L, x, y, (C.marble[1], C.sea[2], C.marble[1], C.roman_red[2])[(y // 2) % 4] if x % 2 else C.marble[0])
    b.put(L)
    L = b.layer()
    for x in (20, 25):                                               # wool bales
        rect(L, x, b.sy(27, 4), 4, 4, C.marble[1])
        hline(L, x, x + 3, b.sy(27, 4), C.white)
        vline(L, x + 3, b.sy(27, 3), b.sy(27), C.marble[0])
    b.put(L)
    finish_front(b)


def _cloth_line(L, x0, x1, y, colors):
    hline(L, x0, x1, y, C.earth[1])
    for i, x in enumerate(range(x0 + 1, x1 - 2, 4)):
        c = colors[i % len(colors)]
        for yy in range(y + 1, y + 5):
            px(L, x, yy, c)
            px(L, x + 1, yy, c)
            px(L, x + 2, yy, c if yy < y + 4 else C.outline)


@recipe
def dye_works(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    block(b, 1, 1, 22, 12, 9, mat="stucco", roof="hip")
    L = b.layer()
    for i, (cx, c) in enumerate(((29, C.roman_red[1]), (38, C.tyrian[1]), (29, C.sea[1]), (38, C.ochre[1]))):
        vat(L, cx, b.sy(6 + (i // 2) * 9, 2), 4, c)
    b.put(L)
    L = b.layer()
    for x in (3, 20):
        vline(L, x, b.sy(24, 9), b.sy(24), C.earth[1])
    _cloth_line(L, 3, 20, b.sy(24, 9), (C.roman_red[2], C.tyrian[1], C.sea[2], C.gold))
    b.put(L)


@recipe
def dyer(b):
    yr, f = workshop(b, mat="stucco", roof="hip", yard_kind="paving", frac=0.45)
    L = b.layer()
    vat(L, 7, b.sy(21), 4, C.tyrian[1])
    vat(L, 17, b.sy(22), 4, C.roman_red[1])
    b.put(L)
    L = b.layer()
    vline(L, 22, b.sy(26, 8), b.sy(26), C.earth[1])
    vline(L, 29, b.sy(26, 8), b.sy(26), C.earth[1])
    _cloth_line(L, 22, 29, b.sy(26, 8), (C.tyrian[1], C.sea[2]))
    b.put(L)
    finish_front(b)


@recipe
def carpenter(b):
    yr, f = workshop(b, mat="wood", roof="hip", yard_kind="dirt", frac=0.45)
    L = b.layer()
    planks(L, 3, b.sy(23), w=10, rows=3)
    planks(L, 4, b.sy(27), w=8, rows=2)
    # stool and table
    rect(L, 19, b.sy(24, 4), 7, 2, C.earth[3])
    vline(L, 19, b.sy(24, 2), b.sy(24), C.earth[1])
    vline(L, 25, b.sy(24, 2), b.sy(24), C.earth[1])
    rect(L, 21, b.sy(28, 3), 3, 1, C.earth[3])
    vline(L, 21, b.sy(28, 2), b.sy(28), C.earth[1])
    vline(L, 23, b.sy(28, 2), b.sy(28), C.earth[1])
    b.put(L)
    finish_front(b, mat="wood")


@recipe
def scriptorium(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    L = b.layer()
    f0, f1 = box(b, L, 1, 1, W - 2, 18, 11, mat="stucco", roof="hip")
    colonnade(L, 1, W - 2, f0, f1, mat="marble", step=6, back=C.earth[0])
    b.put(L)
    L = b.layer()
    for x in (4, 18):                                    # writing desks with scrolls
        rect(L, x, b.sy(25, 3), 8, 2, C.earth[3])
        vline(L, x, b.sy(25, 1), b.sy(25), C.earth[1])
        vline(L, x + 7, b.sy(25, 1), b.sy(25), C.earth[1])
        px(L, x + 2, b.sy(25, 4), C.travertine[2])
        px(L, x + 3, b.sy(25, 4), C.marble[1])
        px(L, x + 5, b.sy(25, 4), C.ochre[3])
    b.put(L)
    finish_front(b)


@recipe
def glassworks(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    block(b, 1, 1, 20, 12, 9, mat="stucco", roof="hip")
    L = b.layer()
    dome(b, L, 33, 12, 9, 6, 8, 0, ramp=(C.roof_terracotta[0], C.roof_terracotta[1],
                                       C.roof_terracotta[2], C.roof_terracotta[3]))
    for x in (28, 34):
        rect(L, x, b.sy(17, 3), 3, 3, C.outline)
        glow(L, x + 1, b.sy(17, 2), 1, 2)
    b.put(L)
    L = b.layer()
    hline(L, 3, 20, b.sy(24, 4), C.earth[2])            # shelf with glassware
    for i, x in enumerate(range(4, 20, 3)):
        c = (C.sea[3], C.sea[4], C.olive[2], C.sea[3])[i % 4]
        px(L, x, b.sy(24, 5), c)
        px(L, x, b.sy(24, 6), c)
        px(L, x, b.sy(24, 7), C.white)
    pile(L, 40, b.sy(D - 3), 4, 3, (C.ochre[0], C.ochre[1], C.ochre[2]))
    b.put(L)


@recipe
def jeweler(b):
    yr, f = workshop(b, mat="red", roof="hip", yard_kind="paving", frac=0.55)
    L = b.layer()
    rect(L, 8, b.sy(24, 3), 16, 3, C.travertine[2])     # display counter
    hline(L, 8, 23, b.sy(24, 3), C.marble[1])
    for x in range(10, 23, 3):
        px(L, x, b.sy(24, 4), C.gold if x % 2 else C.white)
    banner(L, 27, b.sy(20, 12), C.tyrian[1])
    b.put(L)
    finish_front(b, mat="red")


@recipe
def slave_quarters(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    L = b.layer()
    f0, f1 = box(b, L, 1, 1, W - 2, 20, 9, mat="ochre", roof="hip", ramp="old")
    for x in range(4, W - 4, 5):
        rect(L, x, f0 + 3, 1, 2, C.outline)
    door(L, 12, f1 - 1, h=5, w=3, kind="dark")
    door(L, 36, f1 - 1, h=5, w=3, kind="dark")
    b.put(L)
    L = b.layer()
    rect(L, 20, b.sy(26, 2), 9, 2, C.gray[2])           # water trough
    hline(L, 21, 27, b.sy(26, 2), C.sea[2])
    b.put(L)
