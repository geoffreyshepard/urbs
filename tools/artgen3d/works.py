"""Food and industry buildings (family "works").

Readability: food = saffron awnings and warm ochre walls; industry = charcoal slate roofs,
soot yards and fire glow; pits, mines and quarries are open excavations with spoil heaps.
Every building shows its trade with props on the lot (see docs/13-isometric.md).
Fronts face grid south (-y, front-left on screen); +x is the front-right side.
"""
import bpy

import kit
from kit import amphora, blob, box, cart, cylinder, gable, hip, lot_rect, material, mesh, sack

# ---------------------------------------------------------------- extra materials


def ext(M):
    m = material
    M["whitewash"] = m("whitewash", "#f3efe6", "#e4dfd3", scale=9, bump=0.06)
    M["blue"] = m("blue", "#2f6aa8", "#285c93", scale=12, bump=0.05)
    M["mudbrick"] = m("mudbrick", "#b68a5c", "#a27750", kind="brick", scale=12,
                      brick=(0.2, 0.08), mortar="#8a6a48", bump=0.25)
    M["sandstone"] = m("sandstone", "#dcbc82", "#c9a66a", scale=10, bump=0.12)
    M["thatch"] = m("thatch", "#a88f55", "#8c7642", kind="tiles", scale=7,
                    brick=(0.05, 0.12), bump=0.6, detail=(2.0, "#7a6538"))
    M["frame"] = m("frame", "#4a3324", "#3a281b", scale=14, bump=0.1)
    M["palm"] = m("palm", "#4f7f2c", "#3e6a22", scale=18, bump=0.3)
    M["clay"] = m("clay", "#a35d3c", "#8a4b30", scale=10, bump=0.35, detail=(1.4, "#b87454"))
    M["ash"] = m("ash", "#5f5662", "#4c4450", scale=10, bump=0.35, detail=(1.4, "#77707a"))
    M["sand"] = m("sand", "#e1cb92", "#cdb57a", scale=10, bump=0.3, detail=(1.4, "#ecdcae"))
    M["rock"] = m("rock", "#8f8a82", "#76716a", scale=7, bump=0.6, detail=(1.2, "#a29d95"))
    M["limestone"] = m("limestone", "#e6dfcc", "#d0c7b0", scale=7, bump=0.5,
                       detail=(1.2, "#f2ecdc"))
    M["marble_raw"] = m("marble_raw", "#f7f5f0", "#e4e1da", scale=6, bump=0.35, rough=0.5)
    M["lime"] = m("lime", "#f5f3ee", "#e6e3dc", scale=12, bump=0.2)
    M["flour"] = m("flour", "#f1ebdd", "#e2d9c6", scale=14, bump=0.1)
    M["salt"] = m("salt", "#fbfbf8", "#ecece6", scale=14, bump=0.2, rough=0.6)
    M["brine"] = m("brine", "#8fc3cf", "#a8d2d8", scale=6, rough=0.1, bump=0.02)
    M["sea"] = m("sea", "#3f88ad", "#3478a0", scale=5, rough=0.08, bump=0.03)
    M["marsh"] = m("marsh", "#4f7d77", "#5c8a72", scale=6, rough=0.15, bump=0.05)
    M["mud"] = m("mud", "#6b5a3e", "#5a4a32", scale=10, bump=0.3)
    M["papyrus"] = m("papyrus", "#6f9e35", "#5c8a2a", scale=20, bump=0.3)
    M["papyrus_head"] = m("papyrus_head", "#9dbd4e", "#86a83e", scale=25, bump=0.3)
    M["sheet"] = m("sheet", "#efe2b8", "#e2d2a2", scale=20, bump=0.05)
    M["ore_iron"] = m("ore_iron", "#8c4a2c", "#713a22", scale=14, bump=0.5)
    M["ore_silver"] = m("ore_silver", "#7d8189", "#666a72", scale=14, bump=0.5)
    M["slag"] = m("slag", "#3a3232", "#2a2424", scale=14, bump=0.5)
    M["silver"] = m("silver", "#d6d9de", "#bfc3ca", scale=20, rough=0.25, metal=0.9, bump=0.0)
    M["glass"] = m("glass", "#4fb3a6", "#6cc8d8", scale=20, rough=0.08, bump=0.0)
    M["glass_blue"] = m("glass_blue", "#3b6fd0", "#5a8de0", scale=20, rough=0.08, bump=0.0)
    M["grape"] = m("grape", "#5a2a63", "#47204f", scale=20, bump=0.2)
    M["vine"] = m("vine", "#5f8a2e", "#4a7524", scale=18, bump=0.3)
    M["olive_fruit"] = m("olive_fruit", "#3f4a23", "#556030", scale=25, bump=0.2)
    M["fish"] = m("fish", "#a7b2bc", "#8e9aa6", scale=25, bump=0.1, rough=0.4)
    M["garum"] = m("garum", "#8a4a26", "#a0582c", scale=8, rough=0.15, bump=0.02)
    M["net"] = m("net", "#8f7d58", "#7a6a48", scale=30, bump=0.1)
    M["planks"] = m("planks", "#c49a62", "#ad8550", scale=14, bump=0.15)
    M["logs"] = m("logs", "#6e4e30", "#5c4026", scale=18, bump=0.3)
    M["log_end"] = m("log_end", "#d0a46a", "#b88c56", scale=30, bump=0.1)
    M["sawdust"] = m("sawdust", "#c9a774", "#b8955f", scale=10, bump=0.25, detail=(1.4, "#d8bb8a"))
    M["wool"] = m("wool", "#efe7d4", "#ddd2ba", scale=20, bump=0.3)
    M["awning_red"] = m("awning_red", "#c33a2c", "#f3e6c8", kind="stripes", stripe=0.09,
                        bump=0.0, rough=0.9)
    M["awning_purple"] = m("awning_purple", "#6b2c7d", "#f1e3d0", kind="stripes", stripe=0.09,
                           bump=0.0, rough=0.9)
    for nm, c in (("dye_red", "#b4222b"), ("dye_purple", "#6a237c"), ("dye_yellow", "#e6b416"),
                  ("dye_blue", "#2c5aa6"), ("dye_green", "#3c8a48")):
        M[nm] = m(nm, c, c, scale=8, rough=0.2, bump=0.02)
        M[nm + "_cloth"] = m(nm + "_cloth", c, c, scale=16, rough=0.9, bump=0.05)
    M["cloth"] = m("cloth", "#e9dcc0", "#dccdae", scale=16, bump=0.05, rough=0.9)
    return M


# ---------------------------------------------------------------- regional styles

STYLES = {
    # wall, roof material (None = flat), trim, accent for doors/shutters
    "": dict(wall="plaster_ochre", roof="roof_house", flat=False),
    "east": dict(wall="whitewash", roof=None, flat=True, accent="blue"),
    "egypt": dict(wall="mudbrick", roof=None, flat=True, accent="sandstone", palm=True),
    "north": dict(wall="plaster_humble", roof="thatch", flat=False, frame=True),
}


def house_block(M, x0, y0, x1, y1, zw, wall, roof, axis="x", rh=0.28, region="", lit=True,
                door_x=None):
    """Plain walled building with a door on the south (-y) face and a window or two.

    region changes materials and the roof per STYLES (flat roofs get a parapet)."""
    st = STYLES.get(region, STYLES[""])
    if region:
        wall = M[st["wall"]]
        roof = M[st["roof"]] if st["roof"] else None
    box(x0, y0, 0, x1, y1, zw, wall)
    if roof is None:
        box(x0 - 0.02, y0 - 0.02, zw, x1 + 0.02, y1 + 0.02, zw + 0.05, M["sandstone"] if region == "egypt"
            else wall, bevel=0.006)
        box(x0 + 0.05, y0 + 0.05, zw + 0.02, x1 - 0.05, y1 - 0.05, zw + 0.03,
            M["paving"] if region == "east" else M["sand"], bevel=0)
    else:
        if region == "north":
            rh *= 1.5
        gable(x0, y0, x1, y1, zw, rh, roof, axis=axis, wall=wall)
    if st.get("frame"):
        for x in (x0 + 0.02, (x0 + x1) / 2, x1 - 0.02):
            box(x - 0.025, y0 - 0.015, 0, x + 0.025, y0 + 0.01, zw, M["frame"], bevel=0)
        for y in (y0 + 0.02, (y0 + y1) / 2, y1 - 0.02):
            box(x1 - 0.01, y - 0.025, 0, x1 + 0.015, y + 0.025, zw, M["frame"], bevel=0)
        box(x0, y0 - 0.015, zw * 0.5 - 0.02, x1, y0 + 0.01, zw * 0.5 + 0.02, M["frame"], bevel=0)
    dx = (x0 + x1) / 2 if door_x is None else door_x
    box(dx - 0.07, y0 - 0.02, 0, dx + 0.07, y0 + 0.01, min(0.3, zw * 0.7),
        M[st["accent"]] if st.get("accent") == "blue" else M["dark"], bevel=0)
    if lit:
        kit.windows_x(x1, [(y0 + y1) / 2], zw * 0.45, zw * 0.72, M)
        if x1 - x0 > 0.7:
            kit.windows_y(y0, [x0 + 0.15], zw * 0.45, zw * 0.72, M)
    if st.get("palm"):
        palm(x0 + 0.1, y1 - 0.1, M, 0.95)


def palm(x, y, M, h=0.75):
    cylinder(x, y, 0, h, 0.03, M["trunk"], n=6)
    for i in range(6):
        import math
        a = i * math.pi / 3
        cx, cy = math.cos(a) * 0.14, math.sin(a) * 0.14
        blob(x + cx, y + cy, h - 0.02, 0.15, 0.05, 0.03, M["palm"], segs=8, rings=4)
    blob(x, y, h + 0.02, 0.06, 0.06, 0.05, M["palm"], segs=8, rings=4)


def awning(M, x0, x1, y_face, depth, z, mat=None, posts=True):
    """Sloped awning out from the south face."""
    mat = mat or M["awning"]
    mesh("awning", [(x0, y_face, z), (x1, y_face, z), (x1, y_face - depth, z - 0.13),
                    (x0, y_face - depth, z - 0.13)], [(0, 3, 2, 1)], mat, solidify=0.012)
    if posts:
        for x in (x0 + 0.03, x1 - 0.03):
            box(x - 0.012, y_face - depth, 0, x + 0.012, y_face - depth + 0.025, z - 0.13,
                M["timber"], bevel=0)


def awning_x(M, x_face, y0, y1, depth, z, mat=None):
    """Sloped awning out from the east (+x) face."""
    mat = mat or M["awning"]
    mesh("awning_x", [(x_face, y0, z), (x_face, y1, z), (x_face + depth, y1, z - 0.13),
                      (x_face + depth, y0, z - 0.13)], [(0, 1, 2, 3)], mat, solidify=0.012)
    for y in (y0 + 0.03, y1 - 0.03):
        box(x_face + depth - 0.025, y - 0.012, 0, x_face + depth, y + 0.012, z - 0.13,
            M["timber"], bevel=0)


def smoke(M, x, y, z, n=4, k=1.0):
    for i in range(n):
        r = (0.06 + 0.02 * i) * k
        blob(x + 0.04 * i * k, y + 0.05 * i * k, z + 0.12 * i * k, r, r, r * 0.85, M["smoke"],
             name="smoke")


def point_light(x, y, z, energy=40, colour=(1.0, 0.5, 0.15)):
    lamp = bpy.data.lights.new("glow", "POINT")
    lamp.energy = energy
    lamp.color = colour
    lamp.shadow_soft_size = 0.2
    lo = bpy.data.objects.new("glow", lamp)
    lo.location = (x, y, z)
    bpy.context.scene.collection.objects.link(lo)


def heap(M, x, y, r, h, mat, rings=6):
    blob(x, y, 0, r, r * 0.9, h, mat, segs=14, rings=rings, top_only=True, name="heap")


def pit_lot(fw, fh, hole, depth, gmat, wmat, fmat, inset=0.14, steps_n=1):
    """Ground with a rectangular excavation (sloped or terraced walls)."""
    hx, hy = fw / 2, fh / 2
    a, b, c, d = hole
    for (x0, y0, x1, y1) in ((-hx, -hy, hx, b), (-hx, d, hx, hy), (-hx, b, a, d), (c, b, hx, d)):
        mesh("ground", [(x0, y0, 0), (x1, y0, 0), (x1, y1, 0), (x0, y1, 0)], [(0, 1, 2, 3)], gmat)
    z = 0.0
    dz = depth / steps_n
    di = inset / steps_n
    for k in range(steps_n):
        R = [(a + k * di, b + k * di), (c - k * di, b + k * di), (c - k * di, d - k * di),
             (a + k * di, d - k * di)]
        # vertical-ish cut then a ledge for terraces
        F = [(a + (k + 1) * di, b + (k + 1) * di), (c - (k + 1) * di, b + (k + 1) * di),
             (c - (k + 1) * di, d - (k + 1) * di), (a + (k + 1) * di, d - (k + 1) * di)]
        z1 = z - dz
        for i in range(4):
            j = (i + 1) % 4
            if steps_n > 1:
                mid = [(R[i][0] + (F[i][0] - R[i][0]) * 0.25, R[i][1] + (F[i][1] - R[i][1]) * 0.25),
                       (R[j][0] + (F[j][0] - R[j][0]) * 0.25, R[j][1] + (F[j][1] - R[j][1]) * 0.25)]
                mesh("cut", [(R[i][0], R[i][1], z), (R[j][0], R[j][1], z), (mid[1][0], mid[1][1], z1),
                             (mid[0][0], mid[0][1], z1)], [(0, 1, 2, 3)], wmat)
                mesh("ledge", [(mid[0][0], mid[0][1], z1), (mid[1][0], mid[1][1], z1),
                               (F[j][0], F[j][1], z1), (F[i][0], F[i][1], z1)], [(0, 1, 2, 3)],
                     fmat if k == steps_n - 1 else wmat)
            else:
                mesh("cut", [(R[i][0], R[i][1], z), (R[j][0], R[j][1], z), (F[j][0], F[j][1], z1),
                             (F[i][0], F[i][1], z1)], [(0, 1, 2, 3)], wmat)
        z = z1
    F = [(a + inset, b + inset), (c - inset, b + inset), (c - inset, d - inset), (a + inset, d - inset)]
    mesh("floor", [(F[0][0], F[0][1], -depth), (F[1][0], F[1][1], -depth), (F[2][0], F[2][1], -depth),
                   (F[3][0], F[3][1], -depth)], [(0, 1, 2, 3)], fmat)
    return -depth


def ramp(M, x0, x1, y_top, y_bot, depth, mat):
    """Ramp down into a pit from the south rim (y_top, z=0) to (y_bot, z=-depth)."""
    mesh("ramp", [(x0, y_top, 0.005), (x1, y_top, 0.005), (x1, y_bot, -depth + 0.005),
                  (x0, y_bot, -depth + 0.005)], [(0, 3, 2, 1)], mat)


def block(M, x, y, z, sx, sy, sz, mat):
    box(x - sx / 2, y - sy / 2, z, x + sx / 2, y + sy / 2, z + sz, mat, bevel=0.01)


def crane(M, x, y, h=0.9):
    """Timber shear-legs crane with a hanging block."""
    for dx, dy in ((-0.15, 0.1), (0.15, 0.1)):
        mesh("leg", [(x + dx - 0.02, y + dy, 0), (x + dx + 0.02, y + dy, 0), (x + 0.02, y - 0.15, h),
                     (x - 0.02, y - 0.15, h)], [(0, 1, 2, 3)], M["timber"], solidify=0.04)
    box(x - 0.01, y - 0.17, h * 0.45, x + 0.01, y - 0.15, h, M["dark"], bevel=0)
    box(x - 0.18, y - 0.4, 0.0, x + 0.18, y - 0.36, 0.34, M["timber"], bevel=0)  # treadwheel frame
    cylinder(0.2, x, y - 0.4, y - 0.36, 0.2, M["timber_light"], n=16, axis="y", name="wheel")


def tree(M, x, y, s=1.0):
    cylinder(x, y, 0, 0.22 * s, 0.03 * s, M["trunk"], n=6)
    blob(x, y, 0.32 * s, 0.2 * s, 0.2 * s, 0.17 * s, M["leaf"], name="crown")


def stack_logs(M, x, y, rows=3, n=4, length=0.55, axis="x"):
    r = 0.045
    for i in range(rows):
        for j in range(n - i):
            o = (j - (n - i - 1) / 2) * 2 * r
            zc = r + i * r * 1.7
            if axis == "x":
                cylinder(zc, y + o, x - length / 2, x + length / 2, r, M["logs"], n=8, axis="x")
                box(x + length / 2 - 0.002, y + o - r * 0.8, zc - r * 0.8, x + length / 2 + 0.004,
                    y + o + r * 0.8, zc + r * 0.8, M["log_end"], bevel=0)
            else:
                cylinder(zc, x + o, y - length / 2, y + length / 2, r, M["logs"], n=8, axis="y")
                box(x + o - r * 0.8, y - length / 2 - 0.004, zc - r * 0.8, x + o + r * 0.8,
                    y - length / 2 + 0.002, zc + r * 0.8, M["log_end"], bevel=0)


def planks(M, x, y, n=5, length=0.5, w=0.24, mat=None):
    for i in range(n):
        box(x - length / 2, y - w / 2, i * 0.025, x + length / 2, y + w / 2, i * 0.025 + 0.02,
            mat or M["planks"], bevel=0.003)


def vat(M, x, y, r, liquid, h=0.12, rim=None):
    cylinder(x, y, 0, h, r, rim or M["stone_dark"], n=16)
    cylinder(x, y, h - 0.02, h + 0.002, r * 0.82, liquid, n=16)


def basin(M, x0, y0, x1, y1, liquid, h=0.08, wall=None):
    wall = wall or M["travertine"]
    t = 0.035
    box(x0, y0, 0, x1, y0 + t, h, wall, bevel=0.004)
    box(x0, y1 - t, 0, x1, y1, h, wall, bevel=0.004)
    box(x0, y0, 0, x0 + t, y1, h, wall, bevel=0.004)
    box(x1 - t, y0, 0, x1, y1, h, wall, bevel=0.004)
    box(x0 + t, y0 + t, 0, x1 - t, y1 - t, h * 0.7, liquid, bevel=0)


def drying_line(M, x0, x1, y, z, cloths):
    for x in (x0, x1):
        box(x - 0.015, y - 0.015, 0, x + 0.015, y + 0.015, z, M["timber"], bevel=0)
    box(x0, y - 0.004, z - 0.01, x1, y + 0.004, z, M["timber"], bevel=0)
    n = len(cloths)
    w = (x1 - x0 - 0.06) / n
    for i, mat in enumerate(cloths):
        cx = x0 + 0.03 + i * w
        box(cx + 0.01, y - 0.006, z - 0.24, cx + w - 0.01, y + 0.006, z - 0.012, mat, bevel=0)


def shed(M, x0, y0, x1, y1, z, roof, axis="x", posts=True):
    """Open-sided roof on posts."""
    if posts:
        for x, y in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
            box(x - 0.03, y - 0.03, 0, x + 0.03, y + 0.03, z, M["timber"], bevel=0.004)
    gable(x0, y0, x1, y1, z, 0.22, roof, axis=axis, wall=M["timber"])


def lantern(M, x, y, z):
    """A small lit window on a post so open-air works still glow at night."""
    box(x - 0.012, y - 0.012, 0, x + 0.012, y + 0.012, z, M["timber"], bevel=0)
    box(x - 0.035, y - 0.035, z, x + 0.035, y + 0.035, z + 0.07, M["window"], bevel=0)


# ---------------------------------------------------------------- food

def bakery(M, region=""):
    lot_rect(2, 2, M["dirt"] if region != "egypt" else M["sand"])
    x0, y0, x1, y1, zw = -0.9, -0.15, 0.2, 0.9, 0.48
    house_block(M, x0, y0, x1, y1, zw, M["plaster_ochre"], M["roof_house"], region=region,
                door_x=-0.05)
    awning_x(M, x1, y0 + 0.05, y1 - 0.05, 0.3, 0.44)
    awning(M, x0 + 0.02, x1 - 0.02, y0, 0.42, 0.44)
    box(x0 + 0.08, y0 - 0.22, 0, x1 - 0.08, y0 - 0.08, 0.15, M["timber_light"], bevel=0.006)
    for i in range(7):
        blob(x0 + 0.17 + i * 0.13, y0 - 0.15, 0.17, 0.045, 0.035, 0.03, M["bread"], segs=8, rings=4)
    ox, oy = 0.55, -0.48
    blob(ox, oy, 0.0, 0.3, 0.3, 0.3, M["brick"], segs=16, rings=10, top_only=True, name="oven")
    box(ox - 0.08, oy - 0.31, 0, ox + 0.08, oy - 0.2, 0.15, M["dark"], bevel=0)
    blob(ox, oy - 0.28, 0.03, 0.05, 0.03, 0.04, M["fire"], segs=8, rings=4)
    cylinder(ox + 0.06, oy + 0.06, 0.2, 0.42, 0.05, M["brick"], n=8)
    smoke(M, ox + 0.07, oy + 0.08, 0.5, 4, 1.2)
    mx, my = 0.68, 0.62
    cylinder(mx, my, 0, 0.06, 0.24, M["stone_dark"], n=18)
    blob(mx, my, 0.06, 0.1, 0.1, 0.22, M["stone_dark"], segs=12, rings=6, name="meta")
    box(mx - 0.32, my - 0.015, 0.2, mx + 0.32, my + 0.015, 0.23, M["timber"], bevel=0)
    for dx, dy in [(-0.05, -0.05), (0.08, -0.04), (0.02, 0.06)]:
        sack(0.62 + dx, 0.12 + dy, 0, M)
    lantern(M, -0.75, -0.75, 0.3)


def hourglass_mill(M, x, y, s=1.0):
    cylinder(x, y, 0, 0.06 * s, 0.16 * s, M["stone_dark"], n=16)
    blob(x, y, 0.06 * s, 0.09 * s, 0.09 * s, 0.2 * s, M["stone_dark"], segs=12, rings=6, name="meta")
    blob(x, y, 0.33 * s, 0.12 * s, 0.12 * s, 0.1 * s, M["stone_dark"], segs=12, rings=6, name="catillus")
    blob(x, y, 0.22 * s, 0.12 * s, 0.12 * s, 0.1 * s, M["stone_dark"], segs=12, rings=6, name="catillus2")
    box(x - 0.26 * s, y - 0.012, 0.25 * s, x + 0.26 * s, y + 0.012, 0.28 * s, M["timber"], bevel=0)


def mill(M, region=""):
    lot_rect(2, 2, M["dirt"] if region != "egypt" else M["sand"])
    house_block(M, -0.92, 0.15, 0.1, 0.92, 0.5, M["plaster_ochre"], M["roof_house"], region=region,
                axis="y")
    awning_x(M, 0.1, 0.2, 0.85, 0.32, 0.46)
    # three hourglass mills turned by a donkey, under a saffron shade
    for x, y in ((-0.55, -0.35), (0.05, -0.55), (0.6, -0.2)):
        cylinder(x, y, 0, 0.012, 0.36, M["dirt"], n=20)  # trodden ring
        hourglass_mill(M, x, y)
    blob(0.32, -0.62, 0.14, 0.12, 0.05, 0.07, M["trunk"], segs=8, rings=5, name="donkey")
    blob(0.43, -0.62, 0.18, 0.05, 0.035, 0.045, M["trunk"], segs=8, rings=4, name="head")
    for x in (0.25, 0.39):
        for y in (-0.645, -0.595):
            box(x - 0.01, y - 0.01, 0, x + 0.01, y + 0.01, 0.1, M["trunk"], bevel=0)
    for i, (dx, dy, dz) in enumerate([(0, 0, 0), (0.12, 0, 0), (0.06, 0.1, 0), (0.06, 0.04, 0.09),
                                       (0.24, 0.02, 0)]):
        sack(0.42 + dx, 0.3 + dy, dz, M)
    for x, y, r in ((-0.1, 0.0, 0.18), (-0.75, -0.8, 0.14), (0.75, -0.75, 0.12)):
        blob(x, y, 0, r, r * 0.8, 0.04, M["flour"], segs=12, rings=6, top_only=True)
    lantern(M, -0.85, -0.85, 0.3)


def olive_press(M):
    lot_rect(2, 2, M["dirt"])
    house_block(M, -0.92, 0.2, 0.25, 0.92, 0.5, M["plaster_ochre"], M["roof_house"], axis="x")
    # lever press: long beam from the wall, weight stone at the end
    box(-0.8, -0.18, 0, -0.68, -0.06, 0.42, M["timber"], bevel=0.006)
    box(-0.74, -0.15, 0.36, 0.55, -0.09, 0.42, M["timber"], bevel=0.004)
    box(0.48, -0.17, 0.0, 0.62, -0.07, 0.36, M["timber"], bevel=0.004)
    blob(0.55, -0.12, 0.06, 0.12, 0.12, 0.08, M["stone_dark"], segs=10, rings=6)
    cylinder(-0.3, -0.12, 0, 0.12, 0.2, M["travertine"], n=16)
    cylinder(-0.3, -0.12, 0.1, 0.122, 0.16, M["olive_fruit"], n=16)
    # trapetum crusher: round basin with two half-wheel stones
    cylinder(0.35, -0.6, 0, 0.16, 0.3, M["travertine"], n=20)
    cylinder(0.35, -0.6, 0.14, 0.162, 0.25, M["olive_fruit"], n=20)
    for dx in (-0.12, 0.12):
        cylinder(0.24, 0.35 + dx, -0.6 - 0.04, -0.6 + 0.04, 0.12, M["stone_dark"], n=14, axis="y")
    box(0.35 - 0.01, -0.62, 0.16, 0.35 + 0.01, -0.58, 0.34, M["timber"], bevel=0)
    box(0.0, -0.61, 0.24, 0.7, -0.59, 0.27, M["timber"], bevel=0)
    # baskets of olives and oil amphorae
    for x, y in ((-0.6, -0.65), (-0.4, -0.75)):
        cylinder(x, y, 0, 0.1, 0.08, M["timber_light"], n=12)
        blob(x, y, 0.1, 0.07, 0.07, 0.03, M["olive_fruit"], segs=10, rings=4, top_only=True)
    for i in range(4):
        amphora(0.75, 0.15 + i * 0.12, M, 1.1)
    kit.olive(0.65, 0.7, M, 0.9)
    awning(M, -0.88, 0.2, 0.2, 0.25, 0.46)
    lantern(M, -0.85, -0.8, 0.3)


def winery(M):
    lot_rect(3, 2, M["dirt"])
    house_block(M, -1.4, 0.2, -0.1, 0.92, 0.55, M["plaster_ochre"], M["roof_house"], axis="x")
    # vine pergola with purple grapes on the east side
    px0, px1, py0, py1, pz = 0.2, 1.4, 0.1, 0.9, 0.45
    for x in (px0, px1):
        for y in (py0, py1):
            box(x - 0.025, y - 0.025, 0, x + 0.025, y + 0.025, pz, M["timber"], bevel=0)
    for i in range(6):
        y = py0 + i * (py1 - py0) / 5
        box(px0, y - 0.01, pz, px1, y + 0.01, pz + 0.02, M["timber_light"], bevel=0)
    for i in range(9):
        x = px0 + 0.07 + i * 0.13
        blob(x, 0.5, pz + 0.04, 0.09, 0.42, 0.04, M["vine"], segs=8, rings=4)
        blob(x, 0.15 + (i % 3) * 0.25, pz - 0.05, 0.03, 0.03, 0.05, M["grape"], segs=6, rings=4)
    # treading basin with purple must
    basin(M, -1.3, -0.85, -0.6, -0.25, M["grape"], h=0.12)
    # dolia sunk in the yard: a grid of jar mouths
    for i in range(4):
        for j in range(2):
            x, y = -0.35 + i * 0.25, -0.75 + j * 0.3
            cylinder(x, y, 0, 0.05, 0.1, M["amphora"], n=14)
            cylinder(x, y, 0.045, 0.052, 0.06, M["dark"], n=12)
    for i in range(5):
        amphora(0.95 + (i % 3) * 0.12, -0.5 - (i // 3) * 0.13, M, 1.1)
    cart(1.0, -0.15, M, load=False)
    for i in range(3):
        blob(1.0 - 0.1 + i * 0.1, -0.15, 0.2, 0.05, 0.05, 0.04, M["grape"], segs=6, rings=4)
    awning(M, -1.38, -0.12, 0.2, 0.2, 0.5)
    lantern(M, 0.0, -0.4, 0.3)


def garum_workshop(M):
    lot_rect(2, 2, M["paving"])
    house_block(M, -0.92, 0.25, 0.3, 0.92, 0.48, M["plaster_ochre"], M["roof_house"], axis="x")
    # salting vats: a grid of square basins with red-brown fish sauce
    for i in range(3):
        for j in range(2):
            x, y = -0.75 + i * 0.36, -0.75 + j * 0.36
            basin(M, x, y, x + 0.3, y + 0.3, M["garum"], h=0.12)
    # fish drying rack
    for x in (0.45, 0.85):
        box(x - 0.015, -0.6, 0, x + 0.015, -0.57, 0.38, M["timber"], bevel=0)
    box(0.45, -0.6, 0.36, 0.85, -0.57, 0.38, M["timber"], bevel=0)
    for i in range(6):
        x = 0.5 + i * 0.065
        blob(x, -0.585, 0.27, 0.02, 0.012, 0.07, M["fish"], segs=6, rings=4, name="fish")
    for i in range(4):
        amphora(0.55 + (i % 2) * 0.13, 0.05 + (i // 2) * 0.13, M, 1.1)
    for x, y in ((0.85, -0.15), (0.6, -0.25)):
        cylinder(x, y, 0, 0.08, 0.08, M["timber_light"], n=12)
        blob(x, y, 0.08, 0.06, 0.06, 0.03, M["fish"], segs=10, rings=4, top_only=True)
    awning(M, -0.9, 0.28, 0.25, 0.22, 0.44)
    lantern(M, 0.85, 0.85, 0.3)


def salt_works(M):
    lot_rect(3, 2, M["sand"])
    # evaporation pans: a grid of shallow basins, some brine, some crusted white
    for i in range(4):
        for j in range(2):
            x, y = -1.4 + i * 0.5, -0.9 + j * 0.55
            basin(M, x, y, x + 0.46, y + 0.5, M["brine"] if (i + j) % 2 else M["salt"], h=0.05,
                  wall=M["mud"])
    # salt cones and a hut
    for x, y, r in ((0.75, -0.6, 0.22), (1.15, -0.55, 0.18), (0.95, -0.2, 0.2)):
        blob(x, y, 0, r, r, r * 1.1, M["salt"], segs=14, rings=8, top_only=True, name="cone")
    house_block(M, 0.6, 0.3, 1.38, 0.92, 0.42, M["plaster_ochre"], M["roof_house"], axis="x",
                lit=False)
    for i in range(3):
        sack(0.7 + i * 0.13, 0.15, 0, M)


def fishing_wharf(M):
    # half the lot is sea (north-west), the rest a timber quay
    lot_rect(2, 2, M["dirt"])
    mesh("sea", [(-1.0, 0.0, 0.004), (1.0, 0.0, 0.004), (1.0, 1.0, 0.004), (-1.0, 1.0, 0.004)],
         [(0, 1, 2, 3)], M["sea"])
    box(-1.0, -0.05, -0.06, 1.0, 0.08, 0.06, M["timber_light"], bevel=0.004)  # quay edge
    box(-0.15, 0.08, 0.02, 0.15, 0.95, 0.06, M["planks"], bevel=0.004)  # jetty
    for y in (0.3, 0.6, 0.9):
        for x in (-0.15, 0.15):
            cylinder(x, y, -0.05, 0.08, 0.025, M["timber"], n=8)
    # small boat moored by the jetty
    blob(0.5, 0.55, 0.03, 0.12, 0.33, 0.07, M["timber"], segs=12, rings=6, name="hull")
    box(0.4, 0.3, 0.06, 0.6, 0.8, 0.075, M["timber_light"], bevel=0)
    # hut with saffron awning, net racks, fish baskets
    house_block(M, -0.95, -0.95, -0.35, -0.35, 0.4, M["plaster_ochre"], M["roof_house"], axis="y")
    awning_x(M, -0.35, -0.9, -0.4, 0.22, 0.36)
    for x in (0.2, 0.85):
        box(x - 0.015, -0.6, 0, x + 0.015, -0.57, 0.36, M["timber"], bevel=0)
    mesh("net", [(0.2, -0.585, 0.35), (0.85, -0.585, 0.35), (0.8, -0.585, 0.08), (0.25, -0.585, 0.08)],
         [(0, 1, 2, 3)], M["net"], solidify=0.008)
    for x, y in ((0.6, -0.25), (0.8, -0.3), (-0.1, -0.7)):
        cylinder(x, y, 0, 0.08, 0.08, M["timber_light"], n=12)
        blob(x, y, 0.08, 0.06, 0.06, 0.03, M["fish"], segs=10, rings=4, top_only=True)


# ---------------------------------------------------------------- wood

def lumber_camp(M):
    lot_rect(2, 2, M["sawdust"])
    for x, y, s in ((-0.75, 0.75, 1.0), (-0.35, 0.82, 0.9), (0.75, 0.75, 1.1), (-0.8, 0.3, 0.85)):
        tree(M, x, y, s)
    house_block(M, 0.05, 0.25, 0.75, 0.75, 0.4, M["timber_light"], M["roof_slate"], axis="x",
                lit=False)
    stack_logs(M, -0.35, -0.55, rows=3, n=5, length=0.7)
    stack_logs(M, 0.5, -0.45, rows=2, n=4, length=0.6, axis="y")
    # stumps with an axe, chips
    for x, y in ((-0.2, 0.0), (0.0, -0.15), (-0.7, -0.1)):
        cylinder(x, y, 0, 0.07, 0.07, M["logs"], n=10)
        cylinder(x, y, 0.069, 0.072, 0.06, M["log_end"], n=10)
    box(-0.03, -0.17, 0.07, 0.03, -0.13, 0.2, M["timber"], bevel=0)
    box(-0.02, -0.2, 0.17, 0.06, -0.1, 0.21, M["iron"], bevel=0)


def sawmill(M):
    lot_rect(3, 2, M["sawdust"])
    # open shed over a saw pit with a log on trestles and a big frame saw
    shed(M, -1.35, -0.1, 0.25, 0.85, 0.55, M["roof_slate"], axis="x")
    box(-1.35, 0.75, 0, 0.25, 0.85, 0.55, M["timber_light"], bevel=0)
    for x in (-1.0, -0.2):
        box(x - 0.03, 0.15, 0, x + 0.03, 0.6, 0.2, M["timber"], bevel=0)
    cylinder(0.25, 0.37, -1.15, 0.05, 0.08, M["logs"], n=10, axis="x")
    box(-0.6, 0.3, 0.05, -0.56, 0.44, 0.55, M["iron"], bevel=0)  # frame saw blade
    box(-0.75, 0.33, 0.48, -0.41, 0.41, 0.52, M["timber"], bevel=0)
    # plank stacks and log piles in the yard
    planks(M, 0.65, 0.5, n=6, length=0.6)
    planks(M, 0.65, 0.1, n=4, length=0.6)
    planks(M, 1.15, 0.35, n=5, length=0.35, w=0.5)
    stack_logs(M, -0.6, -0.65, rows=3, n=5, length=0.9)
    stack_logs(M, 0.85, -0.6, rows=2, n=3, length=0.7)
    lantern(M, 0.35, -0.25, 0.32)


def carpenter(M):
    lot_rect(2, 2, M["sawdust"])
    house_block(M, -0.92, 0.1, 0.2, 0.92, 0.5, M["timber_light"], M["roof_slate"], axis="y")
    shed(M, 0.2, 0.2, 0.9, 0.85, 0.42, M["roof_slate"], axis="y")
    # workbench with a chair and cart wheel, planks
    box(-0.6, -0.45, 0.16, 0.1, -0.25, 0.2, M["timber_light"], bevel=0.005)
    for x in (-0.55, 0.05):
        for y in (-0.43, -0.27):
            box(x - 0.015, y - 0.015, 0, x + 0.015, y + 0.015, 0.16, M["timber"], bevel=0)
    box(-0.4, -0.4, 0.2, -0.1, -0.3, 0.24, M["planks"], bevel=0)
    cylinder(0.2, 0.55, -0.62, -0.58, 0.18, M["timber"], n=16, axis="y", name="wheel")
    cylinder(0.2, 0.55, -0.635, -0.565, 0.03, M["iron"], n=8, axis="y")
    # stool and a small table
    for x, y in ((0.3, -0.25), (-0.75, -0.75)):
        box(x - 0.08, y - 0.08, 0.14, x + 0.08, y + 0.08, 0.16, M["planks"], bevel=0.004)
        for dx in (-0.06, 0.06):
            for dy in (-0.06, 0.06):
                box(x + dx - 0.01, y + dy - 0.01, 0, x + dx + 0.01, y + dy + 0.01, 0.14,
                    M["timber"], bevel=0)
    planks(M, 0.55, 0.5, n=5, length=0.55, w=0.22)
    planks(M, -0.4, -0.75, n=3, length=0.5, w=0.2)


# ---------------------------------------------------------------- earth and stone

def clay_pit(M):
    pit_lot(2, 2, (-0.75, -0.55, 0.55, 0.75), 0.28, M["dirt"], M["clay"], M["clay"])
    ramp(M, 0.05, 0.3, -0.55, -0.15, 0.28, M["mud"])
    for x, y, r in ((0.75, 0.6, 0.2), (0.75, 0.15, 0.17), (-0.6, -0.8, 0.16)):
        heap(M, x, y, r, r * 0.8, M["clay"])
    for x, y in ((-0.3, 0.2), (0.1, 0.4)):
        cylinder(x, y, -0.28, -0.2, 0.07, M["timber_light"], n=10)
        blob(x, y, -0.2, 0.06, 0.06, 0.03, M["clay"], segs=10, rings=4, top_only=True)
    cart(0.55, -0.75, M, load=False)
    blob(0.5, -0.75, 0.18, 0.12, 0.07, 0.05, M["clay"], segs=10, rings=4, top_only=True)


def sand_pit(M):
    pit_lot(2, 2, (-0.75, -0.55, 0.55, 0.75), 0.25, M["dirt"], M["sand"], M["sand"])
    ramp(M, 0.05, 0.3, -0.55, -0.15, 0.25, M["sand"])
    for x, y, r in ((0.75, 0.55, 0.22), (-0.5, -0.8, 0.18), (0.8, 0.05, 0.16)):
        heap(M, x, y, r, r * 0.7, M["sand"])
    # sieve frame
    box(-0.3, -0.85, 0.12, 0.05, -0.65, 0.15, M["timber_light"], bevel=0)
    for x in (-0.28, 0.03):
        box(x - 0.012, -0.84, 0, x + 0.012, -0.82, 0.14, M["timber"], bevel=0)
    for i in range(3):
        sack(0.35 + i * 0.12, -0.8, 0, M)


def pozzolana_pit(M):
    pit_lot(2, 2, (-0.75, -0.55, 0.55, 0.75), 0.28, M["dirt"], M["ash"], M["ash"])
    ramp(M, 0.05, 0.3, -0.55, -0.15, 0.28, M["ash"])
    for x, y, r in ((0.72, 0.55, 0.22), (0.78, 0.05, 0.17), (-0.55, -0.82, 0.15)):
        heap(M, x, y, r, r * 0.8, M["ash"])
    for i, (dx, dy, dz) in enumerate([(0, 0, 0), (0.12, 0, 0), (0.06, 0.08, 0.08)]):
        sack(0.3 + dx, -0.8 + dy, dz, M)
    box(-0.1, -0.2, -0.28, 0.0, -0.1, 0.25, M["timber"], bevel=0)  # windlass post
    cylinder(0.25, -0.05, -0.25, 0.1, 0.03, M["timber"], n=8, axis="y")


def quarry_common(M, stone, floor, crane_at=(0.85, 0.5), blocks_mat=None, n_steps=3):
    blocks_mat = blocks_mat or stone
    pit_lot(3, 3, (-1.3, -0.6, 0.65, 1.3), 0.55, M["dirt"], stone, floor, inset=0.45, steps_n=n_steps)
    ramp(M, -0.1, 0.15, -0.6, -0.05, 0.55, M["dirt"])
    # cut blocks: some in the pit, rows waiting in the yard
    for x, y in ((-0.6, 0.6), (-0.3, 0.75), (0.1, 0.5)):
        block(M, x, y, -0.55, 0.22, 0.16, 0.14, blocks_mat)
    for i in range(4):
        block(M, -1.15 + i * 0.27, -1.1, 0.0, 0.22, 0.16, 0.14, blocks_mat)
        if i < 3:
            block(M, -1.02 + i * 0.27, -1.1, 0.14, 0.22, 0.16, 0.13, blocks_mat)
    for i in range(3):
        block(M, 1.1, -0.9 + i * 0.27, 0.0, 0.16, 0.22, 0.14, blocks_mat)
    crane(M, crane_at[0], crane_at[1], 1.0)
    cart(0.55, -1.05, M, load=False)
    block(M, 0.5, -1.05, 0.19, 0.24, 0.16, 0.12, blocks_mat)
    lantern(M, 1.2, -0.3, 0.3)


def quarry(M):
    quarry_common(M, M["rock"], M["stone_dark"])


def limestone_quarry(M):
    quarry_common(M, M["limestone"], M["limestone"], blocks_mat=M["travertine"])


def marble_quarry(M):
    quarry_common(M, M["marble_raw"], M["marble_raw"], blocks_mat=M["marble"], n_steps=4)


def stonemason(M):
    lot_rect(2, 2, M["paving"])
    house_block(M, -0.92, 0.2, 0.3, 0.92, 0.48, M["stone_dark"], M["roof_slate"], axis="x")
    shed(M, 0.3, 0.25, 0.9, 0.9, 0.4, M["roof_slate"], axis="y")
    # blocks, column drums and a half-dressed column on trestles
    for x, y, h in ((-0.7, -0.6, 0.18), (-0.45, -0.75, 0.14), (-0.55, -0.35, 0.12)):
        block(M, x, y, 0, 0.22, 0.18, h, M["travertine"])
    for i in range(3):
        cylinder(0.25 + i * 0.2, -0.65, 0, 0.12, 0.08, M["travertine"], n=14)
    box(-0.1, -0.25, 0, -0.06, -0.05, 0.12, M["timber"], bevel=0)
    box(0.5, -0.25, 0, 0.54, -0.05, 0.12, M["timber"], bevel=0)
    cylinder(0.2, -0.15, -0.2, 0.7, 0.07, M["travertine"], n=14, axis="x")
    blob(0.6, 0.05, 0, 0.14, 0.1, 0.03, M["limestone"], segs=10, rings=4, top_only=True)
    lantern(M, 0.85, -0.85, 0.3)


def sculptor(M):
    lot_rect(2, 2, M["paving"])
    house_block(M, -0.92, 0.25, 0.25, 0.92, 0.5, M["stone_dark"], M["roof_slate"], axis="x")
    # statues on plinths: finished and roughed out
    for x, y, done in ((-0.55, -0.45, True), (0.0, -0.65, True), (0.55, -0.3, False),
                       (0.65, 0.4, True)):
        box(x - 0.13, y - 0.13, 0, x + 0.13, y + 0.13, 0.14, M["travertine"], bevel=0.006)
        if done:
            cylinder(x, y, 0.14, 0.4, 0.07, M["marble"], n=12)  # draped legs
            blob(x, y, 0.47, 0.085, 0.065, 0.12, M["marble"], segs=12, rings=6)  # torso
            blob(x, y, 0.65, 0.05, 0.05, 0.06, M["marble"], segs=10, rings=6)  # head
            mesh("arm", [(x + 0.06, y, 0.52), (x + 0.2, y, 0.68), (x + 0.22, y, 0.66),
                         (x + 0.08, y, 0.48)], [(0, 1, 2, 3)], M["marble"], solidify=0.04)
        else:
            box(x - 0.1, y - 0.08, 0.14, x + 0.1, y + 0.08, 0.6, M["marble_raw"], bevel=0.03)
            blob(x, y, 0.62, 0.05, 0.05, 0.06, M["marble_raw"], segs=10, rings=6)
    for x, y in ((0.75, -0.75), (-0.75, -0.8)):
        block(M, x, y, 0, 0.2, 0.18, 0.18, M["marble_raw"])
    lantern(M, 0.85, 0.85, 0.3)


def brick_kiln(M):
    lot_rect(2, 2, M["dirt"])
    # rectangular updraft kiln with glowing fire mouths, chimney smoke
    x0, y0, x1, y1 = -0.85, 0.0, 0.15, 0.85
    box(x0, y0, 0, x1, y1, 0.55, M["brick"])
    box(x0 + 0.08, y0 + 0.08, 0.55, x1 - 0.08, y1 - 0.08, 0.6, M["brick"], bevel=0.006)
    for x in (-0.6, -0.3):
        box(x - 0.08, y0 - 0.02, 0, x + 0.08, y0 + 0.01, 0.16, M["dark"], bevel=0)
        box(x - 0.06, y0 - 0.025, 0.0, x + 0.06, y0 + 0.012, 0.09, M["glow"], bevel=0)
    box(x1, 0.3, 0, x1 + 0.02, 0.5, 0.14, M["glow"], bevel=0)
    point_light(-0.45, -0.25, 0.15, 30)
    smoke(M, -0.4, 0.45, 0.65, 4, 1.4)
    # stacks of fresh bricks and drying racks
    for k, (cx, cy) in enumerate(((0.5, -0.5), (0.5, 0.2), (-0.35, -0.65))):
        for i in range(4):
            box(cx - 0.18, cy - 0.12, i * 0.05, cx + 0.18, cy + 0.12, i * 0.05 + 0.045,
                M["brick"] if k < 2 else M["clay"], bevel=0.003)
    box(-0.8, -0.85, 0.0, -0.6, -0.4, 0.02, M["clay"], bevel=0)
    kit.windows_x(x1, [0.65], 0.25, 0.4, M)


def lime_kiln(M):
    lot_rect(2, 2, M["dirt"])
    # tall round kiln tower with a fire arch and white lime heaps
    cylinder(-0.3, 0.3, 0, 0.75, 0.45, M["stone_dark"], n=24)
    cylinder(-0.3, 0.3, 0.75, 0.82, 0.38, M["stone_dark"], n=24)
    cylinder(-0.3, 0.3, 0.8, 0.83, 0.3, M["lime"], n=20)
    box(-0.42, -0.17, 0, -0.18, -0.12, 0.25, M["dark"], bevel=0)
    box(-0.38, -0.18, 0, -0.22, -0.13, 0.12, M["glow"], bevel=0)
    point_light(-0.3, -0.35, 0.15, 30)
    smoke(M, -0.3, 0.3, 0.92, 4, 1.6)
    # ramp of earth up the back for loading
    mesh("ramp", [(0.15, 0.6, 0.0), (0.15, 0.9, 0.0), (-0.1, 0.9, 0.6), (-0.1, 0.6, 0.6)],
         [(0, 1, 2, 3)], M["dirt"], solidify=0.05)
    for x, y, r in ((0.55, -0.4, 0.22), (0.15, -0.65, 0.17), (0.7, 0.2, 0.16)):
        heap(M, x, y, r, r * 0.9, M["lime"])
    for x, y in ((-0.75, -0.6), (-0.6, -0.8)):
        block(M, x, y, 0, 0.18, 0.15, 0.12, M["limestone"])


def concrete_works(M):
    lot_rect(3, 3, M["soot"])
    house_block(M, -1.4, 0.4, -0.2, 1.4, 0.55, M["stone_dark"], M["roof_slate"], axis="y")
    # three heaps: lime (white), sand (yellow), pozzolana (grey-purple)
    for (x, y, mat) in ((0.2, 1.0, M["lime"]), (0.75, 1.0, M["sand"]), (1.2, 0.85, M["ash"])):
        heap(M, x, y, 0.26, 0.22, mat)
    # mixing troughs and a formwork mould with grey concrete
    basin(M, -0.05, 0.05, 0.75, 0.45, M["stone_dark"], h=0.12, wall=M["timber_light"])
    box(0.05, 0.12, 0.08, 0.65, 0.38, 0.09, M["ash"], bevel=0)
    basin(M, -1.2, -1.2, -0.3, -0.6, M["rock"], h=0.2, wall=M["timber_light"])
    for i in range(4):
        block(M, 0.2 + i * 0.28, -0.9, 0, 0.22, 0.22, 0.16, M["rock"])
    cart(0.75, -0.35, M, load=False)
    heap(M, 0.65, -0.35, 0.12, 0.1, M["ash"])
    box(-0.15, -0.3, 0, -0.11, -0.26, 0.3, M["timber"], bevel=0)
    box(-0.25, -0.31, 0.27, 0.0, -0.25, 0.3, M["timber"], bevel=0)


# ---------------------------------------------------------------- mines and metal

def mine(M, ore, hill_mat):
    lot_rect(3, 3, M["dirt"])
    # a rugged outcrop: overlapping lumps of rock with scrub
    for (x, y, rx, ry, rz) in ((-0.45, 0.55, 1.0, 0.85, 0.55), (-0.9, 0.2, 0.55, 0.6, 0.42),
                               (0.15, 0.85, 0.75, 0.6, 0.62), (-0.2, 0.25, 0.6, 0.5, 0.48),
                               (-0.75, 0.95, 0.6, 0.5, 0.7), (0.65, 1.05, 0.55, 0.4, 0.38)):
        blob(x, y, 0, rx, ry, rz, hill_mat, segs=9, rings=7, top_only=True, name="hill")
    for (x, y, z) in ((-0.95, 0.55, 0.38), (-0.1, 0.95, 0.55), (0.55, 0.75, 0.3), (-0.6, 0.0, 0.25),
                      (-0.4, 1.1, 0.5)):
        blob(x, y, z, 0.12, 0.11, 0.08, M["olive_leaf"], segs=8, rings=5, name="scrub")
    # timber-framed adit in the hill's south face
    ax, ay = -0.35, -0.42
    box(ax - 0.22, ay - 0.08, 0, ax + 0.22, ay + 0.25, 0.42, hill_mat, bevel=0.02)
    box(ax - 0.13, ay - 0.1, 0, ax + 0.13, ay + 0.02, 0.3, M["dark"], bevel=0)
    for x in (ax - 0.15, ax + 0.15):
        box(x - 0.025, ay - 0.13, 0, x + 0.025, ay - 0.08, 0.33, M["timber"], bevel=0)
    box(ax - 0.19, ay - 0.13, 0.31, ax + 0.19, ay - 0.08, 0.36, M["timber"], bevel=0)
    # rails out of the adit, ore cart, ore heaps
    for dx in (-0.06, 0.06):
        box(ax + dx - 0.01, -1.4, 0, ax + dx + 0.01, ay - 0.1, 0.015, M["iron"], bevel=0)
    for i in range(7):
        box(ax - 0.1, -1.35 + i * 0.13, 0, ax + 0.1, -1.32 + i * 0.13, 0.01, M["timber"], bevel=0)
    box(ax - 0.11, -1.0, 0.04, ax + 0.11, -0.78, 0.17, M["timber_light"], bevel=0.006)
    blob(ax, -0.89, 0.17, 0.09, 0.1, 0.06, ore, segs=10, rings=4, top_only=True)
    for x, y, r in ((0.55, -0.75, 0.28), (1.0, -0.35, 0.22), (0.9, -1.05, 0.18), (-1.1, -1.0, 0.2)):
        heap(M, x, y, r, r * 0.8, ore)
    # windlass headframe over a shaft on the hill
    hx, hy, hz = 0.25, 0.3, 0.55
    for dx in (-0.14, 0.14):
        box(hx + dx - 0.025, hy - 0.025, hz - 0.2, hx + dx + 0.025, hy + 0.025, hz + 0.35, M["timber"],
            bevel=0)
    cylinder(hz + 0.3, hy, hx - 0.18, hx + 0.18, 0.04, M["timber"], n=8, axis="x")
    box(hx - 0.01, hy - 0.01, hz, hx + 0.01, hy + 0.01, hz + 0.3, M["dark"], bevel=0)
    shed(M, 0.75, 0.0, 1.35, 0.55, 0.38, M["roof_slate"], axis="y")
    box(0.75, 0.45, 0, 1.35, 0.55, 0.38, M["timber_light"], bevel=0)
    lantern(M, ax + 0.3, ay - 0.2, 0.3)


def iron_mine(M):
    mine(M, M["ore_iron"], material("rock_red", "#8e6e58", "#755744", scale=5, bump=0.7,
                                    detail=(1.2, "#a3836a")))


def silver_mine(M):
    mine(M, M["ore_silver"], material("rock_grey", "#8b8a88", "#6f6e6c", scale=5, bump=0.7,
                                      detail=(1.2, "#a3a29e")))


def furnace(M, x, y, r=0.18, h=0.5, mat=None):
    mat = mat or M["brick"]
    cylinder(x, y, 0, h, r, mat, n=16)
    cylinder(x, y, h, h + 0.02, r * 0.7, M["glow"], n=12)
    box(x - 0.06, y - r - 0.01, 0, x + 0.06, y - r + 0.03, 0.1, M["glow"], bevel=0)


def smelter_common(M, ingot, ore, white_smoke=False):
    lot_rect(3, 2, M["soot"] if not white_smoke else M["paving"])
    shed(M, -1.35, 0.05, 0.25, 0.9, 0.62, M["roof_slate"], axis="x")
    box(-1.35, 0.8, 0, 0.25, 0.9, 0.62, M["stone_dark"] if not white_smoke else M["travertine"],
        bevel=0)
    for x in (-1.0, -0.5, -0.05):
        if white_smoke:  # low cupellation hearths
            box(x - 0.17, 0.3, 0, x + 0.17, 0.62, 0.2, M["travertine"], bevel=0.01)
            cylinder(x, 0.46, 0.2, 0.215, 0.1, M["glow"], n=14)
        else:
            furnace(M, x, 0.45, 0.17, 0.48)
        smoke(M, x + 0.02, 0.5, 0.95 if not white_smoke else 0.4, 3, 1.1)
    point_light(-0.5, 0.0, 0.3, 50)
    # bellows, charcoal and ore heaps, ingot stacks, slag
    heap(M, 0.65, 0.55, 0.26, 0.2, M["charcoal"])
    heap(M, 1.15, 0.35, 0.2, 0.18, ore)
    for i in range(3):
        for j in range(4 - i):
            x = 0.5 + j * 0.1 + i * 0.05
            box(x - 0.045, -0.62, i * 0.04, x + 0.045, -0.4, i * 0.04 + 0.035, ingot, bevel=0.004)
    heap(M, 1.1, -0.6, 0.16, 0.1, M["slag"])
    blob(-0.75, -0.35, 0.08, 0.14, 0.09, 0.07, M["timber_light"], segs=10, rings=6, name="bellows")
    cart(-0.25, -0.6, M, load=False)
    heap(M, -0.3, -0.6, 0.12, 0.1, ore)


def smelter(M):
    smelter_common(M, M["iron"], M["ore_iron"])


def silver_smelter(M):
    smelter_common(M, M["silver"], M["ore_silver"], white_smoke=True)


def blacksmith(M):
    lot_rect(2, 2, M["soot"])
    x0, y0, x1, y1 = -0.9, -0.35, 0.35, 0.88
    zw = 0.5
    box(x0, y1 - 0.12, 0, x1, y1, zw, M["stone_dark"])
    box(x0, y0 + 0.2, 0, x0 + 0.12, y1, zw, M["stone_dark"])
    for x, y in [(x1 - 0.04, y0), (x0 + 0.04, y0), (x1 - 0.04, y0 + 0.62)]:
        box(x - 0.035, y - 0.035, 0, x + 0.035, y + 0.035, zw, M["timber"], bevel=0.005)
    gable(x0, y0 - 0.04, x1, y1, zw, 0.28, M["roof_slate"], axis="y", wall=M["stone_dark"])
    box(-0.75, 0.25, 0, -0.2, 0.7, 0.2, M["stone_dark"])
    box(-0.7, 0.3, 0.2, -0.25, 0.65, 0.235, M["glow"], bevel=0)
    box(-0.62, 0.5, 0, -0.32, 0.75, 1.02, M["stone_dark"])
    box(-0.6, 0.52, 1.0, -0.34, 0.73, 1.03, M["charcoal"], bevel=0)
    smoke(M, -0.45, 0.65, 1.1, 3, 1.0)
    point_light(-0.45, 0.2, 0.35, 60)
    kit.windows_y(y1 - 0.12, [-0.05], 0.22, 0.36, M, outward=-1)
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


def jeweler(M):
    lot_rect(2, 2, M["paving"])
    house_block(M, -0.92, 0.0, 0.35, 0.92, 0.55, M["plaster_warm"], M["roof_slate"], axis="x")
    awning(M, -0.88, 0.32, 0.0, 0.38, 0.5, mat=M["awning_purple"])
    # display table with gold and silver pieces, a tiny forge
    box(-0.75, -0.3, 0.14, 0.15, -0.12, 0.18, M["timber"], bevel=0.005)
    for x in (-0.7, 0.1):
        box(x - 0.015, -0.28, 0, x + 0.015, -0.14, 0.14, M["timber"], bevel=0)
    for i in range(6):
        mat = M["gold"] if i % 2 == 0 else M["silver"]
        blob(-0.65 + i * 0.14, -0.21, 0.2, 0.035, 0.035, 0.03, mat, segs=8, rings=4)
    box(0.5, -0.55, 0, 0.8, -0.25, 0.22, M["brick"], bevel=0.006)
    box(0.54, -0.51, 0.22, 0.76, -0.29, 0.235, M["glow"], bevel=0)
    point_light(0.65, -0.6, 0.3, 15)
    smoke(M, 0.65, -0.4, 0.3, 2, 0.7)
    for x in (-0.75, 0.75):
        kit.pot_plant(x, -0.8, M)


# ---------------------------------------------------------------- crafts

def potter(M, region=""):
    lot_rect(2, 2, M["dirt"] if region != "egypt" else M["sand"])
    house_block(M, -0.92, 0.15, 0.2, 0.92, 0.48, M["stone_dark"], M["roof_slate"], axis="y",
                region=region)
    # small updraft kiln with smoke
    blob(0.6, 0.5, 0, 0.26, 0.26, 0.34, M["clay"], segs=16, rings=10, top_only=True, name="kiln")
    box(0.52, 0.22, 0, 0.68, 0.26, 0.12, M["glow"], bevel=0)
    smoke(M, 0.6, 0.5, 0.38, 3, 1.1)
    # wheel, rows of pots and amphorae drying on boards
    cylinder(-0.45, -0.3, 0, 0.12, 0.04, M["timber"], n=8)
    cylinder(-0.45, -0.3, 0.12, 0.15, 0.12, M["timber_light"], n=16)
    blob(-0.45, -0.3, 0.2, 0.05, 0.05, 0.06, M["clay"], segs=10, rings=6)
    box(-0.15, -0.85, 0.08, 0.85, -0.65, 0.1, M["timber_light"], bevel=0)
    for i in range(7):
        blob(-0.08 + i * 0.13, -0.75, 0.15, 0.045, 0.045, 0.06, M["amphora"] if i % 2 else M["clay"],
             segs=10, rings=6)
    for i in range(4):
        amphora(0.35 + i * 0.13, -0.35, M)
    cylinder(-0.75, -0.75, 0, 0.1, 0.12, M["clay"], n=14)


def loom(M, x, y, cloth, s=1.0):
    for dx in (-0.14 * s, 0.14 * s):
        box(x + dx - 0.015, y - 0.015, 0, x + dx + 0.015, y + 0.015, 0.42 * s, M["timber"], bevel=0)
    box(x - 0.16 * s, y - 0.015, 0.4 * s, x + 0.16 * s, y + 0.015, 0.43 * s, M["timber"], bevel=0)
    box(x - 0.12 * s, y - 0.006, 0.12 * s, x + 0.12 * s, y + 0.006, 0.4 * s, cloth, bevel=0)
    for i in range(6):
        blob(x - 0.1 * s + i * 0.04 * s, y, 0.09 * s, 0.014, 0.014, 0.018, M["stone_dark"], segs=6,
             rings=4)


def weaver(M, region=""):
    lot_rect(2, 2, M["garden"] if region == "north" else M["dirt"])
    house_block(M, -0.92, 0.3, 0.3, 0.92, 0.5, M["plaster_house"], M["roof_slate"], axis="x",
                region=region)
    # looms under an open shed in front, bolts of cloth and wool skeins
    awning(M, -0.88, 0.28, 0.3, 0.25, 0.46, mat=M["roof_slate"])
    for i, c in enumerate((M["dye_red_cloth"], M["cloth"], M["dye_blue_cloth"])):
        loom(M, -0.6 + i * 0.4, -0.25, c, s=1.35)
    for i, c in enumerate((M["cloth"], M["dye_red_cloth"], M["dye_yellow_cloth"], M["cloth"])):
        cylinder(0.06, 0.62, -0.82 + i * 0.15, -0.7 + i * 0.15, 0.055, c, n=10, axis="y")
    for x, y in ((0.6, 0.4), (0.75, 0.25), (0.6, 0.15)):
        blob(x, y, 0.06, 0.08, 0.08, 0.06, M["wool"], segs=10, rings=6)
    cylinder(0.75, 0.6, 0, 0.1, 0.1, M["timber_light"], n=12)
    blob(0.75, 0.6, 0.1, 0.08, 0.08, 0.04, M["wool"], segs=10, rings=4, top_only=True)


def dyer(M):
    lot_rect(2, 2, M["dirt"])
    house_block(M, -0.92, 0.35, 0.3, 0.92, 0.48, M["plaster_house"], M["roof_slate"], axis="x")
    for (x, y, liq) in ((-0.65, -0.1, M["dye_red"]), (-0.25, -0.1, M["dye_purple"]),
                        (0.15, -0.1, M["dye_yellow"])):
        vat(M, x, y, 0.16, liq, h=0.16)
    smoke(M, -0.25, -0.1, 0.22, 2, 0.8)
    drying_line(M, -0.8, 0.85, -0.65, 0.42,
                [M["dye_red_cloth"], M["dye_purple_cloth"], M["dye_yellow_cloth"], M["dye_blue_cloth"]])
    drying_line(M, 0.55, 0.95, 0.15, 0.42, [M["dye_purple_cloth"], M["dye_red_cloth"]])


def dye_works(M):
    lot_rect(3, 2, M["dirt"])
    house_block(M, -1.42, 0.35, -0.2, 0.92, 0.55, M["plaster_house"], M["roof_slate"], axis="x")
    liqs = [M["dye_red"], M["dye_purple"], M["dye_yellow"], M["dye_blue"], M["dye_green"],
            M["dye_purple"]]
    for i, liq in enumerate(liqs):
        vat(M, -1.2 + (i % 3) * 0.38, -0.2 - (i // 3) * 0.38, 0.16, liq, h=0.15)
    smoke(M, -0.82, -0.2, 0.22, 2, 0.8)
    drying_line(M, 0.0, 1.38, 0.55, 0.48, [M["dye_red_cloth"], M["dye_yellow_cloth"],
                                           M["dye_blue_cloth"], M["dye_purple_cloth"],
                                           M["dye_green_cloth"]])
    drying_line(M, 0.1, 1.38, -0.15, 0.48, [M["dye_purple_cloth"], M["dye_red_cloth"],
                                            M["dye_green_cloth"], M["dye_yellow_cloth"]])
    for i in range(3):
        amphora(0.3 + i * 0.13, -0.75, M)


def glassworks(M):
    lot_rect(3, 2, M["soot"])
    house_block(M, -1.4, 0.3, -0.25, 0.92, 0.55, M["stone_dark"], M["roof_slate"], axis="x")
    # beehive glass furnace with glowing glory holes
    fx, fy = 0.25, 0.25
    blob(fx, fy, 0, 0.4, 0.4, 0.5, M["brick"], segs=18, rings=10, top_only=True, name="furnace")
    for dx in (-0.18, 0.05):
        box(fx + dx - 0.05, fy - 0.4, 0.12, fx + dx + 0.05, fy - 0.33, 0.22, M["glow"], bevel=0)
    box(fx + 0.36, fy - 0.05, 0.12, fx + 0.41, fy + 0.05, 0.22, M["glow"], bevel=0)
    point_light(fx - 0.05, fy - 0.6, 0.25, 50)
    cylinder(fx, fy, 0.45, 0.65, 0.07, M["brick"], n=10)
    smoke(M, fx + 0.03, fy + 0.04, 0.75, 3, 1.2)
    # shelves of coloured glass vessels
    box(-1.2, -0.6, 0.18, -0.2, -0.45, 0.2, M["timber_light"], bevel=0)
    box(-1.2, -0.6, 0.32, -0.2, -0.45, 0.34, M["timber_light"], bevel=0)
    for x in (-1.18, -0.22):
        box(x - 0.012, -0.6, 0, x + 0.012, -0.45, 0.34, M["timber"], bevel=0)
    for i in range(7):
        for z in (0.2, 0.34):
            mat = (M["glass"], M["glass_blue"])[(i + int(z * 10)) % 2]
            blob(-1.12 + i * 0.14, -0.525, z + 0.05, 0.035, 0.035, 0.05, mat, segs=8, rings=5)
    heap(M, 1.0, 0.5, 0.2, 0.12, M["sand"])
    heap(M, 1.05, -0.4, 0.18, 0.1, M["glass"])
    cart(0.75, -0.6, M, load=False)


def scriptorium(M):
    lot_rect(2, 2, M["paving"])
    house_block(M, -0.92, 0.1, 0.3, 0.92, 0.6, M["plaster"], M["roof_slate"], axis="x")
    kit.windows_y(0.1, [-0.6, -0.3, 0.0], 0.3, 0.45, M)
    # papyrus sheets drying on frames, scroll racks and a writing bench in the yard
    for i in range(3):
        x = -0.75 + i * 0.32
        for dx in (-0.12, 0.12):
            box(x + dx - 0.012, -0.6, 0, x + dx + 0.012, -0.58, 0.3, M["timber"], bevel=0)
        mesh("sheet", [(x - 0.12, -0.59, 0.28), (x + 0.12, -0.59, 0.28), (x + 0.12, -0.59, 0.08),
                       (x - 0.12, -0.59, 0.08)], [(0, 1, 2, 3)], M["sheet"], solidify=0.006)
    box(0.45, -0.65, 0, 0.85, -0.3, 0.32, M["timber"], bevel=0.005)
    for r in range(3):
        for c in range(3):
            cylinder(0.12 + r * 0.09, -0.62 + c * 0.13 + 0.06, 0.48, 0.82, 0.035, M["sheet"], n=8,
                     axis="x")
    box(0.4, 0.3, 0.14, 0.85, 0.5, 0.17, M["timber_light"], bevel=0.004)
    cylinder(0.62, 0.4, 0.17, 0.2, 0.04, M["sheet"], n=10)
    for x in (-0.85, 0.85):
        kit.pot_plant(x, -0.85, M)


def papyrus_tile(M, v):
    lot_rect(1, 1, M["marsh"])
    import random
    rnd = random.Random(100 + v)
    mesh("mud", [(-0.5, -0.5, 0.002), (0.5, -0.5, 0.002), (0.5, -0.2 + 0.1 * v, 0.002),
                 (-0.5, -0.35, 0.002)], [(0, 1, 2, 3)], M["mud"])
    for _ in range(9 + v):
        x, y = rnd.uniform(-0.38, 0.38), rnd.uniform(-0.38, 0.38)
        h = rnd.uniform(0.28, 0.45)
        for k in range(3):
            dx, dy = rnd.uniform(-0.04, 0.04), rnd.uniform(-0.04, 0.04)
            box(x + dx - 0.006, y + dy - 0.006, 0, x + dx + 0.006, y + dy + 0.006, h, M["papyrus"],
                bevel=0)
            blob(x + dx, y + dy, h + 0.02, 0.05, 0.05, 0.03, M["papyrus_head"], segs=8, rings=4)
    if v == 2:
        for i in range(3):
            box(-0.3 + i * 0.03, -0.42, 0.02, -0.28 + i * 0.03, -0.1, 0.05, M["papyrus_head"], bevel=0)


# ---------------------------------------------------------------- registry

def _b(fn, *a, **kw):
    return lambda M: fn(ext(M), *a, **kw)


SPRITES = {}


def add(name, fp, fn, lit=False, regions=False):
    SPRITES["bld/" + name] = dict(fp=fp, build=_b(fn), lit=lit)
    if regions:
        for r in ("east", "egypt", "north"):
            SPRITES["bld/%s@%s" % (name, r)] = dict(fp=fp, build=_b(fn, region=r))


add("bakery", (2, 2), bakery, lit=True, regions=True)
add("fishing_wharf", (2, 2), fishing_wharf)
add("garum_workshop", (2, 2), garum_workshop, lit=True)
add("mill", (2, 2), mill, lit=True, regions=True)
add("olive_press", (2, 2), olive_press, lit=True)
add("salt_works", (3, 2), salt_works)
add("winery", (3, 2), winery, lit=True)
add("blacksmith", (2, 2), blacksmith, lit=True)
add("brick_kiln", (2, 2), brick_kiln, lit=True)
add("carpenter", (2, 2), carpenter)
add("clay_pit", (2, 2), clay_pit)
add("concrete_works", (3, 3), concrete_works, lit=True)
add("dye_works", (3, 2), dye_works, lit=True)
add("dyer", (2, 2), dyer, lit=True)
add("glassworks", (3, 2), glassworks, lit=True)
add("iron_mine", (3, 3), iron_mine)
add("jeweler", (2, 2), jeweler, lit=True)
add("lime_kiln", (2, 2), lime_kiln)
add("limestone_quarry", (3, 3), limestone_quarry)
add("lumber_camp", (2, 2), lumber_camp)
add("marble_quarry", (3, 3), marble_quarry)
add("potter", (2, 2), potter, lit=True, regions=True)
add("pozzolana_pit", (2, 2), pozzolana_pit)
add("quarry", (3, 3), quarry)
add("sand_pit", (2, 2), sand_pit)
add("sawmill", (3, 2), sawmill)
add("scriptorium", (2, 2), scriptorium)
add("sculptor", (2, 2), sculptor, lit=True)
add("silver_mine", (3, 3), silver_mine)
add("silver_smelter", (3, 2), silver_smelter)
add("smelter", (3, 2), smelter)
add("stonemason", (2, 2), stonemason, lit=True)
add("weaver", (2, 2), weaver, lit=True, regions=True)
for _v in range(4):
    SPRITES["bld/papyrus_marsh_tile_%d" % _v] = dict(fp=(1, 1), build=_b(papyrus_tile, _v))
