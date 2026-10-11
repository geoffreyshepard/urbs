"""Option C of the art test: procedural 3D buildings rendered to isometric sprites.

Run headless:
    /Applications/Blender.app/Contents/MacOS/Blender -b --python \
        tools/arttest/blender/render_sprites.py -- [ids...] [--samples N]

With no ids it renders every building and the terrain tiles.  Output goes to
tools/arttest/blender/sprites/<id>.png at the "near" scale (a tile diamond is
128x64 px).  Sprite width is N*128; the lot's bottom corner sits on the bottom
centre pixel row.

Conventions: 1 tile = 1 Blender unit, lot centred on the origin, z up.  The
front of a building faces -y (the front-left wall on screen); +x is the
front-right wall.  Camera: orthographic, rotation (60, 0, 45) degrees.
"""
import math
import os
import sys
import time

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "sprites")
TILE_PX = 128
PPU = TILE_PX / math.sqrt(2.0)  # pixels per world unit on screen
RIGHT = Vector((math.sqrt(0.5), math.sqrt(0.5), 0.0))
UP = Vector((-0.5 * math.sqrt(0.5), 0.5 * math.sqrt(0.5), math.sqrt(3) / 2))
VIEW = RIGHT.cross(UP)  # toward the camera
SUN_DIR = Vector((0.62, 0.62, -0.92)).normalized()  # direction light travels


# ---------------------------------------------------------------- colours

def srgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    lin = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in c]
    return (lin[0], lin[1], lin[2], 1.0)


# ---------------------------------------------------------------- materials

_MATS = {}


def _nodes(mat):
    try:
        mat.use_nodes = True
    except Exception:
        pass
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    return nt


def material(name, base, alt=None, kind="noise", scale=6.0, rough=0.85, bump=0.15,
             metal=0.0, emit=None, emit_strength=0.0, alpha=1.0, mortar=None,
             brick=(0.25, 0.12), stripe=0.0, detail=None):
    """Procedural material in UV space (UVs are world units, face aligned).

    kind: noise | brick | tiles (roof imbrex ridges) | stripes | flat
    """
    if name in _MATS:
        return _MATS[name]
    mat = bpy.data.materials.new(name)
    nt = _nodes(mat)
    N, L = nt.nodes, nt.links
    out = N.new("ShaderNodeOutputMaterial")
    bsdf = N.new("ShaderNodeBsdfPrincipled")
    L.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    uv = N.new("ShaderNodeUVMap")
    a, b = srgb(base), srgb(alt or base)

    noise = N.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = scale
    noise.inputs["Detail"].default_value = 6.0
    L.new(uv.outputs["UV"], noise.inputs["Vector"])
    ramp = N.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.3
    ramp.color_ramp.elements[1].position = 0.75
    ramp.color_ramp.elements[0].color = a
    ramp.color_ramp.elements[1].color = b
    L.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    colour = ramp.outputs["Color"]
    height = noise.outputs["Fac"]

    if kind == "brick":
        br = N.new("ShaderNodeTexBrick")
        br.inputs["Scale"].default_value = 1.0
        br.inputs["Brick Width"].default_value = brick[0]
        br.inputs["Row Height"].default_value = brick[1]
        br.inputs["Mortar Size"].default_value = 0.012
        br.inputs["Color1"].default_value = a
        br.inputs["Color2"].default_value = b
        br.inputs["Mortar"].default_value = srgb(mortar or "#777060")
        L.new(uv.outputs["UV"], br.inputs["Vector"])
        mix = N.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 0.35
        L.new(br.outputs["Color"], mix.inputs["A"])
        L.new(colour, mix.inputs["B"])
        colour = mix.outputs["Result"]
        inv = N.new("ShaderNodeMath")
        inv.operation = "SUBTRACT"
        inv.inputs[0].default_value = 1.0
        L.new(br.outputs["Fac"], inv.inputs[1])
        height = inv.outputs["Value"]
    elif kind == "tiles":
        # imbrex ridges run down the slope (along v); courses every brick[1]
        sep = N.new("ShaderNodeSeparateXYZ")
        L.new(uv.outputs["UV"], sep.inputs["Vector"])
        f = N.new("ShaderNodeMath")
        f.operation = "MULTIPLY"
        f.inputs[1].default_value = 2 * math.pi / brick[0]
        L.new(sep.outputs["X"], f.inputs[0])
        s = N.new("ShaderNodeMath")
        s.operation = "SINE"
        L.new(f.outputs["Value"], s.inputs[0])
        ab = N.new("ShaderNodeMath")
        ab.operation = "ABSOLUTE"
        L.new(s.outputs["Value"], ab.inputs[0])
        g = N.new("ShaderNodeMath")
        g.operation = "MULTIPLY"
        g.inputs[1].default_value = 1.0 / brick[1]
        L.new(sep.outputs["Y"], g.inputs[0])
        fr = N.new("ShaderNodeMath")
        fr.operation = "FRACT"
        L.new(g.outputs["Value"], fr.inputs[0])
        m = N.new("ShaderNodeMath")
        m.operation = "MULTIPLY"
        L.new(ab.outputs["Value"], m.inputs[0])
        L.new(fr.outputs["Value"], m.inputs[1])
        height = m.outputs["Value"]
        shade = N.new("ShaderNodeValToRGB")
        shade.color_ramp.elements[0].color = (0.55, 0.55, 0.55, 1)
        shade.color_ramp.elements[1].color = (1, 1, 1, 1)
        L.new(ab.outputs["Value"], shade.inputs["Fac"])
        mix = N.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        L.new(colour, mix.inputs["A"])
        L.new(shade.outputs["Color"], mix.inputs["B"])
        colour = mix.outputs["Result"]
    elif kind == "stripes":
        sep = N.new("ShaderNodeSeparateXYZ")
        L.new(uv.outputs["UV"], sep.inputs["Vector"])
        f = N.new("ShaderNodeMath")
        f.operation = "MULTIPLY"
        f.inputs[1].default_value = 1.0 / stripe
        L.new(sep.outputs["X"], f.inputs[0])
        fr = N.new("ShaderNodeMath")
        fr.operation = "FRACT"
        L.new(f.outputs["Value"], fr.inputs[0])
        st = N.new("ShaderNodeMath")
        st.operation = "GREATER_THAN"
        st.inputs[1].default_value = 0.5
        L.new(fr.outputs["Value"], st.inputs[0])
        mix = N.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        L.new(st.outputs["Value"], mix.inputs["Factor"])
        mix.inputs["A"].default_value = a
        mix.inputs["B"].default_value = b
        colour = mix.outputs["Result"]
    elif kind == "flat":
        colour = None
        bsdf.inputs["Base Color"].default_value = a

    if colour is not None:
        L.new(colour, bsdf.inputs["Base Color"])
    if detail:
        # second, larger-scale tint variation (weathering, grass patches)
        n2 = N.new("ShaderNodeTexNoise")
        n2.inputs["Scale"].default_value = detail[0]
        n2.inputs["Detail"].default_value = 3.0
        L.new(uv.outputs["UV"], n2.inputs["Vector"])
        r2 = N.new("ShaderNodeValToRGB")
        r2.color_ramp.elements[0].position = 0.35
        r2.color_ramp.elements[1].position = 0.7
        r2.color_ramp.elements[0].color = srgb(detail[1])
        r2.color_ramp.elements[1].color = (1, 1, 1, 1)
        L.new(n2.outputs["Fac"], r2.inputs["Fac"])
        mix = N.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        L.new(bsdf.inputs["Base Color"].links[0].from_socket if bsdf.inputs["Base Color"].links
              else ramp.outputs["Color"], mix.inputs["A"])
        L.new(r2.outputs["Color"], mix.inputs["B"])
        L.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    if bump > 0:
        bp = N.new("ShaderNodeBump")
        bp.inputs["Strength"].default_value = bump
        bp.inputs["Distance"].default_value = 0.02
        L.new(height, bp.inputs["Height"])
        L.new(bp.outputs["Normal"], bsdf.inputs["Normal"])
    if emit:
        bsdf.inputs["Emission Color"].default_value = srgb(emit)
        bsdf.inputs["Emission Strength"].default_value = emit_strength
    if alpha < 1.0:
        bsdf.inputs["Alpha"].default_value = alpha
    _MATS[name] = mat
    return mat


def palette():
    M = {}
    M["plaster"] = material("plaster", "#ece0c6", "#dccaa6", scale=9, bump=0.08,
                            detail=(1.5, "#d8c2a0"))
    M["plaster_humble"] = material("plaster_humble", "#d9c7a4", "#c4ae86", scale=9,
                                   bump=0.1, detail=(1.5, "#bfa47e"))
    M["plaster_ochre"] = material("plaster_ochre", "#e6b75c", "#d4a248", scale=9, bump=0.08,
                                  detail=(1.5, "#d9b070"))
    M["plaster_house"] = material("plaster_house", "#e2d3b5", "#d0bd98", scale=9, bump=0.08,
                                  detail=(1.5, "#cdb898"))
    M["plaster_warm"] = material("plaster_warm", "#dcc296", "#c9ab7c", scale=9, bump=0.08,
                                 detail=(1.5, "#c8ae86"))
    M["plaster_humble2"] = material("plaster_humble2", "#d2bfa6", "#bda98c", scale=9, bump=0.1,
                                    detail=(1.5, "#b59c80"))
    M["band_brown"] = material("band_brown", "#7a5638", "#68472d", scale=10, bump=0.05)
    M["roof_house"] = material("roof_house", "#b4613f", "#9b4f33", kind="tiles", scale=7,
                               brick=(0.085, 0.07), bump=0.5, detail=(2.0, "#a88a76"))
    M["band_red"] = material("band_red", "#a13b26", "#8c3220", scale=10, bump=0.05)
    M["roof"] = material("roof", "#c25a32", "#a8462a", kind="tiles", scale=7,
                         brick=(0.085, 0.07), bump=0.5, detail=(2.0, "#b88a70"))
    M["roof_faded"] = material("roof_faded", "#b9805e", "#a46a4c", kind="tiles", scale=7,
                               brick=(0.085, 0.07), bump=0.5, detail=(2.0, "#9f8a78"))
    M["roof_slate"] = material("roof_slate", "#454850", "#33353b", kind="tiles", scale=7,
                               brick=(0.09, 0.075), bump=0.5)
    M["roof_timber"] = material("roof_timber", "#6b4a33", "#55392a", kind="tiles", scale=7,
                                brick=(0.1, 0.08), bump=0.5)
    M["roof_marble"] = material("roof_marble", "#e8e6e0", "#cfcbc2", kind="tiles", scale=7,
                                brick=(0.08, 0.07), bump=0.4)
    M["travertine"] = material("travertine", "#d3c196", "#bca877", kind="brick", scale=12,
                               brick=(0.28, 0.1), mortar="#9a8a66", bump=0.25)
    M["stone_dark"] = material("stone_dark", "#7d756b", "#5f5850", kind="brick", scale=12,
                               brick=(0.18, 0.09), mortar="#45403a", bump=0.3)
    M["brick"] = material("brick", "#b0603e", "#93492e", kind="brick", scale=12,
                          brick=(0.1, 0.045), mortar="#6e4a3a", bump=0.3)
    M["marble"] = material("marble", "#f6f3ec", "#e2ddd2", scale=5, rough=0.45, bump=0.03)
    M["gold"] = material("gold", "#e0b445", "#c8962e", scale=20, rough=0.3, metal=0.9,
                         bump=0.0)
    M["timber"] = material("timber", "#5a3f2b", "#46301f", scale=14, bump=0.15)
    M["timber_light"] = material("timber_light", "#8a6644", "#715236", scale=14, bump=0.15)
    M["dark"] = material("dark", "#2b211b", "#211913", kind="flat", bump=0)
    M["window"] = material("window", "#3a2a22", "#2a1e18", kind="flat", bump=0)
    M["iron"] = material("iron", "#45474c", "#2f3034", scale=20, rough=0.45, metal=0.7,
                         bump=0.05)
    M["awning"] = material("awning", "#f2b01e", "#f6e7bf", kind="stripes", stripe=0.09,
                           bump=0.0, rough=0.9)
    M["sack"] = material("sack", "#d3b67c", "#bb9b62", scale=25, bump=0.2)
    M["bread"] = material("bread", "#c9843a", "#a8652a", scale=20, bump=0.1)
    M["amphora"] = material("amphora", "#bf6a3e", "#a85a32", scale=10, bump=0.05, rough=0.6)
    M["water"] = material("water", "#4d93b8", "#3b7ea3", scale=6, rough=0.1, bump=0.02)
    M["leaf"] = material("leaf", "#5d8130", "#476824", scale=18, bump=0.3)
    M["olive_leaf"] = material("olive_leaf", "#8a9a5e", "#6c7d46", scale=18, bump=0.3)
    M["cypress"] = material("cypress", "#2d4a26", "#21381c", scale=18, bump=0.3)
    M["trunk"] = material("trunk", "#5b4632", "#46362a", scale=20, bump=0.1)
    M["glow"] = material("glow", "#ff8a1e", "#ffb347", scale=15, bump=0.0, emit="#ff7a12",
                         emit_strength=12.0)
    M["fire"] = material("fire", "#ffb030", "#ff8020", scale=15, bump=0.0, emit="#ff9a2a",
                         emit_strength=10.0)
    M["smoke"] = material("smoke", "#b4afa6", "#9d978d", scale=4, bump=0.0, alpha=0.38)
    M["soil"] = material("soil", "#6f5236", "#5c4229", scale=16, bump=0.4)
    M["crop"] = material("crop", "#6aa036", "#4f8a2a", scale=20, bump=0.2)
    M["charcoal"] = material("charcoal", "#26221f", "#3a3430", scale=25, bump=0.5)
    # grounds
    M["grass"] = material("grass", "#6f8f3c", "#5d7c31", scale=11, bump=0.25,
                          detail=(0.9, "#9aa36a"))
    M["garden"] = material("garden", "#78963f", "#668634", scale=12, bump=0.25,
                           detail=(1.2, "#a8b070"))
    M["dirt"] = material("dirt", "#a3825a", "#8c6c47", scale=9, bump=0.3,
                         detail=(1.2, "#b89a74"))
    M["road"] = material("road", "#77736c", "#5f5b55", kind="brick", scale=10,
                         brick=(0.21, 0.17), mortar="#4a463f", bump=0.45,
                         detail=(1.1, "#9b9284"))
    M["soot"] = material("soot", "#6a5c4d", "#57493c", scale=10, bump=0.3,
                         detail=(1.3, "#555050"))
    M["paving"] = material("paving", "#c2b496", "#ab9d7f", kind="brick", scale=12,
                           brick=(0.22, 0.16), mortar="#857a64", bump=0.3)
    M["marble_paving"] = material("marble_paving", "#efebe2", "#dcd6ca", kind="brick",
                                  scale=6, brick=(0.25, 0.25), mortar="#b9b2a4", bump=0.15,
                                  rough=0.5)
    return M


# ---------------------------------------------------------------- geometry

def _uv(bm, off=(0.0, 0.0)):
    layer = bm.loops.layers.uv.verify()
    z = Vector((0, 0, 1))
    for f in bm.faces:
        n = f.normal
        e = n.cross(z)
        if e.length < 1e-4:
            e = Vector((1, 0, 0))
        e.normalize()
        g = n.cross(e).normalized()
        for lp in f.loops:
            lp[layer].uv = (lp.vert.co.dot(e) + off[0], lp.vert.co.dot(g) + off[1])


def mesh(name, verts, faces, mat, smooth=False, bevel=0.0, solidify=0.0, uv_off=(0, 0)):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    vs = [bm.verts.new(v) for v in verts]
    for f in faces:
        try:
            bm.faces.new([vs[i] for i in f])
        except ValueError:
            pass
    bm.normal_update()
    _uv(bm, uv_off)
    for f in bm.faces:
        f.smooth = smooth
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    if solidify:
        m = ob.modifiers.new("solid", "SOLIDIFY")
        m.thickness = solidify
        m.offset = -1.0
    if bevel:
        m = ob.modifiers.new("bevel", "BEVEL")
        m.width = bevel
        m.segments = 2
        m.limit_method = "ANGLE"
    return ob


def box(x0, y0, z0, x1, y1, z1, mat, bevel=0.012, name="box"):
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
         (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    f = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    return mesh(name, v, f, mat, bevel=bevel)


def gable(x0, y0, x1, y1, z, h, mat, axis="x", over=0.06, wall=None):
    """Gable roof; ridge along `axis`.  Gable ends get `wall` material."""
    x0 -= over
    y0 -= over
    x1 += over
    y1 += over
    if axis == "x":
        ym = (y0 + y1) / 2
        v = [(x0, y0, z), (x1, y0, z), (x1, ym, z + h), (x0, ym, z + h),
             (x0, y1, z), (x1, y1, z)]
        slopes = [(0, 1, 2, 3), (3, 2, 5, 4)]
        ends = [(0, 3, 4), (1, 5, 2)]
    else:
        xm = (x0 + x1) / 2
        v = [(x0, y0, z), (xm, y0, z + h), (xm, y1, z + h), (x0, y1, z),
             (x1, y0, z), (x1, y1, z)]
        slopes = [(0, 1, 2, 3), (1, 4, 5, 2)]
        ends = [(0, 4, 1), (3, 2, 5)]
    r = mesh("roof", v, slopes, mat, solidify=0.035)
    if wall is not None:
        # gable triangles on the wall plane; they stay under the overhanging roof
        X0, Y0, X1, Y1 = x0 + over, y0 + over, x1 - over, y1 - over
        if axis == "x":
            ym = (Y0 + Y1) / 2
            verts = [(X0, Y0, z), (X0, Y1, z), (X0, ym, z + h),
                     (X1, Y0, z), (X1, ym, z + h), (X1, Y1, z)]
        else:
            xm = (X0 + X1) / 2
            verts = [(X0, Y0, z), (xm, Y0, z + h), (X1, Y0, z),
                     (X0, Y1, z), (X1, Y1, z), (xm, Y1, z + h)]
        mesh("gable", verts, [(0, 1, 2), (3, 4, 5)], wall)
    return r


def hip(x0, y0, x1, y1, z, h, mat, over=0.06):
    x0 -= over
    y0 -= over
    x1 += over
    y1 += over
    w, d = x1 - x0, y1 - y0
    if w >= d:
        i = d / 2
        v = [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z),
             (x0 + i, y0 + i, z + h), (x1 - i, y0 + i, z + h)]
        f = [(0, 1, 5, 4), (1, 2, 5), (2, 3, 4, 5), (3, 0, 4)]
    else:
        i = w / 2
        v = [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z),
             (x0 + i, y0 + i, z + h), (x0 + i, y1 - i, z + h)]
        f = [(0, 1, 4), (1, 2, 5, 4), (2, 3, 5), (3, 0, 4, 5)]
    return mesh("hip", v, f, mat, solidify=0.035)


def ring_roof(x0, y0, x1, y1, z, h, hole, mat, over=0.06):
    """Atrium house roof: hips outward, a square compluvium in the middle."""
    x0 -= over
    y0 -= over
    x1 += over
    y1 += over
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    rx, ry = (x1 - x0) / 2, (y1 - y0) / 2
    k = 0.55  # ridge ring position between hole and eave
    hx, hy = hole, hole
    mx, my = hx + (rx - hx) * k, hy + (ry - hy) * k
    outer = [(cx - rx, cy - ry, z), (cx + rx, cy - ry, z), (cx + rx, cy + ry, z), (cx - rx, cy + ry, z)]
    mid = [(cx - mx, cy - my, z + h), (cx + mx, cy - my, z + h), (cx + mx, cy + my, z + h),
           (cx - mx, cy + my, z + h)]
    inner = [(cx - hx, cy - hy, z + h * 0.45), (cx + hx, cy - hy, z + h * 0.45),
             (cx + hx, cy + hy, z + h * 0.45), (cx - hx, cy + hy, z + h * 0.45)]
    v = outer + mid + inner
    f = []
    for i in range(4):
        j = (i + 1) % 4
        f.append((i, j, 4 + j, 4 + i))
        f.append((4 + i, 4 + j, 8 + j, 8 + i))
    return mesh("ring", v, f, mat, solidify=0.035)


def cylinder(x, y, z0, z1, r, mat, n=12, name="cyl", smooth=True, rx=None, axis="z"):
    v, f = [], []
    for i in range(n):
        a = 2 * math.pi * i / n
        c, s = math.cos(a) * r, math.sin(a) * (rx or r)
        if axis == "z":
            v += [(x + c, y + s, z0), (x + c, y + s, z1)]
        elif axis == "x":  # z0..z1 along x, centre (x=ignored, y, z=x param)
            v += [(z0, y + c, x + s), (z1, y + c, x + s)]
        else:  # along y
            v += [(y + c, z0, x + s), (y + c, z1, x + s)]
    for i in range(n):
        j = (i + 1) % n
        f.append((2 * i, 2 * j, 2 * j + 1, 2 * i + 1))
    f.append(tuple(2 * i + 1 for i in range(n)))
    f.append(tuple(2 * i for i in reversed(range(n))))
    ob = mesh(name, v, f, mat, smooth=False)
    if smooth:
        for p in ob.data.polygons:
            p.use_smooth = len(p.vertices) == 4
    return ob


def blob(x, y, z, rx, ry, rz, mat, segs=10, rings=6, top_only=False, name="blob"):
    """Ellipsoid (or its upper half, capped flat, when top_only)."""
    last = rings // 2 if top_only else rings - 1
    v = [(x, y, z + rz)]
    for i in range(1, last + 1):
        th = math.pi * i / rings
        for j in range(segs):
            ph = 2 * math.pi * j / segs
            v.append((x + rx * math.sin(th) * math.cos(ph), y + ry * math.sin(th) * math.sin(ph),
                      z + rz * math.cos(th)))
    f = [(0, 1 + j, 1 + (j + 1) % segs) for j in range(segs)]
    for i in range(last - 1):
        for j in range(segs):
            a = 1 + i * segs + j
            b = 1 + i * segs + (j + 1) % segs
            f.append((a, a + segs, b + segs, b))
    base = 1 + (last - 1) * segs
    if top_only:
        f.append(tuple(base + j for j in range(segs)))
    else:
        v.append((x, y, z - rz))
        f += [(len(v) - 1, base + (j + 1) % segs, base + j) for j in range(segs)]
    ob = mesh(name, v, f, mat)
    for p in ob.data.polygons:
        p.use_smooth = True
    return ob


def column(x, y, z0, h, r, mat):
    box(x - r * 1.35, y - r * 1.35, z0, x + r * 1.35, y + r * 1.35, z0 + 0.04, mat, bevel=0.006)
    cylinder(x, y, z0 + 0.04, z0 + h - 0.05, r, mat, n=14, name="shaft")
    box(x - r * 1.45, y - r * 1.45, z0 + h - 0.05, x + r * 1.45, y + r * 1.45, z0 + h, mat,
        bevel=0.006)


def steps(x0, x1, y_front, y_back, z_top, n, mat):
    d = (y_back - y_front) / n
    for i in range(n):
        box(x0, y_front + i * d, 0, x1, y_back, z_top * (i + 1) / n, mat, bevel=0.004)


def fence(pts, mat, h=0.12):
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        n = max(1, int(L / 0.14))
        for i in range(n + 1):
            t = i / n
            px, py = ax + (bx - ax) * t, ay + (by - ay) * t
            box(px - 0.012, py - 0.012, 0, px + 0.012, py + 0.012, h, mat, bevel=0)
        for zz in (h * 0.45, h * 0.85):
            xs, ys = sorted((ax, bx)), sorted((ay, by))
            box(xs[0] - 0.008, ys[0] - 0.008, zz - 0.01, xs[1] + 0.008, ys[1] + 0.008, zz + 0.01,
                mat, bevel=0)


def sack(x, y, z, M, s=1.0):
    blob(x, y, z + 0.045 * s, 0.06 * s, 0.05 * s, 0.05 * s, M["sack"], name="sack")


def amphora(x, y, M, s=1.0):
    blob(x, y, 0.09 * s, 0.045 * s, 0.045 * s, 0.075 * s, M["amphora"], name="amph")
    cylinder(x, y, 0.15 * s, 0.2 * s, 0.018 * s, M["amphora"], n=8)


def cypress(x, y, h, M):
    cylinder(x, y, 0, 0.08, 0.025, M["trunk"], n=6)
    blob(x, y, 0.05 + h / 2, 0.11, 0.11, h / 2, M["cypress"], segs=10, rings=8, name="cyp")


def olive(x, y, M, s=1.0):
    cylinder(x, y, 0, 0.2 * s, 0.03 * s, M["trunk"], n=6)
    blob(x, y, 0.27 * s, 0.2 * s, 0.18 * s, 0.13 * s, M["olive_leaf"], name="crown")
    blob(x + 0.08 * s, y - 0.06 * s, 0.33 * s, 0.11 * s, 0.1 * s, 0.08 * s, M["olive_leaf"],
         name="crown2")


def pot_plant(x, y, M):
    cylinder(x, y, 0.0, 0.07, 0.04, M["amphora"], n=10)
    blob(x, y, 0.11, 0.06, 0.06, 0.06, M["leaf"], name="plant")


def cart(x, y, M, load=True):
    box(x - 0.22, y - 0.12, 0.09, x + 0.22, y + 0.12, 0.13, M["timber_light"], bevel=0.006)
    for sx in (-0.22, 0.22):
        box(x - 0.22, y - 0.12, 0.13, x + 0.22, y - 0.11, 0.19, M["timber_light"], bevel=0)
        box(x - 0.22, y + 0.11, 0.13, x + 0.22, y + 0.12, 0.19, M["timber_light"], bevel=0)
    for wy in (y - 0.14, y + 0.14):
        cylinder(0.09, x, wy - 0.015, wy + 0.015, 0.09, M["timber"], n=14, axis="y",
                 name="wheel")
    box(x + 0.22, y - 0.01, 0.1, x + 0.42, y + 0.01, 0.12, M["timber"], bevel=0)
    if load:
        for i, (dx, dy) in enumerate([(-0.12, -0.05), (0.0, -0.05), (0.12, -0.05),
                                      (-0.06, 0.05), (0.06, 0.05), (-0.16, 0.05)]):
            sack(x + dx, y + dy, 0.13, M, 1.0)
        sack(x - 0.03, y, 0.2, M, 1.0)


def lot(n, mat, curb=None, uv_off=(0, 0)):
    h = n / 2
    mesh("lot", [(-h, -h, 0), (h, -h, 0), (h, h, 0), (-h, h, 0)], [(0, 1, 2, 3)], mat,
         uv_off=uv_off)
    if curb:
        t = 0.05
        box(-h, -h, 0, h, -h + t, 0.03, curb, bevel=0.005)
        box(-h, h - t, 0, h, h, 0.03, curb, bevel=0.005)
        box(-h, -h, 0, -h + t, h, 0.03, curb, bevel=0.005)
        box(h - t, -h, 0, h, h, 0.03, curb, bevel=0.005)


def windows_y(face_y, xs, z0, z1, M, w=0.08, outward=-1):
    for x in xs:
        box(x - w / 2, face_y + outward * 0.012 - 0.01, z0, x + w / 2, face_y + outward * 0.012 + 0.01,
            z1, M["window"], bevel=0)


def windows_x(face_x, ys, z0, z1, M, w=0.08):
    for y in ys:
        box(face_x - 0.01 + 0.012, y - w / 2, z0, face_x + 0.01 + 0.012, y + w / 2, z1, M["window"],
            bevel=0)


# ---------------------------------------------------------------- buildings

def house_small(M, v=0):
    """v=1 mirrors the plan (patch on the left) and uses a gable roof."""
    k = -1 if v else 1
    lot(2, M["garden"])
    # path from the road edge to the door
    mesh("path", [(k * -0.45, -1, 0.004), (k * -0.25, -1, 0.004), (k * -0.25, -0.2, 0.004),
                  (k * -0.45, -0.2, 0.004)], [(0, 1, 2, 3)] if k > 0 else [(0, 3, 2, 1)],
         M["dirt"])
    x0, x1 = sorted((k * -0.85, k * 0.45))
    y0, y1, zw = -0.2, 0.8, 0.4
    wall = M["plaster_humble2"] if v else M["plaster_humble"]
    box(x0, y0, 0, x1, y1, zw, wall)
    if v:
        gable(x0, y0, x1, y1, zw, 0.26, M["roof_faded"], axis="y", wall=wall)
    else:
        hip(x0, y0, x1, y1, zw, 0.24, M["roof_faded"])
    dx = sorted((k * -0.43, k * -0.27))
    box(dx[0], y0 - 0.02, 0, dx[1], y0 + 0.01, 0.26, M["dark"], bevel=0)
    windows_y(y0, [k * 0.12], 0.17, 0.27, M)
    windows_x(x1, [0.3], 0.17, 0.27, M)
    # vegetable patch, fenced, at the front
    px0, px1 = sorted((k * 0.08, k * 0.9))
    box(px0, -0.92, 0, px1, -0.32, 0.03, M["soil"], bevel=0.01)
    for i in range(4):
        for j in range(6):
            blob(px0 + 0.08 + j * 0.135, -0.84 + i * 0.15, 0.06, 0.05, 0.045, 0.04, M["crop"],
                 segs=8, rings=4)
    fx0, fx1 = px0 - 0.03, px1 + 0.03
    fence([(fx0, -0.95), (fx1, -0.95), (fx1, -0.29), (fx0, -0.29), (fx0, -0.6)],
          M["timber_light"], h=0.1)
    amphora(k * -0.65, -0.33, M, 0.9)
    olive(k * 0.75, 0.6, M, 0.8)


def house_large(M, v=0):
    """v=1: warm ochre plaster, brown base band, a cypress instead of pots."""
    lot(2, M["garden"])
    mesh("path", [(-0.15, -1, 0.004), (0.15, -1, 0.004), (0.15, -0.7, 0.004),
                  (-0.15, -0.7, 0.004)], [(0, 1, 2, 3)], M["paving"])
    x0, y0, x1, y1, zw = -0.88, -0.72, 0.88, 0.9, 0.72
    band = M["band_brown"] if v else M["band_red"]
    box(x0, y0, 0.12, x1, y1, zw, M["plaster_warm"] if v else M["plaster_house"])
    box(x0 - 0.015, y0 - 0.015, 0, x1 + 0.015, y1 + 0.015, 0.14, band)
    ring_roof(x0, y0, x1, y1, zw, 0.26, 0.24, M["roof_house"])
    # compluvium: shaded court with the impluvium pool below
    box(-0.24, -0.24 + 0.09, zw - 0.02, 0.24, 0.24 + 0.09, zw + 0.07, M["dark"], bevel=0)
    box(-0.13, -0.13 + 0.09, zw + 0.0, 0.13, 0.13 + 0.09, zw + 0.075, M["water"], bevel=0)
    # door with red-brown frame
    box(-0.13, y0 - 0.03, 0, 0.13, y0 + 0.01, 0.34, band, bevel=0.004)
    box(-0.09, y0 - 0.04, 0, 0.09, y0 + 0.01, 0.3, M["timber"], bevel=0)
    windows_y(y0, [-0.6, -0.35, 0.35, 0.6], 0.48, 0.6, M, w=0.07)
    windows_y(y0, [-0.6, 0.6], 0.22, 0.32, M, w=0.07)
    windows_x(x1, [-0.45, -0.15, 0.15, 0.45, 0.72], 0.48, 0.6, M, w=0.07)
    windows_x(x1, [-0.45, 0.45], 0.22, 0.32, M, w=0.07)
    if v:
        cypress(0.8, -0.86, 0.42, M)
        pot_plant(-0.3, -0.85, M)
    else:
        pot_plant(-0.3, -0.85, M)
        pot_plant(0.3, -0.85, M)
        pot_plant(0.75, -0.85, M)


def bakery(M):
    lot(2, M["dirt"])
    x0, y0, x1, y1, zw = -0.9, -0.15, 0.2, 0.9, 0.48
    box(x0, y0, 0, x1, y1, zw, M["plaster_ochre"])
    gable(x0, y0, x1, y1, zw, 0.3, M["roof_house"], axis="x", wall=M["plaster_ochre"])
    # second awning along the right-hand wall
    mesh("awning2", [(x1, y0 + 0.05, 0.44), (x1, y1 - 0.05, 0.44), (x1 + 0.3, y1 - 0.05, 0.31),
                     (x1 + 0.3, y0 + 0.05, 0.31)], [(0, 1, 2, 3)], M["awning"], solidify=0.012)
    for y in (y0 + 0.08, y1 - 0.08):
        box(x1 + 0.27, y - 0.012, 0, x1 + 0.3, y + 0.012, 0.31, M["timber"], bevel=0)
    # saffron awning over the shop counter
    ax0, ax1 = x0 + 0.02, x1 - 0.02
    mesh("awning", [(ax0, y0, 0.44), (ax1, y0, 0.44), (ax1, y0 - 0.42, 0.3), (ax0, y0 - 0.42, 0.3)],
         [(0, 3, 2, 1)], M["awning"], solidify=0.012)
    for x in (ax0 + 0.03, ax1 - 0.03):
        box(x - 0.012, y0 - 0.42, 0, x + 0.012, y0 - 0.39, 0.3, M["timber"], bevel=0)
    box(x0 + 0.08, y0 - 0.22, 0, x1 - 0.08, y0 - 0.08, 0.15, M["timber_light"], bevel=0.006)
    for i in range(7):
        blob(x0 + 0.17 + i * 0.13, y0 - 0.15, 0.17, 0.045, 0.035, 0.03, M["bread"], segs=8, rings=4)
    box(-0.12, y0 - 0.02, 0, 0.02, y0 + 0.01, 0.28, M["dark"], bevel=0)
    # domed brick oven with a smoke plume
    ox, oy = 0.55, -0.48
    blob(ox, oy, 0.0, 0.3, 0.3, 0.3, M["brick"], segs=16, rings=10, top_only=True, name="oven")
    box(ox - 0.08, oy - 0.31, 0, ox + 0.08, oy - 0.2, 0.15, M["dark"], bevel=0)
    blob(ox, oy - 0.28, 0.03, 0.05, 0.03, 0.04, M["fire"], segs=8, rings=4)
    cylinder(ox + 0.06, oy + 0.06, 0.2, 0.42, 0.05, M["brick"], n=8)
    for i, (dx, dy, dz, r) in enumerate([(0.07, 0.08, 0.5, 0.06), (0.11, 0.13, 0.62, 0.08),
                                          (0.16, 0.19, 0.76, 0.1), (0.22, 0.25, 0.92, 0.12)]):
        blob(ox + dx, oy + dy, dz, r, r, r * 0.85, M["smoke"], name="smoke")
    # donkey millstone and grain sacks
    mx, my = 0.68, 0.62
    cylinder(mx, my, 0, 0.06, 0.24, M["stone_dark"], n=18)
    blob(mx, my, 0.06, 0.1, 0.1, 0.22, M["stone_dark"], segs=12, rings=6, name="meta")
    box(mx - 0.32, my - 0.015, 0.2, mx + 0.32, my + 0.015, 0.23, M["timber"], bevel=0)
    for dx, dy in [(-0.05, -0.05), (0.08, -0.04), (0.02, 0.06)]:
        sack(0.62 + dx, 0.12 + dy, 0, M)


def blacksmith(M):
    lot(2, M["soot"])
    x0, y0, x1, y1 = -0.9, -0.35, 0.35, 0.88
    zw = 0.5
    # stone back and side walls, open front
    box(x0, y1 - 0.12, 0, x1, y1, zw, M["stone_dark"])
    box(x0, y0 + 0.2, 0, x0 + 0.12, y1, zw, M["stone_dark"])
    for x, y in [(x1 - 0.04, y0), (x0 + 0.04, y0), (x1 - 0.04, y0 + 0.62)]:
        box(x - 0.035, y - 0.035, 0, x + 0.035, y + 0.035, zw, M["timber"], bevel=0.005)
    gable(x0, y0 - 0.04, x1, y1, zw, 0.28, M["roof_slate"], axis="y", wall=M["stone_dark"])
    # hearth and chimney
    box(-0.75, 0.25, 0, -0.2, 0.7, 0.2, M["stone_dark"])
    box(-0.7, 0.3, 0.2, -0.25, 0.65, 0.235, M["glow"], bevel=0)
    box(-0.62, 0.5, 0, -0.32, 0.75, 1.02, M["stone_dark"])
    box(-0.6, 0.52, 1.0, -0.34, 0.73, 1.03, M["charcoal"], bevel=0)
    lamp = bpy.data.lights.new("forge", "POINT")
    lamp.energy = 60
    lamp.color = (1.0, 0.45, 0.12)
    lamp.shadow_soft_size = 0.2
    lo = bpy.data.objects.new("forge", lamp)
    lo.location = (-0.45, 0.2, 0.35)
    bpy.context.scene.collection.objects.link(lo)
    # yard: anvil on a stump, iron bars, charcoal heap, quench barrel
    cylinder(0.05, -0.62, 0, 0.14, 0.09, M["timber"], n=10)
    box(-0.07, -0.67, 0.14, 0.17, -0.57, 0.2, M["iron"], bevel=0.008)
    box(0.0, -0.65, 0.12, 0.1, -0.59, 0.15, M["iron"], bevel=0)
    for i in range(4):
        for j in range(5 - i):
            bx = 0.5 + j * 0.06 + i * 0.03
            box(bx - 0.022, -0.85, 0.03 + i * 0.045, bx + 0.022, -0.3, 0.07 + i * 0.045,
                M["iron"], bevel=0.004)
    box(0.48, -0.8, 0, 0.8, -0.78, 0.03, M["timber"], bevel=0)
    box(0.48, -0.37, 0, 0.8, -0.35, 0.03, M["timber"], bevel=0)
    blob(0.62, 0.45, 0.0, 0.26, 0.24, 0.17, M["charcoal"], segs=12, rings=6, top_only=True)
    cylinder(-0.55, -0.6, 0, 0.16, 0.1, M["timber_light"], n=12)
    box(-0.63, -0.68, 0.14, -0.47, -0.52, 0.15, M["water"], bevel=0)


def granary(M):
    lot(3, M["paving"], curb=M["travertine"])
    x0, y0, x1, y1 = -1.38, -0.3, 1.38, 1.36
    box(x0 - 0.04, y0 - 0.04, 0, x1 + 0.04, y1 + 0.04, 0.16, M["stone_dark"])
    box(x0, y0, 0.16, x1, y1, 0.86, M["travertine"])
    for i in range(7):
        bx = x0 + 0.2 + i * (x1 - x0 - 0.4) / 6
        if abs(bx) < 0.3:
            continue
        box(bx - 0.06, y0 - 0.1, 0.16, bx + 0.06, y0, 0.74, M["travertine"], bevel=0.01)
    for j in range(4):
        by = y0 + 0.25 + j * (y1 - y0 - 0.5) / 3
        box(x1, by - 0.06, 0.16, x1 + 0.1, by + 0.06, 0.74, M["travertine"], bevel=0.01)
    gable(x0, y0, x1, y1, 0.86, 0.42, M["roof_timber"], axis="x", over=0.09,
          wall=M["travertine"])
    # vents high on the walls
    windows_y(y0, [-1.05, -0.65, 0.65, 1.05], 0.62, 0.72, M, w=0.05)
    windows_x(x1 + 0.0, [0.0, 0.55, 1.05], 0.62, 0.72, M, w=0.05)
    # big double doors at a loading dock
    box(-0.32, y0 - 0.03, 0.16, 0.32, y0 + 0.01, 0.66, M["travertine"], bevel=0.004)
    box(-0.26, y0 - 0.045, 0.16, -0.01, y0 + 0.01, 0.6, M["timber"], bevel=0.004)
    box(0.01, y0 - 0.045, 0.16, 0.26, y0 + 0.01, 0.6, M["timber"], bevel=0.004)
    box(-0.4, y0 - 0.32, 0, 0.4, y0, 0.16, M["stone_dark"], bevel=0.008)
    # loading yard: cart with sacks, sack piles, amphorae
    cart(0.75, -0.95, M)
    for i, (dx, dy, dz) in enumerate([(0, 0, 0), (0.12, 0, 0), (0.24, 0, 0), (0.06, 0.1, 0),
                                       (0.18, 0.1, 0), (0.06, 0.04, 0.09), (0.18, 0.04, 0.09),
                                       (0.12, 0.08, 0.17)]):
        sack(-1.2 + dx, -1.2 + dy, dz, M)
    for i in range(4):
        amphora(-1.25 + i * 0.12, -0.55, M)
    for i in range(3):
        sack(-0.55 + i * 0.12, -0.75, 0, M)


def temple(M):
    lot(3, M["marble_paving"], curb=M["marble"])
    px0, py0, px1, py1, pz = -0.95, -0.55, 0.95, 1.38, 0.36
    box(px0, py0, 0, px1, py1, pz, M["marble"])
    box(px0 - 0.03, py0 - 0.03, pz - 0.04, px1 + 0.03, py1 + 0.03, pz, M["marble"], bevel=0.006)
    steps(-0.72, 0.72, -1.05, py0 + 0.01, pz, 6, M["marble"])
    # cella
    cz = 1.22
    box(-0.72, 0.15, pz, 0.72, 1.28, cz, M["marble"])
    box(-0.16, 0.13, pz, 0.16, 0.16, pz + 0.55, M["dark"], bevel=0)
    # front columns (hexastyle) and side colonnade
    xs = [-0.78 + i * (1.56 / 5) for i in range(6)]
    for x in xs:
        column(x, -0.42, pz, cz - pz, 0.052, M["marble"])
    for y in (-0.12, 0.2, 0.52, 0.84, 1.16):
        column(-0.78, y, pz, cz - pz, 0.052, M["marble"])
        column(0.78, y, pz, cz - pz, 0.052, M["marble"])
    # entablature with a gold frieze
    ez = cz + 0.13
    box(-0.86, -0.5, cz, 0.86, 1.32, ez, M["marble"], bevel=0.006)
    box(-0.875, -0.515, ez - 0.07, 0.875, 1.335, ez - 0.03, M["gold"], bevel=0)
    gable(-0.86, -0.5, 0.86, 1.32, ez, 0.36, M["roof_marble"], axis="y", over=0.05,
          wall=M["marble"])
    # gold ridge and acroteria
    box(-0.03, -0.55, ez + 0.35, 0.03, 1.37, ez + 0.4, M["gold"], bevel=0.006)
    for x, z in [(0, ez + 0.4), (-0.9, ez + 0.02), (0.9, ez + 0.02)]:
        blob(x, -0.55, z + 0.06, 0.05, 0.03, 0.07, M["gold"], segs=8, rings=4, name="acro")
    blob(0, -0.555, ez + 0.13, 0.08, 0.02, 0.06, M["gold"], segs=8, rings=4, name="pediment_disc")
    # altar with fire on the paving in front
    box(-0.16, -1.38, 0, 0.16, -1.18, 0.16, M["marble"], bevel=0.01)
    box(-0.17, -1.39, 0.15, 0.17, -1.17, 0.18, M["gold"], bevel=0.004)
    blob(0, -1.28, 0.2, 0.06, 0.05, 0.07, M["fire"], segs=8, rings=4)
    cypress(-1.25, 0.3, 0.55, M)
    cypress(1.25, -0.25, 0.55, M)
    cypress(-1.25, 1.1, 0.55, M)


BUILDINGS = {"house_small": (2, house_small), "house_large": (2, house_large),
             "house_small@1": (2, lambda M: house_small(M, 1)),
             "house_large@1": (2, lambda M: house_large(M, 1)),
             "bakery": (2, bakery), "blacksmith": (2, blacksmith),
             "granary": (3, granary), "temple": (3, temple)}


# ---------------------------------------------------------------- scene / render

def reset_scene(samples):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    _MATS.clear()
    s = bpy.context.scene
    s.render.engine = "CYCLES"
    try:
        p = bpy.context.preferences.addons["cycles"].preferences
        p.compute_device_type = "METAL"
        p.get_devices()
        for d in p.devices:
            d.use = True
        s.cycles.device = "GPU"
    except Exception as e:  # CPU fallback
        print("cycles GPU unavailable:", e)
    s.cycles.samples = samples
    s.cycles.use_denoising = True
    s.cycles.max_bounces = 4
    s.render.film_transparent = True
    s.render.filter_size = 1.0
    s.render.image_settings.file_format = "PNG"
    s.render.image_settings.color_mode = "RGBA"
    s.view_settings.view_transform = "AgX"
    for look in ("AgX - Medium High Contrast", "Medium High Contrast"):
        try:
            s.view_settings.look = look
            break
        except Exception:
            pass
    s.view_settings.exposure = 0.15
    w = bpy.data.worlds.new("sky")
    nt = _nodes(w)
    bg = nt.nodes.new("ShaderNodeBackground")
    bg.inputs["Color"].default_value = srgb("#b9c6d8")
    bg.inputs["Strength"].default_value = 1.35
    out = nt.nodes.new("ShaderNodeOutputWorld")
    nt.links.new(bg.outputs["Background"], out.inputs["Surface"])
    s.world = w
    sun = bpy.data.lights.new("sun", "SUN")
    sun.energy = 4.2
    sun.color = (1.0, 0.94, 0.84)
    sun.angle = math.radians(2.0)
    so = bpy.data.objects.new("sun", sun)
    so.rotation_euler = SUN_DIR.to_track_quat("-Z", "Y").to_euler()
    s.collection.objects.link(so)
    fill = bpy.data.lights.new("fill", "SUN")
    fill.energy = 0.9
    fill.color = (0.85, 0.9, 1.0)
    fill.use_shadow = False
    fo = bpy.data.objects.new("fill", fill)
    fo.rotation_euler = (-VIEW).to_track_quat("-Z", "Y").to_euler()
    s.collection.objects.link(fo)
    return s


def frame_and_render(s, n, path, fixed_h=None):
    """Place an ortho camera so the NxN lot's bottom corner is the bottom-centre pixel."""
    bottom = Vector((n / 2, -n / 2, 0)).dot(UP)
    top = bottom
    dg = bpy.context.evaluated_depsgraph_get()
    for ob in s.objects:
        if ob.type != "MESH":
            continue
        ev = ob.evaluated_get(dg)
        for v in ev.data.vertices:
            top = max(top, (ob.matrix_world @ v.co).dot(UP))
    w = n * TILE_PX
    h = fixed_h or int(math.ceil((top - bottom) * PPU)) + 6
    s.render.resolution_x = w
    s.render.resolution_y = h
    s.render.resolution_percentage = 100
    cam = bpy.data.cameras.new("cam")
    cam.type = "ORTHO"
    cam.ortho_scale = max(w, h) / PPU
    cam.clip_start = 0.01
    cam.clip_end = 200
    co = bpy.data.objects.new("cam", cam)
    co.rotation_euler = (math.radians(60), 0, math.radians(45))
    centre = bottom + (h / 2) / PPU
    co.location = UP * centre + VIEW * 50
    s.collection.objects.link(co)
    s.camera = co
    s.render.filepath = path
    bpy.ops.render.render(write_still=True)


def render_building(bid, samples):
    n, fn = BUILDINGS[bid]
    s = reset_scene(samples)
    fn(palette())
    t = time.time()
    frame_and_render(s, n, os.path.join(OUT, bid + ".png"))
    return time.time() - t


def render_tiles(samples):
    """4x4 variants per terrain kind; variant (i, j) continues the noise field of
    its neighbours, so the compositor picks (x % 4, y % 4) for seamless ground."""
    import shutil
    times = []
    for kind in ("grass", "road"):
        for i in range(4):
            for j in range(4):
                s = reset_scene(samples)
                M = palette()
                lot(1, M[kind])
                _offset_uvs(s.objects["lot"], i, -j)
                t = time.time()
                frame_and_render(s, 1, os.path.join(OUT, "tile_%s_%d_%d.png" % (kind, i, j)),
                                 fixed_h=TILE_PX // 2)
                times.append(time.time() - t)
        shutil.copy(os.path.join(OUT, "tile_%s_0_0.png" % kind),
                    os.path.join(OUT, "tile_%s.png" % kind))
    return times


def _offset_uvs(ob, dx, dy):
    """Shift a flat lot's UVs to the world position (dx, dy) so tiles stitch."""
    uvl = ob.data.uv_layers.active
    for lp in ob.data.loops:
        co = ob.data.vertices[lp.vertex_index].co
        uvl.data[lp.index].uv = (co.x + dx, co.y + dy)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    samples = 96
    if "--samples" in argv:
        k = argv.index("--samples")
        samples = int(argv[k + 1])
        del argv[k:k + 2]
    os.makedirs(OUT, exist_ok=True)
    ids = argv or list(BUILDINGS) + ["tiles"]
    report = []
    for bid in ids:
        if bid == "tiles":
            ts = render_tiles(max(16, samples // 3))
            report.append("tiles: %d renders, %.1fs total" % (len(ts), sum(ts)))
        else:
            report.append("%s: %.1fs" % (bid, render_building(bid, samples)))
    print("RENDER REPORT\n" + "\n".join(report))


main()
