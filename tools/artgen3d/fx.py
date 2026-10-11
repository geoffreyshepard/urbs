"""Effects and vehicles as iso sprites (family "fx").

Free sprites (anchor = ground contact at the origin): carts, ships (waterline centre), fire and
smoke animation frames.  Ground-attached 1x1 tiles (anchor = the tile's bottom corner): rubble,
foundation and scaffold, drawn once per footprint tile by the game.
The flat marks (white, dot, tile, select_*, plague_mark, alert) come from fx_flat.py.
"""
import math
import random

import kit
import people

DIRS = people.DIRS


def mats():
    m = kit.material
    return {
        "flame_core": m("f_core", "#000000", kind="flat", bump=0, emit="#ffd040", emit_strength=2.2),
        "flame": m("f_flame", "#000000", kind="flat", bump=0, emit="#ff7000", emit_strength=1.5),
        "flame_red": m("f_red", "#000000", kind="flat", bump=0, emit="#ff4a00", emit_strength=0.9),
        "ember": m("f_ember", "#000000", kind="flat", bump=0, emit="#ffb030", emit_strength=4.0),
        "smoke": m("f_smoke", "#8e8a84", kind="flat", rough=1.0, bump=0, alpha=0.42),
        "smoke_dark": m("f_smoke_dark", "#4e4a46", kind="flat", rough=1.0, bump=0, alpha=0.5),
        "char": m("f_char", "#2a231e", "#3a302a", scale=20, bump=0.4),
        "hull": m("f_hull", "#5a3b24", "#4a301d", kind="stripes", stripe=0.03, bump=0.1),
        "hull_band": m("f_hull_band", "#9b3a24", kind="flat", bump=0),
        "deck": m("f_deck", "#a07a4e", "#8a6640", kind="stripes", stripe=0.04, bump=0.1),
        "sail": m("f_sail", "#efe4cc", "#b23a26", kind="stripes", stripe=0.09, bump=0, rough=0.95),
        "rope": m("f_rope", "#cdb58a", kind="flat", bump=0),
        "pole": m("f_pole", "#9a7650", "#7f5f3e", scale=18, bump=0.15),
    }


def rig_for(d):
    return people.Rig(DIRS[d])


# ---------------------------------------------------------------- vehicles

def cart(d):
    def build(M):
        P = people.mats()
        rig = rig_for(d)
        people.shadow(P, 0.22)
        before = set(bpy_objects())
        kit.cart(-0.15, 0, M, load=True)
        for ob in bpy_objects():
            if ob not in before:
                rig.add(ob, ob.location)
        people.horse(rig, P, 0.6, mat="donkey", scale=0.72)
        # move the donkey ahead of the shafts
        for ob in rig.root.children:
            if ob.name.split(".")[0] in ("barrel", "neck", "hhead", "mane", "tail", "hleg", "hoof"):
                ob.location.x += 0.42
        rig.done()
    return build


def bpy_objects():
    import bpy
    return [ob for ob in bpy.context.scene.objects if ob.type == "MESH"]


def ship(d):
    def build(M):
        F = mats()
        rig = rig_for(d)
        L, W = 0.72, 0.2  # half length, half beam
        # hull: a lofted boat from stations along x (z=0 is the waterline)
        st = [(-1.0, 0.45, 0.30), (-0.8, 0.85, 0.20), (-0.4, 1.0, 0.14), (0.2, 1.0, 0.14), (0.7, 0.75, 0.17),
              (1.0, 0.15, 0.24)]
        v, f = [], []
        for (t, wk, top) in st:
            x = t * L
            w = W * wk
            v += [(x, -w, top), (x, -w * 0.75, -0.02), (x, w * 0.75, -0.02), (x, w, top)]
        for i in range(len(st) - 1):
            a, b = 4 * i, 4 * (i + 1)
            for k in range(3):
                f.append((a + k, b + k, b + k + 1, a + k + 1))
        f.append((0, 1, 2, 3))
        n = 4 * (len(st) - 1)
        f.append((n + 3, n + 2, n + 1, n))
        rig.add(kit.mesh("hull", v, f, F["hull"], smooth=False))
        # deck and a coloured band along the gunwale
        rig.add(kit.box(-L * 0.85, -W * 0.8, 0.1, L * 0.75, W * 0.8, 0.12, F["deck"], bevel=0, name="deck"))
        for sgn in (-1, 1):
            rig.add(kit.box(-L * 0.8, sgn * W * 0.98 - 0.008, 0.11, L * 0.6, sgn * W * 0.98 + 0.008, 0.15,
                            F["hull_band"], bevel=0, name="band"))
        # stern curl and a small deckhouse
        rig.add(kit.blob(-L * 1.0, 0, 0.32, 0.05, 0.05, 0.09, F["hull"], name="sternpost"))
        rig.add(kit.box(-L * 0.75, -0.09, 0.12, -L * 0.45, 0.09, 0.24, M["timber_light"], name="house"))
        rig.add(kit.box(-L * 0.78, -0.1, 0.24, -L * 0.42, 0.1, 0.26, M["roof"], bevel=0, name="houseroof"))
        # mast, yard and square sail
        rig.add(kit.cylinder(0.05, 0, 0.12, 0.95, 0.018, F["pole"], n=8, name="mast"))
        rig.add(kit.cylinder(0.88, 0.05, -0.36, 0.36, 0.012, F["pole"], n=8, axis="y", name="yard"))
        sail = kit.mesh("sail", [(0.07, -0.34, 0.86), (0.07, 0.34, 0.86), (0.11, 0.32, 0.42), (0.11, -0.32, 0.42)],
                        [(0, 1, 2, 3)], F["sail"], solidify=0.01)
        rig.add(sail)
        # amphorae as cargo
        for i, (x, y) in enumerate([(0.25, -0.08), (0.32, 0.06), (0.4, -0.05), (-0.1, 0.07), (-0.18, -0.06)]):
            ob = kit.amphora(x, y, M, 0.7)
        for ob in bpy_objects():
            if ob.parent is None and ob.name.split(".")[0] not in ("rig",):
                rig.add(ob, ob.location)
        # oars
        for sgn in (-1, 1):
            for i in range(5):
                x = -0.25 + i * 0.12
                oar = kit.box(-0.006, 0, -0.006, 0.006, 0.26, 0.006, F["pole"], bevel=0, name="oar")
                rig.add(oar, (x, sgn * W * 0.9, 0.12), (math.radians(-28 if sgn > 0 else 208), 0, 0))
        rig.done()
    return build


# ---------------------------------------------------------------- fire and smoke

def tongue(x, y, h, r, mat, lean=(0.0, 0.0), name="tongue"):
    """A flame tongue: a teardrop that narrows to a point and leans with the draught."""
    n = 10
    rings = [(0.0, 0.55), (0.18, 1.0), (0.42, 0.8), (0.7, 0.42), (0.88, 0.16)]
    v = []
    for (t, k) in rings:
        for i in range(n):
            a = 2 * math.pi * i / n
            v.append((x + lean[0] * t + r * k * math.cos(a), y + lean[1] * t + r * k * math.sin(a), h * t))
    v.append((x + lean[0], y + lean[1], h))
    f = [tuple(reversed(range(n)))]
    for j in range(len(rings) - 1):
        for i in range(n):
            a, b = j * n + i, j * n + (i + 1) % n
            f.append((a, b, b + n, a + n))
    top = len(v) - 1
    base = (len(rings) - 1) * n
    f += [(base + i, base + (i + 1) % n, top) for i in range(n)]
    ob = kit.mesh(name, v, f, mat)
    for p in ob.data.polygons:
        p.use_smooth = True
    return ob


def flame(F, x, y, h, r, k):
    lean = (0.04 * k, -0.02 * k)
    tongue(x, y, h, r, F["flame_red"], lean, "flame_outer")
    tongue(x + 0.015, y - 0.015, h * 0.82, r * 0.7, F["flame"], lean, "flame")
    tongue(x + 0.03, y - 0.03, h * 0.5, r * 0.42, F["flame_core"], lean, "flame_core")


def fire(frame):
    def build(M):
        F = mats()
        rnd = random.Random(1000 + frame)
        base = random.Random(7)
        # the same tongues every frame, each flickering in height and sway
        tongues = [(base.uniform(-0.3, 0.3), base.uniform(-0.3, 0.3), base.uniform(0.35, 0.75),
                    base.uniform(0.08, 0.13)) for _ in range(10)]
        for i, (x, y, h, r) in enumerate(tongues):
            k = math.sin(frame * math.pi / 2 + i * 1.7)
            flame(F, x + 0.02 * k, y - 0.02 * k, h * (0.8 + 0.25 * k), r * (0.95 + 0.1 * rnd.random()), i)
        for _ in range(6):  # embers rising
            kit.blob(rnd.uniform(-0.3, 0.3), rnd.uniform(-0.3, 0.3), rnd.uniform(0.5, 0.85), 0.012, 0.012, 0.012,
                     F["ember"], segs=6, rings=4, name="ember")
        for _ in range(5):  # charred timbers at the base
            ob = kit.box(-0.12, -0.015, 0, 0.12, 0.015, 0.03, F["char"], bevel=0, name="charred")
            ob.location = (rnd.uniform(-0.25, 0.25), rnd.uniform(-0.25, 0.25), 0)
            ob.rotation_euler = (0, 0, rnd.uniform(0, math.pi))
    return build


def smoke(frame):
    def build(M):
        F = mats()
        rnd = random.Random(31)
        # a column of puffs; each frame shifts every puff a quarter of the spacing upward
        for i in range(7):
            t = (i + frame / 4.0) / 7.0
            z = 0.12 + t * 1.45
            r = 0.07 + t * 0.17
            drift = 0.28 * t * t  # bends with the wind toward the screen's right
            mat = F["smoke_dark"] if t < 0.25 else F["smoke"]
            for k in range(3):  # each puff is a small cluster, so the column is ragged
                ox, oy, oz = rnd.uniform(-1, 1) * r * 0.6, rnd.uniform(-1, 1) * r * 0.6, rnd.uniform(-0.3, 0.3) * r
                rr = r * rnd.uniform(0.55, 0.85)
                kit.blob(drift + ox, drift + oy, z + oz, rr, rr, rr * 0.85, mat, segs=10, rings=6, name="puff")
    return build


# ---------------------------------------------------------------- construction and ruins

def rubble(v):
    def build(M):
        F = mats()
        rnd = random.Random(500 + v)
        kit.lot_rect(1, 1, M["soot"] if v % 2 else M["dirt"])
        mats_ = [M["travertine"], M["brick"], M["plaster"], M["stone_dark"]]
        for _ in range(14):
            s = rnd.uniform(0.05, 0.12)
            ob = kit.box(-s, -s * 0.7, 0, s, s * 0.7, s * rnd.uniform(0.5, 1.0), rnd.choice(mats_), bevel=0.01,
                         name="block")
            ob.location = (rnd.uniform(-0.35, 0.35), rnd.uniform(-0.35, 0.35), 0)
            ob.rotation_euler = (rnd.uniform(-0.3, 0.3), rnd.uniform(-0.3, 0.3), rnd.uniform(0, math.pi))
        for _ in range(3):
            ob = kit.box(-0.22, -0.02, 0, 0.22, 0.02, 0.04, F["char"], bevel=0, name="beam")
            ob.location = (rnd.uniform(-0.2, 0.2), rnd.uniform(-0.2, 0.2), rnd.uniform(0.02, 0.08))
            ob.rotation_euler = (0, rnd.uniform(-0.25, 0.25), rnd.uniform(0, math.pi))
        if v in (1, 3):  # a standing stump of wall
            kit.box(-0.4, -0.38, 0, -0.1, -0.3, 0.18 + 0.05 * v, M["travertine"], name="stump")
        if v == 2:
            for k in range(4):
                ob = kit.amphora(rnd.uniform(-0.3, 0.3), rnd.uniform(-0.3, 0.3), M, 0.8)
    return build


def foundation(M):
    kit.lot_rect(1, 1, M["dirt"])
    # levelled bed with a stone course around the edge, so neighbouring tiles join into one footing
    t = 0.08
    for (x0, y0, x1, y1) in ((-0.5, -0.5, 0.5, -0.5 + t), (-0.5, 0.5 - t, 0.5, 0.5), (-0.5, -0.5, -0.5 + t, 0.5),
                             (0.5 - t, -0.5, 0.5, 0.5)):
        kit.box(x0, y0, 0, x1, y1, 0.035, M["travertine"], bevel=0.004, name="course")
    kit.box(-0.36, -0.36, 0, 0.36, 0.36, 0.012, M["paving"], bevel=0, name="bed")
    # mason's stakes
    for (x, y) in ((-0.42, -0.42), (0.42, 0.42)):
        kit.cylinder(x, y, 0, 0.12, 0.01, M["timber_light"], n=6, name="stake")


def scaffold(v):
    def build(M):
        F = mats()
        rnd = random.Random(900 + v)
        H = 0.62
        poles = [(-0.42, -0.42), (0.42, -0.42), (-0.42, 0.42), (0.42, 0.42)]
        for (x, y) in poles:
            kit.cylinder(x, y, 0, H + 0.04 * (v % 2), 0.014, F["pole"], n=6, name="pole")
        for lvl in (0.22, 0.44) if v != 2 else (0.3,):
            # planks along the two outer (viewer-side) edges, rails along all four
            kit.box(-0.45, -0.46, lvl, 0.45, -0.36, lvl + 0.015, M["timber_light"], bevel=0, name="plank")
            kit.box(0.36, -0.45, lvl, 0.46, 0.45, lvl + 0.015, M["timber_light"], bevel=0, name="plank")
            for (x0, y0, x1, y1) in ((-0.42, -0.43, 0.42, -0.41), (-0.42, 0.41, 0.42, 0.43),
                                     (-0.43, -0.42, -0.41, 0.42), (0.41, -0.42, 0.43, 0.42)):
                kit.box(x0, y0, lvl + 0.1, x1, y1, lvl + 0.112, F["pole"], bevel=0, name="rail")
        # cross braces on the front faces
        for (a, b) in (((-0.42, -0.43), (0.42, -0.43)), ((0.43, -0.42), (0.43, 0.42))):
            dx, dy = b[0] - a[0], b[1] - a[1]
            ln = math.hypot(dx, dy)
            ob = kit.box(-ln / 2, -0.006, -0.006, ln / 2, 0.006, 0.006, F["pole"], bevel=0, name="brace")
            ob.location = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, H * 0.45)
            ob.rotation_euler = (0, -math.atan2(H * 0.8, ln), math.atan2(dy, dx))
        if v == 1:  # ladder
            for x in (-0.12, -0.04):
                kit.box(x, -0.47, 0, x + 0.012, -0.455, 0.5, F["pole"], bevel=0, name="ladder")
            for k in range(6):
                kit.box(-0.12, -0.47, 0.06 + k * 0.08, -0.028, -0.458, 0.07 + k * 0.08, F["pole"], bevel=0,
                        name="rung")
        if v == 3:  # treadwheel crane jib with a hoisted block
            ob = kit.box(-0.01, -0.01, 0, 0.01, 0.01, 0.55, F["pole"], bevel=0, name="jib")
            ob.location = (0.3, -0.3, H)
            ob.rotation_euler = (math.radians(-35), 0, math.radians(-45))
            kit.box(0.24, -0.62, H - 0.05, 0.36, -0.52, H + 0.05, M["travertine"], name="hoisted")
        # a few building blocks and a basket stacked on the planks
        for _ in range(3):
            s = 0.04
            ob = kit.box(-s, -s, 0, s, s, s * 1.2, M["travertine"], name="block")
            ob.location = (rnd.uniform(-0.35, 0.35), -0.41, 0.235)
    return build


# ---------------------------------------------------------------- sprite table

SPRITES = {}
for d in DIRS:
    SPRITES["fx/cart_%s" % d] = dict(free=True, build=cart(d))
    SPRITES["fx/ship_%s" % d] = dict(free=True, build=ship(d))
for f in range(4):
    SPRITES["fx/fire_%d" % f] = dict(free=True, frame=(116, 140, 58, 116), build=fire(f), samples=32)
    SPRITES["fx/smoke_%d" % f] = dict(free=True, frame=(140, 230, 52, 214), build=smoke(f), samples=32)
    SPRITES["fx/rubble_%d" % f] = dict(fp=(1, 1), build=rubble(f))
    SPRITES["fx/scaffold_%d" % f] = dict(fp=(1, 1), build=scaffold(f))
SPRITES["fx/foundation"] = dict(fp=(1, 1), build=foundation)
