"""Iso sprites: storage, trade, farms, water and infrastructure (family "logistics").

    Blender -b --python tools/artgen3d/run.py -- logistics [names] --samples 48

Conventions follow docs/13-isometric.md.  Painted pieces (farm tiles, stockpile tiles, line
pieces and farm huts) are drawn over the terrain, so they carry no lot of their own: huts and
line pieces cast their shadow through a shadow-catcher plane.  Line pieces are a centre (_c)
plus one arm per joining neighbour (_n/_e/_s/_w = grid directions); an arm is rendered with the
centre held out, so a back arm never paints over the pier in front of it.
"""
import math

import bpy

import kit
from kit import blob, box, cart, column, cylinder, cypress, fence, gable, hip, lot_rect, mesh
from kit import material, olive, sack, amphora, windows_x, windows_y

REGIONS = ("east", "egypt", "north")


# ---------------------------------------------------------------- materials and helpers

def mats(M):
    M["stone_grey"] = material("stone_grey", "#bdb6a8", "#a7a093", kind="brick", scale=12,
                               brick=(0.24, 0.11), mortar="#868073", bump=0.3)
    M["terracotta"] = material("terracotta", "#b8673f", "#9d5433", scale=14, bump=0.1)
    M["water_deep"] = material("water_deep", "#3f86ad", "#2f7099", scale=5, rough=0.08, bump=0.03)
    M["water_dark"] = material("water_dark", "#3d5a4f", "#2f4a40", scale=6, rough=0.15, bump=0.02)
    M["deck"] = material("deck", "#9a7350", "#7f5c3e", kind="brick", scale=10, brick=(0.9, 0.07),
                         mortar="#5a3f2a", bump=0.25)
    M["straw"] = material("straw", "#d8bb6a", "#c2a252", scale=22, bump=0.4)
    M["thatch"] = material("thatch", "#a98a4e", "#8e7240", kind="tiles", scale=7,
                           brick=(0.05, 0.05), bump=0.6)
    M["shingle"] = material("shingle", "#6d5a48", "#574737", kind="tiles", scale=7,
                            brick=(0.07, 0.06), bump=0.5)
    M["stucco"] = material("stucco", "#f2efe6", "#e2ddd0", scale=9, bump=0.06)
    M["blue"] = material("blue", "#3f6fa8", "#335d90", scale=12, bump=0.05)
    M["mudbrick"] = material("mudbrick", "#c49a68", "#ad8455", kind="brick", scale=12,
                             brick=(0.16, 0.07), mortar="#9a7a52", bump=0.2)
    M["sandstone"] = material("sandstone", "#dcc38f", "#c8ad78", kind="brick", scale=12,
                              brick=(0.3, 0.12), mortar="#a8915f", bump=0.25)
    M["sand_paving"] = material("sand_paving", "#d8c49a", "#c4ae84", kind="brick", scale=10,
                                brick=(0.25, 0.2), mortar="#a99470", bump=0.25)
    M["palm"] = material("palm", "#5f8a35", "#4a7228", scale=18, bump=0.3)
    M["wheat"] = material("wheat", "#d4b45c", "#ae8f45", scale=90, bump=1.0,
                          detail=(3.0, "#b59a52"))
    M["wheat_ear"] = material("wheat_ear", "#d8b862", "#bf9d4a", scale=60, bump=0.9)
    M["wheat_green"] = material("wheat_green", "#b8b04e", "#9a9a3e", scale=40, bump=0.8)
    M["cabbage"] = material("cabbage", "#7fae4a", "#5f9038", scale=25, bump=0.4)
    M["leek"] = material("leek", "#9cc46a", "#7ea852", scale=25, bump=0.3)
    M["vine"] = material("vine", "#5e8a32", "#4a7428", scale=22, bump=0.5)
    M["grape"] = material("grape", "#5a2c5e", "#432048", scale=30, rough=0.4, bump=0.1)
    M["mud"] = material("mud", "#7a5b3c", "#654a30", scale=10, rough=0.5, bump=0.35)
    M["pasture"] = material("pasture", "#7d9a45", "#6c8a3a", scale=11, bump=0.25,
                            detail=(0.9, "#a3a66c"))
    M["dry_grass"] = material("dry_grass", "#9c9a58", "#878447", scale=11, bump=0.25,
                              detail=(0.9, "#b5a874"))
    M["wool"] = material("wool", "#f1ece0", "#ddd5c4", scale=30, bump=0.6)
    M["sheep_face"] = material("sheep_face", "#3a302a", "#2b231e", scale=20, bump=0.1)
    M["pig"] = material("pig", "#e2a49a", "#cf8c82", scale=20, rough=0.6, bump=0.05)
    M["canopy_red"] = material("canopy_red", "#c0392b", "#f3e6cc", kind="stripes", stripe=0.09,
                               bump=0.0, rough=0.9)
    M["canopy_blue"] = material("canopy_blue", "#2f6aa3", "#f3e6cc", kind="stripes", stripe=0.09,
                                bump=0.0, rough=0.9)
    M["canopy_green"] = material("canopy_green", "#3f8a4a", "#f3e6cc", kind="stripes",
                                 stripe=0.09, bump=0.0, rough=0.9)
    M["cloth_red"] = material("cloth_red", "#a8322a", "#8c2822", scale=20, bump=0.1)
    M["cloth_purple"] = material("cloth_purple", "#6a2c6e", "#552258", scale=20, bump=0.1)
    M["cloth_blue"] = material("cloth_blue", "#2e5a8c", "#244a76", scale=20, bump=0.1)
    M["fruit_red"] = material("fruit_red", "#c24a2e", "#a63a24", scale=30, rough=0.5, bump=0.1)
    M["fruit_green"] = material("fruit_green", "#8fb04a", "#76963a", scale=30, rough=0.5, bump=0.1)
    M["fish"] = material("fish", "#a9b4bc", "#8b98a2", scale=30, rough=0.3, metal=0.3, bump=0.1)
    M["crate"] = material("crate", "#a07a50", "#8a6640", kind="brick", scale=10,
                          brick=(0.5, 0.06), mortar="#6b4f33", bump=0.2)
    M["log"] = material("log", "#7a5a3a", "#634830", scale=20, bump=0.3)
    M["block"] = material("block", "#d6d0c2", "#c0b9aa", scale=12, bump=0.15)
    M["lead"] = material("lead", "#7d8288", "#6a6f75", scale=16, rough=0.5, metal=0.4, bump=0.05)
    M["sail"] = material("sail", "#efe5cf", "#ddd0b5", scale=8, bump=0.05)
    M["hull"] = material("hull", "#6a4a30", "#55391f", kind="brick", scale=10, brick=(0.9, 0.05),
                         mortar="#3f2a18", bump=0.2)
    return M


def holdout():
    m = bpy.data.materials.get("holdout_centre")
    if m is None:
        m = bpy.data.materials.new("holdout_centre")
        nt = kit._nodes(m)
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        h = nt.nodes.new("ShaderNodeHoldout")
        nt.links.new(h.outputs[0], out.inputs["Surface"])
    return m


def hold_out_since(first_index):
    """Turn every object created after `first_index` into an invisible, shadowless occluder."""
    hm = holdout()
    for ob in list(bpy.context.scene.objects)[first_index:]:
        if ob.type != "MESH":
            continue
        for slot in ob.material_slots:
            slot.material = hm
        ob.visible_shadow = False


def catcher(fw=1, fh=1, name="catcher"):
    """Invisible ground that only shows the shadows cast on it."""
    hx, hy = fw / 2, fh / 2
    ob = mesh(name, [(-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0)], [(0, 1, 2, 3)],
              material("catcher", "#808080", kind="flat", bump=0))
    ob.is_shadow_catcher = True
    return ob


def lamp(x, y, z, energy=40, colour=(1.0, 0.6, 0.25)):
    l = bpy.data.lights.new("lamp", "POINT")
    l.energy = energy
    l.color = colour
    l.shadow_soft_size = 0.15
    o = bpy.data.objects.new("lamp", l)
    o.location = (x, y, z)
    bpy.context.scene.collection.objects.link(o)


def crate(x, y, z, M, s=0.12):
    box(x - s / 2, y - s / 2, z, x + s / 2, y + s / 2, z + s, M["crate"], bevel=0.006)


def barrel(x, y, M, s=1.0):
    cylinder(x, y, 0, 0.16 * s, 0.065 * s, M["timber_light"], n=12)
    for z in (0.03, 0.13):
        cylinder(x, y, z * s, z * s + 0.012, 0.068 * s, M["iron"], n=12)


def palm(x, y, M, h=0.7):
    for i in range(5):
        t = i / 4
        cylinder(x + 0.04 * t, y + 0.02 * t, h * t * 0.95, h * (t + 0.26) * 0.95, 0.03, M["trunk"], n=6)
    for k in range(6):
        a = 2 * math.pi * k / 6
        dx, dy = math.cos(a), math.sin(a)
        mesh("frond", [(x + 0.04, y + 0.02, h), (x + 0.04 + dx * 0.3 - dy * 0.05, y + 0.02 + dy * 0.3 + dx * 0.05, h - 0.12),
                       (x + 0.04 + dx * 0.34, y + 0.02 + dy * 0.34, h - 0.17),
                       (x + 0.04 + dx * 0.3 + dy * 0.05, y + 0.02 + dy * 0.3 - dx * 0.05, h - 0.12)],
             [(0, 1, 2, 3)], M["palm"], solidify=0.01)


def flat_roof(x0, y0, x1, y1, z, M, mat, parapet=0.05):
    box(x0 - 0.03, y0 - 0.03, z, x1 + 0.03, y1 + 0.03, z + 0.05, mat, bevel=0.005)
    t = 0.035
    for a in ((x0 - 0.03, y0 - 0.03, x1 + 0.03, y0 - 0.03 + t), (x0 - 0.03, y1 + 0.03 - t, x1 + 0.03, y1 + 0.03),
              (x0 - 0.03, y0 - 0.03, x0 - 0.03 + t, y1 + 0.03), (x1 + 0.03 - t, y0 - 0.03, x1 + 0.03, y1 + 0.03)):
        box(a[0], a[1], z + 0.05, a[2], a[3], z + 0.05 + parapet, mat, bevel=0.003)


class Style:
    """Regional look: wall, roof kind and material, yard paving, a tree."""

    def __init__(self, M, region=None):
        self.region = region
        self.wall, self.stone, self.ground = M["plaster"], M["travertine"], M["paving"]
        self.roofmat, self.roof_kind, self.trim = M["roof_timber"], "tile", M["timber"]
        if region == "east":
            self.wall, self.stone, self.roof_kind = M["stucco"], M["stucco"], "flat"
            self.roofmat, self.trim = M["stucco"], M["blue"]
        elif region == "egypt":
            self.wall, self.stone, self.ground = M["mudbrick"], M["sandstone"], M["sand_paving"]
            self.roof_kind, self.roofmat, self.trim = "flat", M["mudbrick"], M["sandstone"]
        elif region == "north":
            self.wall, self.stone, self.ground = M["plaster_humble"], M["stone_dark"], M["dirt"]
            self.roof_kind, self.roofmat = "thatch", M["thatch"]

    def roof(self, x0, y0, x1, y1, z, h, axis="x", wall=None, over=0.07):
        if self.roof_kind == "flat":
            flat_roof(x0, y0, x1, y1, z, None, self.roofmat)
        elif self.roof_kind == "thatch":
            gable(x0, y0, x1, y1, z, h * 1.45, self.roofmat, axis=axis, over=over + 0.05,
                  wall=wall or self.wall)
        else:
            gable(x0, y0, x1, y1, z, h, self.roofmat, axis=axis, over=over, wall=wall or self.wall)

    def tree(self, x, y, M, s=1.0):
        if self.region == "egypt":
            palm(x, y, M, 0.75 * s)
        elif self.region == "north":
            cylinder(x, y, 0, 0.1, 0.03, M["trunk"], n=6)
            blob(x, y, 0.32 * s, 0.17 * s, 0.17 * s, 0.3 * s, M["cypress"], segs=10, rings=8)
        elif self.region == "east":
            cypress(x, y, 0.5 * s, M)
        else:
            olive(x, y, M, 0.85 * s)


def timber_frame(x0, y0, x1, y1, z0, z1, M):
    """North: dark posts and a rail on the two visible faces."""
    for x in (x0, (x0 + x1) / 2, x1):
        box(x - 0.025, y0 - 0.02, z0, x + 0.025, y0 + 0.005, z1, M["timber"], bevel=0)
    for y in (y0, (y0 + y1) / 2, y1):
        box(x1 - 0.005, y - 0.025, z0, x1 + 0.02, y + 0.025, z1, M["timber"], bevel=0)
    box(x0, y0 - 0.02, (z0 + z1) / 2 - 0.02, x1, y0 + 0.005, (z0 + z1) / 2 + 0.02, M["timber"], bevel=0)
    box(x1 - 0.005, y0, (z0 + z1) / 2 - 0.02, x1 + 0.02, y1, (z0 + z1) / 2 + 0.02, M["timber"], bevel=0)


def walls(S, M, x0, y0, x1, y1, z0, z1, mat=None):
    box(x0, y0, z0, x1, y1, z1, mat or S.wall)
    if S.region == "north" and mat is None:
        timber_frame(x0, y0, x1, y1, z0, z1, M)


def double_door(xc, y, z0, h, M, w=0.3):
    box(xc - w, y - 0.045, z0, xc - 0.01, y + 0.01, z0 + h, M["timber"], bevel=0.004)
    box(xc + 0.01, y - 0.045, z0, xc + w, y + 0.01, z0 + h, M["timber"], bevel=0.004)


# ---------------------------------------------------------------- storage

def granary(M, region=None):
    S = Style(M, region)
    lot_rect(3, 3, S.ground, curb=S.stone)
    x0, y0, x1, y1 = -1.38, -0.3, 1.38, 1.36
    box(x0 - 0.04, y0 - 0.04, 0, x1 + 0.04, y1 + 0.04, 0.16, M["stone_dark"])
    walls(S, M, x0, y0, x1, y1, 0.16, 0.86, mat=S.stone if region != "north" else None)
    for i in range(7):
        bx = x0 + 0.2 + i * (x1 - x0 - 0.4) / 6
        if abs(bx) < 0.3:
            continue
        box(bx - 0.06, y0 - 0.1, 0.16, bx + 0.06, y0, 0.74, S.stone, bevel=0.01)
    for j in range(4):
        by = y0 + 0.25 + j * (y1 - y0 - 0.5) / 3
        box(x1, by - 0.06, 0.16, x1 + 0.1, by + 0.06, 0.74, S.stone, bevel=0.01)
    if S.roof_kind == "flat":
        flat_roof(x0, y0, x1, y1, 0.86, M, S.roofmat)
        # grain domes on the roof (eastern/egyptian granaries)
        for x in (-0.8, 0.0, 0.8):
            blob(x, 0.6, 0.91, 0.28, 0.28, 0.3, S.stone, segs=14, rings=8, top_only=True)
    else:
        S.roofmat = M["roof_timber"] if region is None else S.roofmat
        S.roof(x0, y0, x1, y1, 0.86, 0.42, axis="x", wall=S.stone, over=0.09)
    windows_y(y0, [-1.05, -0.65, 0.65, 1.05], 0.62, 0.72, M, w=0.05)
    windows_x(x1, [0.0, 0.55, 1.05], 0.62, 0.72, M, w=0.05)
    box(-0.32, y0 - 0.03, 0.16, 0.32, y0 + 0.01, 0.66, S.stone, bevel=0.004)
    double_door(0, y0, 0.16, 0.44, M, w=0.26)
    box(-0.4, y0 - 0.32, 0, 0.4, y0, 0.16, M["stone_dark"], bevel=0.008)
    cart(0.75, -0.95, M)
    for dx, dy, dz in [(0, 0, 0), (0.12, 0, 0), (0.24, 0, 0), (0.06, 0.1, 0), (0.18, 0.1, 0),
                       (0.06, 0.04, 0.09), (0.18, 0.04, 0.09), (0.12, 0.08, 0.17)]:
        sack(-1.2 + dx, -1.2 + dy, dz, M)
    for i in range(3):
        sack(-0.55 + i * 0.12, -0.75, 0, M)
    if region:
        S.tree(-1.25, -0.55, M, 0.8)


def warehouse(M, region=None):
    """Two-storey store with a hoist beam, crates, barrels and amphorae in the yard."""
    S = Style(M, region)
    lot_rect(3, 3, S.ground, curb=S.stone)
    x0, y0, x1, y1, zw = -1.35, 0.1, 0.55, 1.38, 0.95
    box(x0 - 0.03, y0 - 0.03, 0, x1 + 0.03, y1 + 0.03, 0.12, M["stone_dark"])
    walls(S, M, x0, y0, x1, y1, 0.12, zw, mat=S.stone if region != "north" else None)
    box(x0 - 0.01, y0 - 0.02, 0.5, x1 + 0.02, y1 + 0.01, 0.54, M["timber"], bevel=0)  # floor beam
    if S.roof_kind == "tile":
        hip(x0, y0, x1, y1, zw, 0.32, M["roof_timber"], over=0.08)
    else:
        S.roof(x0, y0, x1, y1, zw, 0.32, axis="x", wall=S.stone)
    # big doors below, loft door and hoist beam above
    double_door(-0.4, y0, 0.12, 0.36, M, w=0.24)
    box(-0.52, y0 - 0.03, 0.6, -0.28, y0 + 0.01, 0.84, M["timber"], bevel=0.004)
    box(-0.42, y0 - 0.35, 0.86, -0.38, y0 + 0.02, 0.9, M["timber"], bevel=0)
    cylinder(-0.4, y0 - 0.33, 0.55, 0.86, 0.006, M["dark"], n=6, smooth=False)
    crate(-0.4, y0 - 0.33, 0.43, M, 0.11)
    windows_y(y0, [0.1, 0.35], 0.66, 0.8, M, w=0.06)
    windows_x(x1, [0.35, 0.7, 1.05], 0.66, 0.8, M, w=0.06)
    windows_x(x1, [0.7], 0.25, 0.4, M, w=0.06)
    # open timber shed on the east side
    sx0, sx1, sy0, sy1 = 0.7, 1.4, 0.15, 1.35
    for x, y in [(sx0, sy0), (sx1, sy0), (sx1, sy1), (sx0, sy1), (sx1, (sy0 + sy1) / 2)]:
        box(x - 0.03, y - 0.03, 0, x + 0.03, y + 0.03, 0.5, M["timber"], bevel=0.004)
    mesh("shed", [(sx0 - 0.05, sy0 - 0.08, 0.6), (sx1 + 0.08, sy0 - 0.08, 0.5),
                  (sx1 + 0.08, sy1 + 0.05, 0.5), (sx0 - 0.05, sy1 + 0.05, 0.6)], [(0, 1, 2, 3)],
         M["roof_timber"] if S.roof_kind != "thatch" else M["thatch"], solidify=0.03)
    for i in range(3):
        for j in range(3):
            barrel(sx0 + 0.15 + i * 0.17, sy0 + 0.2 + j * 0.17, M)
    for i in range(3):
        for j in range(2):
            crate(sx0 + 0.18 + i * 0.15, sy1 - 0.45 + j * 0.15, 0, M)
    crate(sx0 + 0.25, sy1 - 0.37, 0.12, M)
    # yard: crate stacks, amphora rows, a cart
    for i, (x, y, z) in enumerate([(-1.15, -0.4, 0), (-1.02, -0.4, 0), (-0.89, -0.4, 0),
                                   (-1.15, -0.27, 0), (-1.02, -0.27, 0), (-1.08, -0.34, 0.12),
                                   (-0.95, -0.34, 0.12)]):
        crate(x, y, z, M)
    for i in range(5):
        for j in range(2):
            amphora(-1.2 + i * 0.1, -1.0 + j * 0.11, M)
    cart(0.55, -0.75, M, load=False)
    for dx, dy in [(-0.12, 0), (0, 0), (0.12, 0)]:
        crate(0.55 + dx, -0.75 + dy, 0.13, M, 0.1)
    if region:
        S.tree(1.2, -1.2, M, 0.8)


def market(M, region=None):
    """Macellum: a portico behind a square of striped stalls with produce."""
    S = Style(M, region)
    lot_rect(3, 3, S.ground, curb=S.stone)
    # back portico
    x0, x1, y0, y1, zc = -1.42, 1.42, 0.85, 1.42, 0.62
    box(x0, y1 - 0.12, 0, x1, y1, zc, S.wall)
    box(x0, y0, 0, x0 + 0.12, y1, zc, S.wall)
    for i in range(7):
        column(x0 + 0.3 + i * 0.4, y0 + 0.03, 0, zc, 0.04, S.stone)
    if S.roof_kind == "flat":
        flat_roof(x0, y0 - 0.04, x1, y1, zc, M, S.roofmat)
    elif S.roof_kind == "thatch":
        mesh("lean", [(x0 - 0.05, y0 - 0.12, zc), (x1 + 0.05, y0 - 0.12, zc),
                      (x1 + 0.05, y1 + 0.05, zc + 0.35), (x0 - 0.05, y1 + 0.05, zc + 0.35)],
             [(0, 1, 2, 3)], M["thatch"], solidify=0.04)
    else:
        mesh("lean", [(x0 - 0.05, y0 - 0.12, zc), (x1 + 0.05, y0 - 0.12, zc),
                      (x1 + 0.05, y1 + 0.05, zc + 0.25), (x0 - 0.05, y1 + 0.05, zc + 0.25)],
             [(0, 1, 2, 3)], M["roof"], solidify=0.035)
    for i in range(4):
        amphora(-1.1 + i * 0.6, 1.1, M)
    # side hall on the east with a door
    box(1.0, -0.6, 0, 1.42, 0.8, 0.55, S.wall)
    if S.roof_kind == "flat":
        flat_roof(1.0, -0.6, 1.42, 0.8, 0.55, M, S.roofmat)
    else:
        S.roofmat = M["roof"] if region is None else S.roofmat
        S.roof(1.0, -0.6, 1.42, 0.8, 0.55, 0.18, axis="y")
    box(1.42, 0.0, 0, 1.45, 0.2, 0.3, M["dark"], bevel=0)
    windows_x(1.42, [-0.35, 0.45], 0.32, 0.42, M, w=0.06)
    # stalls: striped canopies over counters with goods
    goods = [("fruit_red", "canopy_red"), ("fruit_green", "canopy_green"), ("bread", "awning"),
             ("fish", "canopy_blue"), ("cloth_purple", "canopy_red"), ("amphora", "canopy_green")]
    spots = [(-1.0, 0.25), (-0.3, 0.25), (0.4, 0.25), (-1.0, -0.65), (-0.3, -0.65), (0.4, -0.65)]
    for (gx, gy), (good, canopy) in zip(spots, goods):
        box(gx - 0.24, gy - 0.12, 0, gx + 0.24, gy + 0.12, 0.17, M["timber_light"], bevel=0.006)
        for i in range(4):
            for j in range(2):
                blob(gx - 0.17 + i * 0.115, gy - 0.05 + j * 0.1, 0.2, 0.045, 0.04, 0.035,
                     M[good], segs=8, rings=4)
        for x in (gx - 0.25, gx + 0.25):
            for y in (gy - 0.18, gy + 0.18):
                box(x - 0.012, y - 0.012, 0, x + 0.012, y + 0.012, 0.38, M["timber"], bevel=0)
        mesh("canopy", [(gx - 0.3, gy - 0.24, 0.36), (gx + 0.3, gy - 0.24, 0.36),
                        (gx + 0.3, gy + 0.24, 0.46), (gx - 0.3, gy + 0.24, 0.46)], [(0, 1, 2, 3)],
             M[canopy], solidify=0.01)
    # baskets by the entrance
    for i in range(3):
        cylinder(-0.75 + i * 0.25, -1.25, 0, 0.08, 0.07, M["straw"], n=10)
        blob(-0.75 + i * 0.25, -1.25, 0.08, 0.06, 0.06, 0.03, M[["fruit_red", "fruit_green", "bread"][i]],
             segs=8, rings=4)
    if region:
        S.tree(1.2, -1.2, M, 0.8)


def cart_depot(M, region=None):
    """Open cart shed with carts, mules' manger and hay on a dirt yard."""
    S = Style(M, region)
    lot_rect(3, 3, M["dirt"], curb=M["timber"])
    x0, x1, y0, y1, zp = -1.35, 0.9, 0.35, 1.38, 0.55
    box(x0, y1 - 0.1, 0, x1, y1, zp, S.wall)
    box(x0, y0, 0, x0 + 0.1, y1, zp, S.wall)
    for i in range(5):
        x = x0 + 0.05 + i * (x1 - x0 - 0.1) / 4
        box(x - 0.035, y0 - 0.035, 0, x + 0.035, y0 + 0.035, zp, M["timber"], bevel=0.004)
    gable(x0, y0, x1, y1, zp, 0.3, M["roof_timber"] if S.roof_kind != "thatch" else M["thatch"],
          axis="x", wall=S.wall)
    cart(-0.85, 0.85, M, load=False)
    cart(0.1, 0.85, M, load=True)
    cart(0.6, -0.55, M, load=True)
    # manger and hay
    box(1.05, 0.2, 0, 1.4, 1.3, 0.12, M["timber"], bevel=0.006)
    box(1.08, 0.23, 0.08, 1.37, 1.27, 0.13, M["straw"], bevel=0)
    for i, (x, y, z) in enumerate([(-1.1, -0.8, 0), (-0.85, -0.8, 0), (-0.97, -0.8, 0.16),
                                   (-1.1, -1.1, 0), (-0.85, -1.1, 0)]):
        box(x - 0.11, y - 0.08, z, x + 0.11, y + 0.08, z + 0.16, M["straw"], bevel=0.02)
    # spare wheels and a mule
    for i in range(3):
        cylinder(0.09, -0.2 + i * 0.03, 0.95 - 0.01, 0.95 + 0.01, 0.09, M["timber"], n=14, axis="y")
    mule(-0.2, -0.6, M)


def mule(x, y, M):
    hide = material("mule", "#6b5444", "#584334", scale=20, bump=0.1)
    blob(x, y, 0.2, 0.17, 0.08, 0.08, hide)
    for dx in (-0.11, 0.11):
        for dy in (-0.04, 0.04):
            cylinder(x + dx, y + dy, 0, 0.18, 0.016, hide, n=6)
    blob(x + 0.2, y, 0.3, 0.07, 0.045, 0.05, hide)
    box(x + 0.18, y - 0.025, 0.33, x + 0.2, y - 0.005, 0.4, hide, bevel=0)
    box(x + 0.18, y + 0.005, 0.33, x + 0.2, y + 0.025, 0.4, hide, bevel=0)


def stockpile_tile(v):
    """1x1 painted tile: paving with one low heap of building goods."""
    def build(M):
        lot_rect(1, 1, M["paving"])
        if v == 0:  # timber logs
            for i in range(3):
                for j in range(3 - i):
                    cylinder(0.05 + i * 0.075, -0.3 + j * 0.15 + i * 0.075, -0.35, 0.35, 0.07, M["log"],
                             n=10, axis="x")
        elif v == 1:  # stone blocks
            for i in range(3):
                for j in range(2):
                    box(-0.32 + i * 0.22, -0.3 + j * 0.3, 0, -0.14 + i * 0.22, -0.06 + j * 0.3, 0.14,
                        M["block"], bevel=0.01)
            box(-0.2, -0.2, 0.14, 0.0, 0.05, 0.26, M["block"], bevel=0.01)
        elif v == 2:  # crates and sacks
            for x, y, z in [(-0.2, -0.2, 0), (-0.05, -0.2, 0), (-0.2, -0.05, 0), (-0.12, -0.12, 0.12)]:
                crate(x, y, z, M, 0.13)
            for dx, dy in [(0.15, 0.15), (0.27, 0.15), (0.21, 0.26), (0.21, 0.2)]:
                sack(dx, dy, 0 if dy != 0.2 else 0.08, M)
        else:  # bricks on pallets and a few amphorae
            for i in range(2):
                box(-0.32 + i * 0.32, -0.25, 0, -0.08 + i * 0.32, 0.05, 0.03, M["timber"], bevel=0)
                box(-0.3 + i * 0.32, -0.23, 0.03, -0.1 + i * 0.32, 0.03, 0.17, M["brick"], bevel=0.005)
            for i in range(3):
                amphora(-0.2 + i * 0.12, 0.25, M, 0.9)
    return build


# ---------------------------------------------------------------- trade

def trade_post(M, region=None):
    """Merchant house with an arcade, bales and amphorae, a weighing beam and a pennant."""
    S = Style(M, region)
    lot_rect(3, 3, S.ground, curb=S.stone)
    x0, y0, x1, y1, z1, z2 = -1.3, -0.2, 0.75, 1.35, 0.5, 0.92
    walls(S, M, x0, y0 + 0.25, x1, y1, 0, z2)
    # ground-floor arcade along the front
    box(x0, y0, z1 - 0.06, x1, y0 + 0.26, z1, S.stone, bevel=0.005)
    for i in range(6):
        column(x0 + 0.08 + i * (x1 - x0 - 0.16) / 5, y0 + 0.04, 0, z1 - 0.06, 0.035, S.stone)
    box(x0, y0, z1, x1, y0 + 0.25, z2, S.wall)
    if S.region == "north":
        timber_frame(x0, y0, x1, y1, z1, z2, M)
    if S.roof_kind == "tile":
        hip(x0, y0, x1, y1, z2, 0.3, M["roof"], over=0.07)
    else:
        S.roof(x0, y0, x1, y1, z2, 0.3, axis="x")
    box(-0.4, y0 + 0.24, 0, -0.1, y0 + 0.27, 0.36, M["dark"], bevel=0)
    windows_y(y0, [-1.0, -0.6, 0.0, 0.4], 0.66, 0.8, M, w=0.07)
    windows_x(x1, [0.1, 0.5, 0.9, 1.2], 0.66, 0.8, M, w=0.07)
    windows_x(x1, [0.6, 1.0], 0.2, 0.34, M, w=0.07)
    # pennant
    cylinder(x1 - 0.1, y0 + 0.1, z2, z2 + 0.55, 0.012, M["timber"], n=6)
    mesh("flag", [(x1 - 0.1, y0 + 0.1, z2 + 0.52), (x1 + 0.2, y0 + 0.1, z2 + 0.47),
                  (x1 - 0.1, y0 + 0.1, z2 + 0.38)], [(0, 1, 2)], M["cloth_purple"], solidify=0.008)
    # goods yard: cloth bales, amphorae, crates, a balance
    for i, cm in enumerate(["cloth_red", "cloth_purple", "cloth_blue", "cloth_red"]):
        box(-1.2 + i * 0.2, -0.85, 0, -1.04 + i * 0.2, -0.6, 0.13, M[cm], bevel=0.02)
    box(-1.1, -0.85, 0.13, -0.94, -0.6, 0.26, M["cloth_blue"], bevel=0.02)
    for i in range(4):
        for j in range(2):
            amphora(-0.45 + i * 0.1, -1.15 + j * 0.11, M)
    for x, y, z in [(0.35, -1.1, 0), (0.49, -1.1, 0), (0.42, -1.1, 0.13), (0.35, -0.95, 0)]:
        crate(x, y, z, M, 0.13)
    # weighing beam on a post
    cylinder(1.1, -0.6, 0, 0.45, 0.02, M["timber"], n=6)
    box(0.85, -0.61, 0.44, 1.35, -0.59, 0.46, M["timber"], bevel=0)
    for x in (0.87, 1.33):
        cylinder(x, -0.6, 0.3, 0.44, 0.004, M["dark"], n=4, smooth=False)
        cylinder(x, -0.6, 0.28, 0.3, 0.06, M["gold"], n=10)
    box(1.0, 0.2, 0, 1.4, 1.3, 0.1, M["stone_dark"])
    for i in range(4):
        sack(1.1 + (i % 2) * 0.15, 0.4 + (i // 2) * 0.3, 0.1, M)
    S.tree(1.2, 1.15, M, 0.8)


def boat(x, y, M, s=1.0, sail=True):
    """Small merchant boat, hull along x."""
    L, W = 0.9 * s, 0.24 * s
    v, f = [], []
    pts = [(-L / 2, 0), (-L / 2 + 0.15 * s, -W / 2), (L / 2 - 0.18 * s, -W / 2), (L / 2, 0),
           (L / 2 - 0.18 * s, W / 2), (-L / 2 + 0.15 * s, W / 2)]
    for px, py in pts:
        v.append((x + px * 0.85, y + py * 0.6, 0.0))
    for px, py in pts:
        v.append((x + px, y + py, 0.14 * s))
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        f.append((i, j, n + j, n + i))
    f.append(tuple(reversed(range(n))))
    mesh("hull", v, f, M["hull"])
    box(x - L / 2 + 0.1, y - W / 2 + 0.03, 0.1 * s, x + L / 2 - 0.12, y + W / 2 - 0.03, 0.12 * s, M["deck"], bevel=0)
    if sail:
        cylinder(x, y, 0.1, 0.75 * s, 0.012, M["timber"], n=6)
        mesh("sail", [(x - 0.02, y - 0.24 * s, 0.68 * s), (x - 0.02, y + 0.24 * s, 0.68 * s),
                      (x + 0.02, y + 0.24 * s, 0.3 * s), (x + 0.02, y - 0.24 * s, 0.3 * s)],
             [(0, 1, 2, 3)], M["sail"], solidify=0.01)
    for i in range(3):
        amphora(x - 0.2 + i * 0.12, y, M, 0.7)


def harbor(M):
    """4x3: stone quay at the back with a warehouse and crane, timber piers over the water."""
    box(-2, 0.1, 0, 2, 1.5, 0.05, M["paving"], bevel=0.01)
    box(-2, 0.06, 0, 2, 0.14, 0.08, M["stone_grey"], bevel=0.005)
    # piers (planks on posts); water shows through between them
    for px in (-1.25, 0.15, 1.35):
        box(px - 0.22, -1.5, 0.05, px + 0.22, 0.12, 0.08, M["deck"], bevel=0.004)
        for py in (-1.4, -0.9, -0.4):
            for dx in (-0.2, 0.2):
                cylinder(px + dx, py, -0.1, 0.12, 0.025, M["timber"], n=6)
    # water plane under the piers so the sprite reads as a harbour on any shore
    mesh("water", [(-2, -1.5, -0.02), (2, -1.5, -0.02), (2, 0.1, -0.02), (-2, 0.1, -0.02)],
         [(0, 1, 2, 3)], M["water_deep"])
    boat(-0.55, -0.75, M, 1.0)
    boat(0.8, -1.05, M, 0.8, sail=False)
    # warehouse on the quay
    x0, x1, y0, y1, zw = -1.9, -0.2, 0.55, 1.45, 0.62
    box(x0, y0, 0.05, x1, y1, zw, M["travertine"])
    gable(x0, y0, x1, y1, zw, 0.3, M["roof"], axis="x", wall=M["travertine"])
    double_door(-1.05, y0, 0.05, 0.36, M, w=0.2)
    windows_y(y0, [-1.6, -1.35, -0.75, -0.5], 0.42, 0.53, M, w=0.06)
    windows_x(x1, [0.8, 1.2], 0.42, 0.53, M, w=0.06)
    # crane: post, jib and a hanging load
    cylinder(0.55, 0.35, 0.05, 1.1, 0.04, M["timber"], n=8)
    mesh("jib", [(0.55, 0.35, 1.05), (0.55, -0.45, 0.85), (0.55, -0.45, 0.82), (0.55, 0.35, 1.0)],
         [(0, 1, 2, 3)], M["timber"], solidify=0.05)
    cylinder(0.55, -0.43, 0.45, 0.83, 0.005, M["dark"], n=4, smooth=False)
    crate(0.55, -0.43, 0.33, M, 0.12)
    # cargo on the quay
    for i in range(5):
        amphora(0.9 + i * 0.1, 0.35, M)
    for x, y, z in [(1.5, 0.35, 0.05), (1.64, 0.35, 0.05), (1.57, 0.35, 0.18), (1.5, 0.5, 0.05)]:
        crate(x, y, z, M, 0.13)
    for i in range(3):
        sack(0.2 + i * 0.12, 1.1, 0.05, M)
    cypress(1.8, 1.3, 0.5, M)


# ---------------------------------------------------------------- farms

def hut_base(M):
    catcher(1, 1)


def wheat_farm_hut(M):
    hut_base(M)
    box(-0.4, -0.05, 0, 0.3, 0.42, 0.32, M["plaster_humble"])
    gable(-0.4, -0.05, 0.3, 0.42, 0.32, 0.22, M["thatch"], axis="x", over=0.08, wall=M["plaster_humble"])
    box(-0.12, -0.075, 0, 0.0, -0.04, 0.22, M["dark"], bevel=0)
    windows_x(0.3, [0.2], 0.15, 0.24, M, w=0.06)
    # wicker grain bin and sheaves
    cylinder(0.33, -0.25, 0, 0.22, 0.1, M["straw"], n=12)
    blob(0.33, -0.25, 0.22, 0.11, 0.11, 0.08, M["thatch"], top_only=True)
    for dx, dy in [(-0.3, -0.32), (-0.18, -0.36), (-0.06, -0.3)]:
        cylinder(dx, dy, 0, 0.16, 0.04, M["wheat"], n=8)


def vegetable_garden_hut(M):
    hut_base(M)
    box(-0.3, 0.05, 0, 0.2, 0.4, 0.26, M["timber_light"])
    mesh("lean", [(-0.35, -0.02, 0.32), (0.25, -0.02, 0.32), (0.25, 0.45, 0.22), (-0.35, 0.45, 0.22)],
         [(0, 1, 2, 3)], M["roof_faded"], solidify=0.03)
    box(-0.1, 0.02, 0, 0.02, 0.05, 0.2, M["dark"], bevel=0)
    # tools, baskets of greens, a water butt
    for i in range(3):
        cylinder(-0.25 + i * 0.16, -0.28, 0, 0.08, 0.06, M["straw"], n=10)
        blob(-0.25 + i * 0.16, -0.28, 0.08, 0.05, 0.05, 0.03, M[["cabbage", "leek", "fruit_red"][i]],
             segs=8, rings=4)
    barrel(0.32, -0.05, M, 1.0)
    box(0.26, 0.2, 0, 0.28, 0.22, 0.3, M["timber"], bevel=0)


def pig_farm_hut(M):
    hut_base(M)
    box(-0.4, 0.0, 0, 0.35, 0.42, 0.24, M["stone_dark"])
    mesh("lean", [(-0.45, -0.08, 0.3), (0.4, -0.08, 0.3), (0.4, 0.47, 0.2), (-0.45, 0.47, 0.2)],
         [(0, 1, 2, 3)], M["thatch"], solidify=0.04)
    box(-0.15, -0.02, 0, 0.0, 0.02, 0.15, M["dark"], bevel=0)
    box(-0.35, -0.35, 0, 0.1, -0.25, 0.06, M["timber"], bevel=0.005)  # trough
    box(-0.33, -0.33, 0.04, 0.08, -0.27, 0.055, M["water_dark"], bevel=0)
    pig(0.25, -0.3, M)


def pig(x, y, M, a=0.0):
    c, s = math.cos(a), math.sin(a)
    blob(x, y, 0.07, 0.11, 0.07, 0.065, M["pig"])
    blob(x + 0.1 * c, y + 0.1 * s, 0.08, 0.045, 0.045, 0.04, M["pig"])
    for dx in (-0.06, 0.06):
        for dy in (-0.035, 0.035):
            cylinder(x + dx * c - dy * s, y + dx * s + dy * c, 0, 0.05, 0.014, M["pig"], n=6)


def sheep(x, y, M, a=0.0):
    c, s = math.cos(a), math.sin(a)
    blob(x, y, 0.1, 0.1, 0.07, 0.07, M["wool"])
    blob(x + 0.1 * c, y + 0.1 * s, 0.12, 0.035, 0.03, 0.035, M["sheep_face"])
    for dx in (-0.05, 0.05):
        for dy in (-0.03, 0.03):
            cylinder(x + dx * c - dy * s, y + dx * s + dy * c, 0, 0.06, 0.01, M["sheep_face"], n=6)


def sheep_farm_hut(M):
    hut_base(M)
    box(-0.4, 0.0, 0, 0.3, 0.42, 0.3, M["plaster_humble2"])
    gable(-0.4, 0.0, 0.3, 0.42, 0.3, 0.2, M["roof_faded"], axis="y", over=0.07, wall=M["plaster_humble2"])
    box(-0.12, -0.02, 0, 0.0, 0.01, 0.22, M["dark"], bevel=0)
    # fleece bales and a hay rick
    for i in range(3):
        box(-0.35 + i * 0.14, -0.35, 0, -0.24 + i * 0.14, -0.22, 0.09, M["wool"], bevel=0.02)
    blob(0.3, -0.25, 0.0, 0.14, 0.14, 0.24, M["straw"], top_only=True)


def olive_grove_hut(M):
    hut_base(M)
    box(-0.35, 0.0, 0, 0.3, 0.4, 0.3, M["plaster_humble"])
    hip(-0.35, 0.0, 0.3, 0.4, 0.3, 0.16, M["roof_faded"], over=0.06)
    box(-0.08, -0.02, 0, 0.04, 0.01, 0.22, M["dark"], bevel=0)
    for i in range(4):
        amphora(-0.32 + i * 0.1, -0.25, M, 0.85)
    for i in range(2):
        cylinder(0.2 + i * 0.12, -0.3, 0, 0.07, 0.055, M["straw"], n=10)
        blob(0.2 + i * 0.12, -0.3, 0.07, 0.045, 0.045, 0.025, M["olive_leaf"], segs=8, rings=4)


def vineyard_hut(M):
    hut_base(M)
    box(-0.38, 0.0, 0, 0.3, 0.42, 0.3, M["plaster_warm"])
    gable(-0.38, 0.0, 0.3, 0.42, 0.3, 0.2, M["roof_house"], axis="x", over=0.07, wall=M["plaster_warm"])
    box(-0.1, -0.02, 0, 0.02, 0.01, 0.22, M["dark"], bevel=0)
    windows_x(0.3, [0.2], 0.15, 0.23, M, w=0.06)
    for x, y in [(-0.28, -0.25), (-0.13, -0.25), (-0.2, -0.38)]:
        barrel(x, y, M, 0.9)
    for i in range(2):
        cylinder(0.18 + i * 0.13, -0.28, 0, 0.08, 0.06, M["straw"], n=10)
        blob(0.18 + i * 0.13, -0.28, 0.08, 0.05, 0.05, 0.03, M["grape"], segs=8, rings=4)


def _mound(y, half, h, mat, x0=-0.5, x1=0.5, n=6):
    """A rounded crop row along x, centred on y."""
    prof = [(-half, 0.0)] + [(-half * math.cos(math.pi * k / n), h * math.sin(math.pi * k / n))
                             for k in range(1, n)] + [(half, 0.0)]
    v = [(x0, y + dy, z) for dy, z in prof] + [(x1, y + dy, z) for dy, z in prof]
    m = len(prof)
    f = [(i, i + 1, m + i + 1, m + i) for i in range(m - 1)]
    f += [tuple(range(m)), tuple(m + i for i in reversed(range(m)))]
    ob = mesh("row", v, f, mat)
    for p in ob.data.polygons:
        p.use_smooth = True
    return ob


def wheat_tile(v):
    def build(M):
        lot_rect(1, 1, M["soil"])
        for j in range(8):
            _mound(-0.4375 + j * 0.125, 0.07, 0.11, M["wheat"])
        if v == 1:
            for dx, dy in [(-0.2, -0.31), (0.25, 0.19), (0.05, -0.06)]:
                blob(dx, dy, 0.11, 0.022, 0.022, 0.018, M["fruit_red"], segs=6, rings=4)
        if v == 2:
            for dx in (-0.06, 0.0, 0.06):
                cylinder(0.25 + dx, -0.2, 0.0, 0.2, 0.035, M["wheat_ear"], n=8)
            blob(0.25, -0.2, 0.2, 0.08, 0.08, 0.05, M["wheat_ear"], segs=8, rings=4)
    return build


def vegetable_tile(v):
    def build(M):
        lot_rect(1, 1, M["soil"])
        for j in range(4):
            y = -0.375 + j * 0.25
            box(-0.5, y - 0.07, 0, 0.5, y + 0.07, 0.03, M["soil"], bevel=0.02)
            kind = (j + v) % 3
            for i in range(5):
                x = -0.4 + i * 0.2
                if kind == 0:
                    blob(x, y, 0.06, 0.07, 0.065, 0.055, M["cabbage"], segs=8, rings=5)
                elif kind == 1:
                    for k in range(3):
                        cylinder(x - 0.04 + k * 0.04, y, 0.02, 0.13, 0.012, M["leek"], n=5)
                else:
                    blob(x, y, 0.05, 0.06, 0.05, 0.04, M["leaf"], segs=8, rings=5)
        if v == 3:  # bean poles
            for i in range(3):
                x = -0.3 + i * 0.3
                cylinder(x, 0.0, 0, 0.35, 0.01, M["timber"], n=5)
                blob(x, 0.0, 0.2, 0.05, 0.05, 0.13, M["vine"], segs=8, rings=5)
    return build


def vineyard_tile(v):
    def build(M):
        lot_rect(1, 1, M["dirt"])
        for j in range(3):
            y = -0.33 + j * 0.33
            for x in (-0.45, 0.0, 0.45):
                box(x - 0.015, y - 0.015, 0, x + 0.015, y + 0.015, 0.26, M["timber"], bevel=0)
            box(-0.5, y - 0.005, 0.24, 0.5, y + 0.005, 0.25, M["timber"], bevel=0)
            mesh("vines", [(-0.5, y - 0.06, 0.08), (0.5, y - 0.06, 0.08), (0.5, y - 0.07, 0.25),
                           (0.5, y + 0.07, 0.25), (0.5, y + 0.06, 0.08), (-0.5, y + 0.06, 0.08),
                           (-0.5, y + 0.07, 0.25), (-0.5, y - 0.07, 0.25)],
                 [(0, 1, 2, 7), (7, 2, 3, 6), (6, 3, 4, 5), (0, 7, 6, 5), (1, 4, 3, 2)], M["vine"])
            for i in range(4 + v):
                gx = -0.4 + (i * 0.23 + v * 0.07) % 0.85
                blob(gx, y - 0.07, 0.13, 0.025, 0.02, 0.035, M["grape"], segs=6, rings=4)
    return build


def olive_tile(v):
    def build(M):
        lot_rect(1, 1, M["dry_grass"])
        spots = [[(0.0, 0.0)], [(-0.15, 0.15)], [(0.15, -0.1)], [(0.05, 0.12)]][v]
        for x, y in spots:
            olive(x, y, M, 1.3)
        if v == 2:
            for i in range(2):
                cylinder(-0.3 + i * 0.1, -0.3, 0, 0.06, 0.045, M["straw"], n=8)
    return build


def pig_tile(v):
    def build(M):
        lot_rect(1, 1, M["mud"])
        for dx, dy in [(-0.3, 0.3), (0.2, -0.25), (0.3, 0.35)][: v % 3]:
            blob(dx, dy, 0.0, 0.12, 0.09, 0.01, M["water_dark"], top_only=True)
        pigs = [[(-0.15, 0.1, 0.4)], [(0.15, -0.1, 2.5), (-0.2, 0.15, 0.9)], [], [(0.0, 0.0, 4.0)]][v]
        for x, y, a in pigs:
            pig(x, y, M, a)
        if v == 2:  # fence corner along the north-west edges
            fence([(-0.48, 0.48), (0.48, 0.48)], M["timber_light"], h=0.12)
    return build


def sheep_tile(v):
    def build(M):
        lot_rect(1, 1, M["pasture"])
        flock = [[(-0.1, 0.1, 0.3), (0.2, -0.15, 2.0)], [(0.0, 0.0, 4.2)],
                 [(-0.2, -0.1, 1.0), (0.15, 0.2, 5.5), (0.25, -0.2, 3.0)], []][v]
        for x, y, a in flock:
            sheep(x, y, M, a)
        if v == 3:
            blob(0.0, 0.0, 0.0, 0.12, 0.12, 0.16, M["straw"], top_only=True)
        for k in range(6):
            blob(-0.4 + ((k * 0.37 + v * 0.11) % 0.8), -0.4 + ((k * 0.53 + v * 0.29) % 0.8), 0.0, 0.03,
                 0.03, 0.03, M["leaf"], segs=6, rings=4, top_only=True)
    return build


# ---------------------------------------------------------------- water

def _seg(side, s0, s1, w, z0, z1, mat, bevel=0.006):
    if side == "n":
        return box(-w, s0, z0, w, s1, z1, mat, bevel=bevel)
    if side == "s":
        return box(-w, -s1, z0, w, -s0, z1, mat, bevel=bevel)
    if side == "e":
        return box(s0, -w, z0, s1, w, z1, mat, bevel=bevel)
    return box(-s1, -w, z0, -s0, w, z1, mat, bevel=bevel)


def _axis_pt(side, s, c, z):
    """Point at distance s along `side` from the centre, c across it."""
    return {"n": (c, s, z), "s": (-c, -s, z), "e": (s, -c, z), "w": (-s, c, z)}[side]


AQ = dict(p=0.11, t=0.095, zs=0.26, zt=0.76, ch=0.11, cw=0.13)


def aqueduct_centre(M):
    p, zt, ch, cw = AQ["p"], AQ["zt"], AQ["ch"], AQ["cw"]
    box(-p - 0.02, -p - 0.02, 0, p + 0.02, p + 0.02, 0.06, M["stone_grey"], bevel=0.006)
    box(-p, -p, 0.06, p, p, zt, M["stone_grey"], bevel=0.006)
    box(-p - 0.015, -p - 0.015, AQ["zs"] - 0.03, p + 0.015, p + 0.015, AQ["zs"], M["stone_grey"], bevel=0.004)
    box(-cw, -cw, zt, cw, cw, zt + 0.03, M["stone_grey"], bevel=0.004)
    # channel walls at the corners and the water square
    k = 0.035
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(min(sx * cw, sx * (cw - k)), min(sy * cw, sy * (cw - k)), zt,
                max(sx * cw, sx * (cw - k)), max(sy * cw, sy * (cw - k)), zt + ch, M["stone_grey"], bevel=0.003)
    box(-cw + k, -cw + k, zt + 0.03, cw - k, cw - k, zt + ch - 0.025, M["water_deep"], bevel=0)


def aqueduct_arm(side):
    def build(M):
        catcher(1, 1)
        n0 = len(bpy.context.scene.objects)
        aqueduct_centre(M)
        hold_out_since(n0)
        p, t, zs, zt, ch, cw = AQ["p"], AQ["t"], AQ["zs"], AQ["zt"], AQ["ch"], AQ["cw"]
        R = 0.5 - p
        # spandrel with half an arch, springing at the pier and peaking at the tile edge
        prof = [(p, zt)]
        for i in range(10):
            s = p + (0.5 - p) * i / 9
            prof.append((s, zs + math.sqrt(max(0.0, R * R - (0.5 - s) ** 2))))
        prof.append((0.5, zt))
        front = [_axis_pt(side, s, -t, z) for s, z in prof]
        back = [_axis_pt(side, s, t, z) for s, z in prof]
        n = len(prof)
        faces = [tuple(range(n)), tuple(reversed(range(n, 2 * n)))]
        for i in range(n):
            j = (i + 1) % n
            faces.append((i, n + i, n + j, j))
        mesh("arch", front + back, faces, M["stone_grey"])
        # channel: floor, two walls, water
        _seg(side, cw - 0.001, 0.5, cw, zt, zt + 0.03, M["stone_grey"], bevel=0.003)
        k = 0.035
        if side in "ns":
            for c in (-1, 1):
                x0, x1 = sorted((c * cw, c * (cw - k)))
                s0, s1 = (cw - 0.001, 0.5)
                y0, y1 = (s0, s1) if side == "n" else (-s1, -s0)
                box(x0, y0, zt, x1, y1, zt + ch, M["stone_grey"], bevel=0.003)
        else:
            for c in (-1, 1):
                y0, y1 = sorted((c * cw, c * (cw - k)))
                s0, s1 = (cw - 0.001, 0.5)
                x0, x1 = (s0, s1) if side == "e" else (-s1, -s0)
                box(x0, y0, zt, x1, y1, zt + ch, M["stone_grey"], bevel=0.003)
        _seg(side, cw - k, 0.5, cw - k, zt + 0.03, zt + ch - 0.025, M["water_deep"], bevel=0)
    return build


def aqueduct_c(M):
    catcher(1, 1)
    aqueduct_centre(M)


def pipe_centre(M):
    cylinder(0, 0, 0, 0.08, 0.14, M["stone_grey"], n=14)
    cylinder(0, 0, 0.08, 0.095, 0.11, M["lead"], n=14)


def pipe_arm(side):
    def build(M):
        catcher(1, 1)
        n0 = len(bpy.context.scene.objects)
        pipe_centre(M)
        hold_out_since(n0)
        # a terracotta pipe half sunk in a gravel bed, with collar joints
        _seg(side, 0.05, 0.5, 0.1, 0, 0.015, M["dirt"], bevel=0.004)
        ax = "y" if side in "ns" else "x"
        for s0, s1 in ((0.08, 0.29), (0.29, 0.5)):
            a, b = sorted((s0, s1)) if side in "ne" else sorted((-s1, -s0))
            if ax == "y":
                cylinder(0.04, 0.0, a, b, 0.055, M["terracotta"], n=12, axis="y")
            else:
                cylinder(0.04, 0.0, a, b, 0.055, M["terracotta"], n=12, axis="x")
            jc = s0 + 0.01
            j0, j1 = sorted((jc - 0.012, jc + 0.012)) if side in "ne" else sorted((-jc - 0.012, -jc + 0.012))
            cylinder(0.04, 0.0, j0, j1, 0.065, M["terracotta"], n=12, axis=ax)
    return build


def pipe_c(M):
    catcher(1, 1)
    pipe_centre(M)


def sewer_centre(M):
    box(-0.13, -0.13, 0, 0.13, 0.13, 0.04, M["stone_dark"], bevel=0.006)
    box(-0.08, -0.08, 0.035, 0.08, 0.08, 0.045, M["iron"], bevel=0)
    for i in range(4):
        box(-0.07 + i * 0.04, -0.07, 0.045, -0.06 + i * 0.04, 0.07, 0.05, M["dark"], bevel=0)


def sewer_arm(side):
    def build(M):
        catcher(1, 1)
        n0 = len(bpy.context.scene.objects)
        sewer_centre(M)
        hold_out_since(n0)
        # open stone gutter: two kerbs and dark water, a cover slab in the middle
        w, k = 0.12, 0.04
        if side in "ns":
            for c in (-1, 1):
                x0, x1 = sorted((c * w, c * (w - k)))
                y0, y1 = (0.13, 0.5) if side == "n" else (-0.5, -0.13)
                box(x0, y0, 0, x1, y1, 0.04, M["stone_dark"], bevel=0.004)
        else:
            for c in (-1, 1):
                y0, y1 = sorted((c * w, c * (w - k)))
                x0, x1 = (0.13, 0.5) if side == "e" else (-0.5, -0.13)
                box(x0, y0, 0, x1, y1, 0.04, M["stone_dark"], bevel=0.004)
        _seg(side, 0.13, 0.5, w - k, 0, 0.015, M["water_dark"], bevel=0)
        _seg(side, 0.27, 0.37, w + 0.01, 0.04, 0.06, M["stone_dark"], bevel=0.004)
    return build


def sewer_c(M):
    catcher(1, 1)
    sewer_centre(M)


def bridge_centre(M):
    box(-0.3, -0.3, 0.0, 0.3, 0.3, 0.07, M["stone_grey"], bevel=0.006)
    box(-0.27, -0.27, 0.07, 0.27, 0.27, 0.085, M["deck"], bevel=0)
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(sx * 0.28 - 0.025, sy * 0.28 - 0.025, 0.07, sx * 0.28 + 0.025, sy * 0.28 + 0.025, 0.2,
                M["stone_grey"], bevel=0.004)


def bridge_arm(side):
    def build(M):
        catcher(1, 1)
        n0 = len(bpy.context.scene.objects)
        bridge_centre(M)
        hold_out_since(n0)
        _seg(side, 0.3, 0.5, 0.3, 0.0, 0.07, M["stone_grey"], bevel=0.004)
        _seg(side, 0.27, 0.5, 0.27, 0.07, 0.085, M["deck"], bevel=0)
        # parapets on both sides of the deck
        for c in (-1, 1):
            if side in "ns":
                x0, x1 = sorted((c * 0.3, c * 0.25))
                y0, y1 = (0.3, 0.5) if side == "n" else (-0.5, -0.3)
            else:
                y0, y1 = sorted((c * 0.3, c * 0.25))
                x0, x1 = (0.3, 0.5) if side == "e" else (-0.5, -0.3)
            box(x0, y0, 0.07, x1, y1, 0.17, M["stone_grey"], bevel=0.004)
    return build


def bridge_c(M):
    catcher(1, 1)
    bridge_centre(M)


def fountain(M):
    lot_rect(1, 1, M["paving"])
    cylinder(0, 0, 0, 0.12, 0.38, M["stone_grey"], n=24)
    cylinder(0, 0, 0.1, 0.115, 0.33, M["water_deep"], n=24)
    cylinder(0, 0, 0.12, 0.4, 0.05, M["marble"], n=12)
    cylinder(0, 0, 0.4, 0.44, 0.13, M["marble"], n=16)
    cylinder(0, 0, 0.42, 0.435, 0.11, M["water_deep"], n=16)
    blob(0, 0, 0.5, 0.04, 0.04, 0.07, M["marble"], segs=10, rings=6)
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        blob(0.12 * math.cos(a), 0.12 * math.sin(a), 0.26, 0.02, 0.02, 0.12,
             material("spray", "#cfe6f2", "#b2d4e6", scale=8, rough=0.1, alpha=0.7), segs=6, rings=4)


def well(M):
    lot_rect(1, 1, M["paving"])
    cylinder(0, 0, 0, 0.2, 0.24, M["stone_grey"], n=20)
    cylinder(0, 0, 0.18, 0.205, 0.19, M["water_dark"], n=20)
    for sx in (-1, 1):
        box(sx * 0.26 - 0.025, -0.025, 0, sx * 0.26 + 0.025, 0.025, 0.55, M["timber"], bevel=0.004)
    cylinder(0.52, 0.0, -0.26, 0.26, 0.03, M["timber"], n=8, axis="x")
    cylinder(0.0, 0.0, 0.3, 0.52, 0.004, M["dark"], n=4, smooth=False)
    cylinder(0.0, 0.0, 0.25, 0.32, 0.045, M["timber_light"], n=10)
    gable(-0.3, -0.12, 0.3, 0.12, 0.55, 0.12, M["roof"], axis="x", over=0.04)
    cylinder(0.3, -0.25, 0, 0.12, 0.05, M["timber_light"], n=10)


def reservoir(M):
    """Castellum aquae: raised stone tank with arched sides, inlet channel and outlet pipes."""
    lot_rect(3, 3, M["paving"], curb=M["stone_grey"])
    x0, y0, x1, y1, zt = -1.25, -1.0, 1.25, 1.3, 0.62
    box(x0, y0, 0, x1, y1, zt, M["stone_grey"])
    # blind arches on the two visible faces
    for i in range(4):
        x = x0 + 0.32 + i * (x1 - x0 - 0.64) / 3
        box(x - 0.17, y0 - 0.02, 0.05, x + 0.17, y0 + 0.005, 0.38, M["stone_dark"], bevel=0)
    for j in range(4):
        y = y0 + 0.3 + j * (y1 - y0 - 0.6) / 3
        box(x1 - 0.005, y - 0.17, 0.05, x1 + 0.02, y + 0.17, 0.38, M["stone_dark"], bevel=0)
    box(x0 - 0.04, y0 - 0.04, zt, x1 + 0.04, y1 + 0.04, zt + 0.08, M["stone_grey"], bevel=0.006)
    box(x0 + 0.12, y0 + 0.12, zt + 0.02, x1 - 0.12, y1 - 0.12, zt + 0.081, M["water_deep"], bevel=0)
    # inlet channel from the back on a pier
    box(-0.15, y1, 0, 0.15, 1.5, zt + 0.08, M["stone_grey"], bevel=0.006)
    box(-0.1, y1 - 0.1, zt + 0.06, 0.1, 1.5, zt + 0.081, M["water_deep"], bevel=0)
    # outlet pipes and a basin at the front
    for x in (-0.6, 0.0, 0.6):
        cylinder(0.25, x, y0 - 0.2, y0, 0.04, M["lead"], n=10, axis="y")
    box(-0.9, -1.45, 0, 0.9, -1.2, 0.12, M["stone_grey"], bevel=0.006)
    box(-0.85, -1.4, 0.05, 0.85, -1.25, 0.121, M["water_deep"], bevel=0)
    for x in (-0.6, 0.0, 0.6):
        blob(x, y0 - 0.22, 0.17, 0.02, 0.02, 0.08,
             material("spray", "#cfe6f2", "#b2d4e6", scale=8, rough=0.1, alpha=0.7), segs=6, rings=4)
    cypress(-1.3, -1.3, 0.45, M)
    cypress(1.3, -1.3, 0.45, M)


# ---------------------------------------------------------------- infrastructure

def engineers_post(M, region=None):
    S = Style(M, region)
    lot_rect(1, 1, M["dirt"] if region != "egypt" else M["sand_paving"])
    x0, y0, x1, y1, zw = -0.42, -0.05, 0.25, 0.45, 0.4
    walls(S, M, x0, y0, x1, y1, 0, zw)
    if S.roof_kind == "tile":
        gable(x0, y0, x1, y1, zw, 0.2, M["roof"], axis="x", over=0.06, wall=S.wall)
    else:
        S.roof(x0, y0, x1, y1, zw, 0.2, axis="x")
    box(-0.2, y0 - 0.02, 0, -0.08, y0 + 0.01, 0.27, M["dark"], bevel=0)
    windows_x(x1, [0.2], 0.2, 0.3, M, w=0.06)
    # groma (surveying cross) and building stock
    cylinder(0.38, -0.3, 0, 0.5, 0.012, M["timber"], n=6)
    box(0.26, -0.31, 0.48, 0.5, -0.29, 0.5, M["timber"], bevel=0)
    box(0.37, -0.42, 0.48, 0.39, -0.18, 0.5, M["timber"], bevel=0)
    for x in (0.26, 0.5):
        cylinder(x, -0.3, 0.38, 0.48, 0.003, M["dark"], n=4, smooth=False)
    box(-0.4, -0.42, 0, -0.18, -0.25, 0.1, M["block"], bevel=0.008)
    box(-0.35, -0.4, 0.1, -0.22, -0.27, 0.18, M["block"], bevel=0.008)
    for i in range(3):
        box(-0.12, -0.42 + i * 0.065, 0, 0.16, -0.37 + i * 0.065, 0.05, M["log"], bevel=0.01)


# ---------------------------------------------------------------- registry

def _with_mats(fn, *a):
    def build(M):
        mats(M)
        fn(M, *a)
    return build


SPRITES = {}


def _add(name, fn, fp, args=(), **kw):
    SPRITES["bld/" + name] = dict(fp=fp, build=_with_mats(fn, *args), **kw)


for _name, _fn, _fp in [("granary", granary, (3, 3)), ("warehouse", warehouse, (3, 3)),
                        ("market", market, (3, 3)), ("trade_post", trade_post, (3, 3)),
                        ("engineers_post", engineers_post, (1, 1))]:
    _add(_name, _fn, _fp, lit=_name == "trade_post")
    for _r in REGIONS:
        _add(_name + "@" + _r, _fn, _fp, args=(_r,))
_add("cart_depot", cart_depot, (3, 3))
_add("harbor", harbor, (4, 3), lit=True)
for _v in range(4):
    _add("stockpile_tile_%d" % _v, stockpile_tile(_v), (1, 1))
for _farm, _hut, _tile in [("wheat_farm", wheat_farm_hut, wheat_tile),
                           ("vegetable_garden", vegetable_garden_hut, vegetable_tile),
                           ("pig_farm", pig_farm_hut, pig_tile),
                           ("sheep_farm", sheep_farm_hut, sheep_tile),
                           ("olive_grove", olive_grove_hut, olive_tile),
                           ("vineyard", vineyard_hut, vineyard_tile)]:
    _add(_farm + "_hut", _hut, (1, 1))
    for _v in range(4):
        _add("%s_tile_%d" % (_farm, _v), _tile(_v), (1, 1))
for _line, _centre, _arm in [("aqueduct", aqueduct_c, aqueduct_arm), ("pipe", pipe_c, pipe_arm),
                             ("sewer", sewer_c, sewer_arm), ("bridge", bridge_c, bridge_arm)]:
    _add(_line + "_c", _centre, (1, 1))
    for _s in "nesw":
        _add("%s_%s" % (_line, _s), _arm(_s), (1, 1))
_add("fountain", fountain, (1, 1))
_add("well", well, (1, 1))
_add("reservoir", reservoir, (3, 3))
