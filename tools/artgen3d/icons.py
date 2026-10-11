"""Iso icons: goods and build-menu categories (family "icons").

    Blender -b --python tools/artgen3d/run.py -- icons [names] --samples 64

Every icon is a small 3D model in the world-art look (same camera, sun and palette), scaled to
fill a fixed square canvas and centred on it (anchor = the canvas centre):

  goods/<id>_16   64 x 64   UI icon (panels, chips, tooltips; drawn at ~20-30 points)
  goods/<id>      40 x 40   carried-goods icon over walking citizens (drawn ~20 iso px wide)
  ui/cat_<id>     96 x 96   build-menu category rail (drawn at 24-48 points): a miniature on a
                            small diamond of the category's ground

A thin dark contour (Freestyle) keeps the silhouettes readable at small sizes.  The names are
the legacy pixel-art names; tools/atlas/pack.py prefers these over assets/sprites/goods and ui.
"""
import math

import bpy
from mathutils import Matrix, Vector

import kit
from kit import blob, box, cylinder, material, mesh

UI, WORLD, CAT = 64, 40, 96


# ---------------------------------------------------------------- materials

def glass(name, colour, alpha=0.55):
    if name in kit._MATS:
        return kit._MATS[name]
    mat = bpy.data.materials.new(name)
    nt = kit._nodes(mat)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    b = nt.nodes.new("ShaderNodeBsdfPrincipled")
    b.inputs["Base Color"].default_value = kit.srgb(colour)
    b.inputs["Roughness"].default_value = 0.05
    b.inputs["Transmission Weight"].default_value = 0.7
    b.inputs["IOR"].default_value = 1.45
    b.inputs["Alpha"].default_value = alpha
    nt.links.new(b.outputs["BSDF"], out.inputs["Surface"])
    kit._MATS[name] = mat
    return mat


def veined(name, base, vein):
    """Polished stone with dark wandering veins (wave texture bands, heavily distorted)."""
    if name in kit._MATS:
        return kit._MATS[name]
    mat = bpy.data.materials.new(name)
    nt = kit._nodes(mat)
    N, L = nt.nodes, nt.links
    out = N.new("ShaderNodeOutputMaterial")
    b = N.new("ShaderNodeBsdfPrincipled")
    b.inputs["Roughness"].default_value = 0.2
    uv = N.new("ShaderNodeUVMap")
    w = N.new("ShaderNodeTexWave")
    w.inputs["Scale"].default_value = 1.1
    w.inputs["Distortion"].default_value = 7.0
    w.inputs["Detail"].default_value = 4.0
    L.new(uv.outputs["UV"], w.inputs["Vector"])
    r = N.new("ShaderNodeValToRGB")
    r.color_ramp.elements[0].position = 0.0
    r.color_ramp.elements[0].color = kit.srgb(vein)
    r.color_ramp.elements[1].position = 0.07
    r.color_ramp.elements[1].color = kit.srgb(base)
    L.new(w.outputs["Fac"], r.inputs["Fac"])
    L.new(r.outputs["Color"], b.inputs["Base Color"])
    L.new(b.outputs["BSDF"], out.inputs["Surface"])
    kit._MATS[name] = mat
    return mat


def mats(M):
    m = material
    M["linen"] = m("linen", "#f1e6cc", "#e2d4b4", scale=30, bump=0.15)
    M["flour_sack"] = m("flour_sack", "#efe8d8", "#ddd3bd", scale=30, bump=0.2)
    M["flour"] = m("flour", "#fbf8f0", "#efeae0", scale=20, bump=0.1)
    M["burlap"] = m("burlap", "#c9a468", "#b08a52", scale=40, bump=0.3)
    M["straw_gold"] = m("straw_gold", "#e4c25a", "#c9a23e", scale=40, bump=0.3)
    M["ear"] = m("ear", "#d9a93c", "#bf8e2c", scale=60, bump=0.6)
    M["twine"] = m("twine", "#8a5a2c", "#734822", scale=30, bump=0.1)
    M["wicker"] = m("wicker", "#a8783e", "#8a602e", kind="brick", brick=(0.06, 0.025),
                    mortar="#5e4020", scale=10, bump=0.4)
    M["cabbage"] = m("cabbage", "#7fbf4a", "#5e9e34", scale=18, bump=0.4)
    M["carrot"] = m("carrot", "#ec8a24", "#d4721a", scale=25, bump=0.2)
    M["leafy"] = m("leafy", "#4f9a35", "#3c7e28", scale=25, bump=0.2)
    M["turnip"] = m("turnip", "#e9dcd8", "#c890b8", scale=8, bump=0.1)
    M["fish"] = m("fish", "#b8c7d2", "#8ea2b2", scale=30, rough=0.25, metal=0.35, bump=0.1)
    M["fish_back"] = m("fish_back", "#4f6f86", "#3e5a70", scale=30, rough=0.3, metal=0.3, bump=0.1)
    M["ham"] = m("ham", "#c9605a", "#b04a46", scale=25, rough=0.5, bump=0.15)
    M["fat"] = m("fat", "#f3dcc8", "#e8c8b0", scale=25, rough=0.5, bump=0.05)
    M["bone"] = m("bone", "#f2ecdc", "#e0d8c4", scale=20, bump=0.05)
    M["olive_fruit"] = m("olive_fruit", "#6e8a2a", "#58721e", scale=40, rough=0.3, bump=0.05)
    M["olive_black"] = m("olive_black", "#3a2a3a", "#2a1e2c", scale=40, rough=0.3, bump=0.05)
    M["olive_leaf"] = m("olive_leaf_i", "#8a9a5e", "#6c7d46", scale=30, bump=0.1)
    M["grape"] = m("grape_i", "#6a2f78", "#4e2058", scale=40, rough=0.35, bump=0.05)
    M["vine_leaf"] = m("vine_leaf", "#5e9a32", "#4a8028", scale=30, bump=0.2)
    M["stem"] = m("stem", "#6a5030", "#54401f", scale=30, bump=0.1)
    M["crust"] = m("crust", "#c47a30", "#a85e22", scale=18, bump=0.25)
    M["crumb"] = m("crumb", "#ecd29a", "#dcbc7e", scale=30, bump=0.2)
    M["terracotta"] = m("terracotta_i", "#c8693a", "#b05a30", scale=12, rough=0.6, bump=0.05)
    M["terracotta_dk"] = m("terracotta_dk", "#9a4a28", "#843e20", scale=12, rough=0.6, bump=0.05)
    M["oil_jar"] = m("oil_jar", "#b49a4c", "#9c853c", scale=12, rough=0.55, bump=0.05)
    M["band_red"] = m("band_red_i", "#8e1c22", "#76141a", scale=20, rough=0.5, bump=0)
    M["band_green"] = m("band_green", "#5a7a1e", "#4a6818", scale=20, rough=0.5, bump=0)
    M["wax_red"] = m("wax_red", "#7a1020", "#5c0a18", scale=20, rough=0.35, bump=0.05)
    M["oil_gold"] = m("oil_gold", "#d8c040", "#bca630", scale=20, rough=0.2, bump=0)
    M["cork"] = m("cork", "#a88458", "#8c6c44", scale=30, bump=0.2)
    M["salt"] = m("salt", "#fbfbf8", "#e6eaee", scale=50, rough=0.3, bump=0.5)
    M["sand"] = m("sand_i", "#e2c588", "#cdae70", scale=50, bump=0.5)
    M["ash"] = m("ash", "#6e5250", "#5a4040", scale=50, bump=0.6)
    M["garum"] = m("garum", "#5a2c14", "#46200e", scale=20, rough=0.2, bump=0)
    M["log"] = m("log_i", "#7a5434", "#634428", scale=30, bump=0.3)
    M["endgrain"] = m("endgrain", "#e0b878", "#c89c5c", scale=30, bump=0.2)
    M["plank"] = m("plank", "#d2a468", "#b88a52", scale=14, bump=0.15)
    M["clay"] = m("clay_i", "#9a4a2e", "#843c24", scale=14, rough=0.3, bump=0.3)
    M["brick"] = m("brick_i", "#c4603a", "#a84e2e", scale=25, bump=0.3)
    M["rock"] = m("rock", "#a49c8e", "#878074", scale=14, bump=0.6)
    M["ashlar"] = m("ashlar", "#c4a874", "#ae925e", scale=14, bump=0.3)
    M["limestone"] = m("limestone", "#eadba8", "#d6c48c", scale=14, bump=0.5)
    M["lime"] = m("lime", "#fafaf6", "#eeeee8", scale=30, bump=0.2)
    M["concrete"] = m("concrete", "#b4b0a8", "#9c978e", scale=30, bump=0.4)
    M["sigillata"] = m("sigillata", "#b8442a", "#a03a24", scale=12, rough=0.25, bump=0.02)
    M["marble"] = m("marble_i", "#f8f6f2", "#e4e0da", scale=4, rough=0.25, bump=0.02)
    M["marble_veined"] = veined("marble_veined", "#f4f2ee", "#9aa0aa")
    M["iron_ore"] = m("iron_ore", "#6a4838", "#4a3830", scale=18, bump=0.6)
    M["rust"] = m("rust", "#a8542c", "#8a4224", scale=30, bump=0.4)
    M["silver_ore"] = m("silver_ore", "#4c5260", "#3a3f4a", scale=18, bump=0.6)
    M["iron"] = m("iron_i", "#5a5d64", "#46484e", scale=20, rough=0.4, metal=0.75, bump=0.05)
    M["steel"] = m("steel", "#c8ccd4", "#aeb4bc", scale=20, rough=0.2, metal=0.9, bump=0)
    M["helmet"] = m("helmet", "#9aa0aa", "#868c96", scale=20, rough=0.35, metal=0.8, bump=0)
    M["silver"] = m("silver_i", "#e4e8ee", "#c8ccd4", scale=20, rough=0.18, metal=0.95, bump=0)
    M["gold"] = m("gold_i", "#f2c440", "#d8a42a", scale=20, rough=0.22, metal=0.95, bump=0)
    M["bronze"] = m("bronze", "#c08a48", "#a87438", scale=20, rough=0.3, metal=0.85, bump=0)
    M["wood"] = m("wood_i", "#8a5e36", "#704a2a", scale=20, bump=0.15)
    M["wood_dark"] = m("wood_dark", "#5a3a22", "#482e1a", scale=20, bump=0.15)
    M["red"] = m("red_i", "#b8262a", "#9e1e22", scale=20, rough=0.6, bump=0.05)
    M["crest"] = m("crest", "#d02a28", "#b01e1e", scale=60, bump=0.6)
    M["wool"] = m("wool_i", "#f6f2e8", "#e2dccc", scale=30, bump=0.8)
    M["yarn"] = m("yarn", "#e8d6b0", "#d4bf96", kind="stripes", stripe=0.012, bump=0.1)
    M["purple"] = m("purple", "#7a2a8e", "#64207a", scale=20, rough=0.6, bump=0.1)
    M["crimson"] = m("crimson", "#c42438", "#a81c2e", scale=20, rough=0.6, bump=0.1)
    M["saffron"] = m("saffron", "#f0a820", "#d89018", scale=20, rough=0.6, bump=0.1)
    M["indigo"] = m("indigo", "#2c4a9a", "#223c80", scale=20, rough=0.6, bump=0.1)
    M["gold_trim"] = m("gold_trim", "#e8b840", "#d0a030", scale=20, rough=0.4, metal=0.5, bump=0)
    M["glass"] = glass("glass", "#7fd0c8")
    M["glass_blue"] = glass("glass_blue", "#4f8fd8")
    M["papyrus"] = m("papyrus", "#e8d8a4", "#d4c08a", scale=20, bump=0.1)
    M["reed"] = m("reed", "#6a9a3a", "#56842c", scale=30, bump=0.1)
    M["tuft"] = m("tuft", "#8ab848", "#70a03a", scale=40, bump=0.4)
    M["parchment"] = m("parchment", "#efe0b4", "#ddca98", scale=20, bump=0.05)
    M["gem_red"] = m("gem_red", "#d81830", "#b01024", scale=20, rough=0.1, bump=0)
    M["gem_blue"] = m("gem_blue", "#2a6ad8", "#1c52b0", scale=20, rough=0.1, bump=0)
    M["gem_green"] = m("gem_green", "#20a060", "#188048", scale=20, rough=0.1, bump=0)
    M["leather"] = m("leather", "#8a5a30", "#704826", scale=20, bump=0.1)
    M["water"] = m("water_i", "#4aa8c8", "#3a90b4", scale=6, rough=0.08, bump=0.03)
    M["water_green"] = m("water_green", "#2aa8a0", "#1e9088", scale=6, rough=0.08, bump=0.03)
    M["patina"] = m("patina", "#5fa08a", "#4e8a76", kind="tiles", scale=7, brick=(0.06, 0.05),
                    bump=0.4)
    M["banner"] = m("banner", "#b82024", "#981a1e", scale=20, bump=0.05)
    M["bread_ochre"] = m("bread_ochre", "#c99040", "#b07a30", scale=20, bump=0.1)
    M["fruit_red"] = m("fruit_red_i", "#d0402a", "#b03020", scale=30, rough=0.4, bump=0.05)
    M["fruit_yellow"] = m("fruit_yellow", "#e8c030", "#d0a820", scale=30, rough=0.4, bump=0.05)
    M["basalt"] = m("basalt", "#6e6b68", "#5a5755", kind="brick", scale=10, brick=(0.16, 0.12),
                    mortar="#3e3c3a", bump=0.5)
    M["kerb"] = m("kerb", "#b8b0a0", "#a0988a", scale=12, bump=0.2)
    M["wheat_field"] = m("wheat_field", "#e0bc58", "#c8a040", scale=60, bump=0.9)
    M["soil"] = m("soil_i", "#7a5636", "#644428", scale=16, bump=0.4)
    M["crate"] = m("crate_i", "#7a5a3a", "#664a2e", kind="brick", scale=10, brick=(0.5, 0.06),
                   mortar="#4a3420", bump=0.2)
    M["stone_grey"] = m("stone_grey_i", "#bdb6a8", "#a7a093", kind="brick", scale=12,
                        brick=(0.24, 0.11), mortar="#868073", bump=0.3)
    M["saffron_awning"] = m("saffron_awning", "#f4a814", "#d2780e", kind="stripes", stripe=0.07, bump=0,
                            rough=0.9)
    M["stripe"] = m("stripe_i", "#c8302a", "#f3e6cc", kind="stripes", stripe=0.05, bump=0, rough=0.9)
    M["seat"] = m("seat", "#d8ccb0", "#c4b898", kind="brick", scale=12, brick=(0.2, 0.05),
                  mortar="#a09478", bump=0.2)
    return M


# ---------------------------------------------------------------- geometry helpers

def mark():
    return len(bpy.context.scene.objects)


def since(n):
    return [o for o in list(bpy.context.scene.objects)[n:] if o.type == "MESH"]


def xform(objs, mx):
    bpy.context.view_layer.update()
    for o in objs:
        o.matrix_world = mx @ o.matrix_world


def place(n, loc=(0, 0, 0), rot=(0, 0, 0), scale=1.0):
    """Rotate (degrees, XYZ about the origin), scale and move everything made since mark n."""
    mx = (Matrix.Translation(Vector(loc)) @ Matrix.Rotation(math.radians(rot[2]), 4, "Z")
          @ Matrix.Rotation(math.radians(rot[1]), 4, "Y") @ Matrix.Rotation(math.radians(rot[0]), 4, "X")
          @ Matrix.Scale(scale, 4))
    xform(since(n), mx)


def lathe(prof, mat, x=0.0, y=0.0, n=24, name="lathe", smooth=True, top=True, bottom=True):
    """Surface of revolution about the z axis at (x, y); prof = [(r, z), ...] bottom to top."""
    v, f = [], []
    for r, z in prof:
        for j in range(n):
            a = 2 * math.pi * j / n
            v.append((x + r * math.cos(a), y + r * math.sin(a), z))
    for i in range(len(prof) - 1):
        for j in range(n):
            a, b = i * n + j, i * n + (j + 1) % n
            f.append((a, b, b + n, a + n))
    if bottom and prof[0][0] > 1e-4:
        f.append(tuple(reversed(range(n))))
    if top and prof[-1][0] > 1e-4:
        f.append(tuple((len(prof) - 1) * n + j for j in range(n)))
    ob = mesh(name, v, f, mat)
    for p in ob.data.polygons:
        p.use_smooth = smooth and len(p.vertices) == 4
    return ob


def rod(p0, p1, r, mat, n=10, name="rod", r1=None):
    """Cylinder (or cone, r1) between two points."""
    p0, p1 = Vector(p0), Vector(p1)
    d = (p1 - p0)
    L = d.length
    q = Vector((0, 0, 1)).rotation_difference(d.normalized())
    nn = mark()
    lathe([(r, 0), (r1 if r1 is not None else r, L)], mat, n=n, name=name)
    xform(since(nn), Matrix.Translation(p0) @ q.to_matrix().to_4x4())
    return since(nn)[0]


def rock(x, y, z, s, mat, seed=0, flat=0.7):
    """Irregular stone: a low-poly ellipsoid with jittered vertices."""
    ob = blob(x, y, z, s, s * 0.9, s * flat, mat, segs=9, rings=6, name="rock")
    k = seed * 7.13 + 1.7
    for i, vtx in enumerate(ob.data.vertices):
        j = math.sin(i * 12.9898 + k) * 43758.5453
        j = j - math.floor(j)
        c = vtx.co
        c.x = x + (c.x - x) * (0.82 + 0.36 * j)
        c.y = y + (c.y - y) * (0.82 + 0.36 * ((j * 7.3) % 1))
        c.z = z + (c.z - z) * (0.85 + 0.3 * ((j * 3.1) % 1))
    for p in ob.data.polygons:
        p.use_smooth = False
    return ob


def cone_heap(x, y, r, h, mat, n=28):
    return lathe([(r, 0), (r * 0.82, h * 0.25), (r * 0.5, h * 0.66), (r * 0.18, h * 0.93), (0.0, h)],
                 mat, x=x, y=y, n=n, name="heap")


def screen_basis():
    """Matrix mapping local (x right, y into the screen, z up) to world: lay things in the view plane."""
    R, U, V = kit.RIGHT, kit.UP, kit.VIEW
    return Matrix(((R.x, -V.x, U.x, 0), (R.y, -V.y, U.y, 0), (R.z, -V.z, U.z, 0), (0, 0, 0, 1)))


def facing(n, tilt=25.0, spin=0.0):
    """Turn everything made since mark n (modelled lying in local x/z, facing -y) toward the
    camera, tipped back by `tilt` degrees so the light rakes across it."""
    mx = screen_basis() @ Matrix.Rotation(math.radians(-tilt), 4, "X") @ Matrix.Rotation(
        math.radians(spin), 4, "Y")
    xform(since(n), mx)


def diamond(mat, s=1.0, z=0.0, curb=None, name="ground"):
    """A one-tile ground diamond (the category icons stand on one)."""
    h = s / 2
    ob = mesh(name, [(-h, -h, z), (h, -h, z), (h, h, z), (-h, h, z)], [(0, 1, 2, 3)], mat)
    box(-h, -h, z - 0.07, h, h, z - 0.004, curb or mat, bevel=0.006, name="slab")
    return ob


# ---------------------------------------------------------------- framing and style

def fit(size, margin):
    """Scale and move the scene so its screen extent fills the size x size canvas, centred."""
    bpy.context.view_layer.update()
    s = bpy.context.scene
    r0, r1, u0, u1 = kit._screen_extent(s)
    span = max(r1 - r0, u1 - u0, 1e-6)
    k = (size - 2 * margin) / kit.PPU / span
    rc, uc = (r0 + r1) / 2, (u0 + u1) / 2
    t = -(kit.RIGHT * rc + kit.UP * uc) * k
    xform([o for o in s.objects if o.type == "MESH"], Matrix.Translation(t) @ Matrix.Scale(k, 4))


def outline(size):
    """Freestyle contour: a thin warm-dark line around the silhouette and major creases."""
    s = bpy.context.scene
    s.render.use_freestyle = True
    s.render.line_thickness_mode = "ABSOLUTE"
    s.render.line_thickness = 1.0
    vl = s.view_layers[0]
    vl.use_freestyle = True
    fs = vl.freestyle_settings
    fs.as_render_pass = False
    ls = fs.linesets[0] if len(fs.linesets) else fs.linesets.new("contour")
    ls.select_by_visibility = True
    ls.visibility = "VISIBLE"
    ls.select_by_edge_types = True
    for k in ("select_silhouette", "select_border", "select_crease", "select_contour",
              "select_external_contour"):
        setattr(ls, k, False)
    ls.select_silhouette = True
    ls.select_border = True
    ls.select_external_contour = True
    st = ls.linestyle or bpy.data.linestyles.new("contour")
    ls.linestyle = st
    st.color = (0.13, 0.08, 0.05)
    st.alpha = 0.85
    st.thickness = {UI: 1.1, WORLD: 1.0, CAT: 1.3}.get(size, 1.0)
    st.thickness_position = "INSIDE"
    s.cycles.film_transparent_glass = True


def icon(fn, size, margin=None):
    def build(M):
        mats(M)
        fn(M)
        fit(size, margin if margin is not None else (2 if size <= WORLD else 3))
        outline(size)
    return build


# ---------------------------------------------------------------- goods

def g_wheat(M):
    """A sheaf: golden stalks tied at the waist, ears fanning out at the top."""
    for ring, cnt in ((0.0, 1), (0.035, 7), (0.07, 12)):
        for j in range(cnt):
            a = 2 * math.pi * j / cnt + ring * 9
            bx, by = ring * math.cos(a), ring * math.sin(a)
            tx, ty = bx * 2.6, by * 2.6
            rod((bx * 1.4, by * 1.4, 0), (bx * 0.6, by * 0.6, 0.3), 0.012, M["straw_gold"], n=5)
            rod((bx * 0.6, by * 0.6, 0.3), (tx, ty, 0.62), 0.012, M["straw_gold"], n=5)
            d = Vector((tx - bx * 0.6, ty - by * 0.6, 0.32)).normalized()
            c = Vector((tx, ty, 0.62)) + d * 0.06
            ob = blob(0, 0, 0, 0.022, 0.022, 0.075, M["ear"], segs=6, rings=5, name="ear")
            q = Vector((0, 0, 1)).rotation_difference(d)
            ob.matrix_world = Matrix.Translation(c) @ q.to_matrix().to_4x4()
    cylinder(0, 0, 0.27, 0.33, 0.062, M["twine"], n=16)


def g_vegetables(M):
    """Wicker basket heaped with a cabbage, carrots and a turnip."""
    lathe([(0.2, 0.0), (0.24, 0.04), (0.29, 0.16), (0.3, 0.17)], M["wicker"], n=24, top=False)
    lathe([(0.29, 0.17), (0.3, 0.16), (0.02, 0.15)], M["wicker"], n=24)
    blob(-0.06, 0.06, 0.25, 0.15, 0.15, 0.13, M["cabbage"], segs=14, rings=8)
    for a in range(5):
        ang = a * 1.3
        blob(-0.06 + 0.1 * math.cos(ang), 0.06 + 0.1 * math.sin(ang), 0.22, 0.09, 0.07, 0.06,
             M["leafy"], segs=8, rings=5)
    for i, (x, y, rz) in enumerate([(0.12, -0.12, -20), (0.17, -0.02, 10), (0.06, -0.18, -40)]):
        nn = mark()
        rod((0, 0, 0), (0, 0, 0.26), 0.035, M["carrot"], n=10, r1=0.004)
        for k in range(3):
            rod((0, 0, -0.01), (0.03 * (k - 1), 0.02, -0.09), 0.008, M["leafy"], n=5)
        place(nn, loc=(x, y, 0.26), rot=(80, 0, 120 + rz))
    blob(-0.2, -0.12, 0.2, 0.08, 0.08, 0.075, M["turnip"], segs=12, rings=7)
    rod((-0.2, -0.12, 0.27), (-0.22, -0.14, 0.35), 0.01, M["leafy"], n=5)


def g_fish(M):
    """Two fish, silver bellies and blue backs, shown side on."""
    def fish(x, z, ang, s=1.0, y=0.0):
        nn = mark()
        blob(0, 0, 0, 0.26, 0.045, 0.09, M["fish"], segs=16, rings=8)
        blob(0.0, 0.0, 0.035, 0.24, 0.048, 0.06, M["fish_back"], segs=14, rings=6, top_only=True)
        mesh("tail", [(-0.22, 0, 0), (-0.36, 0, 0.1), (-0.32, 0, 0), (-0.36, 0, -0.09)],
             [(0, 1, 2, 3)], M["fish_back"], solidify=0.012)
        mesh("fin", [(-0.06, 0, 0.07), (0.06, 0, 0.08), (-0.1, 0, 0.14)], [(0, 1, 2)], M["fish_back"],
             solidify=0.008)
        blob(0.18, -0.04, 0.02, 0.018, 0.012, 0.018, M["dark"], segs=6, rings=4)
        place(nn, loc=(x, y, z), rot=(0, ang, 0), scale=s)
    nn = mark()
    fish(0.02, 0.1, 12)
    fish(-0.02, -0.08, -8, 0.95, y=-0.03)
    facing(nn, tilt=15)


def g_meat(M):
    """A ham: pink cured leg with a fat rim and a white bone end."""
    nn = mark()
    blob(0, 0, 0, 0.2, 0.15, 0.14, M["ham"], segs=16, rings=10)
    blob(-0.02, 0, 0.02, 0.2, 0.152, 0.13, M["fat"], segs=16, rings=10, top_only=True)
    blob(-0.02, 0, 0.035, 0.17, 0.13, 0.12, M["ham"], segs=16, rings=10, top_only=True)
    rod((0.15, 0, 0), (0.34, 0, 0.02), 0.035, M["bone"], n=10)
    blob(0.35, 0, 0.02, 0.05, 0.05, 0.045, M["bone"], segs=8, rings=6)
    place(nn, loc=(0, 0, 0.14), rot=(0, -15, 40))


def g_olives(M):
    """Terracotta bowl heaped with green and black olives, a sprig of leaves."""
    lathe([(0.12, 0), (0.18, 0.04), (0.26, 0.12), (0.27, 0.13)], M["terracotta"], n=24, top=False)
    lathe([(0.255, 0.12), (0.02, 0.1)], M["terracotta_dk"], n=24, bottom=False, top=False)
    k = 0
    for ring, cnt, z in ((0.0, 1, 0.21), (0.08, 6, 0.18), (0.16, 11, 0.145), (0.215, 15, 0.125)):
        for j in range(cnt):
            a = 2 * math.pi * j / cnt + ring * 5
            mat = M["olive_black"] if (k * 5) % 3 == 0 else M["olive_fruit"]
            ob = blob(ring * math.cos(a), ring * math.sin(a), z, 0.032, 0.026, 0.026, mat, segs=8, rings=5)
            ob.rotation_euler = (0, 0, a)
            k += 1
    for i, a in enumerate((0.3, 1.1, 1.9)):
        nn = mark()
        blob(0.07, 0, 0, 0.08, 0.022, 0.006, M["olive_leaf"], segs=8, rings=4)
        place(nn, loc=(0.08, -0.12, 0.22), rot=(0, -10, math.degrees(a) - 90))


def g_grapes(M):
    """A bunch of purple grapes hanging from a woody stem with a vine leaf."""
    for row in range(6):
        r = 0.15 * (1 - row / 6.5)
        cnt = max(1, int(7 - row))
        z = 0.5 - row * 0.075
        for j in range(cnt):
            a = 2 * math.pi * j / cnt + row * 0.6
            rr = r if cnt > 1 else 0.0
            blob(rr * math.cos(a), rr * math.sin(a), z, 0.055, 0.055, 0.055, M["grape"], segs=10, rings=6)
    blob(0, 0, 0.47, 0.1, 0.1, 0.1, M["grape"], segs=10, rings=6)
    rod((0, 0, 0.55), (0.03, 0.02, 0.72), 0.014, M["stem"], n=6)
    rod((-0.07, 0, 0.7), (0.1, 0.04, 0.73), 0.012, M["stem"], n=6)
    nn = mark()
    v = [(0, 0, 0)]
    for j in range(11):
        a = math.pi * j / 10
        rr = 0.17 * (0.75 + 0.25 * math.cos(5 * a))
        v.append((rr * math.cos(a) * 1.1, 0, rr * math.sin(a) * 1.2 - 0.03))
    mesh("leaf", v, [tuple(range(len(v)))], M["vine_leaf"], solidify=0.01)
    place(nn, loc=(0.09, 0.02, 0.69), rot=(-50, 0, 20))


def g_flour(M):
    """A plump pale sack tied at the neck, its ruffled top open on the flour, a spill beside it."""
    prof = [(0.0, 0.0), (0.18, 0.0), (0.235, 0.04), (0.255, 0.14), (0.235, 0.25), (0.17, 0.33),
            (0.11, 0.37), (0.1, 0.39), (0.15, 0.44), (0.18, 0.47)]
    ob = lathe(prof, M["flour_sack"], n=28, top=False)
    for v in ob.data.vertices:  # ruffle the open top
        if v.co.z > 0.43:
            a = math.atan2(v.co.y, v.co.x)
            v.co.z += 0.025 * math.sin(a * 7)
    lathe([(0.1, 0.39), (0.08, 0.42), (0.0, 0.43)], M["flour"], n=24, bottom=False, top=False)
    cylinder(0, 0, 0.365, 0.395, 0.105, M["twine"], n=24)
    cone_heap(0.25, -0.22, 0.11, 0.06, M["flour"])


def g_bread(M):
    """Two round loaves scored into eight wedges (panis quadratus)."""
    prof = [(0.0, 0.17), (0.07, 0.165), (0.13, 0.15), (0.2, 0.1), (0.225, 0.04), (0.2, 0.0), (0.0, 0.0)]

    def zat(r):
        for (r0, z0), (r1, z1) in zip(prof, prof[1:4]):
            if r0 <= r <= r1:
                return z0 + (z1 - z0) * (r - r0) / (r1 - r0)
        return prof[3][1]

    def loaf(x, y, s=1.0):
        nn = mark()
        lathe(list(reversed(prof)), M["crust"], n=32)
        for j in range(4):
            a = math.pi * j / 4 + 0.2
            pts = [(r * math.cos(a), r * math.sin(a), zat(abs(r)) + 0.004)
                   for r in (-0.19, -0.13, -0.07, 0.0, 0.07, 0.13, 0.19)]
            for p0, p1 in zip(pts, pts[1:]):
                rod(p0, p1, 0.011, M["crumb"], n=5)
        place(nn, loc=(x, y, 0), scale=s)
    loaf(-0.08, 0.08)
    loaf(0.16, -0.14, 0.8)


def amphora_shape(mat, h=0.62, body=0.15, neck=0.045, globe=False):
    if globe:  # Dressel 20: the globular oil amphora
        prof = [(0.0, 0.0), (0.06, 0.01), (0.17, 0.08), (0.21, 0.2), (0.2, 0.3), (0.14, 0.38),
                (0.06, 0.42), (0.055, 0.5), (0.07, 0.52)]
    else:  # Dressel 1: the slender wine amphora with a pointed toe
        prof = [(0.0, 0.0), (0.02, 0.0), (0.025, 0.05), (0.06, 0.11), (0.11, 0.21), (0.14, 0.31),
                (body, 0.38), (0.145, 0.43), (0.11, 0.47), (neck + 0.01, 0.49), (neck, 0.52),
                (neck, h - 0.02), (neck + 0.012, h)]
    return lathe(prof, mat, n=24, name="amph", top=False)


def handles(mat, z0, z1, r_out, r_in=0.05):
    """Two loop handles in the screen plane (x along the camera's right)."""
    d = kit.RIGHT
    for sgn in (-1, 1):
        a = d * (sgn * r_in) + Vector((0, 0, z1))
        b = d * (sgn * (r_out + 0.045)) + Vector((0, 0, z1 + 0.01))
        c = d * (sgn * r_out) + Vector((0, 0, z0))
        rod(a, b, 0.02, mat, n=6)
        rod(b, c, 0.02, mat, n=6)


def g_wine(M):
    """Slender wine amphora: red painted band, dark-red wax seal, a little vine leaf tag."""
    amphora_shape(M["terracotta"])
    handles(M["terracotta"], 0.43, 0.57, 0.1)
    lathe([(0.141, 0.3), (0.152, 0.36), (0.143, 0.42)], M["band_red"], n=24, top=False, bottom=False)
    lathe([(0.06, 0.6), (0.068, 0.64), (0.04, 0.68), (0.0, 0.69)], M["wax_red"], n=16, bottom=False)


def g_olive_oil(M):
    """Globular oil amphora (Dressel 20) in a greenish slip, green band and golden oil stopper."""
    amphora_shape(M["oil_jar"], globe=True)
    handles(M["oil_jar"], 0.33, 0.48, 0.13, r_in=0.055)
    lathe([(0.202, 0.18), (0.212, 0.22), (0.205, 0.27)], M["band_green"], n=28, top=False, bottom=False)
    lathe([(0.06, 0.5), (0.07, 0.53), (0.045, 0.57), (0.0, 0.58)], M["oil_gold"], n=16, bottom=False)
    for i in range(3):
        nn = mark()
        blob(0.06, 0, 0, 0.065, 0.018, 0.005, M["olive_leaf"], segs=8, rings=4)
        place(nn, loc=(0.0, -0.205, 0.2), rot=(90, 0, -90 + 50 * (i - 1)))


def g_salt(M):
    """A heap of white salt crystals in a shallow dark dish."""
    lathe([(0.2, 0), (0.28, 0.03), (0.33, 0.07), (0.34, 0.08)], M["wood_dark"], n=28, top=False)
    lathe([(0.32, 0.07), (0.02, 0.05)], M["wood_dark"], n=28, top=False, bottom=False)
    cone_heap(0, 0, 0.29, 0.3, M["salt"])
    for i in range(7):
        a = i * 2.1
        box(-0.02, -0.02, -0.02, 0.02, 0.02, 0.02, M["salt"], bevel=0.003).matrix_world = (
            Matrix.Translation((0.2 * math.cos(a), 0.2 * math.sin(a), 0.1 + 0.05 * (i % 3))) @
            Matrix.Rotation(a, 4, "Z") @ Matrix.Rotation(0.6, 4, "X"))


def g_garum(M):
    """A one-handled jug (urceus) of fish sauce, a small fish lying against it."""
    lathe([(0.0, 0.0), (0.1, 0.0), (0.16, 0.08), (0.17, 0.18), (0.13, 0.28), (0.07, 0.33),
           (0.065, 0.4), (0.085, 0.42)], M["terracotta_dk"], n=24, top=False)
    lathe([(0.07, 0.41), (0.0, 0.4)], M["garum"], n=16, bottom=False, top=False)
    rod((0.07, 0, 0.39), (0.19, 0, 0.36), 0.018, M["terracotta_dk"], n=6)
    rod((0.19, 0, 0.36), (0.15, 0, 0.22), 0.018, M["terracotta_dk"], n=6)
    lathe([(0.168, 0.15), (0.172, 0.18), (0.164, 0.21)], M["band_red"], n=24, top=False, bottom=False)
    nn = mark()
    blob(0, 0, 0, 0.14, 0.03, 0.045, M["fish"], segs=12, rings=6)
    blob(0, 0, 0.015, 0.13, 0.025, 0.032, M["fish_back"], segs=10, rings=6, top_only=True)
    mesh("tail", [(-0.12, 0, 0), (-0.2, 0, 0.05), (-0.18, 0, 0), (-0.2, 0, -0.045)], [(0, 1, 2, 3)],
         M["fish_back"], solidify=0.01)
    place(nn, loc=(-0.05, -0.2, 0.045), rot=(0, 0, 15))


def g_wood(M):
    """Log stack: three logs below, two above, pale end grain toward the light."""
    for row, xs in ((0, (-0.16, 0.0, 0.16)), (1, (-0.08, 0.08))):
        for x in xs:
            z = 0.08 + row * 0.14
            nn = mark()
            lathe([(0.075, -0.24), (0.078, 0.24)], M["log"], n=14, top=False, bottom=False)
            lathe([(0.075, -0.24), (0.0, -0.245)], M["endgrain"], n=14, top=False, bottom=False)
            lathe([(0.078, 0.24), (0.0, 0.245)], M["endgrain"], n=14, top=False, bottom=False)
            place(nn, loc=(x, 0, z), rot=(90, 0, 0))


def g_planks(M):
    """Two crossed layers of sawn boards."""
    for layer in range(4):
        z = layer * 0.045
        for i in range(3):
            o = (i - 1) * 0.11
            if layer % 2 == 0:
                box(-0.26, o - 0.05, z, 0.26, o + 0.05, z + 0.04, M["plank"], bevel=0.006)
            else:
                box(o - 0.05, -0.26, z, o + 0.05, 0.26, z + 0.04, M["plank"], bevel=0.006)


def g_clay(M):
    """Wet red clay: soft wire-cut blocks heaped on a board."""
    box(-0.3, -0.24, 0, 0.3, 0.24, 0.035, M["wood"], bevel=0.008)
    for (x, y, z, rz) in [(-0.11, 0.07, 0.035, 8), (0.12, 0.06, 0.035, -6), (0.0, -0.12, 0.035, 20),
                          (0.0, 0.02, 0.17, -12)]:
        nn = mark()
        box(-0.12, -0.09, 0, 0.12, 0.09, 0.13, M["clay"], bevel=0.045)
        place(nn, loc=(x, y, z), rot=(0, 0, rz))


def g_bricks(M):
    """A neat stack of fired bricks (lateres)."""
    for layer in range(3):
        z = layer * 0.075
        for i in range(2):
            for j in range(2 if layer < 2 else 1):
                if layer % 2 == 0:
                    box(-0.2 + j * 0.205, -0.1 + i * 0.105 - 0.05, z, -0.2 + j * 0.205 + 0.2,
                        -0.1 + i * 0.105 + 0.05, z + 0.07, M["brick"], bevel=0.008)
                else:
                    box(-0.1 + i * 0.105 - 0.05 + 0.05, -0.2 + j * 0.205, z, -0.1 + i * 0.105 + 0.05 + 0.05,
                        -0.2 + j * 0.205 + 0.2, z + 0.07, M["brick"], bevel=0.008)


def g_stone(M):
    """Rough field stones in a pile."""
    rock(-0.12, 0.06, 0.1, 0.16, M["rock"], 1)
    rock(0.14, 0.1, 0.09, 0.14, M["rock"], 2)
    rock(0.03, -0.14, 0.08, 0.13, M["rock"], 3)
    rock(0.0, 0.03, 0.25, 0.12, M["rock"], 4)


def g_cut_stone(M):
    """Squared ashlar blocks, one on two."""
    box(-0.28, -0.14, 0, -0.01, 0.14, 0.17, M["ashlar"], bevel=0.012)
    box(0.01, -0.14, 0, 0.28, 0.14, 0.17, M["ashlar"], bevel=0.012)
    box(-0.15, -0.12, 0.17, 0.15, 0.12, 0.33, M["ashlar"], bevel=0.012)


def g_limestone(M):
    """Pale quarried limestone: rough, chunky, cream coloured."""
    for i, (x, y, z, s) in enumerate([(-0.12, 0.05, 0.11, 0.17), (0.13, 0.08, 0.1, 0.15),
                                      (0.02, -0.13, 0.1, 0.15), (0.02, 0.04, 0.27, 0.12)]):
        ob = rock(x, y, z, s, M["limestone"], i + 11, flat=0.75)
        for v in ob.data.vertices:  # flatter, blockier facets
            v.co.z = max(v.co.z, z - s * 0.5)


def g_lime(M):
    """A wooden tub of white quicklime, a pale heap rising out of it."""
    lathe([(0.17, 0), (0.2, 0.22)], M["wood"], n=24, top=False)
    lathe([(0.2, 0.22), (0.185, 0.22), (0.165, 0.0)], M["wood_dark"], n=24, top=False, bottom=False)
    for z in (0.04, 0.17):
        lathe([(0.175 + z * 0.135, z), (0.177 + z * 0.135, z + 0.03)], M["iron"], n=24, top=False,
              bottom=False)
    lathe([(0.19, 0.2), (0.14, 0.27), (0.07, 0.31), (0.0, 0.32)], M["lime"], n=24, bottom=False)
    cone_heap(0.24, -0.18, 0.08, 0.06, M["lime"])


def g_pozzolana(M):
    """A heap of dark red-grey volcanic ash with a shovel."""
    cone_heap(0, 0, 0.3, 0.28, M["ash"])
    rod((0.12, -0.25, 0.05), (-0.05, 0.05, 0.36), 0.014, M["wood"], n=6)
    box(-0.05, -0.04, -0.01, 0.05, 0.04, 0.01, M["iron"], bevel=0.004).matrix_world = (
        Matrix.Translation((0.15, -0.3, 0.03)) @ Matrix.Rotation(0.5, 4, "Z") @ Matrix.Rotation(0.6, 4, "X"))


def g_concrete(M):
    """A wooden mortar trough of grey opus caementicium with a hoe in it."""
    box(-0.3, -0.2, 0, 0.3, 0.2, 0.14, M["wood_dark"], bevel=0.01)
    box(-0.27, -0.17, 0.02, 0.27, 0.17, 0.135, M["concrete"], bevel=0.01)
    blob(-0.05, 0.02, 0.13, 0.2, 0.13, 0.06, M["concrete"], segs=14, rings=8, top_only=True)
    for i in range(7):
        rock(-0.2 + i * 0.065, -0.07 + 0.07 * ((i * 7) % 3 - 1), 0.15 + 0.02 * (i % 2), 0.028, M["rock"], i + 20)
    rod((0.12, -0.02, 0.15), (0.34, 0.22, 0.5), 0.016, M["wood"], n=6)
    box(0.03, -0.11, 0.12, 0.17, -0.0, 0.16, M["iron"], bevel=0.005).rotation_euler = (0, 0, 0.6)


def g_marble(M):
    """A polished white marble block with grey veining, a thin slab leaning on it."""
    box(-0.26, -0.18, 0, 0.26, 0.18, 0.3, M["marble_veined"], bevel=0.012)
    nn = mark()
    box(-0.15, -0.02, 0, 0.15, 0.02, 0.26, M["marble_veined"], bevel=0.008)
    place(nn, loc=(0.06, -0.26, 0.0), rot=(-12, 0, 0))


def g_sand(M):
    cone_heap(0, 0, 0.3, 0.26, M["sand"])
    cone_heap(0.22, -0.18, 0.12, 0.08, M["sand"])


def g_iron_ore(M):
    """Dark rust-streaked ore lumps."""
    for i, (x, y, z, s, mat) in enumerate([(-0.12, 0.06, 0.09, 0.14, "iron_ore"), (0.13, 0.09, 0.08, 0.13, "rust"),
                                           (0.02, -0.13, 0.08, 0.12, "iron_ore"), (0.0, 0.03, 0.22, 0.11, "rust"),
                                           (-0.18, -0.14, 0.05, 0.07, "rust")]):
        rock(x, y, z, s, M[mat], i + 30)


def g_silver_ore(M):
    """Dark blue-grey galena lumps flecked with bright metal."""
    for i, (x, y, z, sz) in enumerate([(-0.12, 0.06, 0.09, 0.14), (0.13, 0.09, 0.08, 0.13),
                                       (0.02, -0.13, 0.08, 0.12), (0.0, 0.03, 0.22, 0.11)]):
        rock(x, y, z, sz, M["silver_ore"], i + 40)
        for k in range(3):
            a = k * 2.1 + i
            rock(x + 0.55 * sz * math.cos(a), y + 0.55 * sz * math.sin(a) - 0.3 * sz, z + 0.45 * sz, 0.03,
                 M["silver"], i * 3 + k + 50)


def ingots(M, mat, wide=False):
    """Trapezoid bars stacked crosswise."""
    def bar(x, y, z, rot):
        nn = mark()
        L, W, H, t = (0.3, 0.1, 0.07, 0.025)
        v = [(-L / 2, -W / 2, 0), (L / 2, -W / 2, 0), (L / 2, W / 2, 0), (-L / 2, W / 2, 0),
             (-L / 2 + t, -W / 2 + t, H), (L / 2 - t, -W / 2 + t, H), (L / 2 - t, W / 2 - t, H),
             (-L / 2 + t, W / 2 - t, H)]
        f = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        mesh("ingot", v, f, mat, bevel=0.006)
        place(nn, loc=(x, y, z), rot=(0, 0, rot))
    for i in range(3):
        bar(0, (i - 1) * 0.115, 0, 0)
    for i in range(2):
        bar((i - 0.5) * 0.12, 0, 0.07, 90)
    bar(0, 0, 0.14, 0)


def g_iron(M):
    ingots(M, M["iron"])


def g_silver(M):
    ingots(M, M["silver"])


def g_gold(M):
    ingots(M, M["gold"])


def g_tools(M):
    """A hammer and a pick laid crosswise, facing the viewer."""
    nn = mark()
    # hammer: handle up to the right, a squared iron head across its top
    a, b = Vector((-0.24, 0, -0.26)), Vector((0.16, 0, 0.2))
    rod(a, b, 0.032, M["wood"], n=8)
    e = (b - a).normalized()
    side = Vector((-e.z, 0, e.x))
    rod(b - side * 0.11, b + side * 0.13, 0.065, M["iron"], n=4)
    # pick: handle up to the left, a curved iron head
    a, b = Vector((0.24, 0.02, -0.26)), Vector((-0.16, 0.02, 0.2))
    rod(a, b, 0.032, M["wood"], n=8)
    pts = [(-0.4, 0.06), (-0.3, 0.16), (-0.16, 0.21), (-0.02, 0.2), (0.1, 0.13)]
    for (ax, az), (bx, bz) in zip(pts, pts[1:]):
        rod((ax, 0.02, az), (bx, 0.02, bz), 0.034, M["iron"], n=8)
    facing(nn, tilt=20, spin=0)


def g_weapons(M):
    """A gladius and a spear crossed."""
    nn = mark()
    # gladius: blade along the diagonal, gold pommel and guard
    d = Vector((1, 0, 1)).normalized()
    p0 = Vector((-0.24, 0, -0.24))
    blade = [(0, -0.05), (0.34, -0.05), (0.42, 0.0), (0.34, 0.05), (0, 0.05)]
    v = []
    for (a, b) in blade:
        p = p0 + d * a + Vector((-d.z, 0, d.x)) * b
        v.append((p.x, 0.0, p.z))
    mesh("blade", v, [tuple(range(len(v)))], M["steel"], solidify=0.018)
    gp = p0 - d * 0.0
    rod(gp - d * 0.0 + Vector((-d.z, 0, d.x)) * 0.07, gp - Vector((-d.z, 0, d.x)) * 0.07, 0.02,
        M["gold_trim"], n=8)
    rod(gp, gp - d * 0.12, 0.03, M["wood_dark"], n=8)
    blob((gp - d * 0.14).x, 0, (gp - d * 0.14).z, 0.035, 0.035, 0.035, M["gold_trim"], segs=8, rings=6)
    # spear: shaft the other way, leaf-shaped iron head
    a, b = Vector((0.3, 0.03, -0.3)), Vector((-0.2, 0.03, 0.2))
    rod(a, b, 0.024, M["wood"], n=8)
    e = (b - a).normalized()
    side = Vector((-e.z, 0, e.x))
    tip = [b, b + e * 0.06 + side * 0.06, b + e * 0.2, b + e * 0.06 - side * 0.06]
    mesh("head", [tuple(p) for p in tip], [(0, 1, 2, 3)], M["iron"], solidify=0.015)
    facing(nn, tilt=20)


def g_armor(M):
    """A legionary helmet (galea): bright iron bowl, brow ridge, cheek guards, transverse red crest."""
    lathe([(0.0, 0.31), (0.07, 0.305), (0.13, 0.28), (0.17, 0.22), (0.19, 0.14), (0.195, 0.07)],
          M["helmet"], n=28, bottom=False, top=False)
    lathe([(0.195, 0.07), (0.215, 0.07), (0.215, 0.095), (0.19, 0.11)], M["bronze"], n=28, top=False,
          bottom=False)
    # flared neck guard at the back (grid north = away from the viewer)
    mesh("neck", [(-0.18, 0.1, 0.09), (0.18, 0.1, 0.09), (0.22, 0.3, 0.02), (-0.22, 0.3, 0.02)],
         [(0, 1, 2, 3)], M["helmet"], solidify=0.02)
    # cheek guards hanging at the front sides
    for sx in (-1, 1):
        nn = mark()
        mesh("cheek", [(-0.06, 0, 0.0), (0.06, 0, 0.0), (0.05, 0, -0.16), (-0.02, 0, -0.2)], [(0, 1, 2, 3)],
             M["bronze"], solidify=0.02)
        place(nn, loc=(sx * 0.15, -0.1, 0.08), rot=(0, 0, sx * 55))
    box(-0.05, -0.015, 0.29, 0.05, 0.015, 0.34, M["bronze"], bevel=0.004)
    blob(0, 0, 0.335, 0.22, 0.05, 0.13, M["crest"], segs=16, rings=8, top_only=True)
    facing(0, tilt=12)


def g_pottery(M):
    """Stacked glossy red-slip bowls (terra sigillata) and a jug with a black band."""
    for i in range(3):
        z = i * 0.05
        lathe([(0.1, z), (0.16, z + 0.03), (0.24, z + 0.09), (0.25, z + 0.1)], M["sigillata"], n=28,
              top=False)
    lathe([(0.24, 0.19), (0.02, 0.15)], M["sigillata"], n=28, top=False, bottom=False)
    lathe([(0.0, 0.0), (0.07, 0.0), (0.11, 0.06), (0.115, 0.15), (0.08, 0.22), (0.045, 0.26),
           (0.05, 0.33), (0.06, 0.34)], M["terracotta"], x=0.17, y=-0.2, n=22, top=False)
    lathe([(0.112, 0.09), (0.116, 0.12), (0.114, 0.15)], M["dark"], x=0.17, y=-0.2, n=22, top=False,
          bottom=False)
    rod((0.22, -0.2, 0.32), (0.3, -0.2, 0.28), 0.014, M["terracotta"], n=6)
    rod((0.3, -0.2, 0.28), (0.27, -0.2, 0.16), 0.014, M["terracotta"], n=6)


def g_amphorae(M):
    """Two plain, empty amphorae, one leaning on the other."""
    nn = mark()
    amphora_shape(M["terracotta"])
    handles(M["terracotta"], 0.43, 0.57, 0.1)
    place(nn, loc=(-0.1, 0.06, 0))
    nn = mark()
    amphora_shape(M["terracotta_dk"])
    handles(M["terracotta_dk"], 0.43, 0.57, 0.1)
    place(nn, loc=(0.13, -0.08, 0.02), rot=(0, 18, 0))


def g_wool(M):
    """A fluffy fleece with a ball of spun yarn on a wooden spindle."""
    for i in range(9):
        a = i * 2.39
        r = 0.12 + 0.05 * (i % 2)
        blob(r * math.cos(a), r * math.sin(a), 0.09, 0.11, 0.1, 0.09, M["wool"], segs=10, rings=6)
    blob(0, 0, 0.14, 0.16, 0.16, 0.12, M["wool"], segs=12, rings=7)
    blob(0.16, -0.2, 0.1, 0.1, 0.1, 0.1, M["yarn"], segs=14, rings=8)
    rod((0.02, -0.06, 0.02), (0.3, -0.34, 0.2), 0.012, M["wood"], n=6)


def folded(M, mat, trim, x, y, z, w=0.42, d=0.28, h=0.06, rot=0):
    nn = mark()
    box(-w / 2, -d / 2, 0, w / 2, d / 2, h, mat, bevel=0.022)
    if trim is not None:
        box(-w / 2 - 0.002, -d / 2 - 0.004, h * 0.15, w / 2 + 0.002, -d / 2 + 0.03, h * 0.85, trim, bevel=0.006)
    place(nn, loc=(x, y, z), rot=(0, 0, rot))


def tunic(M, cloth, trim, x=0.0, y=0.0, z=0.0, rot=0.0, s=1.0):
    """A T-shaped tunic laid flat toward the viewer, with two vertical stripes (clavi)."""
    nn = mark()
    o = [(-0.16, -0.3), (0.16, -0.3), (0.17, 0.12), (0.34, 0.08), (0.36, 0.22), (0.12, 0.3),
         (0.05, 0.27), (-0.05, 0.27), (-0.12, 0.3), (-0.36, 0.22), (-0.34, 0.08), (-0.17, 0.12)]
    v = [(px, 0, pz) for px, pz in o]
    mesh("tunic", v, [tuple(range(len(v)))], cloth, solidify=0.03, bevel=0.01)
    for sx in (-0.085, 0.085):
        box(sx - 0.022, -0.035, -0.3, sx + 0.022, -0.028, 0.285, trim, bevel=0)
    box(-0.16, -0.035, -0.3, 0.16, -0.028, -0.27, trim, bevel=0)
    place(nn, loc=(x, y, z), rot=(0, 0, rot), scale=s)
    return nn


def g_clothing(M):
    """A plain linen tunic with brown clavi, a folded one beneath."""
    folded(M, M["linen"], None, 0.0, 0.18, -0.34, w=0.5, d=0.24, h=0.07)
    nn = tunic(M, M["linen"], M["leather"])
    facing(nn, tilt=20)


def g_dye(M):
    """Three little bowls of dyestuff: purple, crimson, saffron."""
    for (x, y), mat in zip([(-0.17, 0.08), (0.17, 0.08), (0.0, -0.16)], ("purple", "crimson", "saffron")):
        lathe([(0.06, 0), (0.1, 0.03), (0.15, 0.09), (0.155, 0.1)], M["terracotta"], x=x, y=y, n=24, top=False)
        cone_heap(x, y, 0.145, 0.1, M[mat]).location.z = 0.07


def g_dyed_clothing(M):
    """A Tyrian-purple tunic with gold borders over folded crimson cloth."""
    folded(M, M["crimson"], M["gold_trim"], 0.0, 0.18, -0.34, w=0.5, d=0.24, h=0.07)
    nn = tunic(M, M["purple"], M["gold_trim"])
    facing(nn, tilt=20)


def g_furniture(M):
    """A wooden chair with a curved back, beside a small three-legged table."""
    box(-0.16, -0.15, 0.22, 0.16, 0.15, 0.26, M["wood"], bevel=0.01)
    for sx in (-0.13, 0.13):
        for sy in (-0.12, 0.12):
            box(sx - 0.03, sy - 0.03, 0, sx + 0.03, sy + 0.03, 0.22, M["wood_dark"], bevel=0.004)
    for sx in (-0.13, 0.13):
        box(sx - 0.03, 0.1, 0.26, sx + 0.03, 0.15, 0.55, M["wood_dark"], bevel=0.004)
    box(-0.16, 0.09, 0.42, 0.16, 0.15, 0.56, M["wood"], bevel=0.01)
    cylinder(0.3, -0.24, 0.24, 0.27, 0.13, M["wood"], n=24)
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.4
        rod((0.3 + 0.1 * math.cos(a), -0.24 + 0.1 * math.sin(a), 0), (0.3 + 0.06 * math.cos(a),
            -0.24 + 0.06 * math.sin(a), 0.24), 0.02, M["wood_dark"], n=6)


def g_glassware(M):
    """A blue-green glass flask and a beaker."""
    lathe([(0.0, 0.0), (0.12, 0.0), (0.15, 0.06), (0.14, 0.16), (0.08, 0.22), (0.04, 0.26),
           (0.035, 0.4), (0.05, 0.42)], M["glass"], n=28, top=False)
    lathe([(0.0, 0.0), (0.07, 0.0), (0.085, 0.18), (0.09, 0.19)], M["glass_blue"], x=0.17, y=-0.17,
          n=24, top=False)
    lathe([(0.08, 0.02), (0.0, 0.02)], M["glass_blue"], x=0.17, y=-0.17, n=24, top=False, bottom=False)


def g_papyrus(M):
    """A bundle of papyrus reeds with big green tufted heads, on a pale sheet."""
    box(-0.32, -0.22, 0, 0.32, 0.22, 0.012, M["papyrus"], bevel=0.003)
    for j in range(5):
        a = 2 * math.pi * j / 5 + 0.3
        bx, by = 0.03 * math.cos(a), 0.03 * math.sin(a)
        tx, ty = bx * 5.0, by * 5.0
        rod((bx * 0.6, by * 0.6, 0.0), (tx, ty, 0.5), 0.018, M["reed"], n=6)
        for k in range(12):
            b = 2 * math.pi * k / 12
            rod((tx, ty, 0.5), (tx + 0.11 * math.cos(b), ty + 0.11 * math.sin(b), 0.56 + 0.03 * (k % 3)),
                0.012, M["tuft"], n=4)
    cylinder(0, 0, 0.12, 0.17, 0.05, M["twine"], n=12)


def g_scrolls(M):
    """Two rolled scrolls with dark knobbed rods, one partly unrolled."""
    def scroll(x, y, z, rot, L=0.4):
        nn = mark()
        lathe([(0.06, -L / 2), (0.06, L / 2)], M["parchment"], n=18)
        for s in (-1, 1):
            lathe([(0.012, 0), (0.012, 0.04), (0.03, 0.045), (0.03, 0.07), (0.0, 0.08)], M["wood_dark"], n=10)
            ob = since(mark() - 1)[0]
            ob.matrix_world = Matrix.Translation((0, 0, s * L / 2)) @ Matrix.Rotation(0 if s > 0 else math.pi, 4, "X")
        place(nn, loc=(x, y, z), rot=(90, 0, rot))
    scroll(-0.02, 0.08, 0.06, 15)
    scroll(0.04, -0.06, 0.06, -10)
    scroll(0.0, 0.0, 0.17, 2)


def g_jewelry(M):
    """A gold necklace of beads with a red pendant, a ring with a blue stone."""
    nn = mark()
    for i in range(17):
        a = math.pi * (0.1 + 0.8 * i / 16)
        blob(0.25 * math.cos(a) * 1.0, 0, -0.25 * math.sin(a) * 0.9 + 0.2, 0.036, 0.036, 0.036, M["gold"],
             segs=8, rings=5)
    blob(0, 0, -0.06, 0.06, 0.04, 0.08, M["gem_red"], segs=12, rings=8)
    lathe([(0.065, -0.01), (0.075, 0.0), (0.075, 0.02), (0.065, 0.03)], M["gold"], x=0, y=0, n=16)
    since(mark() - 1)[0].matrix_world = Matrix.Translation((0, 0.01, -0.07)) @ Matrix.Rotation(math.pi / 2, 4, "X")
    facing(nn, tilt=25)
    nn = mark()
    lathe([(0.055, 0.0), (0.07, 0.0), (0.07, 0.03), (0.055, 0.03)], M["gold"], n=20)
    blob(0, -0.07, 0.03, 0.03, 0.03, 0.03, M["gem_blue"], segs=8, rings=6)
    place(nn, loc=(0.24, -0.12, -0.2), rot=(70, 0, 30))


def g_statues(M):
    """A marble bust on a squat pedestal: broad draped shoulders, a head with a short beard."""
    lathe([(0.0, 0.0), (0.15, 0.0), (0.15, 0.05), (0.1, 0.07), (0.08, 0.13), (0.12, 0.15),
           (0.12, 0.17)], M["marble"], n=24)
    blob(0, 0, 0.26, 0.21, 0.1, 0.11, M["marble"], segs=18, rings=10)
    box(-0.2, -0.09, 0.17, 0.2, 0.09, 0.24, M["marble"], bevel=0.04)
    cylinder(0, 0, 0.3, 0.4, 0.05, M["marble"], n=12)
    blob(0, -0.005, 0.47, 0.085, 0.095, 0.105, M["marble"], segs=16, rings=10)
    blob(0, 0.02, 0.5, 0.09, 0.09, 0.085, M["marble"], segs=14, rings=10)
    blob(0, -0.095, 0.46, 0.014, 0.022, 0.024, M["marble"], segs=6, rings=4)
    place(0, rot=(0, 0, 30))


def g_denarii(M):
    """Stacks of silver coins (denarii) with a few loose ones."""
    def coin(x, y, z, rot=(0, 0)):
        nn = mark()
        lathe([(0.0, 0.0), (0.085, 0.0), (0.09, 0.008), (0.09, 0.016), (0.085, 0.024), (0.0, 0.024)],
              M["silver"], n=24)
        lathe([(0.06, 0.024), (0.065, 0.027), (0.0, 0.027)], M["silver"], n=16, bottom=False)
        place(nn, loc=(x, y, z), rot=(rot[0], 0, rot[1]))
    for i in range(6):
        coin(-0.09 + 0.004 * (i % 2), 0.06, i * 0.026)
    for i in range(4):
        coin(0.1, 0.1 - 0.003 * (i % 2), i * 0.026)
    coin(0.08, -0.14, 0.0)
    coin(-0.12, -0.14, 0.03, (35, 30))


GOODS = {
    "wheat": g_wheat, "vegetables": g_vegetables, "fish": g_fish, "meat": g_meat, "olives": g_olives,
    "grapes": g_grapes, "flour": g_flour, "bread": g_bread, "olive_oil": g_olive_oil, "wine": g_wine,
    "salt": g_salt, "garum": g_garum, "wood": g_wood, "planks": g_planks, "clay": g_clay,
    "bricks": g_bricks, "stone": g_stone, "cut_stone": g_cut_stone, "limestone": g_limestone,
    "lime": g_lime, "pozzolana": g_pozzolana, "concrete": g_concrete, "marble": g_marble,
    "sand": g_sand, "iron_ore": g_iron_ore, "iron": g_iron, "silver_ore": g_silver_ore,
    "silver": g_silver, "gold": g_gold, "tools": g_tools, "weapons": g_weapons, "armor": g_armor,
    "pottery": g_pottery, "amphorae": g_amphorae, "wool": g_wool, "clothing": g_clothing,
    "dye": g_dye, "dyed_clothing": g_dyed_clothing, "furniture": g_furniture,
    "glassware": g_glassware, "papyrus": g_papyrus, "scrolls": g_scrolls, "jewelry": g_jewelry,
    "statues": g_statues, "denarii": g_denarii,
}

# ---------------------------------------------------------------- build categories

def arch_wall(x0, x1, y, zs, zt, pier, mat, t=0.12, n=12, name="arch"):
    """A wall in the x/z plane at y (thickness t toward +y) pierced by one round arch."""
    xa, xb = x0 + pier, x1 - pier
    r = (xb - xa) / 2
    pts = [(x0, 0.0), (xa, 0.0), (xa, zs)]
    for i in range(1, n):
        a = math.pi * i / n
        pts.append(((xa + xb) / 2 - r * math.cos(a), zs + r * math.sin(a)))
    pts += [(xb, zs), (xb, 0.0), (x1, 0.0), (x1, zt), (x0, zt)]
    v = [(px, y, pz) for px, pz in pts]
    ob = mesh(name, v, [tuple(reversed(range(len(v))))], mat, solidify=t)
    ob.modifiers["solid"].offset = 1.0
    return ob


def gable_end(x0, x1, y, z, h, mat, t=0.02):
    mesh("pediment", [(x0, y, z), (x1, y, z), ((x0 + x1) / 2, y, z + h)], [(0, 1, 2)], mat, solidify=t)


def c_roads(M):
    """A stretch of basalt paving between kerbs and grass, with a milestone."""
    diamond(M["grass"], curb=M["soil"])
    box(-0.5, -0.24, 0, 0.5, 0.24, 0.025, M["basalt"], bevel=0.004)
    for y in (-0.27, 0.24):
        box(-0.5, y, 0, 0.5, y + 0.035, 0.045, M["kerb"], bevel=0.006)
    cylinder(0.3, -0.36, 0, 0.26, 0.05, M["kerb"], n=16)
    blob(0.3, -0.36, 0.26, 0.05, 0.05, 0.03, M["kerb"], segs=12, rings=6, top_only=True)


def c_housing(M):
    """A small plastered house with a faded tile roof on a garden plot."""
    diamond(M["garden"], curb=M["soil"])
    box(-0.28, -0.2, 0, 0.28, 0.24, 0.34, M["plaster_house"])
    kit.gable(-0.28, -0.2, 0.28, 0.24, 0.34, 0.18, M["roof_house"], axis="x", over=0.05,
              wall=M["plaster_house"])
    box(-0.06, -0.215, 0, 0.06, -0.19, 0.2, M["timber"], bevel=0.004)
    kit.windows_y(-0.2, [-0.18, 0.18], 0.16, 0.25, M, w=0.07)
    kit.windows_x(0.28, [0.02], 0.16, 0.25, M, w=0.07)
    kit.cypress(-0.36, 0.34, 0.45, M)
    kit.pot_plant(0.2, -0.32, M)


def c_farms(M):
    """Rows of ripe wheat on tilled soil, a short fence behind."""
    diamond(M["soil"], curb=M["soil"])
    for i in range(5):
        y = -0.4 + i * 0.2
        box(-0.46, y - 0.06, 0, 0.46, y + 0.06, 0.03, M["soil"], bevel=0.01)
        for j in range(12):
            x = -0.42 + j * 0.077
            blob(x, y, 0.12, 0.04, 0.05, 0.11, M["wheat_field"], segs=6, rings=5)
    kit.fence([(-0.48, 0.48), (0.48, 0.48)], M["timber_light"], h=0.12)
    kit.fence([(0.48, 0.48), (0.48, -0.1)], M["timber_light"], h=0.12)


def c_food(M):
    """A market stall under a saffron-striped awning: bread, fruit and an amphora."""
    diamond(M["dirt"], curb=M["paving"])
    for x in (-0.3, 0.3):
        for y in (-0.22, 0.22):
            box(x - 0.02, y - 0.02, 0, x + 0.02, y + 0.02, 0.52, M["timber"], bevel=0.003)
    mesh("awning", [(-0.38, -0.36, 0.42), (0.38, -0.36, 0.42), (0.38, 0.28, 0.62), (-0.38, 0.28, 0.62)],
         [(0, 1, 2, 3)], M["saffron_awning"], solidify=0.02)
    for i in range(6):  # scalloped valance along the front edge
        x = -0.38 + 0.0633 + i * 0.1267
        mesh("flap", [(x - 0.063, -0.365, 0.42), (x + 0.063, -0.365, 0.42), (x, -0.37, 0.36)], [(0, 1, 2)],
             M["saffron"], solidify=0.01)
    box(-0.32, -0.24, 0, 0.32, 0.0, 0.2, M["timber_light"], bevel=0.008)
    for i, x in enumerate((-0.22, -0.07, 0.08)):
        blob(x, -0.12, 0.23, 0.06, 0.05, 0.04, M["bread_ochre"], segs=10, rings=6)
    for i in range(5):
        blob(0.17 + 0.05 * (i % 3), -0.16 + 0.06 * (i // 3), 0.24, 0.03, 0.03, 0.03,
             M["fruit_red"] if i % 2 else M["fruit_yellow"], segs=8, rings=5)
    nn = mark()
    amphora_shape(M["terracotta"])
    place(nn, loc=(0.36, -0.38, 0), scale=0.55)
    kit.sack(-0.36, -0.36, 0, M, 1.4)


def c_industry(M):
    """An anvil on a stump, a hammer, and a forge glowing under a slate hood."""
    diamond(M["soot"], curb=M["stone_dark"])
    # forge at the back
    box(-0.42, 0.05, 0, 0.0, 0.42, 0.24, M["stone_dark"])
    box(-0.36, 0.1, 0.24, -0.06, 0.36, 0.26, M["charcoal"], bevel=0.004)
    for i in range(6):
        blob(-0.3 + 0.05 * i, 0.2 + 0.04 * (i % 2), 0.27, 0.04, 0.04, 0.025, M["glow"], segs=8, rings=4)
    mesh("hood", [(-0.44, 0.03, 0.5), (0.02, 0.03, 0.5), (0.02, 0.44, 0.62), (-0.44, 0.44, 0.62)],
         [(0, 1, 2, 3)], M["roof_slate"], solidify=0.03)
    for x in (-0.42, 0.0):
        box(x - 0.02, 0.03, 0.24, x + 0.02, 0.07, 0.5, M["timber"], bevel=0.003)
    # anvil, big and in front
    cylinder(0.18, -0.16, 0, 0.14, 0.13, M["timber"], n=14)
    box(0.07, -0.23, 0.14, 0.29, -0.09, 0.19, M["iron"], bevel=0.006)
    box(0.11, -0.2, 0.19, 0.25, -0.12, 0.26, M["iron"], bevel=0.004)
    box(-0.02, -0.25, 0.26, 0.36, -0.07, 0.34, M["iron"], bevel=0.01)
    rod((0.36, -0.16, 0.3), (0.5, -0.16, 0.32), 0.045, M["iron"], n=8, r1=0.006)
    rod((-0.08, -0.36, 0.02), (0.2, -0.44, 0.03), 0.016, M["timber_light"], n=6)
    box(-0.13, -0.4, 0.0, -0.03, -0.29, 0.07, M["iron"], bevel=0.006)


def c_storage(M):
    """Dark timber crates, grain sacks, an amphora and a barrel on a paved yard."""
    diamond(M["paving"], curb=M["stone_dark"])
    for (x, y, z) in [(-0.18, 0.12, 0), (0.08, 0.14, 0), (-0.05, 0.14, 0.24)]:
        box(x - 0.12, y - 0.12, z, x + 0.12, y + 0.12, z + 0.24, M["crate"], bevel=0.008)
        box(x - 0.123, y - 0.123, z + 0.1, x + 0.123, y + 0.123, z + 0.13, M["timber"], bevel=0)
    for dx, dy, dz in [(0, 0, 0), (0.13, 0, 0), (0.065, 0.02, 0.09)]:
        kit.sack(-0.3 + dx, -0.22 + dy, dz, M, 1.6)
    nn = mark()
    amphora_shape(M["terracotta"])
    place(nn, loc=(0.3, -0.05, 0), scale=0.6)
    cylinder(0.2, -0.3, 0, 0.24, 0.09, M["timber_light"], n=14)
    for z in (0.04, 0.19):
        cylinder(0.2, -0.3, z, z + 0.018, 0.093, M["iron"], n=14)


def c_water(M):
    """Two aqueduct arches carrying a water channel, a spill into a basin."""
    diamond(M["grass"], curb=M["soil"])
    arch_wall(-0.5, 0.0, -0.06, 0.32, 0.6, 0.07, M["stone_grey"])
    arch_wall(0.0, 0.5, -0.06, 0.32, 0.6, 0.07, M["stone_grey"])
    box(-0.52, -0.08, 0.6, 0.52, 0.08, 0.64, M["stone_grey"], bevel=0.004)
    box(-0.52, -0.08, 0.64, 0.52, -0.05, 0.72, M["stone_grey"], bevel=0.003)
    box(-0.52, 0.05, 0.64, 0.52, 0.08, 0.72, M["stone_grey"], bevel=0.003)
    box(-0.52, -0.05, 0.64, 0.52, 0.05, 0.7, M["water"], bevel=0)
    cylinder(0.25, -0.3, 0, 0.08, 0.16, M["stone_grey"], n=20)
    cylinder(0.25, -0.3, 0.06, 0.075, 0.14, M["water"], n=20)


def c_health(M):
    """A bath: a big blue-green plunge pool in white plaster, a labrum basin, columns behind."""
    diamond(M["paving"], curb=M["plaster"])
    box(-0.44, -0.44, 0, 0.26, 0.26, 0.09, M["plaster"], bevel=0.008)
    box(-0.38, -0.38, 0.02, 0.2, 0.2, 0.094, M["water_green"], bevel=0)
    for k in range(3):
        box(0.26, -0.36 + k * 0.08, 0, 0.32, -0.28 + k * 0.08, 0.075 - k * 0.022, M["plaster"], bevel=0.004)
    cylinder(0.3, -0.3, 0, 0.22, 0.035, M["marble"], n=12)
    lathe([(0.03, 0.2), (0.11, 0.24), (0.14, 0.27)], M["marble"], x=0.3, y=-0.3, n=24, top=False)
    cylinder(0.3, -0.3, 0.255, 0.26, 0.13, M["water_green"], n=24)
    for x in (-0.38, -0.12, 0.14):
        kit.column(x, 0.38, 0, 0.5, 0.035, M["marble"])
    box(-0.44, 0.32, 0.5, 0.2, 0.44, 0.56, M["plaster"], bevel=0.006)


def c_religion(M):
    """A small white marble temple front: podium, steps, four columns, pediment, gold acroteria."""
    diamond(M["marble_paving"], curb=M["travertine"])
    box(-0.32, -0.2, 0, 0.32, 0.42, 0.1, M["marble"], bevel=0.006)
    kit.steps(-0.22, 0.22, -0.34, -0.2, 0.1, 3, M["marble"])
    box(-0.26, 0.05, 0.1, 0.26, 0.4, 0.5, M["marble"], bevel=0.004)
    for x in (-0.24, -0.08, 0.08, 0.24):
        kit.column(x, -0.14, 0.1, 0.42, 0.035, M["marble"])
    box(-0.3, -0.2, 0.52, 0.3, 0.42, 0.58, M["marble"], bevel=0.004)
    kit.gable(-0.3, -0.2, 0.3, 0.42, 0.58, 0.16, M["roof_marble"], axis="y", over=0.03, wall=M["marble"])
    for x, z in ((-0.33, 0.6), (0.33, 0.6), (0.0, 0.76)):
        blob(x, -0.24, z, 0.03, 0.03, 0.045, M["gold"], segs=8, rings=5)
    box(-0.08, 0.04, 0.1, 0.08, 0.06, 0.36, M["dark"], bevel=0)


def c_civic(M):
    """A travertine basilica with a green-patina roof and red banners on its front."""
    diamond(M["paving"], curb=M["travertine"])
    box(-0.4, -0.2, 0, 0.4, 0.38, 0.42, M["travertine"])
    kit.gable(-0.4, -0.2, 0.4, 0.38, 0.42, 0.18, M["patina"], axis="x", over=0.05, wall=M["travertine"])
    box(-0.3, -0.06, 0.42, 0.3, 0.3, 0.56, M["travertine"])
    kit.gable(-0.3, -0.06, 0.3, 0.3, 0.56, 0.14, M["patina"], axis="x", over=0.04, wall=M["travertine"])
    for x in (-0.33, -0.11, 0.11, 0.33):
        kit.column(x, -0.27, 0, 0.36, 0.028, M["travertine"])
    box(-0.4, -0.31, 0.36, 0.4, -0.2, 0.42, M["travertine"], bevel=0.004)
    for x in (-0.22, 0.22):
        mesh("banner", [(x - 0.06, -0.316, 0.35), (x + 0.06, -0.316, 0.35), (x + 0.06, -0.316, 0.12),
                        (x, -0.316, 0.07), (x - 0.06, -0.316, 0.12)], [(0, 1, 2, 3, 4)], M["banner"],
             solidify=0.008)
    kit.steps(-0.4, 0.4, -0.42, -0.3, 0.05, 2, M["travertine"])
    box(-0.06, -0.205, 0.0, 0.06, -0.19, 0.24, M["dark"], bevel=0)


def c_culture(M):
    """A small theatre: curved stone seating facing a stage, striped awning and banners."""
    diamond(M["dirt"], curb=M["paving"])
    for k in range(4):
        r0, r1 = 0.18 + k * 0.08, 0.26 + k * 0.08
        z = 0.06 * (k + 1)
        v, f = [], []
        n = 14
        for i in range(n + 1):
            a = math.pi * i / n
            for r in (r0, r1):
                v.append((r * math.cos(a), 0.0 + r * math.sin(a) - 0.12, 0))
                v.append((r * math.cos(a), 0.0 + r * math.sin(a) - 0.12, z))
        for i in range(n):
            a, b = i * 4, (i + 1) * 4
            f += [(a + 1, b + 1, b + 3, a + 3), (a, a + 1, b + 1, b), (a + 2, b + 2, b + 3, a + 3)]
        f += [(0, 2, 3, 1), (n * 4, n * 4 + 1, n * 4 + 3, n * 4 + 2)]
        mesh("cavea", v, f, M["seat"])
    box(-0.34, -0.42, 0, 0.34, -0.2, 0.08, M["timber_light"], bevel=0.006)
    box(-0.34, -0.22, 0.08, 0.34, -0.18, 0.3, M["travertine"], bevel=0.004)
    for x in (-0.42, 0.42):
        rod((x, 0.0, 0), (x, 0.0, 0.66), 0.014, M["timber"], n=6)
        mesh("flag", [(x, 0.0, 0.64), (x, 0.0, 0.48), (x + (0.16 if x < 0 else -0.16), 0.0, 0.56)],
             [(0, 1, 2)], M["banner"], solidify=0.006)
    mesh("velum", [(-0.36, 0.42, 0.42), (0.36, 0.42, 0.42), (0.3, 0.12, 0.5), (-0.3, 0.12, 0.5)],
         [(0, 1, 2, 3)], M["stripe"], solidify=0.012)


def c_military(M):
    """A red legionary shield with a gold boss, two pila crossed behind, on a dirt yard."""
    diamond(M["dirt"], curb=M["stone_dark"])
    nn = mark()
    # curved scutum: a strip of quads bent around a vertical axis
    v, f = [], []
    n = 8
    for i in range(n + 1):
        a = -0.6 + 1.2 * i / n
        x, y = 0.4 * math.sin(a), 0.4 * (1 - math.cos(a))
        v += [(x, y, 0.0), (x, y, 0.62)]
    for i in range(n):
        f.append((2 * i, 2 * i + 2, 2 * i + 3, 2 * i + 1))
    mesh("scutum", v, f, M["red"], solidify=0.02, bevel=0.006)
    blob(0, -0.01, 0.31, 0.07, 0.04, 0.07, M["gold"], segs=12, rings=6)
    for z in (0.08, 0.54):
        box(-0.2, -0.0, z - 0.01, 0.2, 0.02, z + 0.01, M["gold_trim"], bevel=0)
    rod((0, -0.005, 0.12), (0, -0.005, 0.5), 0.012, M["gold_trim"], n=6)
    place(nn, loc=(0, -0.12, 0.0), rot=(-8, 0, 0))
    for sx in (-1, 1):
        rod((sx * 0.4, 0.25, 0.0), (-sx * 0.25, 0.05, 0.9), 0.012, M["timber_light"], n=6)
        rod((-sx * 0.25, 0.05, 0.9), (-sx * 0.3, 0.035, 1.02), 0.012, M["iron"], n=6, r1=0.002)


def c_monument(M):
    """A triumphal arch in white stone with a gilded group on top."""
    diamond(M["marble_paving"], curb=M["travertine"])
    arch_wall(-0.42, 0.42, -0.14, 0.42, 0.74, 0.16, M["marble"], t=0.28)
    for x in (-0.34, 0.34):
        kit.column(x, -0.18, 0.0, 0.62, 0.03, M["marble"])
    box(-0.45, -0.2, 0.74, 0.45, 0.16, 0.8, M["travertine"], bevel=0.005)
    box(-0.4, -0.17, 0.8, 0.4, 0.13, 0.96, M["marble"], bevel=0.005)
    box(-0.25, -0.172, 0.83, 0.25, -0.16, 0.93, M["travertine"], bevel=0)
    box(-0.16, -0.06, 0.96, 0.16, 0.06, 1.04, M["gold"], bevel=0.01)
    blob(-0.06, 0.0, 1.12, 0.05, 0.05, 0.08, M["gold"], segs=8, rings=6)
    blob(0.08, 0.0, 1.09, 0.08, 0.03, 0.05, M["gold"], segs=8, rings=6)


CATS = {"roads": c_roads, "housing": c_housing, "farms": c_farms, "food": c_food, "industry": c_industry,
        "storage": c_storage, "water": c_water, "health": c_health, "religion": c_religion,
        "civic": c_civic, "culture": c_culture, "military": c_military, "monument": c_monument}

SPRITES = {}
for _id, _fn in GOODS.items():
    SPRITES["goods/%s_16" % _id] = dict(free=True, frame=(UI, UI, UI // 2, UI // 2), build=icon(_fn, UI))
    SPRITES["goods/%s" % _id] = dict(free=True, frame=(WORLD, WORLD, WORLD // 2, WORLD // 2),
                                     build=icon(_fn, WORLD))
for _id, _fn in CATS.items():
    SPRITES["ui/cat_%s" % _id] = dict(free=True, frame=(CAT, CAT, CAT // 2, CAT // 2), build=icon(_fn, CAT))
