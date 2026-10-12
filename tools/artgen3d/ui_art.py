"""Rendered UI art: HUD icons, god and advisor portraits, the logo (family "ui_art").

    Blender -b --python tools/artgen3d/run.py -- ui_art [names] --samples 64

Same look as the goods and category icons (tools/artgen3d/icons.py): small models in the world
palette and materials, lit by the world sun (from the upper left), scaled to fill a fixed canvas,
centred (anchor = the canvas centre), with a thin dark Freestyle contour.  Pictures that face the
viewer (portraits, glyphs, the logo) get an extra warm key light from the upper left, because the
world sun only grazes surfaces turned toward the camera.

  ui/icon_<id>      96 x 96   HUD buttons, top-bar stats, panel headers (drawn at 20-38 points)
  ui/icon_pause..   96 x 96   speed controls: embossed bronze glyphs on a shared scale
  ui/god_<id>      128 x 128  religion panel portraits (drawn at ~42 points; 24 in building panels)
  ui/advisor_<id>  128 x 128  advisor portraits (drawn at 56-64 points)
  ui/logo          600 x 180  main menu title (drawn at 300 x 90 points)

Portraits are busts on a round medallion: bronze rim, a coloured ground per god or advisor, the
figure's attribute beside or on it.  Gods are white marble; advisors are painted.  The names are
the legacy pixel-art names; tools/atlas/pack.py prefers these over assets/sprites/ui.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

import icons as I
import kit
from icons import facing, lathe, mark, place, rod, since, xform
from kit import blob, box, cylinder, material, mesh

ICON, PORT = 96, 128
LOGO = (600, 180)
FONT = os.path.join(kit.ROOT, "assets", "fonts", "Cinzel.ttf")


# ---------------------------------------------------------------- materials

def mats(M):
    I.mats(M)
    m = material
    M["marble_bust"] = m("marble_bust", "#f1ebdf", "#e2dacb", scale=5, rough=0.35, bump=0.03)
    M["marble_logo"] = m("marble_logo", "#ece4d4", "#ddd3c0", scale=4, rough=0.3, bump=0.03)
    M["marble_hair"] = m("marble_hair", "#cdbfa8", "#b8aa92", scale=70, rough=0.55, bump=1.0)
    M["skin"] = m("skin", "#e0a67c", "#d29a70", scale=8, rough=0.55, bump=0.02)
    M["skin_dk"] = m("skin_dk", "#b87a52", "#a86c46", scale=8, rough=0.55, bump=0.02)
    M["hair_dk"] = m("hair_dk", "#3a2a1e", "#2c2016", scale=60, rough=0.6, bump=0.7)
    M["hair_grey"] = m("hair_grey", "#b8b2a8", "#9e978c", scale=60, rough=0.6, bump=0.7)
    M["hair_black"] = m("hair_black", "#1e1a1e", "#141014", scale=60, rough=0.5, bump=0.5)
    M["eye"] = m("eye", "#2a1c14", "#20140e", scale=10, rough=0.3, bump=0)
    M["lip"] = m("lip", "#a8544a", "#94463e", scale=10, rough=0.5, bump=0)
    M["toga"] = m("toga", "#f3ead6", "#e3d7bd", scale=25, rough=0.85, bump=0.15)
    M["tunic_ochre"] = m("tunic_ochre", "#b9873e", "#a27434", scale=25, rough=0.85, bump=0.15)
    M["tunic_red"] = m("tunic_red", "#b5372e", "#9a2c26", scale=25, rough=0.85, bump=0.15)
    M["tunic_blue"] = m("tunic_blue", "#3f6aa8", "#33588e", scale=25, rough=0.85, bump=0.15)
    M["tunic_green"] = m("tunic_green", "#5a8a3a", "#4a742e", scale=25, rough=0.85, bump=0.15)
    M["veil"] = m("veil", "#ece4d4", "#dcd2bc", scale=25, rough=0.85, bump=0.15)
    M["bronze_ui"] = m("bronze_ui", "#c8904c", "#b37c3c", scale=12, rough=0.32, metal=0.7, bump=0.03)
    M["bronze_dk"] = m("bronze_dk", "#8a5a2c", "#784c24", scale=12, rough=0.4, metal=0.6, bump=0.05)
    M["bronze_glyph"] = m("bronze_glyph", "#3e1a06", "#361604", scale=12, rough=0.45, metal=0.0, bump=0.02)
    M["bronze_rim"] = m("bronze_rim", "#b47434", "#9c622a", scale=12, rough=0.35, metal=0.45, bump=0.03)
    M["marble_eye"] = m("marble_eye", "#a89e8e", "#9c9282", scale=10, rough=0.5, bump=0)
    M["marble_lip"] = m("marble_lip", "#d2c2ae", "#c6b6a2", scale=10, rough=0.5, bump=0)
    M["flame"] = m("flame", "#f08a20", "#e87818", scale=15, rough=0.9, bump=0.0, emit="#ff7a10", emit_strength=0.5)
    M["flame_core"] = m("flame_core", "#ffd040", "#ffc030", scale=15, rough=0.9, bump=0.0, emit="#ffb820",
                        emit_strength=0.7)
    M["gilt"] = m("gilt", "#f0c040", "#d9a42c", scale=12, rough=0.3, metal=0.55, bump=0.02)
    M["mask"] = m("mask", "#f6ecd8", "#eadcc2", scale=8, rough=0.45, bump=0.02)
    M["blush"] = m("blush", "#e88a78", "#d87a6a", scale=8, rough=0.5, bump=0)
    M["shell_pink"] = m("shell_pink", "#f2b8a8", "#e29a8c", kind="stripes", stripe=0.05, rough=0.4, bump=0.1)
    M["peacock"] = m("peacock", "#1f8a7a", "#2a6ab0", scale=6, rough=0.3, bump=0.05)
    M["eyespot"] = m("eyespot", "#1a3a8a", "#102a6a", scale=6, rough=0.3, bump=0)
    M["owl"] = m("owl", "#9a7a56", "#7e6244", scale=40, rough=0.8, bump=0.5)
    M["owl_eye"] = m("owl_eye", "#f0c030", "#e0a820", scale=10, rough=0.3, bump=0)
    M["sun_disc"] = m("sun_disc", "#d8302a", "#c02420", scale=8, rough=0.3, metal=0.2, bump=0)
    M["sail"] = m("sail", "#f2e6c8", "#e2d4b0", scale=20, rough=0.9, bump=0.1)
    M["hull"] = m("hull", "#7a4e2c", "#5e3c20", kind="brick", scale=10, brick=(0.6, 0.05),
                  mortar="#4a2e18", bump=0.2)
    M["wax"] = m("wax", "#3a2e24", "#2e241c", scale=20, rough=0.35, bump=0.05)
    # honey-coloured wax with dark scratched lines: reads on the top bar's black medallions
    M["wax_honey"] = m("wax_honey", "#d6a04a", "#c48e3c", scale=20, rough=0.35, bump=0.05)
    M["scratch"] = m("scratch", "#4a2a12", "#3e220e", scale=10, rough=0.6, bump=0)
    M["parch"] = m("parch", "#f0e2b8", "#e2d0a0", scale=12, rough=0.8, bump=0.1)
    M["ink_red"] = m("ink_red", "#8e1616", "#7a1212", scale=10, rough=0.4, bump=0)
    M["shield_blue"] = m("shield_blue", "#2c5aa0", "#244c88", scale=6, rough=0.4, bump=0.05)
    M["tile_grass"] = m("tile_grass", "#6fae48", "#5e9a3c", scale=30, bump=0.3)
    M["tile_ochre"] = m("tile_ochre", "#d8b070", "#c49c5c", scale=30, bump=0.3)
    return M


def enamel(name, colour):
    """A medallion ground: the colour, a touch of cloudiness, satin finish."""
    c = Vector(kit.srgb(colour)[:3])
    dk = "#%02x%02x%02x" % tuple(int(255 * min(1.0, max(0.0, v)) ** (1 / 2.2) * 0.85) for v in c)
    return material("enamel_" + name, colour, dk, scale=3, rough=0.5, bump=0.02)


# ---------------------------------------------------------------- geometry helpers

def fix_normals(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(ob.data)
    bm.free()
    return ob


def slab(pts, d, mat, y=0.0, bevel=0.0, segs=2, name="slab", smooth=False, edge=None):
    """A flat shape in the picture plane: polygon pts [(x, z)] extruded d deep along y (edge: a
    second material for the bevel, e.g. a gilt rim around a dark face)."""
    area = sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1]
               for i in range(len(pts)))
    if area < 0:
        pts = list(reversed(pts))
    n = len(pts)
    v = [(x, y - d / 2, z) for x, z in pts] + [(x, y + d / 2, z) for x, z in pts]
    f = [tuple(range(n)), tuple(range(2 * n - 1, n - 1, -1))]
    f += [(i, n + i, n + (i + 1) % n, (i + 1) % n) for i in range(n)]
    ob = fix_normals(mesh(name, v, f, mat))
    if bevel:
        mo = ob.modifiers.new("bevel", "BEVEL")
        mo.width = bevel
        mo.segments = segs
        mo.limit_method = "ANGLE"
        mo.harden_normals = False
        if edge is not None:
            ob.data.materials.append(edge)
            mo.material = 1
    if smooth:
        for p in ob.data.polygons:
            p.use_smooth = True
    return ob


def sweep(pts, radii, mat, n=10, name="tube", caps=True, flat=1.0):
    """A tube along the polyline pts (3D), radius per point (or one radius); flat < 1 squashes
    the section along the frame's second axis (ribbons, leaves of metal)."""
    pts = [Vector(p) for p in pts]
    if not isinstance(radii, (list, tuple)):
        radii = [radii] * len(pts)
    v, f = [], []
    ref = Vector((0.0, -1.0, 0.0))
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        a = ref - t * ref.dot(t)
        if a.length < 1e-4:
            a = Vector((1.0, 0.0, 0.0)) - t * t.x
        a.normalize()
        b = t.cross(a)
        for j in range(n):
            ang = 2 * math.pi * j / n
            v.append(p + (a * math.cos(ang) * flat + b * math.sin(ang)) * radii[i])
    for i in range(len(pts) - 1):
        for j in range(n):
            k = (j + 1) % n
            f.append((i * n + j, i * n + k, (i + 1) * n + k, (i + 1) * n + j))
    if caps:
        f.append(tuple(range(n - 1, -1, -1)))
        f.append(tuple((len(pts) - 1) * n + j for j in range(n)))
    ob = fix_normals(mesh(name, v, f, mat))
    for p in ob.data.polygons:
        p.use_smooth = len(p.vertices) == 4
    return ob


def arc(cx, cz, r, a0, a1, n, y=0.0, rz=None):
    """Points on a circular arc in the picture plane (degrees, counter-clockwise from +x)."""
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), y,
             cz + (rz or r) * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def torus(cx, cz, R, t, mat, y=0.0, n=64, m=10, name="ring"):
    """A ring in the picture plane (axis along y)."""
    nn = mark()
    lathe([(R + t * math.cos(2 * math.pi * k / m), t * math.sin(2 * math.pi * k / m)) for k in range(m + 1)],
          mat, n=n, name=name, top=False, bottom=False)
    xform(since(nn), Matrix.Translation((cx, y, cz)) @ Matrix.Rotation(math.radians(90), 4, "X"))
    return since(nn)[0]


def disc(cx, cz, r, y0, y1, mat, n=64, name="disc"):
    """A flat cylinder in the picture plane, front face at y0 (toward the viewer), back at y1."""
    return cylinder(cz, cx, y0, y1, r, mat, n=n, axis="y", name=name)


def leaf(L, W, mat, fold=0.25, name="leaf"):
    """A pointed leaf along +x from the origin, lying in the picture plane, midrib creased."""
    v, f = [(0.0, 0.0, 0.0)], []
    k = 6
    for i in range(1, k):
        t = i / k
        w = W * math.sin(math.pi * t) ** 0.8 * (1.0 - 0.25 * t)
        v += [(L * t, -W * fold * math.sin(math.pi * t), 0.0), (L * t, 0.0, w), (L * t, 0.0, -w)]
    v.append((L, 0.0, 0.0))
    for i in range(k - 2):
        a, b = 1 + 3 * i, 1 + 3 * (i + 1)
        f += [(a, b, b + 1, a + 1), (a, a + 2, b + 2, b)]
    f += [(0, 1, 2), (0, 3, 1), (len(v) - 4, len(v) - 1, len(v) - 3), (len(v) - 4, len(v) - 2, len(v) - 1)]
    ob = mesh(name, v, f, mat, solidify=W * 0.12)
    for p in ob.data.polygons:
        p.use_smooth = True
    return ob


def leaf_at(p, ang, L, W, mat, fold=0.25):
    """A leaf from point p (x, y, z) pointing at angle ang (degrees) in the picture plane."""
    nn = mark()
    leaf(L, W, mat, fold)
    xform(since(nn), Matrix.Translation(Vector(p)) @ Matrix.Rotation(math.radians(-ang), 4, "Y"))


def laurel(cx, cz, r, a0, a1, mat, pairs=7, L=0.22, W=0.07, y=0.0, stem=None, tip=True):
    """A laurel branch along an arc: a thin stem with paired leaves pointing toward a1."""
    pts = arc(cx, cz, r, a0, a1, 24, y)
    sweep(pts, W * 0.18, stem or mat, n=6)
    d = 1 if a1 > a0 else -1
    for i in range(pairs):
        t = (i + 0.6) / (pairs + 0.4)
        a = a0 + (a1 - a0) * t
        p = (cx + r * math.cos(math.radians(a)), y - 0.004 * i, cz + r * math.sin(math.radians(a)))
        tang = a + 90 * d
        s = 1.0 - 0.25 * t
        leaf_at(p, tang + 38, L * s, W * s, mat)
        leaf_at(p, tang - 38, L * s, W * s, mat)
    if tip:
        a = a1
        leaf_at((cx + r * math.cos(math.radians(a)), y, cz + r * math.sin(math.radians(a))), a + 90 * d,
                L * 0.75, W * 0.75, mat)


def _area(pts):
    return sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1]
               for i in range(len(pts))) / 2


def _inside(p, poly):
    x, y, c = p[0], p[1], False
    for i in range(len(poly)):
        (x0, y0), (x1, y1) = poly[i], poly[i - 1]
        if (y0 > y) != (y1 > y) and x < (x1 - x0) * (y - y0) / (y1 - y0) + x0:
            c = not c
    return c


def text_mesh(s, size, depth, bevel, mat, x=0.0, z=0.0, y=0.0, spacing=1.0, name="text"):
    """Cinzel lettering in the picture plane, centred on (x, z), front face toward the viewer.

    Cinzel.ttf is a variable font whose glyphs are made of overlapping contours; filled as one
    curve they cancel where they overlap.  So each outer contour (with the holes inside it) is
    extruded as its own solid, and the solids are merged with an exact boolean union.  Returns
    the mesh object."""
    cu = bpy.data.curves.new(name, "FONT")
    cu.body = s
    cu.font = bpy.data.fonts.load(FONT, check_existing=True)
    cu.size = 1.0  # built at unit size and scaled at the end (the voxel remesh stalls on tiny text)
    k = size
    depth, bevel = depth / k, bevel / k
    cu.align_x = "CENTER"
    cu.align_y = "CENTER"
    cu.space_character = spacing
    ob = bpy.data.objects.new(name + "_crv", cu)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    crv = ob.evaluated_get(dg).to_curve(dg)
    contours = []
    for sp in crv.splines:
        pts = sp.bezier_points if sp.type == "BEZIER" else sp.points
        poly = [(q.co[0], q.co[1]) for q in pts]
        contours.append((sp, poly, _area(poly)))
    outer_sign = math.copysign(1.0, max(contours, key=lambda c: abs(c[2]))[2])
    outers = [c for c in contours if c[2] * outer_sign > 0]
    groups = {id(o[0]): [o] for o in outers}
    for c in contours:
        if c[2] * outer_sign > 0:
            continue
        host = [o for o in outers if _inside(c[1][0], o[1])]
        if host:
            groups[id(min(host, key=lambda o: abs(o[2]))[0])].append(c)
    mx = Matrix.Translation((x, y, z)) @ Matrix.Rotation(math.radians(90), 4, "X") @ Matrix.Scale(k, 4)
    out = []
    for k, grp in enumerate(groups.values()):
        nc = bpy.data.curves.new("%s_%d" % (name, k), "CURVE")
        nc.dimensions = "2D"
        nc.fill_mode = "BOTH"
        nc.extrude = depth
        nc.bevel_depth = bevel
        nc.bevel_resolution = 3
        nc.resolution_u = 10
        for sp, _, _ in grp:
            if sp.type == "BEZIER":
                ns = nc.splines.new("BEZIER")
                ns.bezier_points.add(len(sp.bezier_points) - 1)
                for a, b in zip(sp.bezier_points, ns.bezier_points):
                    b.co, b.handle_left, b.handle_right = a.co, a.handle_left, a.handle_right
                    b.handle_left_type, b.handle_right_type = a.handle_left_type, a.handle_right_type
            else:
                ns = nc.splines.new("POLY")
                ns.points.add(len(sp.points) - 1)
                for a, b in zip(sp.points, ns.points):
                    b.co = a.co
            ns.use_cyclic_u = True
        co = bpy.data.objects.new(nc.name, nc)
        bpy.context.scene.collection.objects.link(co)
        bpy.context.view_layer.update()
        me = bpy.data.meshes.new_from_object(co.evaluated_get(bpy.context.evaluated_depsgraph_get()))
        bpy.data.objects.remove(co)
        out.append(me)
    bpy.data.objects.remove(ob)
    bm = bmesh.new()
    for me in out:
        bm.from_mesh(me)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    mo = bpy.data.objects.new(name, me)
    mo.matrix_world = mx
    bpy.context.scene.collection.objects.link(mo)
    # a voxel remesh merges the overlapping solids into one clean surface
    rm = mo.modifiers.new("merge", "REMESH")
    rm.mode = "VOXEL"
    rm.voxel_size = 0.008
    rm.use_smooth_shade = True
    dc = mo.modifiers.new("thin", "DECIMATE")  # the voxel grid is far denser than needed
    dc.decimate_type = "COLLAPSE"
    dc.ratio = 0.06
    bpy.context.view_layer.update()
    me = bpy.data.meshes.new_from_object(mo.evaluated_get(bpy.context.evaluated_depsgraph_get()))
    mo.modifiers.clear()
    mo.data = me
    bm = bmesh.new()
    bm.from_mesh(me)
    kit._uv(bm)
    bm.to_mesh(me)
    bm.free()
    me.materials.clear()
    me.materials.append(mat)
    return mo


def clip_circle(ob, r, cx=0.0, cz=0.0):
    """Keep only the part of ob inside a cylinder of radius r about (cx, cz) along y."""
    cut = cylinder(cz, cx, -3.0, 3.0, r, kit._MATS["eye"], n=64, axis="y", name="cutter")
    cut.hide_render = True
    mo = ob.modifiers.new("clip", "BOOLEAN")
    mo.operation = "INTERSECT"
    mo.object = cut
    mo.solver = "EXACT"
    return ob


def coin(x, y, z, mat, r=0.09, rot=(0, 0)):
    nn = mark()
    lathe([(0.0, 0.0), (r * 0.95, 0.0), (r, r * 0.09), (r, r * 0.18), (r * 0.95, r * 0.27), (0.0, r * 0.27)],
          mat, n=24)
    lathe([(r * 0.7, r * 0.27), (r * 0.74, r * 0.31), (0.0, r * 0.31)], mat, n=16, bottom=False)
    place(nn, loc=(x, y, z), rot=(rot[0], 0, rot[1]))


def upright(n, turn=45.0):
    """Turn a model built facing local -y so it faces the iso camera (for upright 3D objects)."""
    place(n, rot=(0, 0, turn))


# ---------------------------------------------------------------- lights, framing, style

def key_light(energy):
    d = (kit.VIEW * 0.8 + kit.UP * 0.5 - kit.RIGHT * 0.45).normalized()  # toward the light
    li = bpy.data.lights.new("key", "SUN")
    li.energy = energy
    li.color = (1.0, 0.95, 0.86)
    li.angle = math.radians(4.0)
    ob = bpy.data.objects.new("key", li)
    ob.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.collection.objects.link(ob)


def fit_rect(w, h, margin, span=None):
    """Scale and centre the scene so it fills w x h (span: fixed world span for the larger side)."""
    bpy.context.view_layer.update()
    s = bpy.context.scene
    r0, r1, u0, u1 = kit._screen_extent(s)
    if span:
        k = (min(w, h) - 2 * margin) / kit.PPU / span
    else:
        k = min((w - 2 * margin) / kit.PPU / max(r1 - r0, 1e-6), (h - 2 * margin) / kit.PPU / max(u1 - u0, 1e-6))
    rc, uc = (r0 + r1) / 2, (u0 + u1) / 2
    t = -(kit.RIGHT * rc + kit.UP * uc) * k
    xform([o for o in s.objects if o.type == "MESH"], Matrix.Translation(t) @ Matrix.Scale(k, 4))


def style(thick):
    I.outline(ICON)
    ls = bpy.context.scene.view_layers[0].freestyle_settings.linesets[0]
    ls.linestyle.thickness = thick


def sprite(fn, w, h=None, margin=3, thick=1.3, key=0.0, span=None):
    def build(M):
        mats(M)
        fn(M)
        fit_rect(w, h or w, margin, span)
        style(thick)
        if key:
            key_light(key)
    return dict(free=True, frame=(w, h or w, w // 2, (h or w) // 2), build=build)


# ---------------------------------------------------------------- figures

def head(M, skin, hair=None, hz=0.28, hy=-0.25, female=False, beard=False, eyes=False, style_="short",
         hair_front=True):
    """A stylised head facing the viewer (local -y): egg skull, jaw, nose, brow, ears, hair."""
    sx = 0.9 if female else 1.0
    blob(0, hy, hz, 0.22 * sx, 0.25, 0.29, skin, segs=24, rings=16, name="skull")
    blob(0, hy - 0.06, hz - 0.15, 0.15 * sx, 0.17, 0.14, skin, segs=18, rings=10, name="jaw")
    blob(0, hy - 0.215, hz + 0.075, 0.15 * sx, 0.05, 0.035, skin, segs=12, rings=6, name="brow")
    nose = [(0.0, hy - 0.235, hz + 0.07), (0.0, hy - 0.31, hz - 0.06), (-0.042, hy - 0.235, hz - 0.075),
            (0.042, hy - 0.235, hz - 0.075), (0.0, hy - 0.19, hz - 0.02)]
    mesh("nose", nose, [(0, 1, 2), (0, 3, 1), (1, 3, 2), (0, 2, 4), (0, 4, 3), (2, 3, 4)], skin)
    for s in (-1, 1):
        blob(s * 0.215 * sx, hy + 0.02, hz - 0.01, 0.035, 0.05, 0.07, skin, segs=8, rings=6, name="ear")
        if eyes:
            em = M["eye"] if eyes == "paint" else M["marble_eye"]
            blob(s * 0.085 * sx, hy - 0.222, hz + 0.02, 0.034, 0.02, 0.022, em, segs=10, rings=6, name="eye")
    if eyes:
        blob(0, hy - 0.212, hz - 0.135, 0.055, 0.02, 0.016, M["lip"] if eyes == "paint" else M["marble_lip"],
             segs=10, rings=6, name="lip")
    if hair is None:
        return
    if style_ in ("short", "curly"):
        blob(0, hy + 0.02, hz + 0.06, 0.235 * sx, 0.265, 0.275, hair, segs=24, rings=14, top_only=True,
             name="hair")
        blob(0, hy + 0.08, hz - 0.02, 0.225 * sx, 0.2, 0.25, hair, segs=18, rings=10, name="nape")
        if hair_front:
            for i in range(5):
                x = (-0.14 + 0.07 * i) * sx
                blob(x, hy - 0.19 + abs(x) * 0.5, hz + 0.2 - abs(x) * 0.25, 0.05, 0.04, 0.045, hair, segs=8,
                     rings=6, name="lock")
    if style_ == "female":
        blob(0, hy + 0.02, hz + 0.05, 0.215, 0.265, 0.28, hair, segs=24, rings=14, top_only=True, name="hair")
        blob(0, hy + 0.1, hz - 0.03, 0.21, 0.2, 0.27, hair, segs=18, rings=10, name="nape")
        blob(0, hy + 0.27, hz + 0.08, 0.12, 0.1, 0.11, hair, segs=14, rings=8, name="bun")
        for s in (-1, 1):
            blob(s * 0.18, hy - 0.06, hz + 0.08, 0.07, 0.13, 0.1, hair, segs=10, rings=8, name="wave")
            blob(s * 0.2, hy + 0.04, hz - 0.12, 0.06, 0.08, 0.12, hair, segs=10, rings=8, name="curl")
    if beard:
        blob(0, hy - 0.1, hz - 0.21, 0.2, 0.17, 0.2, hair, segs=18, rings=10, name="beard")
        blob(0, hy - 0.215, hz - 0.085, 0.085, 0.03, 0.03, hair, segs=10, rings=6, name="moustache")
        for s in (-1, 1):
            blob(s * 0.17, hy - 0.04, hz - 0.08, 0.06, 0.1, 0.12, hair, segs=10, rings=6, name="whisker")


def chest(M, mat, rx=0.66, cz=-0.86, r_clip=0.93, female=False):
    """Shoulders and chest, clipped to the medallion's inner circle."""
    ob = blob(0, -0.12, cz, rx * (0.88 if female else 1.0), 0.32, 0.55, mat, segs=32, rings=16, name="chest")
    clip_circle(ob, r_clip)
    return ob


def neck(M, mat, hz=0.28, hy=-0.25, r=0.135):
    cylinder(0, hy + 0.06, hz - 0.75, hz - 0.12, r, mat, n=16, name="neck")


HEAD_SCALE, HEAD_DROP = 1.3, 0.1


def big_head(nn):
    """Enlarge everything made since mark nn (a head and its headgear) about the head centre."""
    xform(since(nn), Matrix.Translation((0, -0.25, 0.28 - HEAD_DROP)) @ Matrix.Scale(HEAD_SCALE, 4) @
          Matrix.Translation((0, 0.25, -0.28)))


def surf_y(cy, rx, ry, rz, cx, cz):
    """Front surface (most negative y) of an ellipsoid as a function of (x, z)."""
    def f(x, z):
        q = 1.0 - ((x - cx) / rx) ** 2 - ((z - cz) / rz) ** 2
        return cy - ry * math.sqrt(max(q, 0.0))
    return f


def drape(M, mat, pts, r=0.07, chest_f=None, flat=0.55):
    """A fold of cloth lying on the chest: a squashed tube along (x, z) points."""
    f = chest_f or surf_y(-0.12, 0.66, 0.32, 0.55, 0.0, -0.86)
    sweep([(x, f(x, z) - r * 0.4, z) for x, z in pts], r, mat, n=10, flat=flat)


def medallion(M, colour, name):
    disc(0, 0, 1.0, 0.04, 0.12, enamel(name, colour), name="ground")
    torus(0, 0, 1.0, 0.08, M["bronze_rim"], y=0.05, n=72, name="rim")
    torus(0, 0, 0.9, 0.018, M["gilt"], y=0.035, n=64, name="bead")


# ---------------------------------------------------------------- HUD icons

def i_population(M):
    """A man in a red tunic and a woman in a blue stola side by side."""
    def person(x, h, cloth, hair, female):
        nn = mark()
        if female:
            lathe([(0.15 * h, 0.0), (0.13 * h, 0.3 * h), (0.11 * h, 0.55 * h), (0.13 * h, 0.75 * h),
                   (0.07 * h, 0.83 * h)], cloth, n=20, name="stola")
        else:
            for s in (-1, 1):
                cylinder(s * 0.045 * h, 0, 0.0, 0.34 * h, 0.035 * h, M["skin"], n=8, name="leg")
                box(s * 0.045 * h - 0.04 * h, -0.05 * h, 0.0, s * 0.045 * h + 0.04 * h, 0.03 * h, 0.03 * h,
                    M["leather"], bevel=0.01 * h)
            lathe([(0.15 * h, 0.3 * h), (0.12 * h, 0.5 * h), (0.13 * h, 0.75 * h), (0.07 * h, 0.83 * h)],
                  cloth, n=20, name="tunic")
        for s in (-1, 1):
            sweep([(s * 0.13 * h, 0, 0.76 * h), (s * 0.16 * h, -0.02 * h, 0.6 * h), (s * 0.15 * h, -0.04 * h,
                                                                                          0.47 * h)],
                  0.035 * h, cloth if female else M["skin"], n=8)
            blob(s * 0.15 * h, -0.04 * h, 0.45 * h, 0.035 * h, 0.035 * h, 0.04 * h, M["skin"], segs=8, rings=6)
        cylinder(0, 0, 0.8 * h, 0.88 * h, 0.04 * h, M["skin"], n=10)
        blob(0, 0, 0.95 * h, 0.085 * h, 0.09 * h, 0.1 * h, M["skin"], segs=14, rings=10)
        blob(0, 0.012 * h, 0.975 * h, 0.09 * h, 0.095 * h, 0.09 * h, hair, segs=14, rings=10, top_only=True)
        blob(0, 0.03 * h, 0.95 * h, 0.085 * h, 0.075 * h, 0.09 * h, hair, segs=12, rings=8)
        if female:
            blob(0, 0.09 * h, 0.98 * h, 0.05 * h, 0.04 * h, 0.045 * h, hair, segs=10, rings=6)
        place(nn, loc=(x, 0, 0))
    person(-0.17, 1.0, M["tunic_red"], M["hair_dk"], False)
    person(0.18, 0.88, M["tunic_blue"], M["hair_black"], True)
    upright(0, 45)


def i_happiness(M):
    """A smiling comedy mask, cream with dark eye holes, red lips and gold ribbons."""
    f = surf_y(0.0, 0.5, 0.26, 0.62, 0.0, 0.0)
    blob(0, 0, 0, 0.5, 0.26, 0.62, M["mask"], segs=32, rings=20, name="mask")
    blob(0, -0.1, 0.36, 0.42, 0.18, 0.2, M["mask"], segs=20, rings=10, name="brow")

    def decal(pts, mat, d=0.03, off=0.01):
        ob = slab(pts, d, mat)
        for vtx in ob.data.vertices:
            x, z = vtx.co.x, vtx.co.z
            vtx.co.y += f(x, z) - off
        return ob
    for s in (-1, 1):
        # upturned crescent eyes
        pts = [(s * (0.2 + 0.14 * math.cos(math.radians(a))), 0.14 + 0.11 * math.sin(math.radians(a)))
               for a in range(0, 181, 20)]
        pts += [(s * (0.2 + 0.11 * math.cos(math.radians(a))), 0.15 + 0.025 * math.sin(math.radians(a)))
                for a in range(180, -1, -20)]
        decal(pts, M["eye"], off=0.03)
        blob(s * 0.27, f(s * 0.27, -0.1) + 0.04, -0.1, 0.1, 0.05, 0.08, M["blush"], segs=10, rings=6)
        decal([(s * 0.08, 0.35), (s * 0.34, 0.36), (s * 0.36, 0.31), (s * 0.08, 0.3)], M["hair_dk"], off=0.02)
    pts = [(0.34 * math.cos(math.radians(a)), -0.14 + 0.26 * math.sin(math.radians(a))) for a in range(195, 346, 10)]
    pts += [(0.27 * math.cos(math.radians(a)), -0.17 + 0.08 * math.sin(math.radians(a))) for a in range(345, 194, -10)]
    decal(pts, M["eye"], off=0.03)
    facing(0, tilt=15)


def i_culture(M):
    """A golden lyre: curved horns on a tortoiseshell sound box, crossbar, five strings."""
    blob(0, 0, -0.42, 0.34, 0.12, 0.2, M["wood_dark"], segs=20, rings=10, name="box")
    for s in (-1, 1):
        sweep([(s * 0.22, 0, -0.42), (s * 0.34, 0, -0.2), (s * 0.3, 0, 0.05), (s * 0.36, 0, 0.3),
               (s * 0.46, 0, 0.48), (s * 0.42, 0, 0.6)], [0.055, 0.05, 0.045, 0.04, 0.035, 0.03],
              M["gilt"], n=10)
    sweep([(-0.4, 0, 0.42), (0.4, 0, 0.42)], 0.03, M["gilt"], n=10)
    for i in range(5):
        x = -0.16 + 0.08 * i
        rod((x, -0.06, -0.4), (x, -0.03, 0.42), 0.007, M["bone"], n=5)
    box(-0.2, -0.13, -0.48, 0.2, -0.07, -0.44, M["gilt"], bevel=0.008)
    facing(0, tilt=18)


def i_prosperity(M):
    """A golden cornucopia spilling grapes, apples and coins."""
    pts, radii = [], []
    for i in range(14):
        t = i / 13
        a = math.radians(-60 + 220 * t)
        r = 0.45 * (1 - 0.5 * t)
        pts.append((0.15 + 0.45 * t - 0.2 * math.cos(a) * (1 - t), 0.0, -0.3 + r * math.sin(a) * 0.9 * (1 - t) +
                    0.15 * t))
        radii.append(0.04 + 0.2 * (1 - t) ** 1.6)
    pts = [(0.5 - p[0] * 1.3, 0, p[2]) for p in pts]
    sweep(pts, radii, M["gilt"], n=18, name="horn")
    torus(pts[0][0], pts[0][2], 0.23, 0.03, M["gilt"], n=32)
    x0, z0 = pts[0][0], pts[0][2]
    for k, (dx, dz) in enumerate([(-0.06, 0.12), (0.06, 0.06), (0.0, -0.06), (-0.12, -0.04), (0.12, 0.18)]):
        blob(x0 + dx * 1.2 - 0.06, -0.12, z0 + dz, 0.035, 0.035, 0.035, M["grape"], segs=8, rings=6)
    for (dx, dz, mat) in [(0.05, -0.18, M["fruit_red"]), (-0.12, 0.1, M["fruit_yellow"]),
                          (0.1, 0.14, M["fruit_red"])]:
        blob(x0 + dx, -0.18, z0 + dz, 0.1, 0.1, 0.1, mat, segs=14, rings=10)
    leaf_at((x0 - 0.05, -0.25, z0 + 0.22), 120, 0.2, 0.07, M["vine_leaf"])
    for i, (x, z) in enumerate([(x0 + 0.12, z0 - 0.38), (x0 + 0.3, z0 - 0.45), (x0 - 0.05, z0 - 0.5)]):
        nn = mark()
        coin(0, 0, 0, M["gold"], r=0.1)
        place(nn, loc=(x, -0.25, z), rot=(80, 0, 10 * i))
    facing(0, tilt=15)


def i_security(M):
    """A round blue shield: gold rim and boss, gold leaves and rivets."""
    def toward_viewer(n):
        xform(since(n), Matrix.Rotation(math.radians(90), 4, "X"))  # lathe axis z -> -y
    nn = mark()
    lathe([(0.0, 0.07), (0.3, 0.06), (0.5, 0.025), (0.56, 0.0)], M["shield_blue"], n=48, name="face",
          bottom=False)
    lathe([(0.0, 0.0), (0.15, 0.06), (0.14, 0.12), (0.09, 0.17), (0.0, 0.19)], M["gilt"], n=32, bottom=False)
    toward_viewer(nn)
    torus(0, 0, 0.56, 0.045, M["gilt"], n=48)
    f = lambda x, z: -0.07 + 0.07 * min(1.0, (x * x + z * z) / 0.3)
    for k in range(8):
        a = 2 * math.pi * (k + 0.5) / 8
        x, z = 0.46 * math.cos(a), 0.46 * math.sin(a)
        blob(x, f(x, z) - 0.01, z, 0.03, 0.025, 0.03, M["gilt"], segs=8, rings=6)
    for k in range(4):
        a = 90 * k + 45
        x, z = 0.17 * math.cos(math.radians(a)), 0.17 * math.sin(math.radians(a))
        leaf_at((x, f(x, z) - 0.02, z), a, 0.22, 0.07, M["gilt"])
    facing(0, tilt=15)


def i_favor(M):
    """A gold laurel wreath tied with a red ribbon."""
    laurel(0, 0, 0.5, 262, 112, M["gilt"], pairs=8, L=0.24, W=0.075)
    laurel(0, 0, 0.5, -82, 68, M["gilt"], pairs=8, L=0.24, W=0.075)
    blob(0, -0.04, -0.5, 0.07, 0.05, 0.06, M["red"], segs=10, rings=6)
    for s in (-1, 1):
        sweep([(0, -0.03, -0.5), (s * 0.12, -0.05, -0.62), (s * 0.1, -0.04, -0.78), (s * 0.18, -0.03, -0.9)],
              0.045, M["red"], n=8, flat=0.3)
    facing(0, tilt=12)


def i_build(M):
    """A mason's trowel resting on a stack of bricks with mortar."""
    for row in range(3):
        for k in range(2 - row // 2):
            x0 = -0.3 + k * 0.32 + (0.16 if row == 1 else 0) + (0.16 if row == 2 else 0)
            box(x0, -0.15, row * 0.12, x0 + 0.3, 0.15, row * 0.12 + 0.1, M["brick"], bevel=0.01)
            box(x0 + 0.01, -0.14, row * 0.12 + 0.1, x0 + 0.29, 0.14, row * 0.12 + 0.12, M["lime"], bevel=0.005)
    nn = mark()
    mesh("blade", [(0.0, 0.0, 0.0), (0.42, -0.2, 0.0), (0.58, 0.0, 0.0), (0.42, 0.2, 0.0)], [(0, 1, 2, 3)],
         M["steel"], solidify=0.015)
    rod((0.02, 0, 0.0), (-0.05, 0, 0.12), 0.012, M["iron"], n=6)
    rod((-0.05, 0, 0.12), (-0.32, 0, 0.18), 0.04, M["wood"], n=10, r1=0.035)
    place(nn, loc=(-0.05, -0.05, 0.37), rot=(0, -14, 20))
    upright(0, 35)


def i_rome(M):
    """The eagle standard: a gold eagle with raised wings over a red SPQR plaque on a short pole."""
    sweep([(0, 0.02, -0.62), (0, 0.02, 0.42)], 0.04, M["wood_dark"], n=10)
    box(-0.4, 0.0, -0.34, 0.4, 0.04, 0.12, M["red"], bevel=0.01)
    box(-0.44, -0.01, 0.1, 0.44, 0.05, 0.16, M["gilt"], bevel=0.012)
    box(-0.44, -0.01, -0.37, 0.44, 0.05, -0.31, M["gilt"], bevel=0.012)
    for s in (-1, 1):
        sweep([(s * 0.41, 0.0, -0.35), (s * 0.41, 0.0, -0.5)], 0.016, M["gilt"], n=6)
        blob(s * 0.41, 0, -0.53, 0.045, 0.035, 0.06, M["gilt"], segs=8, rings=6)
    text_mesh("SPQR", 0.25, 0.014, 0.005, M["gilt"], z=-0.11, y=-0.005, spacing=1.05)
    torus(0, -0.5, 0.12, 0.025, M["gilt"], y=0.02, n=24)
    # eagle: perched on a bar, wings raised wide
    box(-0.16, -0.02, 0.42, 0.16, 0.06, 0.47, M["gilt"], bevel=0.008)
    blob(0, 0.0, 0.64, 0.13, 0.11, 0.19, M["gilt"], segs=16, rings=10, name="body")
    blob(0.05, -0.03, 0.86, 0.08, 0.08, 0.08, M["gilt"], segs=12, rings=8, name="head")
    mesh("beak", [(0.12, -0.05, 0.88), (0.2, -0.04, 0.83), (0.1, -0.06, 0.81), (0.09, -0.01, 0.85)],
         [(0, 1, 2), (0, 3, 1), (1, 3, 2), (0, 2, 3)], M["gilt"])
    blob(0, 0.03, 0.48, 0.08, 0.05, 0.1, M["gilt"], segs=10, rings=6, name="tail")
    for s in (-1, 1):
        w = [(0.06, 0.62), (0.2, 0.8), (0.34, 0.98), (0.44, 1.06), (0.42, 0.9), (0.38, 0.84), (0.37, 0.72),
             (0.3, 0.7), (0.29, 0.6), (0.2, 0.6), (0.18, 0.5), (0.08, 0.52)]
        slab([(s * x * 1.3, 0.62 + (z - 0.62) * 1.15) for x, z in w], 0.04, M["gilt"], y=0.02, bevel=0.012)
    facing(0, tilt=8)


def i_advisors(M):
    """A painted counsellor: head and shoulders in a toga with a purple stripe."""
    head(M, M["skin"], M["hair_grey"], eyes=True)
    neck(M, M["skin"])
    ob = blob(0, -0.12, -0.8, 0.62, 0.32, 0.55, M["toga"], segs=32, rings=16, name="chest")
    cut = box(-2, -2, -0.82, 2, 2, 2, M["eye"], bevel=0, name="cutter")
    cut.hide_render = True
    mo = ob.modifiers.new("clip", "BOOLEAN")
    mo.operation = "INTERSECT"
    mo.object = cut
    drape(M, M["toga"], [(-0.55, -0.42), (-0.25, -0.5), (0.1, -0.68), (0.3, -0.8)], r=0.09)
    drape(M, M["purple"], [(-0.5, -0.5), (-0.2, -0.6), (0.12, -0.8)], r=0.04)
    facing(0, tilt=4)


def i_finance(M):
    """A leather purse spilling gold coins beside a short stack."""
    lathe([(0.0, 0.0), (0.2, 0.02), (0.27, 0.14), (0.24, 0.3), (0.12, 0.42), (0.08, 0.44)], M["leather"],
          n=24, name="purse", top=False)
    for k in range(8):
        a = 2 * math.pi * k / 8
        blob(0.1 * math.cos(a), 0.1 * math.sin(a), 0.48, 0.05, 0.05, 0.07, M["leather"], segs=8, rings=6)
    torus(0, 0, 0.09, 0.018, M["twine"], n=24)
    xform([o for o in since(0) if o.name.startswith("ring")], Matrix.Translation((0, 0, 0.42)) @
          Matrix.Rotation(math.radians(-90), 4, "X"))
    for i in range(5):
        coin(0.36, 0.0, i * 0.027, M["gold"], r=0.1)
    for (x, y, z, r1, r2) in [(-0.1, -0.3, 0.0, 0, 0), (0.12, -0.32, 0.0, 0, 0), (0.02, -0.2, 0.03, 30, 40),
                              (-0.28, -0.12, 0.0, 0, 0), (0.25, -0.2, 0.0, 0, 0)]:
        coin(x, y, z, M["gold"], r=0.1, rot=(r1, r2))
    upright(0, 45)


def i_goods(M):
    """A wooden crate with a grain sack and an amphora leaning on it."""
    box(-0.2, -0.2, 0, 0.2, 0.2, 0.36, M["crate"], bevel=0.01)
    for z in (0.02, 0.32):
        box(-0.21, -0.21, z, 0.21, 0.21, z + 0.035, M["wood_dark"], bevel=0.005)
    blob(0.0, 0.0, 0.46, 0.17, 0.15, 0.12, M["burlap"], segs=16, rings=10)
    blob(0.0, 0.0, 0.58, 0.05, 0.05, 0.05, M["burlap"], segs=10, rings=6)
    nn = mark()
    I.amphora_shape(M["terracotta"], h=0.62, body=0.13, neck=0.04)
    place(nn, loc=(0.34, -0.3, 0.0), rot=(0, 14, 0))
    upright(0, 30)


def i_trade(M):
    """A merchant ship: dark hull, swan-neck stern, a square sail with a red stripe."""
    secs = []
    n = 13
    for i in range(n):
        t = i / (n - 1)
        x = -0.75 + 1.5 * t
        w = 0.24 * math.sin(math.pi * (0.06 + 0.88 * t)) ** 0.7
        zt = 0.3 + 0.12 * (2 * t - 1) ** 4
        zk = 0.0 + 0.08 * (2 * t - 1) ** 2
        secs.append([(x, -w, zt), (x, -w * 0.85, zk + 0.12), (x, 0.0, zk), (x, w * 0.85, zk + 0.12), (x, w, zt)])
    v = [p for s in secs for p in s]
    f = [(i * 5 + j, i * 5 + j + 1, (i + 1) * 5 + j + 1, (i + 1) * 5 + j) for i in range(n - 1) for j in range(4)]
    hull = fix_normals(mesh("hull", v, f, M["hull"], solidify=0.02))
    for p in hull.data.polygons:
        p.use_smooth = True
    deck = [(s[0][0], s[0][1], s[0][2] - 0.03) for s in secs] + [(s[4][0], s[4][1], s[4][2] - 0.03)
                                                                for s in reversed(secs)]
    mesh("deck", deck, [tuple(range(len(deck)))], M["plank"])
    box(-0.75, -0.02, 0.0, 0.75, 0.02, 0.06, M["wood_dark"], bevel=0.005)
    sweep([(-0.72, 0, 0.38), (-0.85, 0, 0.5), (-0.84, 0, 0.62), (-0.76, 0, 0.66), (-0.72, 0, 0.6)], 0.03,
          M["wood_dark"], n=8)
    sweep([(0.7, 0, 0.36), (0.82, 0, 0.42)], 0.025, M["wood_dark"], n=8)
    for s in (-1, 1):
        box(-0.6, s * 0.24 - 0.01, 0.26, 0.6, s * 0.24 + 0.01, 0.3, M["red"], bevel=0.003)
    rod((0.0, 0.0, 0.2), (0.0, 0.0, 1.2), 0.025, M["wood_dark"], n=8)
    rod((0.0, -0.5, 1.12), (0.0, 0.5, 1.12), 0.018, M["wood_dark"], n=8)
    v, f = [], []
    for i in range(5):
        z = 1.1 - 0.17 * i
        for j in range(5):
            y = -0.46 + 0.23 * j
            v.append((0.06 + 0.12 * math.sin(math.pi * j / 4) * math.sin(math.pi * (i + 1) / 6), y, z))
    f = [(i * 5 + j, i * 5 + j + 1, (i + 1) * 5 + j + 1, (i + 1) * 5 + j) for i in range(4) for j in range(4)]
    sail = mesh("sail", v, f, M["sail"], solidify=0.01)
    for p in sail.data.polygons:
        p.use_smooth = True
    for z in (0.76, 0.86):
        vv = [(0.07 + 0.12 * math.sin(math.pi * j / 4) * math.sin(math.pi * (1.1 - z) / 0.85 + 0.2),
               -0.46 + 0.23 * j, z) for j in range(5)]
        sweep(vv, 0.025, M["red"], n=6, flat=0.4)
    upright(0, 12)


def i_labor(M):
    """A hammer and a saw crossed."""
    nn = mark()
    teeth = [(0.0, 0.0)]
    for i in range(12):
        teeth += [(0.06 + 0.07 * i, -0.04), (0.095 + 0.07 * i, 0.0)]
    pts = [(0.0, 0.18), (0.9, 0.12)] + [(0.9, 0.0)] + list(reversed(teeth))[:-1] + [(0.0, 0.0)]
    slab(pts, 0.014, M["steel"], bevel=0.003)
    box(-0.26, -0.04, 0.0, 0.02, 0.04, 0.19, M["wood"], bevel=0.03)
    place(nn, loc=(-0.45, 0, -0.1), rot=(0, -40, 0))
    nn = mark()
    rod((0, 0, -0.5), (0, 0, 0.45), 0.035, M["wood"], n=10)
    box(-0.26, -0.085, 0.38, 0.22, 0.085, 0.58, M["iron"], bevel=0.02)
    place(nn, loc=(0.0, -0.1, 0.0), rot=(0, 38, 0))
    facing(0, tilt=20)


def i_religion(M):
    """A stone altar with bolsters and a garland, fire burning in a bronze dish on top."""
    box(-0.32, -0.26, 0.0, 0.32, 0.26, 0.08, M["travertine"], bevel=0.01)
    box(-0.26, -0.2, 0.08, 0.26, 0.2, 0.5, M["travertine"], bevel=0.01)
    box(-0.31, -0.25, 0.5, 0.31, 0.25, 0.58, M["travertine"], bevel=0.01)
    for s in (-1, 1):
        cylinder(0.62, s * 0.2, -0.24, 0.24, 0.06, M["travertine"], n=14, axis="x")
    sweep([(-0.26, -0.205, 0.42), (-0.13, -0.215, 0.31), (0.0, -0.215, 0.28), (0.13, -0.215, 0.31),
           (0.26, -0.205, 0.42)], 0.035, M["leafy"], n=8)
    for x in (-0.13, 0.0, 0.13):
        blob(x, -0.24, 0.3 - (0.03 if x == 0 else 0), 0.03, 0.03, 0.03, M["fruit_red"], segs=8, rings=6)
    lathe([(0.04, 0.58), (0.16, 0.6), (0.2, 0.66)], M["bronze_ui"], n=24, top=False)
    lathe([(0.17, 0.64), (0.2, 0.72), (0.16, 0.84), (0.08, 0.96), (0.0, 1.1)], M["flame"], n=16, name="flame")
    lathe([(0.1, 0.64), (0.06, 0.78), (0.0, 0.88)], M["flame_core"], x=0.0, y=-0.06, n=12, name="core")
    for s in (-1, 1):
        lathe([(0.06, 0.62), (0.03, 0.76), (0.0, 0.84)], M["flame"], x=s * 0.12, y=-0.02, n=10)
    upright(0, 45)


def i_military(M):
    """A bronze helmet with cheek guards and a tall red crest, seen three-quarter."""
    lathe([(0.0, 0.33), (0.08, 0.325), (0.14, 0.3), (0.185, 0.23), (0.2, 0.14), (0.205, 0.06)],
          M["bronze_ui"], n=32, bottom=False, top=False)
    lathe([(0.205, 0.06), (0.225, 0.06), (0.225, 0.09), (0.2, 0.11)], M["gilt"], n=32, top=False, bottom=False)
    blob(0, -0.17, 0.14, 0.14, 0.04, 0.03, M["gilt"], segs=10, rings=6, name="brow")
    mesh("neck", [(-0.18, 0.1, 0.08), (0.18, 0.1, 0.08), (0.22, 0.3, 0.0), (-0.22, 0.3, 0.0)], [(0, 1, 2, 3)],
         M["bronze_ui"], solidify=0.02)
    for sx in (-1, 1):
        nn = mark()
        mesh("cheek", [(-0.07, 0, 0.0), (0.07, 0, 0.0), (0.05, 0, -0.18), (-0.03, 0, -0.21)], [(0, 1, 2, 3)],
             M["bronze_ui"], solidify=0.02)
        place(nn, loc=(sx * 0.15, -0.1, 0.08), rot=(0, 0, sx * 55))
    box(-0.02, -0.18, 0.31, 0.02, 0.2, 0.37, M["gilt"], bevel=0.005)
    # crest: a fan of horsehair running front to back, modelled in x/z then turned onto the y axis
    nn = mark()
    prof = [(0.26 * math.cos(math.radians(a)), 0.34 + 0.22 * math.sin(math.radians(a))) for a in range(-10, 191, 10)]
    prof += [(-0.24, 0.3), (0.0, 0.33), (0.24, 0.3)]
    slab(prof, 0.08, M["crest"], bevel=0.03, segs=3)
    xform(since(nn), Matrix.Translation((0, 0.04, 0)) @ Matrix.Rotation(math.radians(90), 4, "Z"))
    place(0, rot=(0, 0, 35))
    facing(0, tilt=12)


def i_research(M):
    """An open wax tablet (diptych) with lines scratched in it and a bronze stylus."""
    for s in (-1, 1):
        x0, x1 = (0.02, 0.5) if s > 0 else (-0.5, -0.02)
        box(x0, -0.34, 0.0, x1, 0.34, 0.05, M["wood"], bevel=0.01)
        box(x0 + 0.05, -0.29, 0.03, x1 - 0.05, 0.29, 0.055, M["wax_honey"], bevel=0)
        for k in range(5):
            y = -0.2 + 0.1 * k
            w = 0.3 if (k + (s > 0)) % 3 else 0.2
            box(x0 + 0.08, y - 0.01, 0.055, x0 + 0.08 + w, y + 0.01, 0.058, M["scratch"], bevel=0)
    for y in (-0.2, 0.2):
        cylinder(0.03, y, -0.02, 0.02, 0.025, M["leather"], n=8, axis="x")
    rod((-0.3, -0.42, 0.08), (0.45, 0.25, 0.14), 0.02, M["bronze_ui"], n=8)
    rod((0.45, 0.25, 0.14), (0.5, 0.3, 0.145), 0.02, M["bronze_ui"], n=8, r1=0.035)
    rod((-0.3, -0.42, 0.08), (-0.36, -0.47, 0.075), 0.02, M["bronze_ui"], n=8, r1=0.001)
    xform(since(0), Matrix.Rotation(math.radians(90), 4, "X"))
    facing(0, tilt=50)


def i_alerts(M):
    """A bright gilt bell with a clapper and a hanging loop (gilt rather than bronze, so it reads on the top bar's
    black medallions)."""
    lathe([(0.0, 0.62), (0.1, 0.61), (0.16, 0.56), (0.18, 0.44), (0.2, 0.3), (0.26, 0.16), (0.34, 0.06),
           (0.36, 0.0), (0.33, 0.0), (0.0, 0.0)], M["gilt"], n=36, name="bell")
    lathe([(0.355, 0.0), (0.37, 0.03), (0.35, 0.07)], M["bronze_ui"], n=36, top=False, bottom=False)
    torus(0, 0.7, 0.09, 0.03, M["bronze_dk"], n=20)
    blob(0, 0, -0.06, 0.07, 0.07, 0.07, M["bronze_dk"], segs=12, rings=8)
    xform(since(0), Matrix.Rotation(math.radians(-12), 4, "Y"))
    upright(0, 45)


def i_help(M):
    """An unrolled scroll with a large red question mark."""
    box(-0.36, 0.0, -0.5, 0.36, 0.02, 0.5, M["parch"], bevel=0.005)
    for z in (-0.52, 0.52):
        cylinder(z, 0.0, -0.42, 0.42, 0.06, M["parch"], n=16, axis="x")
        for s in (-1, 1):
            cylinder(z, 0.0, s * 0.42, s * 0.5, 0.035, M["wood_dark"], n=10, axis="x")
            blob(s * 0.52, 0.0, z, 0.04, 0.04, 0.04, M["wood_dark"], segs=10, rings=6)
    text_mesh("?", 0.95, 0.04, 0.035, M["ink_red"], z=0.02, y=-0.03)
    facing(0, tilt=10)


def i_menu(M):
    """Three embossed bars, styled like the speed glyphs."""
    for z in (-0.34, 0.0, 0.34):
        slab([(-0.5, z - 0.1), (0.5, z - 0.1), (0.5, z + 0.1), (-0.5, z + 0.1)], 0.2, M["bronze_glyph"],
             bevel=0.07, segs=4, edge=M["gilt"])
    facing(0, tilt=0)


def i_overlay(M):
    """Three map tiles stacked with gaps: paving, water, grass."""
    for k, mat in enumerate((M["tile_ochre"], M["water"], M["tile_grass"])):
        z = 0.28 * k
        box(-0.5, -0.5, z, 0.5, 0.5, z + 0.07, mat, bevel=0.015)
        box(-0.5, -0.5, z - 0.02, 0.5, 0.5, z, M["stone_dark"], bevel=0.01)
    box(-0.42, -0.06, 0.63, 0.42, 0.06, 0.635, M["kerb"], bevel=0)


def i_road(M):
    """A straight stretch of basalt paving between kerbs."""
    box(-0.5, -0.24, 0.0, 0.5, 0.24, 0.06, M["basalt"], bevel=0.006)
    box(-0.5, 0.24, 0.0, 0.5, 0.32, 0.1, M["kerb"], bevel=0.008)
    box(-0.5, -0.32, 0.0, 0.5, -0.24, 0.1, M["kerb"], bevel=0.008)
    box(-0.5, 0.32, 0.0, 0.5, 0.44, 0.04, M["grass"], bevel=0.006)
    box(-0.5, -0.44, 0.0, 0.5, -0.32, 0.04, M["grass"], bevel=0.006)
    place(0, rot=(0, 0, 90))


def i_demolish(M):
    """A pickaxe over a broken wall stub and loose bricks."""
    for row in range(3):
        for k in range(3 - row):
            x0 = -0.45 + k * 0.3 + row * 0.15
            box(x0, -0.12, row * 0.11, x0 + 0.28, 0.12, row * 0.11 + 0.1, M["brick"], bevel=0.01)
    nn = mark()
    box(0, 0, 0, 0.28, 0.24, 0.1, M["brick"], bevel=0.01)
    place(nn, loc=(0.4, -0.3, 0.0), rot=(0, 0, 25))
    nn = mark()
    box(0, 0, 0, 0.2, 0.24, 0.1, M["brick"], bevel=0.01)
    place(nn, loc=(-0.6, -0.35, 0.0), rot=(0, 18, -20))
    for i, (x, y) in enumerate([(0.2, -0.35), (-0.1, -0.4), (0.0, -0.3), (-0.3, -0.42)]):
        I.rock(x, y, 0.02, 0.04, M["brick"], seed=i)
    nn = mark()
    rod((0, 0, -0.1), (0, 0, 0.95), 0.035, M["wood"], n=10)
    sweep([(-0.5, 0, 0.82), (-0.25, 0, 0.92), (0.0, 0, 0.95), (0.25, 0, 0.92), (0.5, 0, 0.82)],
          [0.01, 0.04, 0.055, 0.04, 0.01], M["iron"], n=8)
    place(nn, loc=(0.1, -0.05, 0.3), rot=(-10, -35, 30))
    upright(0, 30)


# ---------------------------------------------------------------- speed glyphs

def triangle(x, h, M):
    w = h * 0.86
    return slab([(x, -h / 2), (x + w, 0.0), (x, h / 2)], 0.2, M["bronze_glyph"], bevel=0.075, segs=4,
                edge=M["gilt"])


def bar(x, h, M):
    return slab([(x, -h / 2), (x + 0.3, -h / 2), (x + 0.3, h / 2), (x, h / 2)], 0.2, M["bronze_glyph"],
                bevel=0.075, segs=4, edge=M["gilt"])


def glyphs(kind):
    def fn(M):
        h = 1.0
        if kind == "pause":
            bar(-0.4, h, M)
            bar(0.1, h, M)
        elif kind == "play":
            triangle(-0.35, h, M)
        else:
            k = 2 if kind == "fast" else 3
            step = 0.5 if k == 2 else 0.44
            x0 = -(step * (k - 1) + 0.86) / 2
            for i in range(k):
                triangle(x0 + step * i, h * (1.0 if k == 2 else 0.86), M)
        facing(0, tilt=0)
    return fn


# ---------------------------------------------------------------- portraits

def god(colour, female=False, beard=False, hair="curly", attr=None, head_attr=None, trim=None):
    def fn(M):
        medallion(M, colour, "ground")
        nn = mark()
        head(M, M["marble_bust"], M["marble_hair"], female=female, beard=beard, eyes="marble",
             style_="female" if female else hair, hair_front=not female)
        if head_attr:
            head_attr(M)
        big_head(nn)
        neck(M, M["marble_bust"])
        chest(M, M["marble_bust"], female=female)
        drape(M, M["marble_hair"], [(-0.62, -0.42), (-0.3, -0.5), (0.0, -0.66), (0.25, -0.88)], r=0.08)
        if trim:
            drape(M, M[trim], [(-0.6, -0.52), (-0.28, -0.6), (0.0, -0.77), (0.2, -0.92)], r=0.035)
        if attr:
            attr(M)
        facing(0, tilt=0)
    return fn


def a_thunderbolt(M):
    pts = [(0.0, 0.32), (0.12, 0.32), (0.04, 0.08), (0.14, 0.08), (-0.06, -0.3), (0.0, -0.04), (-0.1, -0.04)]
    nn = mark()
    slab([(x, z) for x, z in pts], 0.05, M["gilt"], bevel=0.012)
    place(nn, loc=(0.6, -0.45, -0.32), rot=(0, -12, 0), scale=1.15)


def a_jupiter_crown(M):
    for s in (-1, 1):
        laurel(0, 0.33, 0.27, 180 if s < 0 else 0, 100 if s < 0 else 80, M["gilt"], pairs=4, L=0.12, W=0.045,
               y=-0.27)


def a_peacock(M):
    for k in range(7):
        a = math.radians(30 + 120 * k / 6)
        x, z = 0.82 * math.cos(a), 0.12 + 0.72 * math.sin(a)
        sweep([(0.0, 0.0, 0.0), (x * 0.85, 0.0, z * 0.85 + 0.0)], 0.008, M["gilt"], n=5)
        blob(x * 0.88, 0.0, z * 0.88, 0.1, 0.03, 0.13, M["peacock"], segs=12, rings=8)
        blob(x * 0.88, -0.03, z * 0.88, 0.045, 0.02, 0.06, M["eyespot"], segs=10, rings=6)
        blob(x * 0.88, -0.045, z * 0.88, 0.02, 0.01, 0.025, M["gilt"], segs=8, rings=5)


def a_diadem(M):
    v, f = [], []
    n = 12
    for i in range(n + 1):
        a = math.radians(200 - 220 * i / n)
        x, y = 0.24 * math.cos(a), -0.25 - 0.27 * math.sin(a)
        hgt = 0.12 * (1 - abs(i - n / 2) / (n / 2)) + 0.03
        v += [(x, y, 0.4), (x, y - 0.0, 0.4 + hgt)]
    for i in range(n):
        f.append((2 * i, 2 * i + 2, 2 * i + 3, 2 * i + 1))
    mesh("diadem", v, f, M["gilt"], solidify=0.025)


def a_mars_helmet(M):
    lathe([(0.0, 0.62), (0.12, 0.61), (0.21, 0.56), (0.26, 0.46), (0.27, 0.36)], M["bronze_ui"], x=0, y=-0.24,
          n=32, bottom=False, top=False, name="helm")
    blob(0, -0.48, 0.38, 0.2, 0.05, 0.035, M["gilt"], segs=12, rings=6)
    for s in (-1, 1):
        mesh("cheek", [(s * 0.25, -0.3, 0.38), (s * 0.24, -0.42, 0.36), (s * 0.2, -0.42, 0.1), (s * 0.23, -0.3, 0.08)],
             [(0, 1, 2, 3)], M["bronze_ui"], solidify=0.02)
    blob(0, -0.2, 0.72, 0.07, 0.32, 0.17, M["crest"], segs=16, rings=10, top_only=False, name="crest")
    box(-0.03, -0.45, 0.6, 0.03, 0.05, 0.66, M["gilt"], bevel=0.005)


def a_spear(M):
    sweep([(0.66, -0.4, -0.85), (0.66, -0.4, 0.55)], 0.022, M["wood_dark"], n=8)
    slab([(0.6, 0.55), (0.72, 0.55), (0.66, 0.85)], 0.03, M["iron"], y=-0.4, bevel=0.008)


def a_shell(M):
    for k in range(9):
        a = math.radians(20 + 140 * k / 8)
        sweep([(0.0, 0.03, -0.2), (0.86 * math.cos(a), 0.05, -0.2 + 0.98 * math.sin(a))],
              [0.03, 0.11], M["shell_pink"], n=10, flat=0.5)
    pts = arc(0, -0.2, 0.9, 18, 162, 30, y=0.06, rz=1.0)
    sweep(pts, 0.04, M["shell_pink"], n=8)


def a_venus_hair(M):
    blob(0, -0.3, 0.53, 0.1, 0.08, 0.08, M["marble_hair"], segs=12, rings=8)


def a_minerva(M):
    lathe([(0.0, 0.66), (0.12, 0.65), (0.21, 0.6), (0.26, 0.5), (0.27, 0.42)], M["bronze_ui"], x=0, y=-0.22,
          n=32, bottom=False, top=False, name="helm")
    blob(0, -0.42, 0.47, 0.18, 0.08, 0.05, M["bronze_ui"], segs=12, rings=6, name="visor")
    blob(0, -0.18, 0.78, 0.06, 0.28, 0.15, M["crest"], segs=16, rings=10, name="crest")


def a_owl(M):
    """An owl perched on the right shoulder."""
    nn = mark()
    ox, oy, oz = 0.0, 0.0, 0.0
    blob(ox, oy, oz, 0.13, 0.11, 0.17, M["owl"], segs=14, rings=10)
    blob(ox, oy - 0.02, oz + 0.2, 0.12, 0.1, 0.1, M["owl"], segs=14, rings=10)
    for s in (-1, 1):
        blob(ox + s * 0.05, oy - 0.1, oz + 0.21, 0.04, 0.02, 0.04, M["owl_eye"], segs=10, rings=6)
        blob(ox + s * 0.05, oy - 0.115, oz + 0.21, 0.017, 0.01, 0.017, M["eye"], segs=8, rings=5)
        slab([(ox + s * 0.05, oz + 0.27), (ox + s * 0.11, oz + 0.35), (ox + s * 0.11, oz + 0.27)], 0.03,
             M["owl"], y=oy)
    mesh("beak", [(ox - 0.02, oy - 0.1, oz + 0.17), (ox + 0.02, oy - 0.1, oz + 0.17), (ox, oy - 0.14, oz + 0.12)],
         [(0, 1, 2)], M["gilt"], solidify=0.01)
    place(nn, loc=(0.55, -0.55, -0.42), scale=1.45)


def a_mercury(M):
    blob(0, -0.22, 0.5, 0.28, 0.3, 0.12, M["bronze_ui"], segs=24, rings=10, top_only=True, name="petasos")
    lathe([(0.27, 0.5), (0.36, 0.48), (0.38, 0.46)], M["bronze_ui"], x=0, y=-0.22, n=32, top=False,
          bottom=False)
    for s in (-1, 1):
        for k in range(3):
            nn = mark()
            leaf(0.26 - 0.05 * k, 0.07, M["gilt"], fold=0.1)
            place(nn, loc=(s * 0.25, -0.15, 0.55 + 0.02 * k), rot=(0, 0, 0))
            xform(since(nn), Matrix.Translation((s * 0.25, -0.15, 0.55)) @ Matrix.Rotation(
                math.radians(-(90 - s * (60 - 20 * k)) if s > 0 else -(90 + 60 - 20 * k)), 4, "Y") @
                Matrix.Translation((-s * 0.25, 0.15, -0.55)))


def a_caduceus(M):
    x, y = 0.66, -0.45
    sweep([(x, y, -0.85), (x, y, 0.45)], 0.022, M["gilt"], n=8)
    blob(x, y, 0.48, 0.04, 0.04, 0.04, M["gilt"], segs=10, rings=6)
    for s in (-1, 1):
        pts = [(x + s * 0.07 * math.sin(t * 3 * math.pi), y - 0.02 * math.cos(t * 3 * math.pi), -0.55 + 0.9 * t)
               for t in [i / 24 for i in range(25)]]
        sweep(pts, 0.016, M["bronze_dk"], n=6)
        nn = mark()
        leaf(0.2, 0.06, M["gilt"], fold=0.1)
        place(nn, loc=(x, y + 0.01, 0.4), rot=(0, -20 if s > 0 else -160, 0))


def a_trident(M):
    x, y = 0.64, -0.42
    sweep([(x, y, -0.85), (x, y, 0.42)], 0.024, M["gilt"], n=8)
    sweep([(x - 0.17, y, 0.62), (x - 0.17, y, 0.42), (x, y, 0.38), (x + 0.17, y, 0.42), (x + 0.17, y, 0.62)],
          0.02, M["gilt"], n=8, caps=True)
    sweep([(x, y, 0.38), (x, y, 0.72)], 0.022, M["gilt"], n=8)
    for dx, top in ((-0.17, 0.62), (0.0, 0.72), (0.17, 0.62)):
        slab([(x + dx - 0.04, top), (x + dx + 0.04, top), (x + dx, top + 0.12)], 0.035, M["gilt"], y=y)


def a_veil(M, mat="marble_bust"):
    blob(0, -0.1, 0.38, 0.29, 0.3, 0.35, M[mat], segs=28, rings=16, name="veil")
    for s in (-1, 1):
        sweep([(s * 0.25, -0.15, 0.3), (s * 0.3, -0.12, 0.0), (s * 0.36, -0.15, -0.2)],
              [0.07, 0.09, 0.1], M[mat], n=10)


def a_flame(M):
    x, y, z = 0.56, -0.62, -0.62
    lathe([(0.03, z), (0.18, z + 0.03), (0.24, z + 0.12)], M["gilt"], x=x, y=y, n=28, top=False)
    lathe([(0.03, z - 0.25), (0.05, z - 0.02)], M["gilt"], x=x, y=y, n=12)
    lathe([(0.18, z + 0.1), (0.15, z + 0.3), (0.08, z + 0.48), (0.0, z + 0.66)], M["flame"], x=x, y=y, n=16)
    lathe([(0.1, z + 0.1), (0.06, z + 0.3), (0.0, z + 0.42)], M["flame_core"], x=x, y=y - 0.03, n=12)


def a_wheat_crown(M):
    for k in range(9):
        a = math.radians(25 + 130 * k / 8)
        x, z = 0.27 * math.cos(a), 0.33 + 0.3 * math.sin(a)
        base = (x * 0.9, -0.3, z - 0.02)
        tip = (x * 1.6, -0.3, z + 0.18 * math.sin(a))
        sweep([base, tip], 0.012, M["straw_gold"], n=5)
        nn = mark()
        blob(0, 0, 0, 0.11, 0.04, 0.04, M["ear"], segs=10, rings=6)
        place(nn, loc=tip, rot=(0, -math.degrees(a) * 0.9, 0))


def a_sheaves(M):
    for s in (-1, 1):
        x0 = 0.6 * s
        for k in range(3):
            sweep([(x0, -0.42, -0.85), (x0 + s * 0.06 * (k - 1), -0.42, 0.1 + 0.06 * k)], 0.012, M["straw_gold"],
                  n=5)
            nn = mark()
            blob(0, 0, 0, 0.045, 0.045, 0.14, M["ear"], segs=10, rings=6)
            place(nn, loc=(x0 + s * 0.06 * (k - 1), -0.42, 0.22 + 0.06 * k), rot=(0, s * 8 * (k - 1), 0))


def a_vulcan(M):
    lathe([(0.25, 0.4), (0.2, 0.55), (0.12, 0.66), (0.0, 0.72)], M["bronze_dk"], x=0, y=-0.2, n=28,
          bottom=False, name="pileus")


def a_tools(M):
    """Vulcan's hammer and anvil, a spark of glowing iron."""
    x, y = 0.62, -0.45
    nn = mark()
    sweep([(0, 0, -0.62), (0, 0, 0.12)], 0.03, M["wood"], n=8)
    box(-0.16, -0.07, 0.08, 0.16, 0.07, 0.24, M["iron"], bevel=0.02)
    place(nn, loc=(x, y, -0.25), rot=(0, 18, 0))
    nn = mark()
    box(-0.28, -0.1, 0.0, 0.18, 0.1, 0.1, M["iron"], bevel=0.02)
    mesh("horn", [(-0.28, -0.08, 0.1), (-0.44, -0.0, 0.08), (-0.28, 0.08, 0.1), (-0.28, 0.0, 0.0)],
         [(0, 1, 2), (0, 3, 1), (1, 3, 2), (0, 2, 3)], M["iron"])
    box(-0.12, -0.07, -0.18, 0.04, 0.07, 0.0, M["iron"], bevel=0.01)
    place(nn, loc=(-0.5, -0.55, -0.6))
    blob(-0.5, -0.62, -0.42, 0.06, 0.03, 0.04, M["glow"], segs=10, rings=6)


def a_isis(M):
    """Tripartite black wig with gold bands, a red sun disc between gold cow horns, a uraeus."""
    blob(0, -0.1, 0.36, 0.28, 0.29, 0.33, M["hair_black"], segs=24, rings=14, name="wig_top")
    for s in (-1, 1):
        box(s * 0.24 - 0.08, -0.36, -0.3, s * 0.24 + 0.08, -0.16, 0.3, M["hair_black"], bevel=0.04)
        for k in range(4):
            box(s * 0.24 - 0.085, -0.37, -0.26 + 0.15 * k, s * 0.24 + 0.085, -0.15, -0.24 + 0.15 * k, M["gilt"],
                bevel=0.005)
    torus(0, 0.6, 0.2, 0.025, M["gilt"], y=-0.2, n=40)
    disc(0, 0.8, 0.12, -0.26, -0.2, M["sun_disc"], n=40)
    torus(0, 0.8, 0.12, 0.022, M["gilt"], y=-0.23, n=40)
    for s in (-1, 1):
        sweep([(s * 0.05, -0.22, 0.62), (s * 0.19, -0.22, 0.7), (s * 0.2, -0.22, 0.86), (s * 0.13, -0.22, 0.94)],
              [0.035, 0.03, 0.022, 0.01], M["gilt"], n=8)
    blob(0, -0.49, 0.5, 0.04, 0.03, 0.05, M["gilt"], segs=10, rings=6, name="uraeus")


def a_serapis(M):
    lathe([(0.14, 0.52), (0.18, 0.62), (0.24, 0.84), (0.25, 0.86), (0.0, 0.86)], M["gilt"], x=0, y=-0.2, n=28,
          bottom=False, name="modius")
    for k in range(6):
        a = math.radians(-60 + 24 * k)
        nn = mark()
        leaf(0.12, 0.045, M["tunic_green"])
        place(nn, loc=(0.22 * math.sin(a), -0.2 - 0.22 * math.cos(a) - 0.01, 0.62), rot=(0, -90, 0))



def a_sceptre(M):
    sweep([(0.62, -0.42, -0.85), (0.62, -0.42, 0.3)], 0.026, M["gilt"], n=8)
    blob(0.62, -0.42, 0.34, 0.06, 0.06, 0.06, M["gilt"], segs=12, rings=8)


def a_divus(M):
    for s in (-1, 1):
        laurel(0, 0.32, 0.27, 180 if s < 0 else 0, 95 if s < 0 else 85, M["gilt"], pairs=5, L=0.13, W=0.05,
               y=-0.27)
    for s in (-1, 1):
        sweep([(s * 0.24, -0.1, 0.3), (s * 0.27, 0.05, 0.25), (s * 0.2, 0.15, 0.1)], 0.02, M["red"], n=6,
              flat=0.3)


# Advisors: painted busts with an attribute.

def advisor(colour, cloth, hair, attr, head_attr=None, beard=False, skin="skin", trim=None, hair_style="short"):
    def fn(M):
        medallion(M, colour, "ground")
        nn = mark()
        head(M, M[skin], M[hair] if hair else None, beard=beard, eyes="paint", style_=hair_style)
        if head_attr:
            head_attr(M)
        big_head(nn)
        neck(M, M[skin])
        chest(M, M[cloth])
        drape(M, M[cloth], [(-0.62, -0.42), (-0.3, -0.5), (0.0, -0.66), (0.25, -0.88)], r=0.085)
        if trim:
            drape(M, M[trim], [(-0.6, -0.52), (-0.28, -0.6), (0.0, -0.77), (0.2, -0.92)], r=0.035)
        attr(M)
        facing(0, tilt=0)
    return fn


def b_quaestor(M):
    x, y, z = 0.55, -0.5, -0.5
    lathe([(0.0, 0.0), (0.12, 0.02), (0.17, 0.12), (0.15, 0.24), (0.07, 0.32), (0.05, 0.34)], M["leather"],
          x=x, y=y, n=20, top=False)
    xform(since(0)[-1:], Matrix.Translation((0, 0, z)))
    torus(x, z + 0.32, 0.06, 0.015, M["twine"], y=y, n=16)
    for k in range(4):
        coin(-0.55 + 0.004 * (k % 2), -0.5, -0.78 + k * 0.03, M["gold"], r=0.11)
    nn = mark()
    coin(0, 0, 0, M["gold"], r=0.11)
    place(nn, loc=(-0.5, -0.6, -0.5), rot=(80, 0, 0))


def b_engineer(M):
    # groma: a staff with a cross on top, plumb bobs hanging from the arms
    x, y = 0.58, -0.45
    sweep([(x, y, -0.85), (x, y, 0.55)], 0.022, M["wood"], n=8)
    sweep([(x - 0.3, y, 0.55), (x + 0.3, y, 0.55)], 0.02, M["wood_dark"], n=8)
    for s in (-1, 1):
        sweep([(x + s * 0.28, y, 0.55), (x + s * 0.28, y, 0.25)], 0.004, M["twine"], n=4)
        nn = mark()
        lathe([(0.0, 0.0), (0.04, 0.05), (0.03, 0.08), (0.0, 0.09)], M["bronze_ui"], n=12)
        place(nn, loc=(x + s * 0.28, y, 0.17))
    # dividers on the left
    for s in (-1, 1):
        rod((-0.58, -0.5, 0.15), (-0.58 + s * 0.16, -0.5, -0.5), 0.022, M["bronze_ui"], n=8, r1=0.006)
    blob(-0.58, -0.5, 0.16, 0.045, 0.045, 0.045, M["bronze_ui"], segs=10, rings=6)


def b_haruspex(M):
    """A lituus: a gilt staff curled into a spiral at the top."""
    x, y, r0, z0 = 0.62, -0.48, 0.13, 0.32
    curl = [(x, y, -0.85)]
    for i in range(25):
        a = math.radians(18 * i)
        r = r0 * (1 - 18 * i / 760)
        curl.append((x - r0 + r * math.cos(a), y, z0 + r * math.sin(a)))
    sweep(curl, 0.026, M["gilt"], n=8)


def b_legate_helmet(M):
    """Attic helmet with a transverse red crest."""
    lathe([(0.0, 0.62), (0.12, 0.61), (0.21, 0.56), (0.26, 0.46), (0.27, 0.36)], M["helmet"], x=0, y=-0.24,
          n=32, bottom=False, top=False, name="helm")
    blob(0, -0.48, 0.4, 0.2, 0.05, 0.04, M["gilt"], segs=12, rings=6)
    for s in (-1, 1):
        mesh("cheek", [(s * 0.25, -0.3, 0.38), (s * 0.24, -0.42, 0.36), (s * 0.2, -0.42, 0.1),
                       (s * 0.23, -0.3, 0.08)], [(0, 1, 2, 3)], M["helmet"], solidify=0.02)
    blob(0, -0.24, 0.68, 0.32, 0.07, 0.16, M["crest"], segs=16, rings=10, name="crest")


def b_legate(M):
    """A red cloak over the bronze cuirass, pinned with a gold brooch."""
    for s in (-1, 1):
        sweep([(s * 0.32, -0.15, -0.36), (s * 0.58, -0.2, -0.5), (s * 0.7, -0.25, -0.8)],
              [0.12, 0.14, 0.15], M["red"], n=12)
    blob(-0.34, -0.38, -0.4, 0.06, 0.03, 0.06, M["gilt"], segs=10, rings=6, name="fibula")


def b_aedile(M):
    nn = mark()
    cylinder(0.0, 0.0, -0.32, 0.32, 0.08, M["parch"], n=16, axis="x")
    for s in (-1, 1):
        cylinder(0.0, 0.0, s * 0.32, s * 0.38, 0.03, M["wood_dark"], n=10, axis="x")
        blob(s * 0.4, 0.0, 0.0, 0.035, 0.035, 0.035, M["wood_dark"], segs=10, rings=6)
    box(-0.03, -0.085, -0.08, 0.03, 0.085, 0.08, M["red"], bevel=0.004)
    place(nn, loc=(0.42, -0.55, -0.55), rot=(0, 55, 0))


# ---------------------------------------------------------------- logo

def logo(M):
    t = text_mesh("URBS", 1.0, 0.1, 0.04, M["marble_logo"], spacing=1.12, name="urbs")
    # a dark bronze copy behind, shifted down and right: the letters read as raised on any ground
    text_mesh("URBS", 1.0, 0.1, 0.04, M["bronze_dk"], spacing=1.12, name="urbs_shadow", x=0.035,
              z=-0.04, y=0.06)
    bpy.context.view_layer.update()
    xs = [(t.matrix_world @ v.co).x for v in t.data.vertices]
    half = (max(xs) - min(xs)) / 2
    zs = [(t.matrix_world @ v.co).z for v in t.data.vertices]
    zc = (max(zs) + min(zs)) / 2
    for s in (-1, 1):
        laurel(s * (half + 0.12), zc, 0.48, -85 if s > 0 else 265, 75 if s > 0 else 105, M["gilt"], pairs=7,
               L=0.2, W=0.065)
    box(-half, -0.04, zc - 0.62, half, 0.06, zc - 0.58, M["gilt"], bevel=0.01)
    facing(0, tilt=0)


# ---------------------------------------------------------------- registry

HUD = {"population": i_population, "happiness": i_happiness, "culture": i_culture,
       "prosperity": i_prosperity, "security": i_security, "favor": i_favor, "build": i_build,
       "rome": i_rome, "advisors": i_advisors, "finance": i_finance, "goods": i_goods, "trade": i_trade,
       "labor": i_labor, "religion": i_religion, "military": i_military, "research": i_research,
       "alerts": i_alerts, "help": i_help, "menu": i_menu, "road": i_road, "demolish": i_demolish,
       "overlay": i_overlay}

GODS = {
    "jupiter": god("#1c3570", beard=True, attr=a_thunderbolt, head_attr=a_jupiter_crown),
    "juno": god("#563a88", female=True, attr=a_peacock, head_attr=a_diadem),
    "mars": god("#8a1e1e", head_attr=a_mars_helmet, attr=a_spear),
    "venus": god("#b04e6a", female=True, attr=a_shell),
    "minerva": god("#3c4a5e", female=True, head_attr=a_minerva, attr=a_owl),
    "mercury": god("#b8721e", head_attr=a_mercury, attr=a_caduceus),
    "neptune": god("#1f6c9c", beard=True, attr=a_trident),
    "vesta": god("#33244a", female=True, head_attr=a_veil, attr=a_flame),
    "ceres": god("#346a30", female=True, head_attr=a_wheat_crown, attr=a_sheaves),
    "vulcan": god("#33302e", beard=True, head_attr=a_vulcan, attr=a_tools),
    "isis": god("#177a7a", female=True, head_attr=a_isis),
    "serapis": god("#9c7438", beard=True, head_attr=a_serapis, attr=a_sceptre),
    "divus": god("#5e1236", head_attr=a_divus, trim="purple"),
}

ADVISORS = {
    "quaestor": advisor("#2a5e3c", "toga", "hair_grey", b_quaestor),
    "engineer": advisor("#7a5a2e", "tunic_ochre", "hair_dk", b_engineer, beard=True, skin="skin_dk"),
    "haruspex": advisor("#4e2860", "veil", None, b_haruspex, head_attr=lambda M: a_veil(M, "veil")),
    "legate": advisor("#6a1a1a", "bronze_ui", None, b_legate, head_attr=b_legate_helmet),
    "aedile": advisor("#244478", "toga", "hair_dk", b_aedile, trim="purple"),
}

SPRITES = {}
for _id, _fn in HUD.items():
    SPRITES["ui/icon_%s" % _id] = sprite(_fn, ICON, thick=2.6 if _id == "menu" else 1.8,
                                         key=0.6 if _id == "menu" else 0.7)
for _id in ("pause", "play", "fast", "faster"):
    SPRITES["ui/icon_%s" % _id] = sprite(glyphs(_id), ICON, thick=2.6, key=0.6, span=1.62)
for _id, _fn in GODS.items():
    SPRITES["ui/god_%s" % _id] = sprite(_fn, PORT, margin=2, thick=1.5, key=1.2)
for _id, _fn in ADVISORS.items():
    SPRITES["ui/advisor_%s" % _id] = sprite(_fn, PORT, margin=2, thick=1.5, key=1.2)
SPRITES["ui/logo"] = sprite(logo, LOGO[0], LOGO[1], margin=6, thick=2.0, key=1.2)
