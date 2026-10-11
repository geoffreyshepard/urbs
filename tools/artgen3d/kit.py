"""URBS 3D art kit: procedural Blender parts, materials and the isometric sprite camera.

Every iso sprite in the game is rendered through this module (see docs/13-isometric.md).
Family modules (housing.py, works.py, ...) import it and declare SPRITES; run.py renders them:

    /Applications/Blender.app/Contents/MacOS/Blender -b --python tools/artgen3d/run.py -- \
        <family> [names...] [--samples N]

Conventions: 1 tile = 1 Blender unit, z up.  Grid x (east) = Blender +x, grid y (south) =
Blender -y.  A footprint fw x fh is centred on the origin: x in [-fw/2, fw/2], y in
[-fh/2, fh/2].  Fronts face grid south (Blender -y, the front-left wall on screen); grid east
(+x) is the front-right wall.  Camera: orthographic, rotation (60, 0, 45) degrees, so a tile
diamond is 128 x 64 px at native scale.  Sun from the screen's upper left.
"""
import json
import math
import os
import sys
import time

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "assets", "sprites_iso")
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



def lot_rect(fw, fh, mat, curb=None, uv_off=(0, 0), name="lot"):
    """Flat ground of an fw x fh footprint centred on the origin (optionally with a curb)."""
    hx, hy = fw / 2, fh / 2
    ob = mesh(name, [(-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0)], [(0, 1, 2, 3)], mat,
              uv_off=uv_off)
    if curb:
        t = 0.05
        box(-hx, -hy, 0, hx, -hy + t, 0.03, curb, bevel=0.005)
        box(-hx, hy - t, 0, hx, hy, 0.03, curb, bevel=0.005)
        box(-hx, -hy, 0, -hx + t, hy, 0.03, curb, bevel=0.005)
        box(hx - t, -hy, 0, hx, hy, 0.03, curb, bevel=0.005)
    return ob


def offset_uvs(ob, dx, dy):
    """Shift a flat mesh's UVs to world position (dx, dy) so neighbouring tiles stitch."""
    uvl = ob.data.uv_layers.active
    for lp in ob.data.loops:
        co = ob.data.vertices[lp.vertex_index].co
        uvl.data[lp.index].uv = (co.x + dx, co.y + dy)


# ---------------------------------------------------------------- rendering

def _screen_extent(s):
    """(min_right, max_right, min_up, max_up) of every mesh vertex, in world units."""
    dg = bpy.context.evaluated_depsgraph_get()
    r0 = u0 = 1e9
    r1 = u1 = -1e9
    for ob in s.objects:
        if ob.type != "MESH" or ob.hide_render:
            continue
        ev = ob.evaluated_get(dg)
        for v in ev.data.vertices:
            p = ob.matrix_world @ v.co
            r, u = p.dot(RIGHT), p.dot(UP)
            r0, r1, u0, u1 = min(r0, r), max(r1, r), min(u0, u), max(u1, u)
    return r0, r1, u0, u1


def _camera(s, w, h, centre_r, centre_u):
    s.render.resolution_x = w
    s.render.resolution_y = h
    s.render.resolution_percentage = 100
    cam = bpy.data.cameras.new("cam")
    cam.type = "ORTHO"
    cam.ortho_scale = max(w, h) / PPU
    cam.clip_start = 0.01
    cam.clip_end = 400
    co = bpy.data.objects.new("cam", cam)
    co.rotation_euler = (math.radians(60), 0, math.radians(45))
    co.location = UP * centre_u + RIGHT * centre_r + VIEW * 100
    s.collection.objects.link(co)
    s.camera = co


def frame_footprint(s, fw, fh, fixed_h=None, margin=4):
    """Camera for a ground-attached sprite of footprint fw x fh.

    Width is exactly (fw + fh) * 64 px; the footprint's bottom (south-east) corner lands on
    the bottom edge.  Returns the anchor (ax, ay) = that corner in continuous sprite pixels.
    """
    bottom = Vector((fw / 2, -fh / 2, 0)).dot(UP)
    _, _, _, top = _screen_extent(s)
    top = max(top, bottom)
    w = (fw + fh) * TILE_PX // 2
    h = fixed_h or int(math.ceil((top - bottom) * PPU)) + margin
    _camera(s, w, h, 0.0, bottom + (h / 2) / PPU)
    return fw * TILE_PX // 2, h


def frame_free(s, frame=None, margin=2):
    """Camera for a free-standing object (citizen, tree, cart) whose foot is the origin.

    frame=(w, h, ax, ay) fixes the canvas (use it for animation frames); otherwise the canvas
    hugs the object.  Returns the anchor (ax, ay): the origin in continuous sprite pixels.
    """
    if frame:
        w, h, ax, ay = frame
    else:
        r0, r1, u0, u1 = _screen_extent(s)
        r0, u0 = min(r0, 0.0), min(u0, 0.0)
        r1, u1 = max(r1, 0.0), max(u1, 0.0)
        w = int(math.ceil((r1 - r0) * PPU)) + 2 * margin
        h = int(math.ceil((u1 - u0) * PPU)) + 2 * margin
        ax = int(round(-r0 * PPU)) + margin
        ay = int(round(u1 * PPU)) + margin
    # origin must land on pixel (ax, ay): centre_r/u are the world coords of the canvas centre
    centre_r = (w / 2 - ax) / PPU
    centre_u = (ay - h / 2) / PPU
    _camera(s, w, h, centre_r, centre_u)
    return ax, ay


def render_to(s, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    s.render.filepath = path
    bpy.ops.render.render(write_still=True)


LIT_MATERIALS = ("window",)


def render_lit(s, path, glow="#ffb35a", strength=6.0):
    """Night-light overlay: windows glow, everything else is held out (transparent)."""
    hold = bpy.data.materials.new("holdout")
    nt = _nodes(hold)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    h = nt.nodes.new("ShaderNodeHoldout")
    nt.links.new(h.outputs[0], out.inputs["Surface"])
    em = bpy.data.materials.new("lit_glow")
    nt = _nodes(em)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    e = nt.nodes.new("ShaderNodeEmission")
    e.inputs["Color"].default_value = srgb(glow)
    e.inputs["Strength"].default_value = strength
    nt.links.new(e.outputs[0], out.inputs["Surface"])
    for ob in s.objects:
        if ob.type != "MESH":
            continue
        for slot in ob.material_slots:
            nm = slot.material.name.split(".")[0] if slot.material else ""
            slot.material = em if nm in LIT_MATERIALS else hold
    render_to(s, path)


class Meta:
    """Anchors for one family, written to assets/sprites_iso/meta/<family>.json."""

    def __init__(self, family):
        self.path = os.path.join(OUT, "meta", family + ".json")
        try:
            with open(self.path) as f:
                self.data = json.load(f)
        except (OSError, ValueError):
            self.data = {}

    def set(self, name, ax, ay):
        self.data[name] = {"ax": int(ax), "ay": int(ay)}

    def save(self):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        with open(self.path, "w") as f:
            json.dump(self.data, f, indent=1, sort_keys=True)
            f.write("\n")
