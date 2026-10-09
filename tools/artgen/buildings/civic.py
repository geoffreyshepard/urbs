"""Fixed buildings: civic, religion, health, safety, entertainment, education,
military, decor and monuments.  Same conventions as works.py."""
import math

from palette import C
from primitives import px, rect, hline, vline, line, ellipse_mask, hash2, get
from .kit import (ground, ground_ellipse, box, face, door, window, windows, colonnade,
                  pediment, steps, arcade, cylinder, dome, amphora, amphorae, crate, sack,
                  barrel, logs, planks, blocks, pile, vat, awning, fence, chimney, glow,
                  statue, tree, crane, cart, banner, smoke, roof_top, MAT, ROOF)
from .works import block, yard, low_wall, workshop, finish_front

RECIPES = {}


def recipe(fn):
    RECIPES[fn.__name__] = fn
    return fn


# ------------------------------------------------------------ temple kit
def temple(b, x, gy, w, d, zpod, zcol, mat="marble", ramp="terracotta", step=None,
           stairs=None, acroteria=False, oculus=False):
    """Classical temple on a podium: gable roof (ridge N-S), pedimented
    columned front, frontal stairs.  Ground rect (x, gy, w, d)."""
    L = b.layer()
    sd = stairs or max(3, d // 5)
    bd = d - sd
    zt = zpod + zcol + 3
    top = b.sy(gy, zt)
    roof_top(L, x, top, w, bd, "gable", ramp, key=b.id + "/t")
    f0 = top + bd
    colonnade(L, x, x + w - 1, f0, f0 + 2 + zcol, mat=mat,
              step=step or max(4, (w - 2) // max(2, (w // 6))), back=C.travertine[0], entab=3)
    face(L, x, f0 + 3 + zcol, w, zpod, "travertine", key=b.id + "/pod", plinth=False)
    ph = pediment(L, x, x + w - 1, f0, ph=max(3, w // 5), mat=mat, oculus=oculus)
    if acroteria:
        cx = x + w // 2
        px(L, cx - 1, f0 - ph - 1, C.gold)
        px(L, cx, f0 - ph - 1, C.gold)
        px(L, cx, f0 - ph - 2, C.gold)
        px(L, x, f0 - 1, C.gold)
        px(L, x + w - 1, f0 - 1, C.gold)
    b.put(L)
    L = b.layer()
    steps(L, x + 2, x + w - 3, b.sy(gy + bd, zpod), b.sy(gy + d - 1), mat=mat)
    b.put(L)
    return f0


def altar(b, cx, gy, w=6):
    L = b.layer()
    box(b, L, cx - w // 2, gy, w, 3, 3, mat="marble", roof="flat")
    px(L, cx - 1, b.sy(gy, 4), C.gold)
    px(L, cx, b.sy(gy, 4), C.roof_terracotta[3])
    b.put(L)


def portico(b, x, gy, w, d, z, roof="shed", ramp="terracotta", step=5, mat="marble"):
    """Covered colonnade: roof on top, open columned face to the south."""
    L = b.layer()
    top = b.sy(gy, z)
    roof_top(L, x, top, w, d, roof, ramp, key="%s/p%d" % (b.id, x))
    colonnade(L, x, x + w - 1, top + d, b.sy(gy + d - 1), mat=mat, step=step,
              back=C.travertine[0])
    b.put(L)


def side_wing(b, x, gy, w, d, z, ramp="terracotta", mat="stucco"):
    L = b.layer()
    box(b, L, x, gy, w, d, z, mat=mat, roof="gable", ramp=ramp)
    b.put(L)


def crenels(L, x0, x1, y, c_lit=None, c_dark=None):
    c_lit = c_lit or C.gray[3]
    for x in range(x0, x1 + 1):
        if (x - x0) % 3 != 2:
            px(L, x, y - 1, c_lit)
        else:
            px(L, x, y - 1, (0, 0, 0, 0))


def tower_box(b, x, gy, w, d, z, mat="stone", slits=True):
    L = b.layer()
    f0, f1 = box(b, L, x, gy, w, d, z, mat=mat, roof="flat")
    top = b.sy(gy, z)
    rect(L, x + 2, top + 2, w - 4, d - 4, C.gray[1])       # sunken walkway
    hline(L, x + 2, x + w - 3, top + 2, C.gray[0])
    for xx in range(x, x + w):                       # merlons on the parapets
        if (xx - x) % 3 != 2:
            px(L, xx, top - 1, MAT[mat]["lit"])
            px(L, xx, top + d - 2, MAT[mat]["base"])
    for yy in range(top, top + d - 1, 3):
        px(L, x, yy, MAT[mat]["shade"])
        px(L, x + w - 1, yy, MAT[mat]["shade"])
    if slits:
        for xx in range(x + 3, x + w - 2, 4):
            vline(L, xx, f0 + 3, f0 + 5, C.outline)
    b.put(L)
    return f0, f1


# ------------------------------------------------------------ decor / civic
@recipe
def statue_(b):
    pass


def _statue(b):
    L = b.layer()
    ground(b, L, 3, 4, 10, 10, "paving")
    b.put(L, ol=False)
    L = b.layer()
    box(b, L, 5, 8, 6, 5, 5, mat="marble", roof="flat")
    b.put(L)
    L = b.layer()
    base = b.sy(10, 5)
    # bronze togate figure with raised arm
    lit, mid, sh = C.ochre[2], C.ochre[0], C.earth[1]
    px(L, 8, base - 10, lit)
    px(L, 7, base - 10, mid)
    for y in range(base - 9, base + 1):
        px(L, 7, y, lit)
        px(L, 8, y, mid)
        px(L, 9, y, sh)
    for k in range(3):
        px(L, 10 + (k > 0), base - 8 - k, mid)
    b.put(L)


RECIPES["statue"] = _statue
del RECIPES["statue_"]


@recipe
def forum(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "marble")
    portico(b, 1, 1, W - 2, 10, 10, step=5)
    for x in (1, W - 8):
        side_wing(b, x, 11, 7, D - 16, 8, mat="stucco")
    L = b.layer()
    box(b, L, 26, 30, 12, 8, 6, mat="travertine", roof="flat")       # rostra
    b.put(L)
    L = b.layer()
    base = b.sy(34, 6)
    # equestrian statue (bronze) on the rostra
    for x in range(28, 35):
        px(L, x, base - 3, C.ochre[0])
    for x in range(28, 35):
        px(L, x, base - 4, C.ochre[2])
    for (x, y) in ((28, -2), (29, -1), (33, -2), (34, -1), (35, -5), (36, -6), (31, -6), (31, -7), (32, -8)):
        px(L, x, base + y, C.ochre[1])
    b.put(L)
    L = b.layer()
    for x in (14, 48):
        statue(L, x, b.sy(D - 10), h=6, mat="marble")
    b.put(L)
    L = b.layer()
    steps(L, 9, W - 10, b.sy(D - 4), b.sy(D - 2), mat="marble")
    b.put(L)


@recipe
def governor_palace(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    block(b, 1, 1, W - 2, 18, 16, mat="red", roof="hip", door_x=False)
    side_wing(b, 1, 19, 14, 38, 12, mat="red")
    side_wing(b, W - 15, 19, 14, 38, 12, mat="red")
    # peristyle garden with fountain
    L = b.layer()
    ground(b, L, 17, 22, W - 34, 32, "lawn")
    ground(b, L, 30, 32, 20, 10, "water")
    b.put(L, ol=False)
    tree(b, 21, b.sy(30), "bush")
    tree(b, 59, b.sy(30), "bush")
    tree(b, 22, b.sy(52), "cypress")
    tree(b, 58, b.sy(52), "cypress")
    L = b.layer()
    statue(L, 40, b.sy(38), h=5, mat="bronze", ped=False)
    b.put(L)
    # front wing with columned portico and pediment
    L = b.layer()
    f0, f1 = box(b, L, 1, 57, W - 2, 20, 14, mat="stucco", roof="hip")
    colonnade(L, 22, W - 23, f0, f1, mat="marble", step=5, back=C.earth[0])
    pediment(L, 22, W - 23, f0, ph=6, mat="marble")
    windows(L, 4, 18, f0 + 3, step=6)
    windows(L, W - 18, W - 5, f0 + 3, step=6)
    b.put(L)
    L = b.layer()
    steps(L, 26, W - 27, b.sy(D - 3, 2), b.sy(D - 2), mat="marble")
    banner(L, 16, b.sy(60, 22), C.tyrian[1])
    banner(L, W - 19, b.sy(60, 22), C.tyrian[1])
    b.put(L)


@recipe
def basilica(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    L = b.layer()
    f0, f1 = box(b, L, 2, 4, W - 4, 48, 12, mat="travertine", roof="hip")
    colonnade(L, 2, W - 3, f0, f1, mat="marble", step=6, back=C.earth[0])
    b.put(L)
    # raised clerestory nave
    L = b.layer()
    g0, g1 = box(b, L, 14, 10, W - 28, 30, 22, mat="travertine", roof="hip", z0=12)
    windows(L, 17, W - 18, g0 + 2, step=5, w=2, h=3, sill=False)
    b.put(L)
    L = b.layer()
    steps(L, 10, W - 11, b.sy(53), b.sy(D - 3), mat="marble")
    for x in (6, W - 10):
        statue(L, x, b.sy(D - 6), h=6)
    b.put(L)


@recipe
def curia(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "marble")
    L = b.layer()
    f0, f1 = box(b, L, 10, 2, 44, 30, 18, mat="brick", roof="gable", plinth=False)
    pediment(L, 10, 53, f0, ph=8, mat="travertine")
    # stucco-faced lower storey and great bronze doors
    face(L, 10, f1 - 7, 44, 8, "travertine", key="curia/low")
    rect(L, 28, f1 - 9, 8, 10, C.ochre[0])
    for x in range(28, 36):
        if x % 2:
            vline(L, x, f1 - 8, f1, C.earth[1])
    vline(L, 31, f1 - 9, f1, C.outline)
    hline(L, 27, 36, f1 - 10, C.marble[1])
    for x in (14, 44):
        rect(L, x, f0 + 3, 5, 6, C.outline)
        hline(L, x, x + 4, f0 + 8, C.travertine[1])
    b.put(L)
    L = b.layer()
    steps(L, 22, 41, b.sy(32), b.sy(D - 3), mat="marble")
    b.put(L)


@recipe
def mint(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    L = b.layer()
    f0, f1 = box(b, L, 2, 2, W - 4, 30, 15, mat="stone", roof="hip", ramp="old")
    for x in range(6, W - 6, 7):
        rect(L, x, f0 + 3, 2, 3, C.outline)
        vline(L, x, f0 + 3, f0 + 5, C.gray[2])           # bars
    door(L, W // 2, f1 - 1, h=7, w=6, leaf=C.gray[1])
    b.put(L)
    L = b.layer()
    chimney(L, 36, b.sy(6, 22), 6)
    b.put(L)
    L = b.layer()
    for k, x in enumerate((6, 10, 14)):                  # coin stacks
        for j in range(3 + k % 2):
            hline(L, x, x + 2, b.sy(D - 4) - j, C.gold if j % 2 == 0 else C.ochre[3])
    for k, x in enumerate((33, 37)):
        for j in range(3):
            hline(L, x, x + 2, b.sy(D - 4) - j, C.gray[3] if j % 2 == 0 else C.white)
    b.put(L)


@recipe
def triumphal_arch(b):
    W, D = b.W, b.D
    L = b.layer()
    ground(b, L, 1, 12, W - 2, 18, "paving")
    b.put(L, ol=False)
    L = b.layer()
    gy, d, z = 14, 12, 26
    top = b.sy(gy, z)
    rect(L, 3, top, W - 6, d, C.marble[0])               # attic top
    hline(L, 3, W - 4, top, C.white)
    f0 = top + d
    f1 = b.sy(gy + d - 1)
    face(L, 3, f0, W - 6, f1 - f0 + 1, "marble", key="arch/f")
    # attic with inscription
    hline(L, 3, W - 4, f0 + 6, C.marble[0])
    for x in range(10, W - 10, 2):
        px(L, x, f0 + 3, C.gray[1])
        if x % 4:
            px(L, x, f0 + 4, C.gray[1])
    # central arch (see-through) and side arches
    cx = W // 2

    def cut(xc, half, top_y):
        for y in range(top_y, f1 + 1):
            for x in range(xc - half, xc + half):
                dx = x + 0.5 - xc
                ay = top_y + half - math.sqrt(max(0, half * half - dx * dx))
                if y >= ay:
                    px(L, x, y, (0, 0, 0, 0))
    cut(cx, 6, f0 + 9)
    cut(9, 3, f0 + 15)
    cut(W - 9, 3, f0 + 15)
    for x in (4, 14, W - 16, W - 6):                     # engaged columns
        vline(L, x, f0 + 8, f1 - 1, C.white)
        vline(L, x + 1, f0 + 8, f1 - 1, C.marble[0])
    b.put(L)
    # gilded quadriga on top
    L = b.layer()
    qy = top + 4
    for i in range(4):
        x = cx - 6 + i * 3
        hline(L, x, x + 2, qy, C.ochre[0])
        px(L, x + 2, qy - 1, C.ochre[2])
        px(L, x, qy + 1, C.ochre[0])
        px(L, x + 2, qy + 1, C.ochre[0])
    rect(L, cx - 2, qy - 3, 4, 3, C.gold)
    px(L, cx, qy - 5, C.ochre[3])
    px(L, cx, qy - 4, C.gold)
    b.put(L)


# ------------------------------------------------------------ religion
@recipe
def shrine(b):
    L = b.layer()
    ground(b, L, 1, 2, 14, 13, "paving")
    b.put(L, ol=False)
    temple(b, 3, 2, 10, 11, 2, 5, step=6, stairs=3)
    altar(b, 8, 13, 4)


@recipe
def temple_(b):
    pass


def _temple(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    temple(b, 8, 2, 32, 38, 4, 10, step=5, stairs=8)
    altar(b, 24, 41, 6)
    tree(b, 4, b.sy(42), "cypress")
    tree(b, 44, b.sy(42), "cypress")


RECIPES["temple"] = _temple
del RECIPES["temple_"]


@recipe
def great_temple(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "marble")
    portico(b, 1, 1, W - 2, 8, 9, step=5)
    for x in (1, W - 7):
        side_wing(b, x, 9, 6, D - 14, 8, mat="travertine")
    temple(b, 16, 10, 48, 54, 6, 14, step=6, stairs=10, acroteria=True)
    altar(b, 40, 68, 8)
    L = b.layer()
    for x in (12, 66):
        statue(L, x, b.sy(D - 6), h=7, mat="bronze")
    b.put(L)


@recipe
def temple_of_jupiter(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "marble")
    portico(b, 1, 1, W - 2, 8, 10, step=6)
    temple(b, 20, 10, 88, 78, 9, 18, step=7, stairs=14, ramp="terracotta", acroteria=True, oculus=True)
    altar(b, 64, 94, 10)
    L = b.layer()
    for x in (8, 22, 106, 120):
        statue(L, x, b.sy(D - 6), h=8, mat="bronze" if x in (22, 106) else "marble")
    b.put(L)
    L = b.layer()
    steps(L, 20, W - 21, b.sy(D - 4), b.sy(D - 2), mat="marble")
    b.put(L)


# ------------------------------------------------------------ health / safety
@recipe
def physician(b):
    yard(b, 1, 1, 14, 14, "paving")
    block(b, 1, 1, 14, 9, 9, mat="stucco", roof="hip", win=False)
    L = b.layer()
    # rod of Asclepius sign
    vline(L, 13, b.sy(13, 9), b.sy(13), C.earth[1])
    for (dx, dz) in ((-1, 8), (1, 7), (-1, 6), (1, 5)):
        px(L, 13 + dx, b.sy(13, dz), C.grass[2])
    amphora(L, 3, b.sy(13), C.olive[2])
    b.put(L)


@recipe
def hospital(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    block(b, 1, 1, W - 2, 14, 12, mat="stucco", roof="hip", door_x=False)
    side_wing(b, 1, 15, 12, 30, 10)
    side_wing(b, W - 13, 15, 12, 30, 10)
    L = b.layer()
    ground(b, L, 13, 16, W - 26, 28, "lawn")
    for gy in range(19, 42, 5):                          # herb beds
        for x in range(16, W - 16):
            if (x // 3) % 2 == 0:
                px(L, x, b.sy(gy), C.grass[3] if x % 2 else C.olive[2])
    b.put(L, ol=False)
    L = b.layer()
    f0, f1 = box(b, L, 1, 45, W - 2, 16, 10, mat="stucco", roof="hip")
    door(L, W // 2, f1 - 1, h=6, w=4)
    windows(L, 4, W - 5, f0 + 3, step=6, skip=(W // 2 - 4, W // 2 + 3))
    b.put(L)
    L = b.layer()
    vline(L, W - 6, b.sy(D - 3, 12), b.sy(D - 3), C.earth[1])
    for (dx, dz) in ((-1, 11), (1, 10), (-1, 9), (1, 8)):
        px(L, W - 6 + dx, b.sy(D - 3, dz), C.grass[2])
    b.put(L)


def _steam(b, L, x, y):
    for i in range(4):
        px(L, x + (i % 2), y - i * 2, C.marble[1])
        if i % 2 == 0:
            px(L, x - 1 + i // 2, y - i * 2 - 1, C.white)


@recipe
def balneum(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    block(b, 1, 1, W - 2, 22, 11, mat="stucco", roof="hip", door_x=False)
    L = b.layer()
    dome(b, L, 30, 12, 8, 6, 7, 11, ramp=(C.gray[1], C.gray[2], C.gray[3], C.marble[0]))
    b.put(L)
    L = b.layer()
    f0, f1 = box(b, L, 1, 23, 16, 22, 8, mat="stucco", roof="gable")
    b.put(L)
    L = b.layer()
    ground(b, L, 21, 27, 22, 13, "water")
    hline(L, 20, 43, b.sy(26), C.marble[1])
    vline(L, 20, b.sy(26), b.sy(40), C.marble[1])
    b.put(L, ol=False)
    L = b.layer()
    _steam(b, L, 30, b.sy(12, 26))
    _steam(b, L, 10, b.sy(8, 18))
    b.put(L, ol=False)
    low_wall(b, 1, D - 2, W - 2, gap=(W // 2 - 3, W // 2 + 2))


@recipe
def thermae(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "marble")
    L = b.layer()
    f0, f1 = box(b, L, 4, 2, W - 8, 44, 18, mat="travertine", roof="flat")
    colonnade(L, 4, W - 5, f0, f1, mat="marble", step=6, back=C.earth[0])
    b.put(L)
    L = b.layer()
    dome(b, L, 48, 22, 18, 13, 14, 18, ramp=(C.gray[1], C.gray[2], C.gray[3], C.marble[0]),
         oculus=True)
    b.put(L)
    for cx in (18, 78):
        L = b.layer()
        dome(b, L, cx, 22, 9, 7, 8, 18, ramp=(C.gray[1], C.gray[2], C.gray[3], C.marble[0]))
        b.put(L)
    L = b.layer()
    _steam(b, L, 48, b.sy(22, 18) - 30)
    _steam(b, L, 18, b.sy(22, 18) - 16)
    _steam(b, L, 78, b.sy(22, 18) - 16)
    b.put(L, ol=False)
    # palaestra with natatio (swimming pool) and porticoes
    for x in (2, W - 10):
        side_wing(b, x, 47, 8, D - 52, 9, mat="travertine", ramp="terracotta")
    L = b.layer()
    ground(b, L, 10, 50, W - 20, 30, "sand")
    ground(b, L, 22, 56, W - 44, 18, "water", border=(C.marble[1], C.marble[0]))
    b.put(L, ol=False)
    L = b.layer()
    for x in (16, W - 18):
        statue(L, x, b.sy(70), h=6)
    b.put(L)
    low_wall(b, 1, D - 2, W - 2, z=3, mat="travertine", gap=(W // 2 - 5, W // 2 + 4))


@recipe
def vigiles_post(b):
    yard(b, 1, 1, 14, 14, "paving")
    L = b.layer()
    f0, f1 = box(b, L, 1, 1, 14, 9, 10, mat="stucco", roof="hip")
    door(L, 8, f1 - 1, h=6, w=4, leaf=C.roman_red[1])
    b.put(L)
    L = b.layer()
    for x in (2, 5):                                   # fire buckets
        rect(L, x, b.sy(13, 2), 2, 2, C.earth[2])
        px(L, x, b.sy(13, 2), C.sea[2])
    vline(L, 12, b.sy(13, 9), b.sy(13), C.earth[2])     # ladder
    vline(L, 14, b.sy(13, 9), b.sy(13), C.earth[1])
    for z in range(1, 9, 2):
        px(L, 13, b.sy(13, z), C.earth[3])
    b.put(L)


@recipe
def guard_post(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    block(b, 1, 1, 16, 12, 9, mat="stone", roof="hip", win=False)
    tower_box(b, 19, 6, 11, 11, 20, mat="stone")
    L = b.layer()
    banner(L, 6, b.sy(18, 10), C.roman_red[2])
    fence(L, 2, b.sy(D - 2), W - 3, b.sy(D - 2))
    b.put(L)


@recipe
def militia_post(b):
    yr, f = workshop(b, mat="stucco", roof="hip", yard_kind="dirt", frac=0.5)
    L = b.layer()
    hline(L, 3, 14, b.sy(24, 6), C.earth[2])
    for x in range(4, 14, 2):
        vline(L, x, b.sy(24, 8), b.sy(24), C.earth[1])
        px(L, x, b.sy(24, 9), C.gray[3])
    banner(L, 22, b.sy(20, 10), C.roman_red[2])
    b.put(L)
    finish_front(b)


# ------------------------------------------------------------ entertainment
@recipe
def tavern(b):
    yr, f = workshop(b, mat="ochre", roof="hip", yard_kind="paving", frac=0.5)
    L = b.layer()
    # L-shaped counter with embedded dolia
    rect(L, 3, b.sy(24, 3), 16, 4, C.marble[0])
    hline(L, 3, 18, b.sy(24, 3), C.marble[1])
    for x in (6, 11, 16):
        px(L, x, b.sy(24, 3), C.outline)
    rect(L, 3, b.sy(24, 3), 3, 9, C.marble[0])
    hline(L, 3, 5, b.sy(24, 3) - 6, C.marble[1])
    b.put(L)
    L = b.layer()
    amphorae(L, 21, b.sy(22), 3, C.roman_red[1])
    rect(L, 22, b.sy(28, 2), 7, 1, C.earth[3])          # bench
    hline(L, 22, 28, b.sy(28, 1), C.earth[1])
    b.put(L)
    L = b.layer()
    vline(L, 28, b.sy(17, 13), b.sy(17, 8), C.earth[1])  # hanging sign
    rect(L, 26, b.sy(17, 12), 4, 3, C.roman_red[2])
    b.put(L)
    finish_front(b, mat="ochre")


def _cavea_rings(b, L, cx, cgy, rx, ry, rings, z_out, z_in, half="north", inner=0.38):
    """Seating bowl: concentric rings stepping down from z_out to z_in."""
    for k in range(rings):
        t = k / float(rings)
        sx = rx * (1 - t * (1 - inner))
        sy_ = ry * (1 - t * (1 - inner))
        z = z_out - (z_out - z_in) * t
        cy = b.sy(cgy, z)
        m = ellipse_mask(cx, cy, sx, sy_)
        col = C.travertine[2] if k % 2 == 0 else C.marble[0]
        if k == 0:
            col = C.travertine[1]
        for (x, y) in m:
            if half == "north" and y > cy:
                continue
            c = col
            ang = math.atan2(y + 0.5 - cy, x + 0.5 - cx)
            if k > 0 and int((ang + 4) * 7) % 5 == 0:
                c = C.travertine[0]                       # radial aisles
            px(L, x, y, c)
        # riser shadow on the inner edge
        for (x, y) in m:
            if half == "north" and y > cy:
                continue
            if (x, y + 1) not in m:
                px(L, x, y, C.travertine[0])


def _ring_facade(b, L, cx, cgy, rx, ry, z, tiers=3, mat="travertine"):
    """Outer wall of an oval building: south half facade with arcades."""
    m = MAT[mat]
    cy_top = b.sy(cgy, z)
    cy_bot = b.sy(cgy)
    for x in range(int(cx - rx), int(cx + rx) + 1):
        dx = (x + 0.5 - cx) / rx
        if abs(dx) > 1:
            continue
        dy = ry * math.sqrt(1 - dx * dx)
        ya = int(round(cy_top + dy))
        yb = int(round(cy_bot + dy))
        shade = m["lit"] if dx < -0.55 else (m["base"] if dx < 0.4 else m["shade"])
        arc_len = math.asin(max(-1, min(1, dx))) * rx
        p = int(arc_len + 400) % 7
        th = (yb - ya + 1) / float(tiers)
        for y in range(ya, yb + 1):
            tier_pos = (y - ya) / th
            ti = int(tier_pos)
            local = (tier_pos - ti) * th
            c = shade
            if local < 1.2:
                c = m["eave"] if dx > -0.55 else m["base"]       # cornice band
            elif p >= 2 and local > 2.5 and local < th - 0.6:
                c = C.outline if dx > -0.3 else C.travertine[0]  # arch opening
                if local < 3.5 and p in (2, 6):
                    c = shade                                   # arch shoulder
            px(L, x, y, c)


@recipe
def theater(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    L = b.layer()
    cx, cgy = W // 2, 52
    # outer wall top (high back of the cavea)
    _cavea_rings(b, L, cx, cgy, 38, 44, 9, 20, 3)
    ground_ellipse(b, L, cx, cgy, 12, 10, "marble", z=0)      # orchestra
    b.put(L)
    # stage building (scaena) along the south
    L = b.layer()
    f0, f1 = box(b, L, 6, 54, W - 12, 12, 12, mat="travertine", roof="flat")
    arcade(L, 6, W - 7, f0, f1, mat="travertine", period=7, opening=C.outline)
    b.put(L)
    L = b.layer()
    ground(b, L, 14, 50, W - 28, 4, "wood", z=12)               # stage deck
    b.put(L, ol=False)
    L = b.layer()
    for x in (8, W - 10):
        banner(L, x, b.sy(54, 22), C.roman_red[2])
    b.put(L)


@recipe
def odeon(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    L = b.layer()
    cylinder(b, L, W // 2, 26, 28, 17, 0, 12, mat="travertine", arches=6)
    b.put(L)
    L = b.layer()
    dome(b, L, W // 2, 26, 29, 18, 6, 12, ramp=ROOF["terracotta"], ribs=12)
    b.put(L)
    L = b.layer()
    f0, f1 = box(b, L, 14, 46, W - 28, 12, 13, mat="marble", roof="gable")
    colonnade(L, 14, W - 15, f0, f1, mat="marble", step=6)
    pediment(L, 14, W - 15, f0, ph=5)
    b.put(L)


def _oval_arena(b, cx, cgy, rx, ry, z, tiers, rings, arena_frac=0.42, velarium=False):
    L = b.layer()
    _ring_facade(b, L, cx, cgy, rx, ry, z, tiers=tiers)
    b.put(L)
    L = b.layer()
    _cavea_rings(b, L, cx, cgy, rx, ry, rings, z, 6, half="all", inner=arena_frac + 0.08)
    b.put(L)
    L = b.layer()
    am = ellipse_mask(cx, b.sy(cgy, 0) - 2, rx * arena_frac, ry * arena_frac)
    for (x, y) in am:
        px(L, x, y, C.ochre[2] if hash2(x, y, 5) % 6 else C.ochre[1])
    ys = {}
    for (x, y) in am:
        ys[x] = min(ys.get(x, 999), y)
    for x, y in ys.items():
        px(L, x, y, C.ochre[0])                          # podium shadow
    for side in (-1, 1):                                  # gates at the ends
        gx = int(cx + side * rx * arena_frac) - (2 if side > 0 else 0)
        rect(L, gx, b.sy(cgy) - 4, 3, 4, C.outline)
    b.put(L, ol=False)
    if velarium:
        L = b.layer()
        for a in range(0, 360, 10):
            t = math.radians(a)
            x = int(cx + rx * 0.98 * math.cos(t))
            y = int(b.sy(cgy, z) + ry * 0.98 * math.sin(t))
            if y < b.sy(cgy, z) + 2:
                vline(L, x, y - 3, y - 1, C.earth[1])
        b.put(L)


@recipe
def amphitheater(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    _oval_arena(b, W // 2, 66, 52, 44, 26, 2, 7)


@recipe
def colosseum(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "marble")
    _oval_arena(b, W // 2, 96, 92, 70, 44, 4, 11, velarium=True)
    L = b.layer()
    for x in (10, W - 12):
        statue(L, x, b.sy(D - 4), h=8, mat="bronze")
    b.put(L)


def _circus(b, stand_z, decorate=False):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    nd = D // 5                                          # stand depth
    # north stand: tiered seats rising to the north
    L = b.layer()
    for k in range(nd):
        gy = 1 + k
        z = stand_z - k * stand_z // (nd + 2)
        y = b.sy(gy, z)
        c = C.travertine[2] if k % 2 == 0 else C.marble[0]
        hline(L, 14, W - 2, y, c)
    f0 = b.sy(nd, stand_z - (nd - 1) * stand_z // (nd + 2))
    for y in range(f0 + 1, b.sy(nd) + 1):
        hline(L, 14, W - 2, y, C.travertine[1] if y < b.sy(nd) else C.travertine[0])
    b.put(L)
    # carceres (starting gates) at the west end
    L = b.layer()
    f0, f1 = box(b, L, 1, 1, 13, D - 2, stand_z, mat="travertine", roof="flat")
    b.put(L)
    L = b.layer()
    for k in range(6):
        y = b.sy(nd + 4 + k * (D - 2 * nd - 8) // 6, 0)
        rect(L, 12, y - 4, 2, 4, C.outline)
    b.put(L, ol=False)
    # track
    L = b.layer()
    ground(b, L, 14, nd + 1, W - 16 - nd, D - 2 * nd - 2, "sand")
    tm = ground_ellipse(b, L, W - 2 - nd * 2, D // 2, nd * 2, D // 2 - nd - 1, "sand")
    b.put(L, ol=False)
    # sphendone (curved east stand)
    L = b.layer()
    cx = W - 2 - nd * 2
    outer = ellipse_mask(cx, b.sy(D // 2, stand_z // 2), nd * 2 + nd - 1, D // 2 - 1)
    inner = ellipse_mask(cx, b.sy(D // 2, stand_z // 2), nd * 2, D // 2 - nd - 1)
    for (x, y) in outer - inner:
        if x > cx:
            px(L, x, y, C.travertine[2] if (x + y) % 4 else C.marble[0])
    b.put(L)
    # spina with obelisk and metae
    L = b.layer()
    sy0 = D // 2 - 2
    x0, x1 = 30, W - 2 - nd * 2 - 8
    box(b, L, x0, sy0, x1 - x0, 4, 3, mat="marble", roof="flat")
    b.put(L)
    L = b.layer()
    for mx in (x0 - 4, x1 + 1):                        # metae: three gilded cones
        for k in range(3):
            px(L, mx + k, b.sy(sy0 + 2, 6), C.gold)
            px(L, mx + k, b.sy(sy0 + 2, 5), C.ochre[0])
            vline(L, mx + k, b.sy(sy0 + 2, 4), b.sy(sy0 + 2), C.ochre[0] if k == 2 else C.ochre[2])
    ox = (x0 + x1) // 2
    hgt = min(26, b.E + sy0 - 2)
    for z in range(3, hgt):                            # obelisk
        px(L, ox, b.sy(sy0 + 2, z), C.marble[1])
        px(L, ox + 1, b.sy(sy0 + 2, z), C.marble[0] if z < hgt - 2 else C.marble[1])
    px(L, ox, b.sy(sy0 + 2, hgt), C.gold)
    for k in range(4 if decorate else 2):              # lap counters
        x = x0 + 10 + k * 8
        px(L, x, b.sy(sy0 + 2, 5), C.gold)
        px(L, x + 1, b.sy(sy0 + 2, 5), C.gold)
    b.put(L)
    # south stand: seats + outer facade with arcade
    L = b.layer()
    sz = stand_z
    g0 = D - nd - 1
    top = b.sy(g0, sz)
    for k in range(nd):
        hline(L, 14, W - 2 - nd, top + k, C.travertine[2] if k % 2 else C.marble[0])
    hline(L, 14, W - 2 - nd, top, C.travertine[0])
    arcade(L, 14, W - 2 - nd, top + nd, b.sy(D - 2), mat="travertine", period=7,
           opening=C.outline, tiers=2 if sz > 20 else 1)
    b.put(L)
    if decorate:
        # imperial box (pulvinar) on the north stand and coloured awnings
        L = b.layer()
        px_ = W // 2
        box(b, L, px_ - 10, 2, 20, nd - 2, stand_z + 6, mat="marble", roof="gable",
            ramp="bronze")
        b.put(L)
        L = b.layer()
        for k, x in enumerate(range(20, W - 30, 18)):
            if abs(x - px_) < 16:
                continue
            awning(L, x, b.sy(2, stand_z + 3), 10, 2,
                   (C.roman_red[2], C.marble[1]) if k % 2 else (C.tyrian[1], C.marble[1]), posts=0)
        b.put(L)


@recipe
def circus(b):
    _circus(b, 16)


@recipe
def circus_maximus(b):
    _circus(b, 26, decorate=True)


# ------------------------------------------------------------ education
@recipe
def school(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    block(b, 1, 1, W - 2, 12, 9, mat="stucco", roof="hip")
    L = b.layer()
    # pergola with awning over benches
    for x in (3, W - 4):
        vline(L, x, b.sy(26, 8), b.sy(26), C.earth[1])
    awning(L, 3, b.sy(16, 8), W - 6, 10, (C.ochre[2], C.marble[1]), posts=0)
    b.put(L)
    L = b.layer()
    for gy in (20, 24):
        hline(L, 6, W - 7, b.sy(gy, 2), C.earth[3])
        hline(L, 6, W - 7, b.sy(gy, 1), C.earth[1])
    b.put(L)
    finish_front(b)


@recipe
def library(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "marble")
    L = b.layer()
    f0, f1 = box(b, L, 3, 2, W - 6, 30, 15, mat="marble", roof="gable")
    colonnade(L, 3, W - 4, f0, f1, mat="marble", step=6, back=C.earth[0], entab=3)
    pediment(L, 3, W - 4, f0, ph=7, oculus=True)
    b.put(L)
    L = b.layer()
    steps(L, 10, W - 11, b.sy(32), b.sy(36), mat="marble")
    b.put(L)
    tree(b, 7, b.sy(44), "bush")
    tree(b, W - 8, b.sy(44), "bush")
    L = b.layer()
    statue(L, W // 2, b.sy(43), h=6)
    b.put(L)


@recipe
def academy(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "paving")
    portico(b, 1, 1, W - 2, 10, 11, step=5)
    for x in (1, W - 8):
        side_wing(b, x, 11, 7, 34, 9, mat="stucco")
    L = b.layer()
    ground(b, L, 10, 14, W - 20, 30, "lawn")
    b.put(L, ol=False)
    L = b.layer()
    # exedra: semicircular marble bench
    for (x, y) in ellipse_mask(W // 2, b.sy(30, 2), 12, 8) - ellipse_mask(W // 2, b.sy(30, 2), 10, 6):
        if y < b.sy(30, 2):
            px(L, x, y, C.marble[1])
    b.put(L)
    tree(b, 16, b.sy(42), "cypress")
    tree(b, W - 16, b.sy(42), "cypress")
    L = b.layer()
    statue(L, W // 2, b.sy(28), h=5, mat="bronze", ped=True)
    b.put(L)
    L = b.layer()
    f0, f1 = box(b, L, 1, 45, W - 2, 16, 10, mat="stucco", roof="hip")
    colonnade(L, 18, W - 19, f0, f1, mat="marble", step=5, back=C.earth[0])
    b.put(L)


# ------------------------------------------------------------ military
@recipe
def gate(b):
    W, D = b.W, b.D
    tower_box(b, 1, 1, 10, 13, 15, mat="stone")
    tower_box(b, W - 11, 1, 10, 13, 15, mat="stone")
    L = b.layer()
    f0, f1 = box(b, L, 11, 3, W - 22, 9, 12, mat="stone", roof="flat")
    for x in range(11, W - 11):
        if (x - 11) % 3 != 2:
            px(L, x, b.sy(3, 12) - 1, C.gray[3])
    # archway
    cx = W // 2
    for y in range(f0 + 2, f1 + 1):
        for x in range(cx - 4, cx + 4):
            dx = x + 0.5 - cx
            if y >= f0 + 2 + 4 - math.sqrt(max(0, 16 - dx * dx)):
                px(L, x, y, (0, 0, 0, 0))
    b.put(L)


@recipe
def tower(b):
    W, D = b.W, b.D
    tower_box(b, 4, 6, 24, 22, 22, mat="stone")
    L = b.layer()
    door(L, 16, b.sy(27), h=6, w=4, kind="dark")
    b.put(L)


@recipe
def barracks(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    block(b, 1, 1, W - 2, 14, 11, mat="stucco", roof="hip", ramp="old")
    side_wing(b, 1, 15, 12, 40, 9, ramp="old")
    side_wing(b, W - 13, 15, 12, 40, 9, ramp="old")
    L = b.layer()
    # aquila standard and training posts on the parade ground
    vline(L, W // 2, b.sy(30, 16), b.sy(30), C.earth[1])
    rect(L, W // 2 - 2, b.sy(30, 16), 5, 2, C.gold)
    px(L, W // 2, b.sy(30, 18), C.gold)
    rect(L, W // 2 + 1, b.sy(30, 12), 4, 4, C.roman_red[2])
    for x in (20, 42):
        vline(L, x, b.sy(42, 7), b.sy(42), C.earth[2])
        px(L, x, b.sy(42, 7), C.earth[3])
    b.put(L)
    low_wall(b, 1, D - 2, W - 2, z=4, mat="stone", gap=(W // 2 - 4, W // 2 + 3))


def _tents(b, L, x0, gy0, cols, rows, dx=12, dgy=10):
    for r_ in range(rows):
        for c_ in range(cols):
            x = x0 + c_ * dx
            gy = gy0 + r_ * dgy
            top = b.sy(gy, 5)
            for k in range(6):
                y = top + k
                half = 1 + k
                for xx in range(x + 4 - min(half, 4), x + 4 + min(half, 4)):
                    c = C.marble[1] if xx < x + 4 else C.travertine[2]
                    if k == 5:
                        c = C.travertine[0]
                    px(L, xx, y, c)
            px(L, x + 4, top + 4, C.outline)
            px(L, x + 4, top + 5, C.outline)


@recipe
def fort(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    # north wall
    tower_box(b, 1, 1, W - 2, 6, 12, mat="stone", slits=False)
    for x in (1, W - 7):
        L = b.layer()
        box(b, L, x, 7, 6, D - 16, 12, mat="stone", roof="flat")
        b.put(L)
    L = b.layer()
    box(b, L, 32, 12, 32, 18, 12, mat="stucco", roof="hip")           # principia
    b.put(L)
    L = b.layer()
    _tents(b, L, 10, 36, 6, 4, dx=13, dgy=11)
    b.put(L)
    L = b.layer()
    banner(L, W // 2, b.sy(32, 18), C.roman_red[2])
    b.put(L)
    # south wall with gate, corner towers
    L = b.layer()
    f0, f1 = box(b, L, 1, D - 9, W - 2, 7, 12, mat="stone", roof="flat")
    for x in range(1, W - 1):
        if (x - 1) % 3 != 2:
            px(L, x, b.sy(D - 9, 12) - 1, C.gray[3])
    cx = W // 2
    for y in range(f0 + 2, f1 + 1):
        for x in range(cx - 5, cx + 5):
            dx = x + 0.5 - cx
            if y >= f0 + 2 + 5 - math.sqrt(max(0, 25 - dx * dx)):
                px(L, x, y, C.outline)
    b.put(L)
    for (x, gy) in ((1, 1), (W - 13, 1), (1, D - 13), (W - 13, D - 13), (cx - 18, D - 13), (cx + 6, D - 13)):
        tower_box(b, x, gy, 12, 11, 17, mat="stone")


@recipe
def training_ground(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "sand")
    block(b, 2, 2, 22, 12, 9, mat="wood", roof="shed", ramp="wood", win=False)
    L = b.layer()
    for gy in range(20, 66, 12):                        # pali (training posts)
        for x in range(30, 72, 12):
            vline(L, x, b.sy(gy, 8), b.sy(gy), C.earth[2])
            px(L, x + 1, b.sy(gy, 8), C.earth[1])
    for (x, gy) in ((8, 40), (8, 56)):                  # archery targets
        e = ellipse_mask(x + 3, b.sy(gy, 5), 3.5, 3.5)
        for (xx, yy) in e:
            px(L, xx, yy, C.ochre[2])
        px(L, x + 3, b.sy(gy, 5), C.roman_red[2])
        px(L, x + 2, b.sy(gy, 5), C.roman_red[2])
    b.put(L)
    L = b.layer()
    for x in range(1, W - 1, 2):                        # palisade (front and back)
        vline(L, x, b.sy(D - 2, 5), b.sy(D - 2), C.earth[2] if x % 4 else C.earth[1])
        px(L, x, b.sy(D - 2, 6), C.earth[3])
    b.put(L)


@recipe
def stables(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    L = b.layer()
    f0, f1 = box(b, L, 1, 1, W - 2, 18, 11, mat="wood", roof="hip", ramp="old")
    for x in range(4, W - 6, 8):
        rect(L, x, f1 - 6, 5, 7, C.earth[0])
        hline(L, x, x + 4, f1 - 3, C.earth[2])
    b.put(L)
    L = b.layer()
    for (x, gy, c) in ((14, 32, C.earth[2]), (36, 38, C.earth[1])):   # horses
        base = b.sy(gy)
        rect(L, x, base - 5, 9, 3, c)
        hline(L, x, x + 8, base - 5, C.earth[3] if c == C.earth[2] else C.earth[2])
        rect(L, x + 8, base - 8, 2, 4, c)
        px(L, x + 10, base - 8, c)
        px(L, x + 8, base - 9, C.earth[0])
        for lx in (x, x + 2, x + 6, x + 8):
            vline(L, lx, base - 2, base, C.earth[0])
        px(L, x - 1, base - 4, C.earth[0])
    pile(L, 54, b.sy(D - 4), 5, 5, (C.ochre[0], C.ochre[1], C.ochre[2]))
    b.put(L)
    L = b.layer()
    fence(L, 2, b.sy(D - 2), W - 3, b.sy(D - 2))
    b.put(L)


@recipe
def siege_workshop(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "dirt")
    block(b, 1, 1, 30, 16, 12, mat="wood", roof="gable", ramp="wood", win=False, door_kind="dark")
    L = b.layer()
    # onager frame
    x, base = 36, b.sy(36)
    rect(L, x, base - 3, 18, 3, C.earth[2])
    hline(L, x, x + 17, base - 3, C.earth[3])
    line(L, x + 4, base - 3, x + 8, base - 12, C.earth[2])
    line(L, x + 12, base - 3, x + 8, base - 12, C.earth[1])
    line(L, x + 8, base - 6, x + 15, base - 14, C.earth[3])
    rect(L, x + 14, base - 16, 3, 2, C.earth[1])
    for wx in (x + 2, x + 13):
        rect(L, wx, base - 1, 3, 2, C.earth[0])
    logs(L, 4, b.sy(D - 4), w=12, rows=2)
    planks(L, 18, b.sy(D - 4), w=10, rows=2)
    b.put(L)


# ------------------------------------------------------------ monuments
@recipe
def lighthouse(b):
    W, D = b.W, b.D
    L = b.layer()
    ground(b, L, 1, 1, W - 2, D - 2, "gravel")
    b.put(L, ol=False)
    L = b.layer()
    box(b, L, 8, 24, 48, 34, 26, mat="travertine", roof="flat")
    b.put(L)
    L = b.layer()
    f0, f1 = box(b, L, 18, 32, 28, 20, 44, mat="travertine", roof="flat", z0=26)
    windows(L, 22, 42, f0 + 4, step=6, w=2, h=3, sill=False)
    b.put(L)
    L = b.layer()
    cylinder(b, L, 32, 42, 8, 6, 44, 56, mat="marble", top=C.marble[0])
    b.put(L)
    L = b.layer()
    fy = b.sy(42, 56)
    for k in range(6):
        hline(L, 29 + k // 2, 34 - k // 2, fy - k, C.gold if k % 2 else C.roof_terracotta[3])
    px(L, 31, fy - 6, C.white)
    b.put(L)
    L = b.layer()
    statue(L, 12, b.sy(24, 26) + 1, h=6, mat="bronze", ped=False)
    statue(L, 52, b.sy(24, 26) + 1, h=6, mat="bronze", ped=False)
    b.put(L)


@recipe
def grand_aqueduct(b):
    W, D = b.W, b.D
    L = b.layer()
    gy, d, z = 12, 8, 28
    top = b.sy(gy, z)
    rect(L, 1, top, W - 2, d, C.travertine[1])               # channel cover
    hline(L, 1, W - 2, top, C.marble[0])
    hline(L, 1, W - 2, top + 3, C.sea[2])                    # open specus
    hline(L, 1, W - 2, top + 4, C.sea[1])
    hline(L, 1, W - 2, top + d - 1, C.travertine[0])
    f0 = top + d
    f1 = b.sy(gy + d - 1)
    arcade(L, 1, W - 2, f0, f1, mat="travertine", period=10, opening=None, tiers=2, pier=3)
    b.put(L)


@recipe
def pantheon(b):
    W, D = b.W, b.D
    yard(b, 1, 1, W - 2, D - 2, "marble")
    L = b.layer()
    cylinder(b, L, 64, 50, 42, 30, 0, 22, mat="travertine", arches=0)
    b.put(L)
    L = b.layer()
    for k in range(3):                                      # stepped rings at the dome base
        for (x, y) in ellipse_mask(64, b.sy(50, 22 + k), 42 - k * 2, 30 - k * 2):
            px(L, x, y, (C.marble[0], C.travertine[2], C.marble[0])[k])
    b.put(L)
    L = b.layer()
    dome(b, L, 64, 50, 37, 26, 15, 25, ramp=(C.gray[1], C.gray[2], C.gray[3], C.marble[0]),
         oculus=True, ribs=10)
    b.put(L)
    # portico: transitional block + pedimented colonnade with 8 columns
    L = b.layer()
    box(b, L, 40, 76, 48, 10, 26, mat="travertine", roof="gable")
    b.put(L)
    temple(b, 30, 84, 68, 34, 2, 20, step=8, stairs=8, acroteria=False)
    L = b.layer()
    for x in (12, W - 14):                                  # obelisk + fountain
        pass
    ground(b, L, 50, 124, 28, 10, "water", border=(C.marble[1], C.marble[0]))
    b.put(L)
    L = b.layer()
    for z in range(2, 22):
        px(L, 64, b.sy(129, z), C.marble[1])
        px(L, 65, b.sy(129, z), C.marble[0])
    px(L, 64, b.sy(129, 22), C.gold)
    b.put(L)
