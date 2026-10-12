"""Housing, religion and decor sprites (family "homes").

Housing evolves on its plot: an empty staked lot, a tent (t0), a hut (t1), a small tiled house
(t2), a two-storey house (t3) and a tall town house with a roof garden (t4).  The domus plot grows
from an atrium house (t0) to one with a peristyle garden (t1) and a villa with a pool (t2).
Houses stay quiet (muted plaster, faded terracotta); richness shows as height, red base bands,
courtyards and gardens.  Temples and shrines are white marble with gold.

Regions (@east, @egypt, @north) keep footprint and layout but swap materials and roofs (see
docs/13-isometric.md).
"""
import math

import kit
from kit import blob, box, column, cylinder, cypress, fence, gable, hip, lot, lot_rect, material, \
    mesh, olive, pot_plant, ring_roof, steps, windows_x, windows_y

REGIONS = ("rome", "east", "egypt", "north")


# ---------------------------------------------------------------- materials / styles

def extra(M):
    """Family materials (added to the kit palette)."""
    M["whitewash"] = material("whitewash", "#f2eee4", "#e2dccd", scale=9, bump=0.08,
                              detail=(1.5, "#dcd4c2"))
    M["blue"] = material("blue", "#3f6f9c", "#335d86", scale=12, bump=0.05)
    M["mudbrick"] = material("mudbrick", "#b88f62", "#a47b51", kind="brick", scale=12,
                             brick=(0.16, 0.07), mortar="#8f704e", bump=0.25)
    M["sandstone"] = material("sandstone", "#dcc28e", "#c9ab74", kind="brick", scale=10,
                              brick=(0.3, 0.12), mortar="#a68d5f", bump=0.25)
    M["plaster_egypt"] = material("plaster_egypt", "#dcc49c", "#cbb088", scale=9, bump=0.1,
                                  detail=(1.5, "#c4a77e"))
    M["ochre_paint"] = material("ochre_paint", "#c98b3a", "#b47a30", scale=12, bump=0.03)
    M["thatch"] = material("thatch", "#b39a5e", "#94794a", kind="stripes", stripe=0.04,
                           bump=0.5, rough=0.95)
    M["shingle"] = material("shingle", "#5e4c3e", "#4b3c31", kind="tiles", scale=7,
                            brick=(0.07, 0.06), bump=0.5)
    M["daub"] = material("daub", "#cbb48c", "#b79e75", scale=10, bump=0.2,
                         detail=(1.5, "#a88e68"))
    M["canvas"] = material("canvas", "#ddd0b0", "#c9b994", scale=8, bump=0.1)
    M["canvas_stripe"] = material("canvas_stripe", "#c7553d", "#e9dcc0", kind="stripes",
                                  stripe=0.08, bump=0.0)
    M["hide"] = material("hide", "#8a6a4a", "#76583b", scale=10, bump=0.15)
    M["reed"] = material("reed", "#c4ad6e", "#a9914f", kind="stripes", stripe=0.03, bump=0.4)
    M["sand"] = material("sand", "#d6bd8c", "#c6a978", scale=8, bump=0.2, detail=(1.2, "#e2cfa4"))
    M["dust"] = material("dust", "#c8b28a", "#b59e76", scale=9, bump=0.25, detail=(1.2, "#d6c4a0"))
    M["grass_north"] = material("grass_north", "#5b7a3a", "#4b6830", scale=11, bump=0.25,
                                detail=(0.9, "#7f8f58"))
    M["garden_east"] = material("garden_east", "#8a9a52", "#77884a", scale=12, bump=0.25,
                                detail=(1.2, "#b4ad78"))
    M["palm"] = material("palm", "#5f8a35", "#4b7428", scale=18, bump=0.3)
    M["palm_trunk"] = material("palm_trunk", "#7b6243", "#634e35", kind="stripes", stripe=0.05,
                               bump=0.4)
    M["pine"] = material("pine", "#2f4a2f", "#263d26", scale=18, bump=0.3)
    M["flower_r"] = material("flower_r", "#c0473a", "#d76a4a", scale=30, bump=0.2)
    M["flower_y"] = material("flower_y", "#e0b84a", "#f0d070", scale=30, bump=0.2)
    M["flower_p"] = material("flower_p", "#9a5a9a", "#b880b0", scale=30, bump=0.2)
    M["hedge"] = material("hedge", "#4f7330", "#3f6126", scale=18, bump=0.35)
    M["bronze"] = material("bronze", "#8a6a3a", "#6e5430", scale=20, rough=0.35, metal=0.85,
                           bump=0.0)
    M["patina"] = material("patina", "#6f9c8a", "#5a8576", scale=20, rough=0.5, metal=0.4,
                           bump=0.0)
    M["painted_red"] = material("painted_red", "#a8432c", "#923a26", scale=12, bump=0.03)
    M["mosaic"] = material("mosaic", "#d9c7a6", "#b8724a", kind="brick", scale=30,
                           brick=(0.05, 0.05), mortar="#efe6d2", bump=0.05)
    M["limestone"] = material("limestone", "#e4dccb", "#d2c8b4", kind="brick", scale=12,
                              brick=(0.28, 0.1), mortar="#b6ab96", bump=0.2)
    return M


def style(M, region):
    """Materials for a region.  Keys: wall, wall2 (humbler), roof, roof_kind (hip|gable|flat|
    thatch), band, ground, yard, trim, door, tree."""
    if region == "east":
        return dict(wall=M["whitewash"], wall2=M["whitewash"], roof=M["roof_faded"],
                    roof_kind="low", band=M["blue"], ground=M["garden_east"], yard=M["dust"],
                    trim=M["blue"], door=M["blue"], tree="olive", hut_roof="flat")
    if region == "egypt":
        return dict(wall=M["plaster_egypt"], wall2=M["mudbrick"], roof=None, roof_kind="flat",
                    band=M["ochre_paint"], ground=M["sand"], yard=M["sand"], trim=M["ochre_paint"],
                    door=M["timber"], tree="palm", hut_roof="flat")
    if region == "north":
        return dict(wall=M["daub"], wall2=M["daub"], roof=M["shingle"], roof_kind="gable",
                    band=M["timber"], ground=M["grass_north"], yard=M["dirt"], trim=M["timber"],
                    door=M["timber"], tree="pine", hut_roof="thatch", frame=True)
    return dict(wall=M["plaster_house"], wall2=M["plaster_humble"], roof=M["roof_faded"],
                roof_kind="hip", band=M["band_red"], ground=M["garden"], yard=M["dirt"],
                trim=M["band_red"], door=M["timber"], tree="cypress", hut_roof="thatch")


_STYLE0 = style  # domus_var swaps the module's style() for a variant one while it builds


# ---------------------------------------------------------------- parts

def palm(x, y, h, M):
    n = 6
    for i in range(n):  # slightly curved trunk of stacked rings
        t = i / n
        cylinder(x + 0.04 * t * t, y, h * t, h * (t + 1 / n) + 0.01, 0.028 - 0.008 * t,
                 M["palm_trunk"], n=8)
    tx = x + 0.04
    for k in range(7):
        a = 2 * math.pi * k / 7 + 0.3
        ex, ey = tx + math.cos(a) * 0.26, y + math.sin(a) * 0.26
        mesh("frond", [(tx, y, h + 0.02), (ex + math.sin(a) * 0.05, ey - math.cos(a) * 0.05, h - 0.07),
                       (ex, ey, h - 0.12), (ex - math.sin(a) * 0.05, ey + math.cos(a) * 0.05, h - 0.07)],
             [(0, 1, 2, 3)], M["palm"])
    blob(tx, y, h, 0.05, 0.05, 0.04, M["palm"], segs=8, rings=4)


def _cone(x, y, z0, z1, r, n=10):
    return [(x + math.cos(2 * math.pi * i / n) * r, y + math.sin(2 * math.pi * i / n) * r, z0)
            for i in range(n)] + [(x, y, z1)]


def cone(x, y, z0, z1, r, mat, n=12, name="cone"):
    v = _cone(x, y, z0, z1, r, n)
    f = [(i, (i + 1) % n, n) for i in range(n)] + [tuple(reversed(range(n)))]
    return mesh(name, v, f, mat)


def pine_tree(x, y, h, M):
    cylinder(x, y, 0, h * 0.3, 0.03, M["trunk"], n=6)
    for z, r in [(0.15, 0.24), (0.42, 0.18), (0.66, 0.11)]:
        cone(x, y, h * z, h * (z + 0.42), r * h * 1.1, M["pine"], n=10)


def tree(kind, x, y, M, s=1.0):
    if kind == "palm":
        palm(x, y, 0.75 * s, M)
    elif kind == "pine":
        pine_tree(x, y, 0.8 * s, M)
    elif kind == "olive":
        olive(x, y, M, 0.85 * s)
    else:
        cypress(x, y, 0.5 * s, M)


def bush(x, y, r, mat):
    blob(x, y, r * 0.7, r, r, r * 0.8, mat, segs=8, rings=5, name="bush")


def door(x, face_y, M, w=0.16, h=0.3, mat=None, frame=None):
    if frame is not None:
        box(x - w / 2 - 0.03, face_y - 0.03, 0, x + w / 2 + 0.03, face_y + 0.01, h + 0.04, frame,
            bevel=0.004)
    box(x - w / 2, face_y - 0.04, 0, x + w / 2, face_y + 0.01, h, mat or M["timber"], bevel=0)


def door_x(face_x, y, M, w=0.16, h=0.3, mat=None):
    box(face_x - 0.01, y - w / 2, 0, face_x + 0.04, y + w / 2, h, mat or M["timber"], bevel=0)


def parapet(x0, y0, x1, y1, z, mat, h=0.06, t=0.04):
    box(x0, y0, z, x1, y0 + t, z + h, mat, bevel=0.004)
    box(x0, y1 - t, z, x1, y1, z + h, mat, bevel=0.004)
    box(x0, y0, z, x0 + t, y1, z + h, mat, bevel=0.004)
    box(x1 - t, y0, z, x1, y1, z + h, mat, bevel=0.004)


def roof(st, x0, y0, x1, y1, z, h, M, axis="x"):
    """Roof in the region's style over a wall box."""
    k = st["roof_kind"]
    if k == "flat":
        box(x0 - 0.02, y0 - 0.02, z, x1 + 0.02, y1 + 0.02, z + 0.03, st["wall"], bevel=0.004)
        parapet(x0 - 0.02, y0 - 0.02, x1 + 0.02, y1 + 0.02, z + 0.03, st["wall"])
    elif k == "low":
        hip(x0, y0, x1, y1, z, h * 0.45, st["roof"], over=0.04)
    elif k == "gable":
        gable(x0, y0, x1, y1, z, h * 1.25, st["roof"], axis=axis, wall=st["wall"])
    elif k == "thatch":
        hip(x0, y0, x1, y1, z, h * 1.3, M["thatch"], over=0.1)
    else:
        hip(x0, y0, x1, y1, z, h, st["roof"])


def frame_beams(x0, y0, x1, y1, z0, z1, M):
    """Timber framing on the two visible walls (north style)."""
    t = M["timber"]
    for x in (x0, (x0 + x1) / 2, x1):
        box(x - 0.018, y0 - 0.016, z0, x + 0.018, y0 + 0.002, z1, t, bevel=0)
    for y in (y0, (y0 + y1) / 2, y1):
        box(x1 - 0.002, y - 0.018, z0, x1 + 0.016, y + 0.018, z1, t, bevel=0)
    for z in (z0 + 0.01, z1 - 0.02):
        box(x0, y0 - 0.016, z, x1, y0 + 0.002, z + 0.025, t, bevel=0)
        box(x1 - 0.002, y0, z, x1 + 0.016, y1, z + 0.025, t, bevel=0)
    # one diagonal brace per wall
    mesh("brace", [(x0 + 0.03, y0 - 0.017, z0 + 0.03), (x0 + 0.07, y0 - 0.017, z0 + 0.03),
                   ((x0 + x1) / 2 - 0.01, y0 - 0.017, z1 - 0.03), ((x0 + x1) / 2 - 0.05, y0 - 0.017, z1 - 0.03)],
         [(0, 1, 2, 3)], t)


def stakes(n, M):
    """Corner stakes and a cord marking an empty plot."""
    h = n / 2 - 0.12
    pts = [(-h, -h), (h, -h), (h, h), (-h, h)]
    for x, y in pts:
        box(x - 0.025, y - 0.025, 0, x + 0.025, y + 0.025, 0.14, M["timber_light"], bevel=0.004)
    for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
        xs, ys = sorted((ax, bx)), sorted((ay, by))
        box(xs[0] - 0.005, ys[0] - 0.005, 0.1, xs[1] + 0.005, ys[1] + 0.005, 0.11, M["canvas"],
            bevel=0)


def stone_ring(x, y, M, r=0.09):
    for k in range(7):
        a = 2 * math.pi * k / 7
        blob(x + math.cos(a) * r, y + math.sin(a) * r, 0.015, 0.03, 0.03, 0.025, M["stone_dark"],
             segs=6, rings=4)
    blob(x, y, 0.02, 0.04, 0.04, 0.025, M["charcoal"], segs=6, rings=4)


def veg_patch(x0, y0, x1, y1, M, crop="crop"):
    box(x0, y0, 0, x1, y1, 0.025, M["soil"], bevel=0.008)
    nx = max(2, int((x1 - x0) / 0.13))
    ny = max(2, int((y1 - y0) / 0.14))
    for i in range(ny):
        for j in range(nx):
            blob(x0 + (j + 0.5) * (x1 - x0) / nx, y0 + (i + 0.5) * (y1 - y0) / ny, 0.05, 0.045,
                 0.04, 0.035, M[crop], segs=6, rings=4)


def ground(n, st, kind="ground"):
    return lot(n, st[kind])


# ---------------------------------------------------------------- housing plot (2x2)

def plot_empty(M):
    extra(M)
    lot(2, M["dirt"])
    stakes(2, M)
    box(-0.12, -0.12, 0, 0.12, 0.12, 0.05, M["travertine"], bevel=0.01)  # boundary stone


def _plot_tent_v0(M, region="rome"):
    st = style(extra(M), region)
    lot(2, st["yard"])
    mesh("grass_patch", [(0.1, 0.1, 0.003), (1, 0.1, 0.003), (1, 1, 0.003), (0.1, 1, 0.003)],
         [(0, 1, 2, 3)], st["ground"])
    if region == "north":  # conical hide tent
        cone(-0.15, 0.15, 0, 0.62, 0.48, M["hide"], n=14)
        for k in range(5):
            a = 2 * math.pi * k / 5
            box(-0.15 + math.cos(a) * 0.05 - 0.01, 0.15 + math.sin(a) * 0.05 - 0.01, 0.55,
                -0.15 + math.cos(a) * 0.05 + 0.01, 0.15 + math.sin(a) * 0.05 + 0.01, 0.72,
                M["timber"], bevel=0)
        box(-0.24, -0.36, 0, -0.06, -0.3, 0.2, M["dark"], bevel=0)
    elif region == "egypt":  # reed and palm-frond shelter
        for x in (-0.6, 0.3):
            for y in (-0.45, 0.45):
                box(x - 0.02, y - 0.02, 0, x + 0.02, y + 0.02, 0.38, M["timber_light"], bevel=0)
        box(-0.66, -0.5, 0.36, 0.36, 0.5, 0.4, M["reed"], bevel=0.005)
        box(-0.62, 0.38, 0, 0.32, 0.46, 0.36, M["reed"], bevel=0)
        palm(0.7, 0.65, 0.65, M)
    else:  # A-frame tent
        mat = M["canvas_stripe"] if region == "east" else M["canvas"]
        x0, x1, y0, y1, h = -0.55, 0.35, -0.4, 0.5, 0.48
        mesh("tent", [(x0, y0, 0), (x1, y0, 0), (x1, (y0 + y1) / 2, h), (x0, (y0 + y1) / 2, h),
                      (x0, y1, 0), (x1, y1, 0)], [(0, 1, 2, 3), (3, 2, 5, 4)], mat, solidify=0.02)
        mesh("tent_back", [(x1, y0, 0), (x1, y1, 0), (x1, (y0 + y1) / 2, h)], [(0, 1, 2)], mat)
        mesh("tent_front", [(x0, y0, 0), (x0, (y0 + y1) / 2, h), (x0, y1, 0)], [(0, 1, 2)], M["dark"])
        box(x0 - 0.02, 0.03, 0, x0 + 0.02, 0.07, h + 0.06, M["timber"], bevel=0)
        box(x1 - 0.02, 0.03, 0, x1 + 0.02, 0.07, h + 0.06, M["timber"], bevel=0)
    stone_ring(0.45, -0.5, M)
    amphora(-0.75, -0.7, M, 0.8)
    kit.sack(-0.55, -0.75, 0, M, 0.9)
    box(0.55, 0.05, 0, 0.85, 0.2, 0.06, M["timber_light"], bevel=0.005)  # firewood
    box(0.58, 0.07, 0.06, 0.82, 0.18, 0.1, M["timber"], bevel=0.005)


def amphora(x, y, M, s=1.0):
    kit.amphora(x, y, M, s)


def _plot_hut_v0(M, region="rome"):
    st = style(extra(M), region)
    lot(2, st["ground"])
    mesh("yard", [(-1, -1, 0.003), (0.2, -1, 0.003), (0.2, -0.1, 0.003), (-1, -0.1, 0.003)],
         [(0, 1, 2, 3)], st["yard"])
    if region == "north":  # round hut, conical thatch
        cylinder(-0.25, 0.25, 0, 0.3, 0.42, M["daub"], n=16)
        cone(-0.25, 0.25, 0.28, 0.8, 0.56, M["thatch"], n=16)
        box(-0.33, -0.2, 0, -0.17, -0.12, 0.24, M["dark"], bevel=0)
    else:
        wall = M["daub"] if region == "rome" else (M["whitewash"] if region == "east" else M["mudbrick"])
        x0, y0, x1, y1, zw = -0.75, -0.25, 0.35, 0.75, 0.34
        box(x0, y0, 0, x1, y1, zw, wall)
        if st["hut_roof"] == "flat":
            box(x0 - 0.03, y0 - 0.03, zw, x1 + 0.03, y1 + 0.03, zw + 0.04, M["timber_light"], bevel=0.005)
            box(x0, y0, zw + 0.04, x1, y1, zw + 0.07, M["reed"], bevel=0.005)
            if region == "egypt":
                for x in (x0 + 0.05, x1 - 0.05):
                    box(x - 0.015, y0 - 0.25, 0, x + 0.015, y0 - 0.22, zw, M["timber_light"], bevel=0)
                box(x0, y0 - 0.27, zw - 0.02, x1, y0 + 0.02, zw, M["reed"], bevel=0)
        else:
            hip(x0, y0, x1, y1, zw, 0.32, M["thatch"], over=0.1)
        door(-0.45, y0, M, w=0.14, h=0.24, mat=st["door"] if region == "east" else M["dark"])
        windows_y(y0, [0.08], 0.16, 0.24, M, w=0.07)
    veg_patch(0.3, -0.85, 0.88, -0.25, M)
    fence([(0.26, -0.9), (0.92, -0.9), (0.92, -0.2), (0.26, -0.2)], M["timber_light"], h=0.09)
    stone_ring(-0.25, -0.65, M, 0.07)
    tree(st["tree"], 0.65, 0.6, M, 0.85)
    amphora(-0.85, -0.2, M, 0.8)


def _plot_small_house_v0(M, region="rome"):
    """t2: one storey, tiled roof, small vegetable patch (the art-test house_small)."""
    st = style(extra(M), region)
    lot(2, st["ground"])
    mesh("path", [(-0.45, -1, 0.004), (-0.25, -1, 0.004), (-0.25, -0.2, 0.004), (-0.45, -0.2, 0.004)],
         [(0, 1, 2, 3)], st["yard"])
    x0, x1, y0, y1, zw = -0.85, 0.45, -0.2, 0.8, 0.42
    wall = st["wall2"] if region != "rome" else M["plaster_humble"]
    box(x0, y0, 0, x1, y1, zw, wall)
    if st.get("frame"):
        frame_beams(x0, y0, x1, y1, 0, zw, M)
    if region == "rome":
        hip(x0, y0, x1, y1, zw, 0.24, M["roof_faded"])
    else:
        roof(st, x0, y0, x1, y1, zw, 0.24, M, axis="y")
    door(-0.35, y0, M, w=0.15, h=0.27, mat=st["door"], frame=st["trim"] if region == "east" else None)
    windows_y(y0, [0.12], 0.18, 0.29, M)
    windows_x(x1, [0.3], 0.18, 0.29, M)
    veg_patch(0.08, -0.92, 0.9, -0.32, M)
    fence([(0.05, -0.95), (0.93, -0.95), (0.93, -0.29), (0.05, -0.29), (0.05, -0.6)],
          M["timber_light"], h=0.1)
    amphora(-0.65, -0.33, M, 0.9)
    tree(st["tree"], 0.75, 0.6, M, 0.85)


def _plot_two_storey_v0(M, region="rome"):
    """t3: two storeys, wooden balcony, red base band."""
    st = style(extra(M), region)
    lot(2, st["ground"])
    mesh("path", [(-0.2, -1, 0.004), (0.1, -1, 0.004), (0.1, -0.55, 0.004), (-0.2, -0.55, 0.004)],
         [(0, 1, 2, 3)], M["paving"] if region != "egypt" else M["sandstone"])
    x0, x1, y0, y1, zw = -0.85, 0.6, -0.55, 0.85, 0.74
    box(x0, y0, 0.1, x1, y1, zw, st["wall"])
    box(x0 - 0.015, y0 - 0.015, 0, x1 + 0.015, y1 + 0.015, 0.12, st["band"])
    if st.get("frame"):
        frame_beams(x0, y0, x1, y1, 0.12, zw, M)
    roof(st, x0, y0, x1, y1, zw, 0.26, M, axis="x")
    door(-0.05, y0, M, w=0.16, h=0.32, mat=st["door"], frame=st["band"])
    windows_y(y0, [-0.6, -0.35, 0.25, 0.45], 0.5, 0.62, M, w=0.07)
    windows_y(y0, [-0.55, 0.35], 0.2, 0.3, M, w=0.07)
    windows_x(x1, [-0.3, 0.0, 0.3, 0.6], 0.5, 0.62, M, w=0.07)
    windows_x(x1, [-0.2, 0.4], 0.2, 0.3, M, w=0.07)
    # balcony on the upper floor (front-left wall)
    box(-0.7, y0 - 0.16, 0.42, 0.05, y0, 0.45, M["timber_light"], bevel=0.004)
    for x in (-0.68, -0.5, -0.32, -0.14, 0.03):
        box(x - 0.01, y0 - 0.155, 0.45, x + 0.01, y0 - 0.135, 0.54, M["timber"], bevel=0)
    box(-0.7, y0 - 0.16, 0.53, 0.05, y0 - 0.13, 0.55, M["timber"], bevel=0)
    for x in (-0.66, 0.0):
        box(x - 0.012, y0 - 0.15, 0, x + 0.012, y0 - 0.12, 0.42, M["timber"], bevel=0)
    pot_plant(0.4, -0.8, M)
    pot_plant(0.7, -0.8, M)
    tree(st["tree"], 0.82, 0.4, M, 0.8)


def _plot_town_house_v0(M, region="rome"):
    """t4: three storeys with a roof garden and a small walled court."""
    st = style(extra(M), region)
    lot(2, st["ground"])
    mesh("court", [(0.15, -1, 0.004), (1, -1, 0.004), (1, -0.15, 0.004), (0.15, -0.15, 0.004)],
         [(0, 1, 2, 3)], M["mosaic"] if region != "north" else M["paving"])
    x0, x1, y0, y1, zw = -0.9, 0.9, -0.1, 0.9, 1.08
    box(x0, y0, 0.12, x1, y1, zw, st["wall"])
    box(x0 - 0.015, y0 - 0.015, 0, x1 + 0.015, y1 + 0.015, 0.14, st["band"])
    box(x0 - 0.012, y0 - 0.012, 0.44, x1 + 0.012, y1 + 0.012, 0.47, st["band"], bevel=0)
    if st.get("frame"):
        frame_beams(x0, y0, x1, y1, 0.47, zw, M)
    # front wing, two storeys, toward the street
    fx0, fx1, fy0 = -0.9, 0.1, -0.85
    box(fx0, fy0, 0.12, fx1, y0 + 0.01, 0.74, st["wall"])
    box(fx0 - 0.015, fy0 - 0.015, 0, fx1 + 0.015, y0 + 0.01, 0.14, st["band"])
    roof(st, fx0, fy0, fx1, y0, 0.74, 0.22, M, axis="x")
    door(-0.4, fy0, M, w=0.18, h=0.34, mat=st["door"], frame=st["band"])
    windows_y(fy0, [-0.72, -0.08], 0.22, 0.34, M, w=0.07)
    windows_y(fy0, [-0.72, -0.55, -0.25, -0.08], 0.52, 0.64, M, w=0.07)
    windows_y(y0, [0.3, 0.55, 0.78], 0.22, 0.34, M, w=0.07)
    windows_y(y0, [-0.7, -0.45, -0.2, 0.3, 0.55, 0.78], 0.85, 0.97, M, w=0.07)
    windows_y(y0, [0.3, 0.55, 0.78], 0.55, 0.67, M, w=0.07)
    windows_x(x1, [0.05, 0.3, 0.55, 0.8], 0.55, 0.67, M, w=0.07)
    windows_x(x1, [0.05, 0.3, 0.55, 0.8], 0.85, 0.97, M, w=0.07)
    windows_x(x1, [0.3, 0.7], 0.22, 0.34, M, w=0.07)
    # roof terrace garden with pergola
    if st["roof_kind"] == "flat":
        box(x0 - 0.02, y0 - 0.02, zw, x1 + 0.02, y1 + 0.02, zw + 0.03, st["wall"], bevel=0.004)
        parapet(x0 - 0.02, y0 - 0.02, x1 + 0.02, y1 + 0.02, zw + 0.03, st["wall"])
        box(-0.2, 0.2, zw + 0.03, 0.6, 0.7, zw + 0.06, M["reed"], bevel=0)  # awning bed
        for x, y in [(-0.2, 0.2), (0.6, 0.2), (-0.2, 0.7), (0.6, 0.7)]:
            box(x - 0.015, y - 0.015, zw + 0.03, x + 0.015, y + 0.015, zw + 0.28, M["timber_light"], bevel=0)
        box(-0.23, 0.17, zw + 0.27, 0.63, 0.73, zw + 0.3, M["reed"], bevel=0)
        for x, y in [(-0.6, 0.1), (-0.6, 0.6)]:
            blob(x, y, zw + 0.1, 0.09, 0.09, 0.07, M["leaf"], segs=8, rings=5)
    else:
        rx1 = 0.2
        roof(st, x0, y0, rx1, y1, zw, 0.28, M, axis="y") if st["roof_kind"] == "gable" else \
            hip(x0, y0, rx1, y1, zw, 0.28, st["roof"])
        # roof garden on the right half
        box(rx1, y0, zw, x1 + 0.02, y1 + 0.02, zw + 0.03, M["paving"], bevel=0.004)
        parapet(rx1, y0 - 0.02, x1 + 0.02, y1 + 0.02, zw + 0.03, st["wall"])
        for x, y in [(0.4, 0.15), (0.72, 0.15), (0.72, 0.65)]:
            box(x - 0.06, y - 0.06, zw + 0.03, x + 0.06, y + 0.06, zw + 0.08, M["amphora"], bevel=0.005)
            blob(x, y, zw + 0.13, 0.08, 0.08, 0.07, M["leaf"], segs=8, rings=5)
        for x, y in [(0.32, 0.6), (0.82, 0.6), (0.32, 0.3), (0.82, 0.3)]:
            box(x - 0.012, y - 0.012, zw + 0.03, x + 0.012, y + 0.012, zw + 0.27, M["timber_light"], bevel=0)
        for y in (0.3, 0.6):
            box(0.3, y - 0.012, zw + 0.25, 0.84, y + 0.012, zw + 0.27, M["timber_light"], bevel=0)
        box(0.3, 0.29, zw + 0.27, 0.84, 0.61, zw + 0.29, M["leaf"], bevel=0)
    # court wall and a tree in the court
    cw = st["wall"]
    box(0.15, -0.98, 0, 0.98, -0.92, 0.2, cw, bevel=0.005)
    box(0.92, -0.98, 0, 0.98, -0.1, 0.2, cw, bevel=0.005)
    tree(st["tree"], 0.62, -0.5, M, 0.9)
    pot_plant(0.3, -0.75, M)


# ---------------------------------------------------------------- house variants

# Every housing tier has VARIANTS looks: v0 is the original design, v1..v3 change the roof form,
# plaster, shutters, annexes and yard (docs/13-isometric.md, "House variants").  Same footprint
# and the same tier cues (storeys, base bands, gardens), so a tier still reads as that tier.
VARIANTS = 4

# wall plaster per variant and region: (base, alt, weathering)
_WALLS = {
    "rome": [None, ("#e3bd84", "#d3aa70", "#cca474"), ("#ebcdbb", "#dcbba8", "#d2ad9a"),
             ("#f2eee6", "#e4ddd0", "#dad2c2")],
    "east": [None, ("#eedcb4", "#e0cba0", "#dccaa6"), ("#f3efe6", "#e4ded0", "#dcd4c4"),
             ("#e6ece8", "#d6dcd8", "#d2d8d2")],
    "egypt": [None, ("#e8dcc0", "#d9cbab", "#d4c4a2"), None, ("#dcb48c", "#cba27a", "#c49c78")],
    "north": [None, ("#e8dfc8", "#d9cfb6", "#d0c4a8"), ("#bba07c", "#a88e6a", "#a08664"), None],
}
_TRIM = {  # shutters / doors
    "rome": ["#5a3f2b", "#4e6a3c", "#8a3b2a", "#3e5f82"],
    "east": ["#3f6f9c", "#3f8a86", "#c98b3a", "#3f6f9c"],
    "egypt": ["#5a3f2b", "#3f6f9c", "#a8432c", "#4e6a3c"],
    "north": ["#5a3f2b", "#4b3a2c", "#6e3a28", "#46503a"],
}
_BANDS = ["#a13b26", "#7a5638", "#a13b26", "#b47a30"]  # Roman base bands per variant


def vstyle(M, region, v, humble=False):
    """style() for variant v: plaster, roof tone, shutters and band change per variant."""
    st = dict(_STYLE0(extra(M), region))
    wall = _WALLS[region][v]
    if humble and region == "rome" and v == 0:
        st["wall"] = M["plaster_humble"]
    if wall:
        a, b, d = wall
        if humble:
            a, b, d = kit.tint(a, -0.05, 0.03), kit.tint(b, -0.05, 0.03), kit.tint(d, -0.05, 0.03)
        key = "wall_%s_%d%s" % (region, v, "_h" if humble else "")
        st["wall"] = material(key, a, b, scale=9, bump=0.08, detail=(1.5, d))
    st["shutter"] = material("trim_%s_%d" % (region, v), _TRIM[region][v], kit.tint(_TRIM[region][v], -0.05),
                             scale=12, bump=0.03)
    if region in ("rome", "east"):
        st["roof"] = M["roof_v%d" % v]
    if region == "rome":
        st["band"] = material("band_v%d" % v, _BANDS[v], kit.tint(_BANDS[v], -0.06), scale=10, bump=0.05)
        st["door"] = st["shutter"] if v else M["timber"]
    elif region == "east":
        st["band"] = st["trim"] = st["door"] = st["shutter"]
    if region == "north":
        if v == 2:
            st["roof"], st["roof_kind"] = M["thatch"], "thatch"
        elif v == 3:
            st["roof"] = material("shingle_dark", "#4a3c32", "#3b2f27", kind="tiles", brick=(0.07, 0.06),
                                  bump=0.5)
    return st


def shed(x0, y0, x1, y1, z, h, mat, down="-y", over=0.06, wall=None):
    """Lean-to (monopitch) roof falling toward `down` (-y front, +y, -x, +x); side triangles in `wall`."""
    X0, Y0, X1, Y1 = x0 - over, y0 - over, x1 + over, y1 + over
    if down in ("-y", "+y"):
        lo, hi = (Y0, Y1) if down == "-y" else (Y1, Y0)
        v = [(X0, lo, z), (X1, lo, z), (X1, hi, z + h), (X0, hi, z + h)]
        top = (v[2], v[3])
    else:
        lo, hi = (X0, X1) if down == "-x" else (X1, X0)
        v = [(lo, Y0, z), (lo, Y1, z), (hi, Y1, z + h), (hi, Y0, z + h)]
        top = (v[2], v[3])
    f = [(0, 1, 2, 3)]
    ob = mesh("shed", v, f, mat, solidify=0.035)
    if any(p.normal.z < 0 for p in ob.data.polygons):
        ob.data.flip_normals()
    kit.roof_caps([top], mat, max(X1 - X0, Y1 - Y0))
    if wall is not None:
        if down in ("-y", "+y"):
            lo, hi = (y0, y1) if down == "-y" else (y1, y0)
            mesh("shed_end", [(x0, lo, z), (x0, hi, z), (x0, hi, z + h * 0.92),
                              (x1, lo, z), (x1, hi, z + h * 0.92), (x1, hi, z)], [(0, 1, 2), (3, 4, 5)], wall)
        else:
            lo, hi = (x0, x1) if down == "-x" else (x1, x0)
            mesh("shed_end", [(lo, y0, z), (hi, y0, z), (hi, y0, z + h * 0.92),
                              (lo, y1, z), (hi, y1, z + h * 0.92), (hi, y1, z)], [(0, 1, 2), (3, 4, 5)], wall)
    return ob


def vroof(st, M, form, x0, y0, x1, y1, z, h, axis="x", down="-y", hole=0.16):
    """A roof of `form` (hip | gable | shed | ring | flat) adapted to the region's roof style."""
    k = st["roof_kind"]
    if k == "flat" or form == "flat":
        box(x0 - 0.02, y0 - 0.02, z, x1 + 0.02, y1 + 0.02, z + 0.03, st["wall"], bevel=0.004)
        parapet(x0 - 0.02, y0 - 0.02, x1 + 0.02, y1 + 0.02, z + 0.03, st["wall"])
        return
    if k == "thatch":
        if form == "gable":
            gable(x0, y0, x1, y1, z, h * 1.4, st["roof"], axis=axis, over=0.09, wall=st["wall"])
        else:
            hip(x0, y0, x1, y1, z, h * 1.3, st["roof"], over=0.1)
        return
    if k == "low":
        h *= 0.5
    if k == "gable" and form in ("hip", "ring"):
        form = "gable"
    if form == "gable":
        gable(x0, y0, x1, y1, z, h * (1.25 if k != "low" else 1.0), st["roof"], axis=axis, wall=st["wall"])
    elif form == "shed":
        shed(x0, y0, x1, y1, z, h * 0.9, st["roof"], down=down, wall=st["wall"])
    elif form == "ring":
        ring_roof(x0, y0, x1, y1, z, h, hole, st["roof"])
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        box(cx - hole, cy - hole, z - 0.02, cx + hole, cy + hole, z + h * 0.45 + 0.01, M["dark"], bevel=0)
    else:
        hip(x0, y0, x1, y1, z, h, st["roof"])


def win_y(face_y, xs, z0, z1, M, st=None, out=-1, w=0.07):
    """Windows on a face of constant y (out = -1 front, +1 back), with shutters when st has them."""
    for x in xs:
        yy = face_y + out * 0.012
        box(x - w / 2, yy - 0.01, z0, x + w / 2, yy + 0.01, z1, M["window"], bevel=0)
        if st is not None and st.get("shutter") is not None:
            for sx in (-1, 1):
                cx = x + sx * (w / 2 + 0.02)
                box(cx - 0.018, yy - 0.012, z0 - 0.005, cx + 0.018, yy + 0.012, z1 + 0.005, st["shutter"],
                    bevel=0)


def win_x(face_x, ys, z0, z1, M, st=None, out=1, w=0.07):
    for y in ys:
        xx = face_x + out * 0.012
        box(xx - 0.01, y - w / 2, z0, xx + 0.01, y + w / 2, z1, M["window"], bevel=0)
        if st is not None and st.get("shutter") is not None:
            for sy in (-1, 1):
                cy = y + sy * (w / 2 + 0.02)
                box(xx - 0.012, cy - 0.018, z0 - 0.005, xx + 0.012, cy + 0.018, z1 + 0.005, st["shutter"],
                    bevel=0)


def all_windows(x0, y0, x1, y1, rows, M, st=None, step=0.3):
    """Windows on all four walls (the backs show in rotations 1 and 2); rows = [(z0, z1), ...]."""
    def spread(a, b):
        n = max(1, int((b - a) / step))
        return [a + (i + 0.5) * (b - a) / n for i in range(n)]
    for z0, z1 in rows:
        win_y(y1, spread(x0 + 0.08, x1 - 0.08), z0, z1, M, st, out=1)
        win_x(x0, spread(y0 + 0.08, y1 - 0.08), z0, z1, M, st, out=-1)


def chimney(x, y, z0, z1, M, mat):
    box(x - 0.045, y - 0.045, z0, x + 0.045, y + 0.045, z1, mat, bevel=0.004)
    box(x - 0.055, y - 0.055, z1, x + 0.055, y + 0.055, z1 + 0.025, M["brick"], bevel=0.003)
    cylinder(x, y, z1 + 0.025, z1 + 0.07, 0.025, M["amphora"], n=8)


def pergola(x0, y0, x1, y1, h, M, vine=True):
    for x, y in [(x0, y0), (x1, y0), (x0, y1), (x1, y1)]:
        box(x - 0.014, y - 0.014, 0, x + 0.014, y + 0.014, h, M["timber_light"], bevel=0)
    for y in (y0, y1):
        box(x0 - 0.03, y - 0.012, h - 0.02, x1 + 0.03, y + 0.012, h + 0.005, M["timber"], bevel=0)
    n = max(2, int((x1 - x0) / 0.12))
    for i in range(n + 1):
        x = x0 + i * (x1 - x0) / n
        box(x - 0.008, y0 - 0.04, h + 0.005, x + 0.008, y1 + 0.04, h + 0.02, M["timber_light"], bevel=0)
    if vine:
        k = 0
        for i in range(n):
            for j in range(2):
                x = x0 + (i + 0.5) * (x1 - x0) / n
                y = y0 + (j + 0.5) * (y1 - y0) / 2
                if (i + j + k) % 3 != 2:
                    blob(x, y, h + 0.03, 0.07, 0.07, 0.03, M["leaf"], segs=7, rings=4)
        blob(x0, y0, h * 0.5, 0.03, 0.03, h * 0.45, M["leaf"], segs=6, rings=4)


def shopfront(x0, x1, face_y, M, st, z_top=0.3, awning=True):
    """A taberna on the street: dark opening, a counter with jars, a plain canvas awning."""
    box(x0, face_y - 0.015, 0.02, x1, face_y + 0.005, z_top, M["dark"], bevel=0)
    box(x0 + 0.02, face_y - 0.08, 0, x1 - 0.02, face_y - 0.015, 0.12, M["travertine"], bevel=0.004)
    for i in range(2):
        amphora(x0 + 0.06 + i * 0.07, face_y - 0.05, M, 0.55)
    if awning:
        mesh("awning", [(x0 - 0.03, face_y, z_top + 0.07), (x1 + 0.03, face_y, z_top + 0.07),
                        (x1 + 0.03, face_y - 0.2, z_top - 0.02), (x0 - 0.03, face_y - 0.2, z_top - 0.02)],
             [(0, 1, 2, 3)], M["canvas"], solidify=0.012)


def low_wall(pts, mat, h=0.16, t=0.05):
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        xs, ys = sorted((ax, bx)), sorted((ay, by))
        box(xs[0] - t / 2, ys[0] - t / 2, 0, xs[1] + t / 2, ys[1] + t / 2, h, mat, bevel=0.005)


def washing_line(x0, x1, y, M, h=0.28):
    for x in (x0, x1):
        box(x - 0.012, y - 0.012, 0, x + 0.012, y + 0.012, h, M["timber"], bevel=0)
    box(x0, y - 0.003, h - 0.02, x1, y + 0.003, h - 0.014, M["canvas"], bevel=0)
    cl = [M["canvas"], M["painted_red"], M["blue"]]
    n = 3
    for i in range(n):
        x = x0 + (i + 0.6) * (x1 - x0) / (n + 0.4)
        mesh("cloth", [(x - 0.05, y, h - 0.02), (x + 0.05, y, h - 0.02), (x + 0.05, y, h - 0.13),
                       (x - 0.05, y, h - 0.13)], [(0, 1, 2, 3)], cl[i % 3])


def goats(pts, M):
    for x, y in pts:
        blob(x, y, 0.07, 0.07, 0.035, 0.035, M["canvas"], segs=7, rings=4)
        blob(x + 0.06, y, 0.1, 0.025, 0.02, 0.025, M["hide"], segs=6, rings=4)


def block(st, M, x0, y0, x1, y1, zw, band=True, z0=0.1):
    """A plastered wall block with the region's base band and (north) timber frame."""
    box(x0, y0, z0 if band else 0, x1, y1, zw, st["wall"])
    if band:
        box(x0 - 0.015, y0 - 0.015, 0, x1 + 0.015, y1 + 0.015, z0 + 0.02, st["band"])
    if st.get("frame"):
        frame_beams(x0, y0, x1, y1, z0 + 0.02 if band else 0, zw, M)


# tent (t0) ---------------------------------------------------------------

def tent_var(M, region, v):
    st = vstyle(M, region, v)
    lot(2, st["yard"])
    canvas = [None, material("canvas_ochre", "#d6aa62", "#c49650", scale=8, bump=0.1),
              material("canvas_grey", "#c9c2b0", "#b5ad9a", scale=8, bump=0.1), M["hide"]][v]
    if region == "east":
        canvas = [None, M["canvas_stripe"], material("canvas_blue", "#5b84a8", "#e8e0cc", kind="stripes",
                                                     stripe=0.08, bump=0.0), M["canvas"]][v]

    def a_tent(x0, x1, y0, y1, h, mat, axis="y"):
        if axis == "y":  # ridge along x, as v0
            mesh("tent", [(x0, y0, 0), (x1, y0, 0), (x1, (y0 + y1) / 2, h), (x0, (y0 + y1) / 2, h),
                          (x0, y1, 0), (x1, y1, 0)], [(0, 1, 2, 3), (3, 2, 5, 4)], mat, solidify=0.02)
            mesh("tent_end", [(x1, y0, 0), (x1, y1, 0), (x1, (y0 + y1) / 2, h)], [(0, 1, 2)], mat)
            mesh("tent_door", [(x0, y0, 0), (x0, (y0 + y1) / 2, h), (x0, y1, 0)], [(0, 1, 2)], M["dark"])
        else:  # ridge along y, door to the street
            xm = (x0 + x1) / 2
            mesh("tent", [(x0, y0, 0), (xm, y0, h), (xm, y1, h), (x0, y1, 0), (x1, y0, 0), (x1, y1, 0)],
                 [(0, 1, 2, 3), (1, 4, 5, 2)], mat, solidify=0.02)
            mesh("tent_end", [(x0, y1, 0), (x1, y1, 0), (xm, y1, h)], [(0, 1, 2)], mat)
            mesh("tent_door", [(x0, y0, 0), (xm, y0, h), (x1, y0, 0)], [(0, 1, 2)], M["dark"])

    if region == "north":
        cones = {1: [(-0.4, 0.3, 0.55, 0.4), (0.45, 0.45, 0.45, 0.32)], 2: [(0.2, 0.2, 0.62, 0.48)],
                 3: [(-0.35, -0.1, 0.5, 0.38)]}[v]
        for x, y, h, r in cones:
            cone(x, y, 0, h, r, M["hide"] if v != 2 else M["thatch"], n=14)
            box(x - 0.08, y - r + 0.02, 0, x + 0.08, y - r + 0.08, h * 0.32, M["dark"], bevel=0)
    elif region == "egypt":
        xs = {1: (-0.7, 0.5), 2: (-0.3, 0.8), 3: (-0.8, 0.0)}[v]
        for x in xs:
            for y in (-0.3, 0.6):
                box(x - 0.02, y - 0.02, 0, x + 0.02, y + 0.02, 0.36, M["timber_light"], bevel=0)
        box(xs[0] - 0.06, -0.35, 0.34, xs[1] + 0.06, 0.65, 0.38, M["reed"], bevel=0.005)
        box(xs[0] - 0.02, 0.55, 0, xs[1] + 0.02, 0.63, 0.34, M["reed"], bevel=0)
        palm(0.75 if v != 2 else -0.75, -0.7 if v != 3 else 0.7, 0.62, M)
    elif v == 1:  # door to the street, cart beside
        a_tent(-0.75, 0.15, -0.45, 0.55, 0.5, canvas, axis="x")
        kit.cart(0.55, 0.45, M, load=True)
    elif v == 2:  # two small tents and a washing line
        a_tent(-0.85, -0.15, 0.0, 0.7, 0.4, canvas)
        a_tent(0.15, 0.85, 0.15, 0.8, 0.36, M["canvas"], axis="x")
        washing_line(-0.7, 0.2, -0.35, M)
    else:  # lean-to of hides against a low stone wall
        low_wall([(-0.85, 0.6), (0.6, 0.6)], M["stone_dark"], h=0.3, t=0.08)
        mesh("leanto", [(-0.8, 0.56, 0.36), (0.55, 0.56, 0.36), (0.55, -0.05, 0.06), (-0.8, -0.05, 0.06)],
             [(0, 1, 2, 3)], canvas, solidify=0.02)
        for x in (-0.75, 0.5):
            box(x - 0.015, -0.07, 0, x + 0.015, -0.04, 0.08, M["timber"], bevel=0)
        goats([(0.6, -0.55), (0.75, -0.3)], M)
    stone_ring(0.45 if v != 2 else -0.3, -0.55 if v != 2 else -0.75, M)
    amphora(-0.8 if v != 3 else 0.8, -0.75, M, 0.8)
    kit.sack(-0.6 if v != 3 else 0.6, -0.8, 0, M, 0.9)


# hut (t1): thatch stays the hut cue, tiles start at t2 ------------------------------------------------------------------

def hut_var(M, region, v):
    st = vstyle(M, region, v, humble=True)
    lot(2, st["ground"])
    wall = {"rome": [M["daub"], material("daub_ochre", "#d4b582", "#c09f6c", scale=10, bump=0.2,
                                          detail=(1.5, "#ae8e60")),
                     material("daub_pale", "#ddd0b4", "#cbbd9e", scale=10, bump=0.2, detail=(1.5, "#b9a888")),
                     M["daub"]][v],
            "east": st["wall"], "egypt": [None, M["mudbrick"], st["wall"], M["mudbrick"]][v],
            "north": M["daub"]}[region]
    mesh("yard", [(-1, -1, 0.003), (1, -1, 0.003), (1, -0.35, 0.003), (-1, -0.35, 0.003)], [(0, 1, 2, 3)],
         st["yard"])
    flat = st["hut_roof"] == "flat"
    if region == "north":
        huts = {1: [(-0.4, 0.35, 0.36), (0.45, 0.5, 0.28)], 2: [(0.2, 0.3, 0.45)], 3: [(-0.3, 0.2, 0.4)]}[v]
        for x, y, r in huts:
            cylinder(x, y, 0, 0.28, r, M["daub"], n=16)
            cone(x, y, 0.26, 0.26 + r * 1.2, r * 1.32, M["thatch"], n=16)
            box(x - 0.07, y - r - 0.02, 0, x + 0.07, y - r + 0.05, 0.22, M["dark"], bevel=0)
        if v == 3:
            box(0.3, 0.2, 0, 0.8, 0.7, 0.3, M["timber"], bevel=0.005)  # byre
            gable(0.3, 0.2, 0.8, 0.7, 0.3, 0.2, M["thatch"], axis="y", wall=M["timber"])
    else:
        if v == 1:  # gable end to the street; goat pen
            x0, y0, x1, y1, zw = -0.8, -0.3, 0.1, 0.8, 0.34
            box(x0, y0, 0, x1, y1, zw, wall)
            if flat:
                vroof(st, M, "flat", x0, y0, x1, y1, zw, 0.3)
            else:
                gable(x0, y0, x1, y1, zw, 0.36, M["thatch"], axis="y", over=0.09, wall=wall)
            door(-0.35, y0, M, w=0.14, h=0.24, mat=st["shutter"])
            fence([(0.25, -0.2), (0.9, -0.2), (0.9, 0.85), (0.25, 0.85), (0.25, -0.2)], M["timber_light"], h=0.1)
            goats([(0.5, 0.2), (0.65, 0.55)], M)
        elif v == 2:  # tiled lean-to hut along the back, woodshed, vegetable rows in front
            x0, y0, x1, y1, zw = -0.85, 0.2, 0.6, 0.85, 0.36
            box(x0, y0, 0, x1, y1, zw, wall)
            if flat:
                vroof(st, M, "flat", x0, y0, x1, y1, zw, 0.3)
            else:
                shed(x0, y0, x1, y1, zw, 0.26, M["thatch"], down="-y", over=0.09, wall=wall)
            door(-0.2, y0, M, w=0.14, h=0.25, mat=st["shutter"])
            win_y(y0, [-0.6, 0.3], 0.17, 0.25, M)
            box(0.65, 0.3, 0, 0.95, 0.85, 0.2, M["timber_light"], bevel=0.005)  # woodpile
            veg_patch(-0.85, -0.85, 0.3, -0.2, M)
        else:  # two rooms in an L, washing line
            box(-0.85, 0.0, 0, 0.2, 0.75, 0.32, wall)
            box(-0.85, -0.6, 0, -0.3, 0.0, 0.3, wall)
            if flat:
                vroof(st, M, "flat", -0.85, 0.0, 0.2, 0.75, 0.32, 0.3)
                vroof(st, M, "flat", -0.85, -0.6, -0.3, 0.0, 0.3, 0.3)
            else:
                hip(-0.85, 0.0, 0.2, 0.75, 0.32, 0.3, M["thatch"], over=0.09)
                hip(-0.85, -0.6, -0.3, 0.0, 0.3, 0.26, M["thatch"], over=0.08)
            door(-0.15, 0.0, M, w=0.14, h=0.23, mat=st["shutter"])
            win_x(-0.3, [-0.3], 0.16, 0.24, M)
            washing_line(0.0, 0.85, -0.55, M)
    stone_ring(0.55 if v != 2 else 0.75, -0.7 if v != 2 else -0.05, M, 0.07)
    tree(st["tree"], -0.75 if v == 2 else 0.75, -0.75 if v == 2 else 0.6 if v != 1 else -0.65, M, 0.8)
    amphora(-0.9, -0.9 if v != 2 else 0.05, M, 0.8)


# casa (t2) -----------------------------------------------------------------

def small_house_var(M, region, v):
    st = vstyle(M, region, v, humble=True)
    lot(2, st["ground"])
    wall = st["wall"]
    if v == 1:  # gable end to the street, lean-to annex, vine pergola
        mesh("path", [(-0.45, -1, 0.004), (-0.15, -1, 0.004), (-0.15, -0.35, 0.004), (-0.45, -0.35, 0.004)],
             [(0, 1, 2, 3)], st["yard"])
        x0, y0, x1, y1, zw = -0.85, -0.35, 0.25, 0.8, 0.44
        block(st, M, x0, y0, x1, y1, zw, band=False)
        vroof(st, M, "gable", x0, y0, x1, y1, zw, 0.26, axis="y")
        box(x1, 0.1, 0, 0.75, y1, 0.3, wall)
        vroof(st, M, "shed", x1, 0.1, 0.75, y1, 0.3, 0.12, down="+x")
        door(-0.3, y0, M, w=0.15, h=0.28, mat=st["door"])
        win_y(y0, [-0.65, 0.05], 0.2, 0.3, M, st)
        win_x(0.75, [0.45], 0.12, 0.22, M, st)
        win_x(x1, [-0.15], 0.2, 0.3, M, st)
        all_windows(x0, y0, x1, y1, [(0.2, 0.3)], M, st)
        pergola(0.15, -0.85, 0.85, -0.25, 0.34, M)
        box(0.3, -0.75, 0, 0.7, -0.68, 0.08, M["timber_light"], bevel=0.004)  # bench
        pot_plant(-0.7, -0.6, M)
        tree(st["tree"], 0.8, 0.0, M, 0.75)
    elif v == 2:  # L-plan with a walled garden and a fig tree
        x0, y0, x1, y1, zw = -0.85, 0.15, 0.6, 0.85, 0.42
        block(st, M, x0, y0, x1, y1, zw, band=False)
        vroof(st, M, "gable", x0, y0, x1, y1, zw, 0.22, axis="x")
        box(-0.85, -0.6, 0, -0.2, 0.15, 0.4, wall)
        vroof(st, M, "gable", -0.85, -0.6, -0.2, 0.2, 0.4, 0.2, axis="y")
        door(-0.52, -0.6, M, w=0.15, h=0.27, mat=st["door"])
        win_y(y0, [0.0, 0.35], 0.2, 0.3, M, st)
        win_x(x1, [0.5], 0.2, 0.3, M, st)
        win_x(-0.2, [-0.3], 0.2, 0.3, M, st)
        all_windows(x0, -0.6, x1, y1, [(0.2, 0.3)], M, st)
        chimney(0.35, 0.65, 0.42, 0.72, M, wall)
        low_wall([(-0.2, -0.95), (0.95, -0.95), (0.95, 0.12)], wall, h=0.18)
        mesh("garden", [(-0.15, -0.9, 0.004), (0.9, -0.9, 0.004), (0.9, 0.1, 0.004), (-0.15, 0.1, 0.004)],
             [(0, 1, 2, 3)], M["garden"])
        olive(0.45, -0.4, M, 0.7)
        for x, y in [(0.0, -0.75), (0.75, -0.75), (0.75, -0.05)]:
            bush(x, y, 0.07, M["hedge"])
        blob(0.05, -0.05, 0.05, 0.05, 0.05, 0.03, M["flower_r"], segs=6, rings=4)
    else:  # lean-to range along the back, a little wing, a yard with jars and a washing line
        x0, y0, x1, y1, zw = -0.9, 0.3, 0.9, 0.85, 0.42
        block(st, M, x0, y0, x1, y1, zw, band=False)
        vroof(st, M, "shed", x0, y0, x1, y1, zw, 0.2, down="-y")
        box(0.35, -0.55, 0, 0.9, 0.3, 0.38, wall)
        vroof(st, M, "hip", 0.35, -0.55, 0.9, 0.3, 0.38, 0.2)
        door(-0.4, y0, M, w=0.15, h=0.28, mat=st["door"])
        door(0.62, -0.55, M, w=0.13, h=0.26, mat=st["door"])
        win_y(y0, [-0.75, 0.0], 0.2, 0.3, M, st)
        win_x(0.9, [-0.25, 0.1, 0.55], 0.2, 0.3, M, st)
        all_windows(x0, y0, x1, y1, [(0.2, 0.3)], M, st)
        mesh("yard2", [(-1, -1, 0.004), (0.35, -1, 0.004), (0.35, 0.3, 0.004), (-1, 0.3, 0.004)],
             [(0, 1, 2, 3)], st["yard"])
        washing_line(-0.85, 0.15, -0.1, M)
        for x in (-0.85, -0.72):
            amphora(x, -0.85, M, 0.8)
        box(-0.1, -0.85, 0, 0.2, -0.65, 0.12, M["timber_light"], bevel=0.005)  # coop
        mesh("coop_roof", [(-0.13, -0.88, 0.12), (0.23, -0.88, 0.12), (0.23, -0.62, 0.17), (-0.13, -0.62, 0.17)],
             [(0, 1, 2, 3)], M["timber"], solidify=0.015)
        tree(st["tree"], 0.6, -0.8, M, 0.8)


# small insula (t3) -----------------------------------------------------------

def two_storey_var(M, region, v):
    st = vstyle(M, region, v)
    lot(2, st["ground"])
    pave = M["paving"] if region != "egypt" else M["sandstone"]
    if v == 1:  # gable end to the street, a shop below
        mesh("pave", [(-1, -1, 0.004), (0.75, -1, 0.004), (0.75, -0.5, 0.004), (-1, -0.5, 0.004)],
             [(0, 1, 2, 3)], pave)
        x0, y0, x1, y1, zw = -0.9, -0.5, 0.55, 0.85, 0.78
        block(st, M, x0, y0, x1, y1, zw)
        vroof(st, M, "gable", x0, y0, x1, y1, zw, 0.3, axis="y")
        shopfront(-0.8, -0.3, y0, M, st)
        door(0.15, y0, M, w=0.15, h=0.3, mat=st["door"], frame=st["band"])
        win_y(y0, [-0.7, -0.4, 0.0, 0.3], 0.5, 0.62, M, st)
        win_x(x1, [-0.3, 0.0, 0.3, 0.6], 0.5, 0.62, M, st)
        win_x(x1, [-0.1, 0.45], 0.2, 0.3, M, st)
        all_windows(x0, y0, x1, y1, [(0.5, 0.62), (0.2, 0.3)], M, st)
        chimney(0.3, 0.6, 0.78, 1.15, M, st["wall"])
        tree(st["tree"], 0.8, 0.3, M, 0.8)
        pot_plant(0.75, -0.8, M)
        bush(0.85, -0.3, 0.08, M["hedge"])
    elif v == 2:  # two storeys and a one-storey wing; pergola garden in front
        x0, y0, x1, y1, zw = -0.9, -0.3, 0.3, 0.9, 0.76
        block(st, M, x0, y0, x1, y1, zw)
        vroof(st, M, "hip", x0, y0, x1, y1, zw, 0.26)
        box(x1, 0.1, 0.1, 0.9, 0.9, 0.42, st["wall"])
        box(x1, 0.085, 0, 0.915, 0.915, 0.12, st["band"])
        vroof(st, M, "shed", x1, 0.1, 0.9, 0.9, 0.42, 0.14, down="+x")
        door(-0.6, y0, M, w=0.16, h=0.32, mat=st["door"], frame=st["band"])
        win_y(y0, [-0.25, 0.1], 0.2, 0.3, M, st)
        win_y(y0, [-0.75, -0.45, -0.1, 0.15], 0.5, 0.62, M, st)
        win_x(x1, [-0.1], 0.5, 0.62, M, st)
        win_x(0.9, [0.3, 0.65], 0.2, 0.3, M, st)
        win_y(0.1, [0.6], 0.2, 0.3, M, st)
        all_windows(x0, y0, x1, y1, [(0.5, 0.62), (0.2, 0.3)], M, st)
        chimney(-0.2, 0.55, 0.76, 1.12, M, st["wall"])
        mesh("path", [(-0.7, -1, 0.004), (-0.5, -1, 0.004), (-0.5, -0.3, 0.004), (-0.7, -0.3, 0.004)],
             [(0, 1, 2, 3)], pave)
        pergola(-0.25, -0.9, 0.85, -0.2, 0.4, M)
        box(0.0, -0.6, 0, 0.6, -0.5, 0.08, M["timber_light"], bevel=0.004)  # table
        pot_plant(0.75, 0.0, M)
        pot_plant(-0.9, -0.8, M)
    else:  # light well in the roof, shuttered windows, an upper loggia on the side
        x0, y0, x1, y1, zw = -0.85, -0.45, 0.6, 0.85, 0.72
        block(st, M, x0, y0, x1, y1, zw)
        vroof(st, M, "ring", x0, y0, x1, y1, zw, 0.24, hole=0.17)
        door(-0.1, y0, M, w=0.17, h=0.32, mat=st["door"], frame=st["band"])
        win_y(y0, [-0.6, -0.35, 0.15, 0.4], 0.5, 0.62, M, st)
        win_y(y0, [-0.55, 0.3], 0.2, 0.3, M, st)
        win_x(x1, [-0.25, 0.65], 0.5, 0.62, M, st)
        win_x(x1, [-0.2, 0.35], 0.2, 0.3, M, st)
        all_windows(x0, y0, x1, y1, [(0.5, 0.62), (0.2, 0.3)], M, st)
        # loggia: a balcony with posts and a little roof on the right wall
        box(x1, 0.0, 0.42, x1 + 0.16, 0.5, 0.45, M["timber_light"], bevel=0.004)
        for y in (0.02, 0.25, 0.48):
            box(x1 + 0.13, y - 0.012, 0.45, x1 + 0.155, y + 0.012, 0.66, M["timber"], bevel=0)
        box(x1 + 0.13, 0.0, 0.53, x1 + 0.16, 0.5, 0.55, M["timber"], bevel=0)
        mesh("loggia_roof", [(x1, -0.03, 0.72), (x1, 0.53, 0.72), (x1 + 0.2, 0.53, 0.66), (x1 + 0.2, -0.03, 0.66)],
             [(0, 1, 2, 3)], st["roof"] or M["timber"], solidify=0.02)
        box(x1, 0.08, 0.45, x1 + 0.012, 0.42, 0.62, M["window"], bevel=0)
        mesh("path", [(-0.25, -1, 0.004), (0.05, -1, 0.004), (0.05, -0.45, 0.004), (-0.25, -0.45, 0.004)],
             [(0, 1, 2, 3)], pave)
        tree(st["tree"], -0.75, -0.75, M, 0.85)
        tree(st["tree"], 0.85, -0.65, M, 0.7)
        bush(0.4, -0.8, 0.08, M["hedge"])


# large insula (t4) -----------------------------------------------------------

def town_house_var(M, region, v):
    st = vstyle(M, region, v)
    lot(2, st["ground"])
    pave = M["paving"] if region != "egypt" else M["sandstone"]
    if v == 1:  # insula with a row of shops, a gallery along the first floor
        mesh("pave", [(-1, -1, 0.004), (1, -1, 0.004), (1, -0.6, 0.004), (-1, -0.6, 0.004)], [(0, 1, 2, 3)], pave)
        x0, y0, x1, y1, zw = -0.92, -0.6, 0.88, 0.9, 1.12
        block(st, M, x0, y0, x1, y1, zw)
        box(x0 - 0.012, y0 - 0.012, 0.44, x1 + 0.012, y1 + 0.012, 0.47, st["band"], bevel=0)
        vroof(st, M, "gable", x0, y0, x1, y1, zw, 0.3, axis="x")
        for xa in (-0.85, -0.38, 0.32):
            shopfront(xa, xa + 0.4, y0, M, st, z_top=0.32)
        door(0.17, y0, M, w=0.13, h=0.34, mat=st["door"], frame=st["band"])
        # gallery at the first floor
        box(x0 + 0.05, y0 - 0.15, 0.47, x1 - 0.05, y0, 0.5, M["timber_light"], bevel=0.004)
        for i in range(10):
            x = x0 + 0.08 + i * (x1 - x0 - 0.16) / 9
            box(x - 0.008, y0 - 0.14, 0.5, x + 0.008, y0 - 0.125, 0.6, M["timber"], bevel=0)
        box(x0 + 0.05, y0 - 0.15, 0.59, x1 - 0.05, y0 - 0.12, 0.61, M["timber"], bevel=0)
        win_y(y0, [-0.75, -0.45, -0.15, 0.15, 0.45, 0.75], 0.58, 0.7, M, st)
        win_y(y0, [-0.75, -0.45, -0.15, 0.15, 0.45, 0.75], 0.88, 1.0, M, st)
        win_x(x1, [-0.35, -0.05, 0.25, 0.55], 0.58, 0.7, M, st)
        win_x(x1, [-0.35, -0.05, 0.25, 0.55], 0.88, 1.0, M, st)
        win_x(x1, [-0.2, 0.4], 0.2, 0.3, M, st)
        all_windows(x0, y0, x1, y1, [(0.58, 0.7), (0.88, 1.0), (0.2, 0.3)], M, st)
        chimney(-0.5, 0.5, 1.12, 1.48, M, st["wall"])
        chimney(0.5, 0.5, 1.12, 1.45, M, st["wall"])
    elif v == 2:  # light-well block, a shop wing and a walled court with a tree
        x0, y0, x1, y1, zw = -0.9, -0.3, 0.9, 0.9, 1.04
        block(st, M, x0, y0, x1, y1, zw)
        box(x0 - 0.012, y0 - 0.012, 0.42, x1 + 0.012, y1 + 0.012, 0.45, st["band"], bevel=0)
        box(x0 - 0.012, y0 - 0.012, 0.72, x1 + 0.012, y1 + 0.012, 0.745, st["band"], bevel=0)
        vroof(st, M, "ring", x0, y0, x1, y1, zw, 0.28, hole=0.2)
        box(0.05, -0.95, 0.1, 0.9, y0, 0.44, st["wall"])
        box(0.035, -0.965, 0, 0.915, y0, 0.12, st["band"])
        vroof(st, M, "shed", 0.05, -0.95, 0.9, y0, 0.44, 0.14, down="-y")
        shopfront(0.15, 0.55, -0.95, M, st, z_top=0.3)
        door(0.75, -0.95, M, w=0.13, h=0.3, mat=st["door"])
        win_x(0.9, [-0.75, -0.5], 0.2, 0.3, M, st)
        door(-0.4, y0, M, w=0.17, h=0.34, mat=st["door"], frame=st["band"])
        win_y(y0, [-0.75, -0.55, -0.2], 0.55, 0.66, M, st)
        win_y(y0, [-0.75, -0.55, -0.2, 0.2, 0.5, 0.75], 0.85, 0.96, M, st)
        win_y(y0, [0.2, 0.5, 0.75], 0.55, 0.66, M, st)
        win_x(x1, [-0.1, 0.2, 0.5, 0.75], 0.55, 0.66, M, st)
        win_x(x1, [-0.1, 0.2, 0.5, 0.75], 0.85, 0.96, M, st)
        all_windows(x0, y0, x1, y1, [(0.55, 0.66), (0.85, 0.96), (0.2, 0.3)], M, st)
        low_wall([(-0.95, -0.3), (-0.95, -0.95), (0.05, -0.95)], st["wall"], h=0.22)
        mesh("court", [(-0.92, -0.92, 0.004), (0.05, -0.92, 0.004), (0.05, -0.3, 0.004), (-0.92, -0.3, 0.004)],
             [(0, 1, 2, 3)], M["mosaic"] if region != "north" else pave)
        tree(st["tree"], -0.6, -0.65, M, 0.9)
        pot_plant(-0.2, -0.8, M)
        chimney(0.55, 0.55, 1.04, 1.4, M, st["wall"])
    else:  # stepped: a tall back range and a lower front range with a roof terrace
        x0, y0, x1, y1, zw = -0.9, 0.15, 0.9, 0.9, 1.14
        block(st, M, x0, y0, x1, y1, zw)
        box(x0 - 0.012, y0 - 0.012, 0.44, x1 + 0.012, y1 + 0.012, 0.47, st["band"], bevel=0)
        vroof(st, M, "hip", x0, y0, x1, y1, zw, 0.28)
        fx0, fy0, fx1, fz = -0.9, -0.8, 0.35, 0.74
        block(st, M, fx0, fy0, fx1, y0 + 0.01, fz)
        box(fx0 - 0.02, fy0 - 0.02, fz, fx1 + 0.02, y0, fz + 0.03, M["paving"], bevel=0.004)
        parapet(fx0 - 0.02, fy0 - 0.02, fx1 + 0.02, y0, fz + 0.03, st["wall"])
        pergola(-0.7, -0.6, 0.15, -0.1, 0.24, M, vine=False)
        for x, y in [(-0.65, -0.55), (0.1, -0.55), (-0.3, -0.15)]:
            box(x - 0.05, y - 0.05, fz + 0.03, x + 0.05, y + 0.05, fz + 0.08, M["amphora"], bevel=0.005)
            blob(x, y, fz + 0.12, 0.07, 0.07, 0.06, M["leaf"], segs=8, rings=5)
        door(-0.3, fy0, M, w=0.18, h=0.34, mat=st["door"], frame=st["band"])
        win_y(fy0, [-0.75, -0.55, -0.05, 0.15], 0.22, 0.34, M, st)
        win_y(fy0, [-0.75, -0.5, 0.0, 0.2], 0.52, 0.64, M, st)
        win_x(fx1, [-0.55, -0.15], 0.52, 0.64, M, st)
        win_y(y0, [0.5, 0.75], 0.22, 0.34, M, st)
        win_y(y0, [-0.75, -0.45, -0.15, 0.15, 0.5, 0.75], 0.88, 1.0, M, st)
        win_y(y0, [0.5, 0.75], 0.58, 0.7, M, st)
        win_x(x1, [0.3, 0.55, 0.8], 0.58, 0.7, M, st)
        win_x(x1, [0.3, 0.55, 0.8], 0.88, 1.0, M, st)
        win_x(x1, [0.35, 0.7], 0.22, 0.34, M, st)
        all_windows(x0, fy0, x1, y1, [(0.58, 0.7), (0.88, 1.0), (0.22, 0.34)], M, st)
        mesh("court", [(0.35, -1, 0.004), (1, -1, 0.004), (1, 0.15, 0.004), (0.35, 0.15, 0.004)],
             [(0, 1, 2, 3)], pave)
        tree(st["tree"], 0.7, -0.4, M, 0.9)
        bush(0.5, -0.85, 0.08, M["hedge"])
        pot_plant(0.85, -0.85, M)


# domus variants: plaster, roof tone, garden, and one structural change each -----------------

def _domus_extras(st, M, region, v, front_y, x_half, tier):
    """Per-variant additions to a domus: shops flanking the door (v2), a garden of fruit trees or
    an upper room over a corner (v3), a mirrored garden (v1)."""
    if v == 2:  # tabernae on the street front, either side of the door
        for xa in (-x_half + 0.15, x_half - 0.6):
            shopfront(xa, xa + 0.45, front_y, M, st, z_top=0.3)
    elif v == 3 and st["roof_kind"] != "flat":  # cenaculum: an upper room over the front-left corner
        x0, y0 = -x_half + 0.05, front_y + 0.05
        zb = {0: 0.62, 1: 0.66, 2: 0.92}[tier]
        box(x0, y0, zb - 0.05, x0 + 0.6, y0 + 0.55, zb + 0.32, st["wall"])
        win_y(y0, [x0 + 0.15, x0 + 0.42], zb + 0.08, zb + 0.2, M, st)
        win_x(x0 + 0.6, [y0 + 0.27], zb + 0.08, zb + 0.2, M, st)
        win_x(x0, [y0 + 0.27], zb + 0.08, zb + 0.2, M, st, out=-1)
        win_y(y0 + 0.55, [x0 + 0.3], zb + 0.08, zb + 0.2, M, st, out=1)
        vroof(st, M, "hip", x0, y0, x0 + 0.6, y0 + 0.55, zb + 0.32, 0.18)


def domus_var(fn, M, region, v):
    """Domus tier fn with variant v: swaps style colours via a patched style() and adds extras."""
    global style
    base_style = style

    def vs(M2, region2):
        return vstyle(M2, region2, v)
    style = vs
    try:
        fn(M, region)
    finally:
        style = base_style
    st = vstyle(M, region, v)
    tier = {domus_atrium: 0, domus_peristyle: 1, domus_villa: 2}[fn]
    front = {0: -1.1, 1: -1.2, 2: -1.25}[tier]
    _domus_extras(st, M, region, v, front, 1.3, tier)
    if v == 1:
        for x, y in [(-1.3, -1.3), (1.3, -1.3)]:
            tree(st["tree"], x, y, M, 0.75)
    if v == 3:
        olive(1.2, -1.3, M, 0.6)


def plot_tent(M, region="rome", v=0):
    return _plot_tent_v0(M, region) if v == 0 else tent_var(M, region, v)


def plot_hut(M, region="rome", v=0):
    return _plot_hut_v0(M, region) if v == 0 else hut_var(M, region, v)


def plot_small_house(M, region="rome", v=0):
    return _plot_small_house_v0(M, region) if v == 0 else small_house_var(M, region, v)


def plot_two_storey(M, region="rome", v=0):
    return _plot_two_storey_v0(M, region) if v == 0 else two_storey_var(M, region, v)


def plot_town_house(M, region="rome", v=0):
    return _plot_town_house_v0(M, region) if v == 0 else town_house_var(M, region, v)


# ---------------------------------------------------------------- domus (3x3)

def domus_empty(M):
    extra(M)
    lot(3, M["dirt"])
    stakes(3, M)
    box(-0.15, -0.15, 0, 0.15, 0.15, 0.06, M["travertine"], bevel=0.01)
    for x, y in [(0.8, -0.8), (0.95, -0.7)]:
        box(x - 0.12, y - 0.06, 0, x + 0.12, y + 0.06, 0.08, M["travertine"], bevel=0.008)


def _atrium_block(st, M, x0, y0, x1, y1, zw, hole=0.26, region="rome"):
    box(x0, y0, 0.12, x1, y1, zw, st["wall"])
    box(x0 - 0.015, y0 - 0.015, 0, x1 + 0.015, y1 + 0.015, 0.14, st["band"])
    if st.get("frame"):
        frame_beams(x0, y0, x1, y1, 0.14, zw, M)
    cy = (y0 + y1) / 2
    if st["roof_kind"] == "flat":
        box(x0 - 0.02, y0 - 0.02, zw, x1 + 0.02, y1 + 0.02, zw + 0.03, st["wall"], bevel=0.004)
        parapet(x0 - 0.02, y0 - 0.02, x1 + 0.02, y1 + 0.02, zw + 0.03, st["wall"])
        box(-hole, cy - hole, zw - 0.02, hole, cy + hole, zw + 0.035, M["dark"], bevel=0)
        box(-hole * 0.55, cy - hole * 0.55, zw, hole * 0.55, cy + hole * 0.55, zw + 0.04, M["water"], bevel=0)
    else:
        rm = st["roof"] if region != "north" else M["shingle"]
        ring_roof(x0, y0, x1, y1, zw, 0.26 if st["roof_kind"] != "low" else 0.14, hole, rm)
        box(-hole, cy - hole, zw - 0.02, hole, cy + hole, zw + 0.07, M["dark"], bevel=0)
        box(-hole * 0.55, cy - hole * 0.55, zw, hole * 0.55, cy + hole * 0.55, zw + 0.075, M["water"], bevel=0)


def _portico(x0, x1, y, z, h, M, mat, n=4, r=0.035):
    xs = [x0 + i * (x1 - x0) / (n - 1) for i in range(n)]
    for x in xs:
        column(x, y, z, h, r, mat)
    box(x0 - 0.05, y - 0.06, z + h, x1 + 0.05, y + 0.06, z + h + 0.06, mat, bevel=0.004)


def domus_atrium(M, region="rome"):
    """t0: atrium house filling the front of the lot, kitchen garden behind."""
    st = style(extra(M), region)
    lot(3, st["ground"])
    mesh("path", [(-0.22, -1.5, 0.004), (0.22, -1.5, 0.004), (0.22, -1.1, 0.004), (-0.22, -1.1, 0.004)],
         [(0, 1, 2, 3)], M["paving"] if region != "egypt" else M["sandstone"])
    x0, y0, x1, y1, zw = -1.3, -1.1, 1.3, 0.55, 0.62
    _atrium_block(st, M, x0, y0, x1, y1, zw, region=region)
    door(0, y0, M, w=0.22, h=0.36, mat=st["door"], frame=st["band"])
    for x in (-0.38, 0.38):  # tabernae-like flanking doors
        box(x - 0.012, y0 - 0.06, 0, x + 0.012, y0, 0.44, st["band"], bevel=0)
    windows_y(y0, [-1.0, -0.75, 0.75, 1.0], 0.38, 0.5, M, w=0.08)
    windows_x(x1, [-0.8, -0.45, -0.1, 0.25], 0.38, 0.5, M, w=0.08)
    veg_patch(0.3, 0.75, 1.3, 1.35, M)
    tree(st["tree"], -0.6, 1.0, M, 1.0)
    tree(st["tree"], -1.1, 1.15, M, 0.9)
    pot_plant(-0.45, -1.3, M)
    pot_plant(0.45, -1.3, M)


def _peristyle(st, M, x0, y0, x1, y1, region, pool=False):
    """Colonnaded garden: low roofs on the far sides, a garden and columns."""
    garden_mat = st["ground"]
    mesh("garden", [(x0, y0, 0.006), (x1, y0, 0.006), (x1, y1, 0.006), (x0, y1, 0.006)],
         [(0, 1, 2, 3)], garden_mat)
    colm = M["plaster_house"] if region != "egypt" else M["sandstone"]
    if region == "east":
        colm = M["whitewash"]
    zc = 0.4
    xs = [x0 + 0.1 + i * (x1 - x0 - 0.2) / 4 for i in range(5)]
    ys = [y0 + 0.1 + i * (y1 - y0 - 0.2) / 3 for i in range(4)]
    for x in xs:
        column(x, y0 + 0.1, 0, zc, 0.03, colm)
        column(x, y1 - 0.1, 0, zc, 0.03, colm)
    for y in ys[1:-1]:
        column(x0 + 0.1, y, 0, zc, 0.03, colm)
        column(x1 - 0.1, y, 0, zc, 0.03, colm)
    # far portico roof (back and left), lean-to
    r = st["roof"] if st["roof_kind"] not in ("flat",) else None
    if r is not None:
        mesh("porch_back", [(x0, y1 - 0.18, zc), (x1, y1 - 0.18, zc), (x1, y1 + 0.02, zc + 0.12),
                            (x0, y1 + 0.02, zc + 0.12)], [(0, 1, 2, 3)], r, solidify=0.03)
        mesh("porch_left", [(x0 - 0.02, y0, zc + 0.12), (x0 + 0.18, y0, zc), (x0 + 0.18, y1, zc),
                            (x0 - 0.02, y1, zc + 0.12)], [(0, 1, 2, 3)], r, solidify=0.03)
    else:
        box(x0 - 0.02, y1 - 0.18, zc, x1, y1 + 0.02, zc + 0.04, st["wall"], bevel=0.004)
        box(x0 - 0.02, y0, zc, x0 + 0.18, y1, zc + 0.04, st["wall"], bevel=0.004)
    box(x0 + 0.1, y0 + 0.08, zc, x1 - 0.1, y0 + 0.12, zc + 0.05, colm, bevel=0.003)  # front beam
    box(x1 - 0.12, y0 + 0.1, zc, x1 - 0.08, y1 - 0.1, zc + 0.05, colm, bevel=0.003)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    if pool:
        box(cx - 0.42, cy - 0.17, 0, cx + 0.42, cy + 0.17, 0.05, M["marble"], bevel=0.008)
        box(cx - 0.38, cy - 0.13, 0.01, cx + 0.38, cy + 0.13, 0.052, M["water"], bevel=0)
    else:
        box(cx - 0.12, cy - 0.12, 0, cx + 0.12, cy + 0.12, 0.05, M["marble"], bevel=0.008)
        box(cx - 0.08, cy - 0.08, 0.01, cx + 0.08, cy + 0.08, 0.052, M["water"], bevel=0)
    for dx, dy in [(-0.45, -0.2), (0.45, -0.2), (-0.45, 0.2), (0.45, 0.2)]:
        box(cx + dx - 0.12, cy + dy - 0.05, 0, cx + dx + 0.12, cy + dy + 0.05, 0.08, M["hedge"], bevel=0.02)
    for dx, dy in [(-0.25, 0.0), (0.25, 0.0)]:
        blob(cx + dx, cy + dy, 0.06, 0.05, 0.05, 0.05, M["flower_r"], segs=6, rings=4)


def domus_peristyle(M, region="rome"):
    """t1: atrium house in front, peristyle garden behind."""
    st = style(extra(M), region)
    lot(3, st["ground"])
    mesh("path", [(-0.22, -1.5, 0.004), (0.22, -1.5, 0.004), (0.22, -1.2, 0.004), (-0.22, -1.2, 0.004)],
         [(0, 1, 2, 3)], M["paving"] if region != "egypt" else M["sandstone"])
    x0, y0, x1, y1, zw = -1.35, -1.2, 1.35, 0.2, 0.66
    _atrium_block(st, M, x0, y0, x1, y1, zw, hole=0.24, region=region)
    door(0, y0, M, w=0.22, h=0.38, mat=st["door"], frame=st["band"])
    windows_y(y0, [-1.05, -0.8, -0.45, 0.45, 0.8, 1.05], 0.4, 0.52, M, w=0.08)
    windows_x(x1, [-0.95, -0.6, -0.25, 0.05], 0.4, 0.52, M, w=0.08)
    # east wing along the right side of the garden
    box(0.85, 0.2, 0.12, 1.35, 1.4, zw - 0.1, st["wall"])
    box(0.835, 0.2, 0, 1.365, 1.415, 0.14, st["band"])
    roof(st, 0.85, 0.2, 1.35, 1.4, zw - 0.1, 0.18, M, axis="y") if st["roof_kind"] != "hip" else \
        hip(0.85, 0.2, 1.35, 1.4, zw - 0.1, 0.18, st["roof"])
    windows_x(1.35, [0.45, 0.8, 1.15], 0.3, 0.42, M, w=0.08)
    _peristyle(st, M, -1.35, 0.25, 0.8, 1.45, region)
    tree(st["tree"], -1.15, 1.25, M, 0.9)
    pot_plant(-0.45, -1.35, M)
    pot_plant(0.45, -1.35, M)


def domus_villa(M, region="rome"):
    """t2: two-storey front range with a loggia, peristyle with a long pool, cypress avenue."""
    st = style(extra(M), region)
    lot(3, st["ground"])
    mesh("path", [(-0.25, -1.5, 0.004), (0.25, -1.5, 0.004), (0.25, -1.25, 0.004), (-0.25, -1.25, 0.004)],
         [(0, 1, 2, 3)], M["marble_paving"])
    x0, y0, x1, y1, zw = -1.4, -1.25, 1.4, 0.1, 0.92
    box(x0, y0, 0.12, x1, y1, zw, st["wall"])
    box(x0 - 0.015, y0 - 0.015, 0, x1 + 0.015, y1 + 0.015, 0.16, st["band"])
    box(x0 - 0.012, y0 - 0.012, 0.5, x1 + 0.012, y1 + 0.012, 0.54, st["band"], bevel=0)
    if st.get("frame"):
        frame_beams(x0, y0, x1, y1, 0.54, zw, M)
    if st["roof_kind"] == "flat":
        box(x0 - 0.02, y0 - 0.02, zw, x1 + 0.02, y1 + 0.02, zw + 0.03, st["wall"], bevel=0.004)
        parapet(x0 - 0.02, y0 - 0.02, x1 + 0.02, y1 + 0.02, zw + 0.03, st["wall"])
    else:
        ring_roof(x0, y0, x1, y1, zw, 0.28 if st["roof_kind"] != "low" else 0.15, 0.22,
                  st["roof"] if region != "north" else M["shingle"])
        box(-0.22, (y0 + y1) / 2 - 0.22, zw - 0.02, 0.22, (y0 + y1) / 2 + 0.22, zw + 0.07, M["dark"], bevel=0)
    # loggia (upper colonnade) in the middle of the front
    colm = M["marble"] if region != "egypt" else M["sandstone"]
    box(-0.6, y0 - 0.2, 0.0, 0.6, y0 + 0.01, 0.06, colm, bevel=0.006)
    _portico(-0.55, 0.55, y0 - 0.12, 0.06, 0.42, M, colm, n=4)
    door(0, y0, M, w=0.24, h=0.4, mat=st["door"], frame=st["band"])
    windows_y(y0, [-1.25, -1.0, -0.8, 0.8, 1.0, 1.25], 0.66, 0.8, M, w=0.08)
    windows_y(y0, [-1.1, -0.85, 0.85, 1.1], 0.26, 0.38, M, w=0.08)
    windows_x(x1, [-1.05, -0.75, -0.45, -0.15], 0.66, 0.8, M, w=0.08)
    windows_x(x1, [-0.9, -0.3], 0.26, 0.38, M, w=0.08)
    _peristyle(st, M, -1.35, 0.15, 1.35, 1.45, region, pool=True)
    for x in (-1.4, 1.4):
        tree(st["tree"], x * 0.98, -1.4, M, 0.85)
    tree(st["tree"], 1.3, 1.35, M, 1.0)
    for x in (-0.55, 0.55):
        box(x - 0.07, -1.45, 0, x + 0.07, -1.31, 0.12, M["marble"], bevel=0.01)  # planters
        blob(x, -1.38, 0.18, 0.08, 0.08, 0.07, M["leaf"], segs=8, rings=5)


def slave_quarters(M):
    extra(M)
    lot_rect(3, 2, M["dirt"])
    x0, y0, x1, y1, zw = -1.35, -0.35, 1.35, 0.85, 0.38
    wall = M["plaster_humble2"]
    box(x0, y0, 0, x1, y1, zw, wall)
    gable(x0, y0, x1, y1, zw, 0.22, M["roof_faded"], axis="x", wall=wall)
    for x in (-1.05, -0.5, 0.05, 0.6, 1.1):
        box(x - 0.07, y0 - 0.025, 0, x + 0.07, y0 + 0.005, 0.24, M["dark"], bevel=0)
        windows_y(y0, [x + 0.2], 0.17, 0.25, M, w=0.06)
    windows_x(x1, [-0.05, 0.4], 0.17, 0.25, M, w=0.06)
    # yard: well-trough, washing line, woodpile
    box(-1.0, -0.85, 0, -0.55, -0.65, 0.1, M["stone_dark"], bevel=0.01)
    box(-0.97, -0.82, 0.02, -0.58, -0.68, 0.101, M["water"], bevel=0)
    for x in (0.2, 1.1):
        box(x - 0.015, -0.72, 0, x + 0.015, -0.69, 0.3, M["timber"], bevel=0)
    box(0.2, -0.715, 0.28, 1.1, -0.705, 0.285, M["canvas"], bevel=0)
    for x in (0.35, 0.6, 0.85):
        mesh("cloth", [(x - 0.07, -0.71, 0.28), (x + 0.07, -0.71, 0.28), (x + 0.07, -0.71, 0.16),
                       (x - 0.07, -0.71, 0.16)], [(0, 1, 2, 3)], M["canvas"])
    box(1.15, -0.25, 0, 1.4, -0.1, 0.08, M["timber_light"], bevel=0.005)


# ---------------------------------------------------------------- religion

def temple(M, region="rome"):
    extra(M)
    if region == "egypt":
        return temple_egypt(M)
    if region == "north":
        return temple_north(M)
    east = region == "east"
    lot(3, M["marble_paving"], curb=M["marble"])
    px0, py0, px1, py1, pz = -0.95, -0.55, 0.95, 1.38, 0.36
    if east:  # Greek: low three-step crepidoma, peristyle all round
        pz = 0.18
        steps(-0.98, 0.98, -0.75, py1, pz, 3, M["marble"])
    else:
        box(px0, py0, 0, px1, py1, pz, M["marble"])
        box(px0 - 0.03, py0 - 0.03, pz - 0.04, px1 + 0.03, py1 + 0.03, pz, M["marble"], bevel=0.006)
        steps(-0.72, 0.72, -1.05, py0 + 0.01, pz, 6, M["marble"])
    cz = 1.22 if not east else 1.04
    box(-0.62, 0.05, pz, 0.62, 1.2, cz, M["marble"])
    box(-0.16, 0.03, pz, 0.16, 0.06, pz + 0.55, M["dark"], bevel=0)
    xs = [-0.78 + i * (1.56 / 5) for i in range(6)]
    cy0 = -0.42 if not east else -0.6
    for x in xs:
        column(x, cy0, pz, cz - pz, 0.055, M["marble"])
    for y in (-0.12, 0.2, 0.52, 0.84, 1.16):
        column(-0.78, y, pz, cz - pz, 0.052, M["marble"])
        column(0.78, y, pz, cz - pz, 0.052, M["marble"])
    if east:
        for x in xs[1:-1]:
            column(x, 1.25, pz, cz - pz, 0.052, M["marble"])
    ez = cz + 0.13
    yb = cy0 - 0.08
    box(-0.86, yb, cz, 0.86, 1.32, ez, M["marble"], bevel=0.006)
    frieze = M["blue"] if east else M["gold"]
    box(-0.875, yb - 0.015, ez - 0.07, 0.875, 1.335, ez - 0.03, frieze, bevel=0)
    if east:
        box(-0.875, yb - 0.016, ez - 0.03, 0.875, 1.336, ez - 0.01, M["painted_red"], bevel=0)
    gable(-0.86, yb, 0.86, 1.32, ez, 0.32 if east else 0.36, M["roof_marble"], axis="y", over=0.05,
          wall=M["marble"])
    rz = ez + (0.32 if east else 0.36)
    box(-0.03, yb - 0.05, rz - 0.01, 0.03, 1.37, rz + 0.04, M["gold"], bevel=0.006)
    for x, z in [(0, rz + 0.04), (-0.9, ez + 0.02), (0.9, ez + 0.02)]:
        blob(x, yb - 0.05, z + 0.06, 0.05, 0.03, 0.07, M["gold"], segs=8, rings=4, name="acro")
    blob(0, yb - 0.055, ez + 0.13, 0.08, 0.02, 0.06, M["gold"], segs=8, rings=4)
    # altar with fire on the paving in front
    box(-0.16, -1.38, 0, 0.16, -1.18, 0.16, M["marble"], bevel=0.01)
    box(-0.17, -1.39, 0.15, 0.17, -1.17, 0.18, M["gold"], bevel=0.004)
    blob(0, -1.28, 0.2, 0.06, 0.05, 0.07, M["fire"], segs=8, rings=4)
    if east:
        olive(-1.2, 0.6, M, 0.9)
        olive(1.2, -0.9, M, 0.8)
    else:
        cypress(-1.25, 0.3, 0.55, M)
        cypress(1.25, -0.25, 0.55, M)
        cypress(-1.25, 1.1, 0.55, M)


def temple_egypt(M):
    """Pylon gate, sandstone hall with painted cornice, obelisks, gold capstones."""
    lot(3, M["marble_paving"], curb=M["limestone"])
    s = M["limestone"]
    # pylon: two battered towers flanking the gate
    for sx in (-1, 1):
        x0, x1 = (0.2, 1.15) if sx > 0 else (-1.15, -0.2)
        mesh("pylon", [(x0, -0.75, 0), (x1, -0.75, 0), (x1, -0.45, 0), (x0, -0.45, 0),
                       (x0 + 0.1 * (sx < 0), -0.71, 1.3), (x1 - 0.1 * (sx > 0), -0.71, 1.3),
                       (x1 - 0.1 * (sx > 0), -0.49, 1.3), (x0 + 0.1 * (sx < 0), -0.49, 1.3)],
             [(0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)], s,
             bevel=0.01)
        xa, xb = (x0 + 0.1 * (sx < 0), x1 - 0.1 * (sx > 0))
        box(xa - 0.04, -0.75, 1.3, xb + 0.04, -0.45, 1.38, s, bevel=0.006)
        box(xa - 0.045, -0.765, 1.3, xb + 0.045, -0.745, 1.35, M["gold"], bevel=0)
        box(xa + 0.05, -0.715, 0.25, xb - 0.05, -0.705, 1.0, M["blue"], bevel=0)
        # banner poles
        box((xa + xb) / 2 - 0.015, -0.8, 0, (xa + xb) / 2 + 0.015, -0.77, 1.45, M["timber"], bevel=0)
        mesh("flag", [((xa + xb) / 2, -0.8, 1.42), ((xa + xb) / 2 + 0.2, -0.8, 1.38),
                      ((xa + xb) / 2 + 0.2, -0.8, 1.26), ((xa + xb) / 2, -0.8, 1.3)], [(0, 1, 2, 3)],
             M["painted_red"])
    box(-0.2, -0.72, 0, 0.2, -0.48, 0.85, s, bevel=0.006)
    box(-0.12, -0.74, 0, 0.12, -0.5, 0.58, M["dark"], bevel=0)
    box(-0.24, -0.76, 0.85, 0.24, -0.46, 0.93, M["gold"], bevel=0.004)
    # hall behind
    box(-0.9, -0.45, 0, 0.9, 1.3, 0.62, s)
    box(-0.93, -0.45, 0.62, 0.93, 1.33, 0.68, s, bevel=0.004)
    box(-0.935, -0.455, 0.62, 0.935, 1.335, 0.645, M["gold"], bevel=0)
    for y in (-0.1, 0.4, 0.9):
        column(0.97, y, 0, 0.6, 0.05, s)
    # obelisks in front
    for x in (-0.75, 0.75):
        box(x - 0.09, -1.25, 0, x + 0.09, -1.07, 0.06, s, bevel=0.005)
        mesh("obelisk", [(x - 0.06, -1.22, 0.06), (x + 0.06, -1.22, 0.06), (x + 0.06, -1.1, 0.06),
                         (x - 0.06, -1.1, 0.06), (x - 0.035, -1.195, 1.0), (x + 0.035, -1.195, 1.0),
                         (x + 0.035, -1.125, 1.0), (x - 0.035, -1.125, 1.0), (x, -1.16, 1.1)],
             [(0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7), (4, 5, 8), (5, 6, 8), (6, 7, 8),
              (7, 4, 8)], s)
        mesh("cap", [(x - 0.035, -1.195, 1.0), (x + 0.035, -1.195, 1.0), (x + 0.035, -1.125, 1.0),
                     (x - 0.035, -1.125, 1.0), (x, -1.16, 1.1)],
             [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)], M["gold"])
    palm(-1.25, 0.6, 0.75, M)
    palm(1.25, 1.15, 0.7, M)
    box(-0.16, -1.42, 0, 0.16, -1.25, 0.14, s, bevel=0.01)
    box(-0.17, -1.43, 0.13, 0.17, -1.24, 0.16, M["gold"], bevel=0.004)


def temple_north(M):
    """Romano-Celtic temple: square tower-cella in an ambulatory, limestone and gold, tiled roofs."""
    lot(3, M["paving"], curb=M["limestone"])
    L = M["limestone"]
    box(-1.1, -1.1, 0, 1.1, 1.1, 0.12, L, bevel=0.01)
    steps(-0.35, 0.35, -1.35, -1.09, 0.12, 3, L)
    # ambulatory: low wall with columns, lean-to roof all round
    z = 0.12
    for i in range(6):
        t = -0.95 + i * 0.38
        for (x, y) in [(t, -0.95), (t, 0.95), (-0.95, t), (0.95, t)]:
            column(x, y, z, 0.38, 0.035, L)
    hip(-1.05, -1.05, 1.05, 1.05, z + 0.38, 0.28, M["roof_marble"], over=0.04)
    box(-1.07, -1.07, z + 0.36, 1.07, 1.07, z + 0.39, M["gold"], bevel=0)
    # tall cella rising through the roof
    box(-0.5, -0.5, z, 0.5, 0.5, 1.25, L)
    box(-0.52, -0.52, 1.18, 0.52, 0.52, 1.25, M["gold"], bevel=0.004)
    windows_y(-0.5, [-0.25, 0.25], 0.95, 1.08, M, w=0.07)
    windows_x(0.5, [-0.25, 0.25], 0.95, 1.08, M, w=0.07)
    hip(-0.52, -0.52, 0.52, 0.52, 1.25, 0.36, M["roof_marble"], over=0.05)
    blob(0, 0, 1.66, 0.05, 0.05, 0.07, M["gold"], segs=8, rings=4)
    box(-0.12, -1.0, z, 0.12, -0.96, 0.42, M["dark"], bevel=0)
    pine_tree(-1.3, 0.5, 0.9, M)
    pine_tree(1.3, 1.25, 0.8, M)
    box(0.85, -1.38, 0, 1.1, -1.18, 0.15, L, bevel=0.01)
    blob(0.975, -1.28, 0.2, 0.06, 0.05, 0.07, M["fire"], segs=8, rings=4)


def great_temple(M):
    """5x5: octastyle temple on a high podium inside a colonnaded precinct."""
    extra(M)
    lot(5, M["marble_paving"], curb=M["marble"])
    # precinct colonnade on the back and sides
    for i in range(11):
        t = -2.3 + i * 0.46
        column(t, 2.3, 0, 0.5, 0.04, M["marble"])
        if i < 10:
            column(-2.3, t, 0, 0.5, 0.04, M["marble"])
            column(2.3, t, 0, 0.5, 0.04, M["marble"])
    box(-2.38, 2.2, 0.5, 2.38, 2.4, 0.56, M["marble"], bevel=0.005)
    box(-2.4, -2.0, 0.5, -2.2, 2.4, 0.56, M["marble"], bevel=0.005)
    box(2.2, -2.0, 0.5, 2.4, 2.4, 0.56, M["marble"], bevel=0.005)
    gable(-2.4, 2.2, 2.4, 2.48, 0.56, 0.1, M["roof_marble"], axis="x", over=0.02)
    # podium
    px0, py0, px1, py1, pz = -1.35, -0.7, 1.35, 1.95, 0.6
    box(px0, py0, 0, px1, py1, pz, M["marble"])
    box(px0 - 0.04, py0 - 0.04, pz - 0.06, px1 + 0.04, py1 + 0.04, pz, M["marble"], bevel=0.006)
    steps(-1.05, 1.05, -1.45, py0 + 0.01, pz, 9, M["marble"])
    cz = 1.85
    box(-0.95, 0.25, pz, 0.95, 1.85, cz, M["marble"])
    box(-0.22, 0.23, pz, 0.22, 0.26, pz + 0.85, M["dark"], bevel=0)
    xs = [-1.15 + i * (2.3 / 7) for i in range(8)]
    for x in xs:
        column(x, -0.52, pz, cz - pz, 0.07, M["marble"])
        column(x, -0.15, pz, cz - pz, 0.07, M["marble"])
    for y in (0.25, 0.7, 1.15, 1.6):
        column(-1.15, y, pz, cz - pz, 0.065, M["marble"])
        column(1.15, y, pz, cz - pz, 0.065, M["marble"])
    ez = cz + 0.18
    box(-1.25, -0.62, cz, 1.25, 1.9, ez, M["marble"], bevel=0.006)
    box(-1.265, -0.635, ez - 0.1, 1.265, 1.915, ez - 0.04, M["gold"], bevel=0)
    gable(-1.25, -0.62, 1.25, 1.9, ez, 0.5, M["roof_marble"], axis="y", over=0.06, wall=M["marble"])
    box(-0.04, -0.7, ez + 0.48, 0.04, 1.97, ez + 0.55, M["gold"], bevel=0.006)
    for x, z in [(0, ez + 0.56), (-1.3, ez + 0.03), (1.3, ez + 0.03)]:
        blob(x, -0.7, z + 0.08, 0.07, 0.04, 0.1, M["gold"], segs=8, rings=4)
    blob(0, -0.69, ez + 0.18, 0.12, 0.03, 0.09, M["gold"], segs=8, rings=4)
    # great altar
    box(-0.4, -2.2, 0, 0.4, -1.8, 0.24, M["marble"], bevel=0.01)
    box(-0.42, -2.22, 0.23, 0.42, -1.78, 0.27, M["gold"], bevel=0.004)
    for x in (-0.2, 0.2):
        blob(x, -2.0, 0.3, 0.07, 0.06, 0.09, M["fire"], segs=8, rings=4)
    for x, y in [(-1.9, -1.9), (1.9, -1.9)]:
        box(x - 0.12, y - 0.12, 0, x + 0.12, y + 0.12, 0.4, M["marble"], bevel=0.008)
        _figure(x, y, 0.4, M, M["gold"], 0.5)
    for x, y in [(-1.85, 0.6), (1.85, 0.0), (-1.85, 1.5), (1.85, 1.3)]:
        cypress(x, y, 0.7, M)


def shrine(M, region="rome"):
    extra(M)
    if region == "egypt":
        lot(1, M["sand"])
        s = M["sandstone"]
        box(-0.28, -0.1, 0, 0.28, 0.35, 0.42, s)
        box(-0.31, -0.13, 0.42, 0.31, 0.38, 0.48, M["ochre_paint"], bevel=0.004)
        box(-0.1, -0.12, 0, 0.1, -0.09, 0.28, M["dark"], bevel=0)
        box(-0.1, -0.13, 0.28, 0.1, -0.1, 0.32, M["gold"], bevel=0)
        box(-0.36, -0.38, 0, -0.26, -0.28, 0.05, s, bevel=0.004)
        mesh("ob", [(-0.34, -0.36, 0.05), (-0.28, -0.36, 0.05), (-0.28, -0.3, 0.05), (-0.34, -0.3, 0.05),
                    (-0.31, -0.33, 0.62)], [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)], s)
        palm(0.32, 0.3, 0.55, M)
        return
    if region == "north":
        lot(1, M["grass_north"])
        box(-0.25, 0.0, 0, 0.25, 0.35, 0.36, M["timber"])
        gable(-0.25, 0.0, 0.25, 0.35, 0.36, 0.2, M["shingle"], axis="y", wall=M["timber"])
        box(-0.08, -0.02, 0, 0.08, 0.01, 0.24, M["dark"], bevel=0)
        box(-0.1, -0.35, 0, 0.1, -0.2, 0.14, M["limestone"], bevel=0.01)
        blob(0, -0.275, 0.17, 0.04, 0.035, 0.05, M["fire"], segs=6, rings=4)
        pine_tree(0.35, 0.35, 0.6, M)
        return
    east = region == "east"
    lot(1, M["marble_paving"])
    box(-0.3, -0.15, 0, 0.3, 0.38, 0.14, M["marble"], bevel=0.008)
    box(-0.22, 0.12, 0.14, 0.22, 0.36, 0.5, M["marble"])
    box(-0.07, 0.1, 0.14, 0.07, 0.13, 0.36, M["dark"], bevel=0)
    for x in (-0.2, 0.2):
        column(x, -0.06, 0.14, 0.36, 0.03, M["marble"])
    box(-0.26, -0.11, 0.5, 0.26, 0.38, 0.55, M["marble"], bevel=0.004)
    box(-0.265, -0.115, 0.52, 0.265, 0.385, 0.54, M["blue"] if east else M["gold"], bevel=0)
    gable(-0.26, -0.11, 0.26, 0.38, 0.55, 0.14, M["roof_marble"], axis="y", over=0.03, wall=M["marble"])
    blob(0, -0.13, 0.7, 0.03, 0.02, 0.04, M["gold"], segs=6, rings=4)
    box(-0.08, -0.42, 0, 0.08, -0.3, 0.12, M["marble"], bevel=0.008)
    blob(0, -0.36, 0.15, 0.035, 0.03, 0.045, M["fire"], segs=6, rings=4)
    if east:
        olive(0.32, 0.32, M, 0.6)
    else:
        cypress(0.36, 0.3, 0.42, M)


# ---------------------------------------------------------------- decor

def _figure(x, y, z, M, mat, s=1.0):
    """Standing figure in a toga: body, head, raised arm."""
    blob(x, y, z + 0.16 * s, 0.07 * s, 0.06 * s, 0.17 * s, mat, segs=10, rings=6)
    blob(x, y, z + 0.37 * s, 0.04 * s, 0.04 * s, 0.045 * s, mat, segs=8, rings=5)
    mesh("arm", [(x + 0.05 * s, y - 0.01, z + 0.27 * s), (x + 0.07 * s, y - 0.01, z + 0.27 * s),
                 (x + 0.13 * s, y - 0.03, z + 0.45 * s), (x + 0.11 * s, y - 0.03, z + 0.46 * s)],
         [(0, 1, 2, 3)], mat, solidify=0.025 * s)


def statue(M):
    extra(M)
    lot(1, M["paving"])
    box(-0.3, -0.3, 0, 0.3, 0.3, 0.04, M["travertine"], bevel=0.006)
    box(-0.15, -0.15, 0.04, 0.15, 0.15, 0.38, M["marble"], bevel=0.01)
    box(-0.17, -0.17, 0.34, 0.17, 0.17, 0.4, M["marble"], bevel=0.006)
    box(-0.155, -0.16, 0.16, 0.155, -0.15, 0.24, M["gold"], bevel=0)  # inscription plate
    _figure(0, 0, 0.4, M, M["bronze"], 1.0)


def garden_tile(M, v):
    """1x1 public garden: grass, a gravel path crossing, low beds; kept flat to sit as ground."""
    extra(M)
    ob = lot_rect(1, 1, M["garden"])
    kit.offset_uvs(ob, v % 2, -(v // 2))
    g = M["dust"]
    if v in (0, 2):
        mesh("path", [(-0.5, -0.07, 0.004), (0.5, -0.07, 0.004), (0.5, 0.07, 0.004), (-0.5, 0.07, 0.004)],
             [(0, 1, 2, 3)], g)
    if v in (1, 2):
        mesh("path2", [(-0.07, -0.5, 0.005), (0.07, -0.5, 0.005), (0.07, 0.5, 0.005), (-0.07, 0.5, 0.005)],
             [(0, 1, 2, 3)], g)
    flowers = [M["flower_r"], M["flower_y"], M["flower_p"]]
    if v == 0:
        for k, (x, y) in enumerate([(-0.28, -0.28), (0.28, -0.28), (-0.28, 0.28), (0.28, 0.28)]):
            box(x - 0.14, y - 0.12, 0, x + 0.14, y + 0.12, 0.04, M["hedge"], bevel=0.015)
            blob(x, y, 0.06, 0.08, 0.06, 0.03, flowers[k % 3], segs=6, rings=4)
    elif v == 1:
        for y in (-0.3, 0.0, 0.3):
            for x in (-0.3, 0.3):
                blob(x, y, 0.05, 0.1, 0.07, 0.05, M["hedge"], segs=8, rings=5)
                blob(x, y, 0.09, 0.05, 0.04, 0.025, flowers[int((y + 0.3) * 5) % 3], segs=6, rings=4)
    elif v == 2:
        box(-0.12, -0.12, 0, 0.12, 0.12, 0.05, M["marble"], bevel=0.008)
        box(-0.09, -0.09, 0.01, 0.09, 0.09, 0.052, M["water"], bevel=0)
        for x, y in [(-0.33, -0.33), (0.33, 0.33)]:
            blob(x, y, 0.06, 0.1, 0.1, 0.06, M["hedge"], segs=8, rings=5)
        for x, y in [(0.33, -0.33), (-0.33, 0.33)]:
            blob(x, y, 0.05, 0.08, 0.08, 0.04, M["flower_y"], segs=8, rings=5)
    else:
        for i in range(5):
            y = -0.4 + i * 0.2
            box(-0.4, y - 0.05, 0, 0.4, y + 0.05, 0.035, M["soil"], bevel=0.01)
            for j in range(5):
                blob(-0.32 + j * 0.16, y, 0.05, 0.05, 0.035, 0.03, flowers[(i + j) % 3], segs=6, rings=4)


def plaza_tile(M, v):
    extra(M)
    ob = lot_rect(1, 1, M["paving"])
    kit.offset_uvs(ob, (v % 2) * 0.5, -(v // 2) * 0.5)
    if v == 1:  # inset marble square
        box(-0.2, -0.2, 0, 0.2, 0.2, 0.006, M["marble_paving"], bevel=0)
    elif v == 2:  # drain cover / worn patch
        box(-0.06, -0.06, 0, 0.06, 0.06, 0.005, M["stone_dark"], bevel=0)
    elif v == 3:
        box(-0.42, -0.02, 0, 0.42, 0.02, 0.004, M["travertine"], bevel=0)


# ---------------------------------------------------------------- sprite table

def _reg(fn, region):
    return lambda M: fn(M, region)


def _sprites():
    S = {}
    suffix = {"rome": "", "east": "@east", "egypt": "@egypt", "north": "@north"}
    S["bld/house_plot"] = dict(fp=(2, 2), build=plot_empty)
    S["bld/domus_plot"] = dict(fp=(3, 3), build=domus_empty)
    tiers = [plot_tent, plot_hut, plot_small_house, plot_two_storey, plot_town_house]
    vs = lambda v: "_v%d" % v if v else ""  # noqa: E731  variant 0 keeps the plain name
    for t, fn in enumerate(tiers):
        for v in range(VARIANTS):
            for r in REGIONS:
                S["bld/house_plot_t%d%s%s" % (t, vs(v), suffix[r])] = dict(
                    fp=(2, 2), build=(lambda fn, r, v: lambda M: fn(M, r, v))(fn, r, v),
                    lit=(r == "rome" and t >= 2))
    for t, fn in enumerate([domus_atrium, domus_peristyle, domus_villa]):
        for v in range(VARIANTS):
            for r in REGIONS:
                build = _reg(fn, r) if v == 0 else (lambda fn, r, v: lambda M: domus_var(fn, M, r, v))(fn, r, v)
                S["bld/domus_plot_t%d%s%s" % (t, vs(v), suffix[r])] = dict(fp=(3, 3), build=build,
                                                                          lit=(r == "rome" and t == 1))
    S["bld/slave_quarters"] = dict(fp=(3, 2), build=slave_quarters)
    for r in REGIONS:
        S["bld/temple" + suffix[r]] = dict(fp=(3, 3), build=_reg(temple, r))
        S["bld/shrine" + suffix[r]] = dict(fp=(1, 1), build=_reg(shrine, r))
    S["bld/great_temple"] = dict(fp=(5, 5), build=great_temple)
    S["bld/statue"] = dict(fp=(1, 1), build=statue)
    for v in range(4):
        S["bld/garden_tile_%d" % v] = dict(fp=(1, 1), build=(lambda v: lambda M: garden_tile(M, v))(v))
        S["bld/plaza_tile_%d" % v] = dict(fp=(1, 1), fixed_h=64, build=(lambda v: lambda M: plaza_tile(M, v))(v))
    return S


SPRITES = _sprites()
