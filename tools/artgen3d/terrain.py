"""Terrain family: 3D things that stand on the ground (trees, bushes, reeds, boulders, resources).

    Blender -b --python tools/artgen3d/run.py -- terrain [names] [--samples N]

Flat ground (terrain tiles, transitions, cliff faces, roads) is painted by terrain_tex.py.
Every sprite here is free-standing: anchored at its foot (the origin).  A shadow-catcher patch
under each object bakes its cast shadow into the sprite.
"""
import math
import random

import bpy

import kit
from kit import blob, box, cylinder, material, mesh, srgb


def _mats():
    return {
        "bark": material("bark", "#5b4632", "#463626", scale=24, bump=0.3),
        "pine": material("pine", "#3f5a2a", "#304a20", scale=22, bump=0.5, detail=(1.4, "#56703a")),
        "oak": material("oak", "#557a2e", "#436526", scale=22, bump=0.5, detail=(1.4, "#6d8c3c")),
        "holm": material("holm", "#46602c", "#384f24", scale=22, bump=0.5, detail=(1.4, "#5a7238")),
        "poplar": material("poplar", "#6a8a34", "#58772b", scale=22, bump=0.5, detail=(1.4, "#86a046")),
        "myrtle": material("myrtle", "#4e6c2e", "#3f5a26", scale=26, bump=0.5),
        "broom": material("broom", "#6b7d34", "#5a6a2c", scale=26, bump=0.5, detail=(0.8, "#d9b636")),
        "juniper": material("juniper", "#3e5634", "#32472b", scale=26, bump=0.5),
        "reed": material("reed", "#8d9a4e", "#6f7d3a", scale=30, bump=0.2, detail=(1.2, "#b5a86a")),
        "reed_head": material("reed_head", "#6a4d30", "#56402a", scale=30, bump=0.2),
        "boulder": material("boulder", "#8c8577", "#6e685c", scale=7, bump=0.6, detail=(1.5, "#a29a8a")),
        "clay": material("clay", "#a4603e", "#8c4f31", scale=10, bump=0.4, detail=(1.4, "#b87250")),
        "clay_wet": material("clay_wet", "#6e3f2a", "#5a3322", scale=8, rough=0.2, bump=0.05),
        "iron": material("iron_ore", "#7a4a34", "#5d3a2b", scale=9, bump=0.6, detail=(1.2, "#a3583a")),
        "silver": material("silver_ore", "#4f4e52", "#3d3c40", scale=9, bump=0.6,
                           detail=(1.6, "#d8dde4")),
        "glint": material("glint", "#e8eef4", "#c9d2dc", scale=20, rough=0.15, metal=0.9),
        "marble_rock": material("marble_rock", "#ece8df", "#d6d0c4", scale=6, rough=0.5, bump=0.2),
        "limestone": material("limestone", "#d9cba6", "#c4b48c", scale=8, bump=0.5),
        "pozzolana": material("pozzolana", "#7b4b3c", "#5f392e", scale=12, bump=0.5,
                              detail=(1.4, "#8d5a48")),
        "olive_crown": material("olive_crown", "#6f7f45", "#5c6a39", scale=22, bump=0.5,
                                detail=(1.4, "#8d9a62")),
        "fish": material("fish", "#9fb3bf", "#7b8f9c", scale=20, rough=0.3, metal=0.4),
        "foam": material("foam", "#e6f0f2", "#d0dee2", scale=10, bump=0.0, alpha=0.75),
    }


def _seed(name):
    return sum(ord(c) * (i + 1) for i, c in enumerate(name))


def _catcher(r0, r1):
    """Shadow-catcher square from r0 to r1 (world units) in x and y."""
    ob = mesh("shadow", [(r0, r0, 0), (r1, r0, 0), (r1, r1, 0), (r0, r1, 0)], [(0, 1, 2, 3)],
              material("catch", "#808080"))
    ob.is_shadow_catcher = True
    return ob


def _clouds(name, size):
    tex = bpy.data.textures.get(name) or bpy.data.textures.new(name, "CLOUDS")
    tex.noise_scale = size
    tex.noise_depth = 2
    return tex


def foliage(x, y, z, rx, ry, rz, mat, seed, rough=0.35, size=0.12):
    """Ellipsoid crown with lumpy leaf clusters (subdivided + cloud displacement)."""
    ob = blob(x, y, z, rx, ry, rz, mat, segs=12, rings=8, name="leaves")
    sub = ob.modifiers.new("sub", "SUBSURF")
    sub.levels = sub.render_levels = 2
    d = ob.modifiers.new("disp", "DISPLACE")
    d.texture = _clouds("clouds%d" % seed, size)
    d.texture_coords = "GLOBAL"
    d.strength = rough * min(rx, ry, rz) * 1.6
    d.mid_level = 0.5
    ob.location.x += (seed % 7) * 0.0  # keeps the field deterministic per seed (no-op offset)
    return ob


def rock(x, y, s, mat, seed, squash=0.65):
    """Irregular boulder: displaced icosphere."""
    me = bpy.data.meshes.new("rock")
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=3, radius=s)
    rnd = random.Random(seed)
    for v in bm.verts:
        v.co.z *= squash
        k = 1.0 + rnd.uniform(-0.12, 0.12)
        v.co *= k
        v.co.z = max(v.co.z, -0.02 * s)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new("rock", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = (x, y, s * squash * 0.55)
    ob.rotation_euler.z = rnd.uniform(0, math.pi)
    for p in me.polygons:
        p.use_smooth = True
    d = ob.modifiers.new("disp", "DISPLACE")
    d.texture = _clouds("rockc%d" % seed, s * 0.6)
    d.texture_coords = "GLOBAL"
    d.strength = s * 0.35
    return ob


# ---------------------------------------------------------------- trees

def cypress(M, X, h=1.05, seed=1):
    cylinder(0, 0, 0, 0.1, 0.03, X["bark"], n=6)
    ob = foliage(0, 0, 0.06 + h / 2, 0.13, 0.13, h / 2, M["cypress"], seed, rough=0.25, size=0.08)
    _catcher(-0.35, 0.35 + h * 0.7)
    return ob


def umbrella_pine(M, X, seed=2):
    cylinder(0, 0, 0, 0.78, 0.045, X["bark"], n=8)
    cylinder(0.04, 0.02, 0.55, 0.82, 0.025, X["bark"], n=6)
    foliage(0.02, 0.01, 0.86, 0.42, 0.38, 0.12, X["pine"], seed, rough=0.45, size=0.1)
    foliage(-0.12, 0.1, 0.8, 0.22, 0.2, 0.08, X["pine"], seed + 1, rough=0.45, size=0.1)
    _catcher(-0.6, 1.2)


def olive_tree(M, X, seed=3):
    cylinder(0, 0, 0, 0.26, 0.05, X["bark"], n=7)
    cylinder(0.05, -0.03, 0.18, 0.34, 0.03, X["bark"], n=6)
    foliage(0, 0, 0.38, 0.3, 0.27, 0.17, X["olive_crown"], seed, rough=0.5, size=0.09)
    foliage(0.12, -0.1, 0.44, 0.16, 0.15, 0.1, X["olive_crown"], seed + 1, rough=0.5, size=0.09)
    _catcher(-0.5, 0.9)


def oak(M, X, seed=4):
    cylinder(0, 0, 0, 0.4, 0.05, X["bark"], n=8)
    foliage(0, 0, 0.58, 0.36, 0.34, 0.28, X["oak"], seed, rough=0.45, size=0.11)
    foliage(-0.15, 0.12, 0.52, 0.2, 0.2, 0.16, X["oak"], seed + 1, rough=0.45, size=0.11)
    foliage(0.16, -0.1, 0.66, 0.2, 0.18, 0.15, X["oak"], seed + 2, rough=0.45, size=0.11)
    _catcher(-0.55, 1.1)


def poplar(M, X, seed=5):
    cylinder(0, 0, 0, 0.12, 0.035, X["bark"], n=6)
    foliage(0, 0, 0.62, 0.17, 0.17, 0.55, X["poplar"], seed, rough=0.35, size=0.1)
    _catcher(-0.4, 1.2)


def holm_oak(M, X, seed=6):
    cylinder(0, 0, 0, 0.3, 0.045, X["bark"], n=7)
    foliage(0, 0, 0.46, 0.3, 0.3, 0.24, X["holm"], seed, rough=0.5, size=0.09)
    foliage(0.1, 0.12, 0.38, 0.18, 0.18, 0.14, X["holm"], seed + 1, rough=0.5, size=0.09)
    _catcher(-0.5, 0.95)


# ---------------------------------------------------------------- small things

def bush(M, X, kind, seed):
    mat = X[("myrtle", "broom", "juniper")[kind]]
    rnd = random.Random(seed)
    for i in range(3):
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0.0, 0.07)
        s = rnd.uniform(0.09, 0.14)
        foliage(r * math.cos(a), r * math.sin(a), s * 0.8, s, s, s * 0.8, mat, seed + i, rough=0.5,
                size=0.06)
    _catcher(-0.3, 0.4)


def reeds(M, X, n, seed):
    rnd = random.Random(seed)
    for i in range(n):
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0, 0.16)
        x, y = r * math.cos(a), r * math.sin(a)
        h = rnd.uniform(0.18, 0.34)
        lean = rnd.uniform(-0.04, 0.04)
        mesh("reed", [(x - 0.014, y, 0), (x + 0.014, y, 0), (x + lean, y + lean * 0.5, h)],
             [(0, 1, 2)], X["reed"])
        mesh("reed2", [(x, y - 0.014, 0), (x, y + 0.014, 0), (x + lean, y + lean * 0.5, h)],
             [(0, 1, 2)], X["reed"])
        if i % 3 == 0:
            cylinder(x + lean, y + lean * 0.5, h - 0.02, h + 0.04, 0.008, X["reed_head"], n=5)
    _catcher(-0.3, 0.45)


def boulder(M, X, kind, seed):
    if kind == 0:
        rock(0, 0, 0.13, X["boulder"], seed)
    elif kind == 1:
        rock(-0.04, 0.02, 0.15, X["boulder"], seed)
        rock(0.12, -0.06, 0.08, X["boulder"], seed + 1)
    else:
        rock(0, 0, 0.09, X["boulder"], seed, squash=0.5)
        rock(0.08, 0.08, 0.06, X["boulder"], seed + 1)
    _catcher(-0.3, 0.4)


def outcrop(mat, seed, n=4, s=0.09, spread=0.16, extra=None):
    rnd = random.Random(seed)
    for i in range(n):
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0, spread)
        rock(r * math.cos(a), r * math.sin(a), s * rnd.uniform(0.6, 1.2), mat, seed + i)
    if extra:
        extra(rnd)
    _catcher(-0.35, 0.45)


def res_clay(M, X):
    blob(0, 0, 0.0, 0.26, 0.2, 0.012, X["clay_wet"], segs=16, rings=6, top_only=True, name="pool")
    outcrop(X["clay"], 31, n=5, s=0.07, spread=0.2)


def res_stone(M, X):
    outcrop(X["boulder"], 32, n=4, s=0.1)


def res_iron(M, X):
    outcrop(X["iron"], 33, n=5, s=0.08)


def res_silver(M, X):
    def glints(rnd):
        for i in range(5):
            a = rnd.uniform(0, 2 * math.pi)
            r = rnd.uniform(0, 0.15)
            blob(r * math.cos(a), r * math.sin(a), 0.07, 0.018, 0.018, 0.02, X["glint"], segs=6,
                 rings=4, name="glint")
    outcrop(X["silver"], 34, n=4, s=0.09, extra=glints)


def res_marble(M, X):
    box(-0.12, -0.08, 0, 0.06, 0.06, 0.1, X["marble_rock"], bevel=0.015)
    box(0.02, 0.0, 0, 0.16, 0.14, 0.07, X["marble_rock"], bevel=0.015)
    rock(-0.08, 0.12, 0.06, X["marble_rock"], 35)
    _catcher(-0.35, 0.45)


def res_limestone(M, X):
    outcrop(X["limestone"], 36, n=4, s=0.09)


def res_pozzolana(M, X):
    blob(0, 0, 0.0, 0.22, 0.2, 0.08, X["pozzolana"], segs=14, rings=8, top_only=True, name="mound")
    outcrop(X["pozzolana"], 37, n=3, s=0.05, spread=0.18)


def res_fish(M, X):
    for i, (x, y, a) in enumerate([(-0.08, 0.02, 0.3), (0.07, -0.05, -0.6), (0.0, 0.1, 1.4)]):
        ob = blob(x, y, 0.01, 0.05, 0.016, 0.01, X["fish"], segs=8, rings=4, name="fish")
        ob.rotation_euler.z = a
    for r in (0.12, 0.2):
        ring = bpy.data.meshes.new("ripple")
        import bmesh
        bm = bmesh.new()
        bmesh.ops.create_circle(bm, cap_ends=False, segments=24, radius=r)
        bm.to_mesh(ring)
        bm.free()
        ob = bpy.data.objects.new("ripple", ring)
        bpy.context.scene.collection.objects.link(ob)
        ob.location.z = 0.005
        ring.materials.append(X["foam"])
        m = ob.modifiers.new("w", "WIREFRAME")
        m.thickness = 0.012


def _wrap(fn, *a, scale=1.0, shadow=False):
    """Build fn's scene.  shadow=False: the object alone (no shadow catcher).  shadow=True: only
    the shadow it casts on the ground (the object is hidden from the camera but still casts)."""
    def build(M):
        X = _mats()
        fn(M, X, *a)
        for ob in list(bpy.context.scene.objects):
            if ob.type != "MESH":
                continue
            if scale != 1.0:
                ob.location *= scale
                ob.scale *= scale
            if ob.name.startswith("shadow"):
                if not shadow:
                    bpy.data.objects.remove(ob, do_unlink=True)
            elif shadow:
                ob.visible_camera = False
    return build


SPRITES = {}


def _add(name, fn, *a, scale=1.0):
    SPRITES[name] = dict(free=True, build=_wrap(fn, *a, scale=scale))
    SPRITES[name + "_shadow"] = dict(free=True, build=_wrap(fn, *a, scale=scale, shadow=True))


TREES = [lambda M, X: cypress(M, X, 1.05, 11), umbrella_pine, olive_tree, oak,
         lambda M, X: cypress(M, X, 0.8, 15), holm_oak, poplar]
for _i, _fn in enumerate(TREES):
    _add("obj/tree_%d" % _i, _fn, scale=1.25)
for _i in range(3):
    _add("obj/bush_%d" % _i, bush, _i, 40 + _i)
    _add("obj/reeds_%d" % _i, reeds, (26, 18, 12)[_i], 50 + _i)
    _add("obj/boulder_%d" % _i, boulder, _i, 60 + _i)
for _r, _fn in [("clay", res_clay), ("stone", res_stone), ("iron", res_iron), ("silver", res_silver),
                ("marble", res_marble), ("limestone", res_limestone), ("pozzolana", res_pozzolana),
                ("fish", res_fish)]:
    _add("obj/res_" + _r, _fn, scale=1.3)
