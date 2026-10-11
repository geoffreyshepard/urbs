"""Military, safety and monument buildings (iso sprites), plus construction placeholders.

    Blender -b --python tools/artgen3d/run.py -- military [names...] [--samples N]

Accents: military = Roman red cloth and shields, iron, stone walls and timber palisades;
vigiles = red doors and bronze fire buckets; monuments = white marble, travertine and gold.
Footprints follow data/defs/buildings.json (fw along grid x / Blender +x, fh along grid y).
"""
import math

import kit
from kit import blob, box, column, cylinder, cypress, gable, hip, mesh, steps

# ---------------------------------------------------------------- materials


def mats(M):
    m = kit.material
    M["red"] = m("mil_red", "#a3271c", "#8a1f16", scale=14, bump=0.05, rough=0.8)
    M["wall_stone"] = m("wall_stone", "#c8bb9b", "#b2a483", kind="brick", scale=12,
                        brick=(0.22, 0.09), mortar="#887d66", bump=0.3)
    M["stucco"] = m("stucco", "#f3f0e8", "#e4dfd4", scale=9, bump=0.06, detail=(1.5, "#e0d8c8"))
    M["mudbrick"] = m("mudbrick", "#bb9264", "#a67d52", kind="brick", scale=12,
                      brick=(0.16, 0.07), mortar="#8d6c49", bump=0.25)
    M["sandstone"] = m("sandstone", "#d9bf8a", "#c6a974", kind="brick", scale=12,
                       brick=(0.3, 0.12), mortar="#a68d62", bump=0.2)
    M["thatch"] = m("thatch", "#a98b52", "#8f7342", scale=30, bump=0.7, detail=(2.0, "#9a8a6a"))
    M["shingle"] = m("shingle", "#6d5a48", "#584838", kind="tiles", scale=7, brick=(0.07, 0.06),
                     bump=0.5)
    M["stucco_roof"] = m("stucco_roof", "#ddd6c6", "#cbc2ae", scale=12, bump=0.15)
    M["mud_roof"] = m("mud_roof", "#c9a679", "#b38f62", scale=12, bump=0.2)
    M["blue"] = m("east_blue", "#3e6c9e", "#335a85", scale=14, bump=0.05)
    M["bronze"] = m("bronze", "#b06b34", "#91552a", scale=20, rough=0.35, metal=0.8, bump=0.0)
    M["leather"] = m("tent", "#d6c6a2", "#c2af88", scale=12, bump=0.1)
    M["hay"] = m("hay", "#d9bb62", "#c4a14a", scale=30, bump=0.6)
    M["horse"] = m("horse", "#6e4a2e", "#5a3b24", scale=18, bump=0.05, rough=0.6)
    M["horse_dark"] = m("horse_dark", "#3a2c22", "#2e231b", scale=18, bump=0.05, rough=0.6)
    M["sand"] = m("arena_sand", "#e0c286", "#cfae70", scale=10, bump=0.25, detail=(1.0, "#d9b880"))
    M["granite"] = m("granite", "#8b8580", "#76706b", scale=30, bump=0.05, rough=0.4)
    M["dome"] = m("dome_lead", "#a3a199", "#8f8d86", kind="brick", scale=8, brick=(0.4, 0.3),
                  mortar="#7c7a73", bump=0.15, rough=0.5)
    M["gold_roof"] = m("gold_roof", "#d9ae4a", "#b98f34", kind="tiles", scale=7,
                       brick=(0.085, 0.07), bump=0.5, metal=0.6, rough=0.35)
    M["rock"] = m("quay_rock", "#8a8378", "#6f685e", kind="brick", scale=9, brick=(0.35, 0.22),
                  mortar="#5a544b", bump=0.45)
    M["seat"] = m("seats", "#e3d8c2", "#cfc1a5", kind="brick", scale=12, brick=(0.6, 0.12),
                  mortar="#a89c84", bump=0.2)
    M["palm"] = m("palm", "#5f7d2e", "#4a6624", scale=18, bump=0.3)
    M["pine"] = m("pine_dark", "#2f4a2a", "#243a20", scale=18, bump=0.3)
    M["white_cloth"] = m("white_cloth", "#efe9dc", "#ddd5c4", scale=14, bump=0.05)
    M["green_cloth"] = m("green_cloth", "#3f7a3a", "#336430", scale=14, bump=0.05)
    M["blue_cloth"] = m("blue_cloth", "#3b5f9c", "#304e82", scale=14, bump=0.05)
    M["target"] = m("target", "#d8c48a", "#c5ae72", scale=30, bump=0.4)
    return M


# ---------------------------------------------------------------- helpers

def obox(cx, cy, z0, z1, length, thick, ang, mat, name="obox"):
    """Box of length x thick centred at (cx, cy), rotated `ang` radians about z."""
    c, s = math.cos(ang), math.sin(ang)
    pts = []
    for dx, dy in ((-length / 2, -thick / 2), (length / 2, -thick / 2), (length / 2, thick / 2),
                   (-length / 2, thick / 2)):
        pts.append((cx + dx * c - dy * s, cy + dx * s + dy * c))
    v = [(x, y, z0) for x, y in pts] + [(x, y, z1) for x, y in pts]
    f = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    return mesh(name, v, f, mat)


def tapered(x, y, z0, z1, w0, w1, mat, name="taper"):
    a, b = w0 / 2, w1 / 2
    v = [(x - a, y - a, z0), (x + a, y - a, z0), (x + a, y + a, z0), (x - a, y + a, z0),
         (x - b, y - b, z1), (x + b, y - b, z1), (x + b, y + b, z1), (x - b, y + b, z1)]
    f = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    return mesh(name, v, f, mat)


def pyramid(x, y, z0, w, h, mat, name="pyr"):
    a = w / 2
    v = [(x - a, y - a, z0), (x + a, y - a, z0), (x + a, y + a, z0), (x - a, y + a, z0),
         (x, y, z0 + h)]
    return mesh(name, v, [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4), (0, 3, 2, 1)], mat)


def cone(x, y, z0, r, h, mat, n=12, name="cone"):
    v = [(x + r * math.cos(2 * math.pi * i / n), y + r * math.sin(2 * math.pi * i / n), z0)
         for i in range(n)] + [(x, y, z0 + h)]
    f = [(i, (i + 1) % n, n) for i in range(n)] + [tuple(reversed(range(n)))]
    return mesh(name, v, f, mat, smooth=True)


def obelisk(x, y, z0, h, w, M):
    box(x - w * 0.9, y - w * 0.9, z0, x + w * 0.9, y + w * 0.9, z0 + w * 0.6, M["marble"])
    tapered(x, y, z0 + w * 0.6, z0 + h, w, w * 0.62, M["granite"], name="obelisk")
    pyramid(x, y, z0 + h, w * 0.62, w * 0.5, M["gold"])


def arch_bay(x0, x1, y0, y1, z_spring, z_top, mat, segs=8):
    """Solid spandrel over a round arch opening (x0..x1), depth y0..y1; open below."""
    r = (x1 - x0) / 2
    xm = x0 + r
    arc = [(xm - r * math.cos(math.pi * i / segs), z_spring + r * math.sin(math.pi * i / segs))
           for i in range(segs + 1)]
    v, f = [], []
    for y in (y0, y1):
        for (ax, az) in arc:
            v.append((ax, y, az))
            v.append((ax, y, z_top))
    n = segs + 1
    for i in range(segs):
        a, b = 2 * i, 2 * (i + 1)
        f.append((a, b, b + 1, a + 1))                  # front strip
        f.append((2 * n + a, 2 * n + a + 1, 2 * n + b + 1, 2 * n + b))  # back strip
        f.append((a, 2 * n + a, 2 * n + b, b))          # intrados
        f.append((a + 1, b + 1, 2 * n + b + 1, 2 * n + a + 1))  # top
    return mesh("arch", v, f, mat)


def arcade(xs, y0, y1, z0, z_spring, z_top, pier, mat, cap=None):
    """Arcade along x: piers at each x in xs, arches between them."""
    for x in xs:
        box(x - pier / 2, y0, z0, x + pier / 2, y1, z_top, mat, bevel=0.004)
    for a, b in zip(xs, xs[1:]):
        arch_bay(a + pier / 2, b - pier / 2, y0, y1, z_spring, z_top, mat)
    if cap is not None:
        box(xs[0] - pier / 2 - 0.03, y0 - 0.03, z_top, xs[-1] + pier / 2 + 0.03, y1 + 0.03,
            z_top + 0.05, cap, bevel=0.004)


def merlons_x(x0, x1, y, z, M, mat, step=0.125, w=0.06, d=0.05, h=0.07, phase=0.0625):
    """Merlons along x on the line y; positions periodic in world x (tiles stitch)."""
    k = math.ceil((x0 - phase) / step - 1e-6)
    while True:
        xc = phase + k * step
        if xc > x1 + 1e-6:
            break
        a, b = max(x0, xc - w / 2), min(x1, xc + w / 2)
        if b - a > 0.01:
            box(a, y - d / 2, z, b, y + d / 2, z + h, mat, bevel=0.003)
        k += 1


def merlons_y(y0, y1, x, z, M, mat, step=0.125, w=0.06, d=0.05, h=0.07, phase=0.0625):
    k = math.ceil((y0 - phase) / step - 1e-6)
    while True:
        yc = phase + k * step
        if yc > y1 + 1e-6:
            break
        a, b = max(y0, yc - w / 2), min(y1, yc + w / 2)
        if b - a > 0.01:
            box(x - d / 2, a, z, x + d / 2, b, z + h, mat, bevel=0.003)
        k += 1


def crenel_block(x0, y0, x1, y1, z0, z1, M, mat, step=0.125):
    box(x0, y0, z0, x1, y1, z1 - 0.012, mat, bevel=0.006)
    box(x0 - 0.02, y0 - 0.02, z1 - 0.05, x1 + 0.02, y1 + 0.02, z1, mat, bevel=0.004)
    merlons_x(x0, x1, y0 + 0.025, z1, M, mat, step)
    merlons_x(x0, x1, y1 - 0.025, z1, M, mat, step)
    merlons_y(y0 + 0.05, y1 - 0.05, x0 + 0.025, z1, M, mat, step)
    merlons_y(y0 + 0.05, y1 - 0.05, x1 - 0.025, z1, M, mat, step)


def banner(x, y, z0, h, M, cloth="red", w=0.13, ch=0.22):
    cylinder(x, y, z0, z0 + h, 0.012, M["timber"], n=6, name="pole")
    box(x - 0.005, y - 0.012, z0 + h - ch - 0.03, x + w, y - 0.004, z0 + h - 0.03, M[cloth],
        bevel=0)
    box(x - 0.02, y - 0.015, z0 + h - 0.035, x + w + 0.015, y + 0.0, z0 + h - 0.02, M["gold"],
        bevel=0)
    blob(x, y, z0 + h + 0.02, 0.025, 0.025, 0.025, M["gold"], segs=6, rings=4, name="finial")


def aquila(x, y, M, h=0.7):
    cylinder(x, y, 0, h, 0.014, M["timber"], n=6, name="pole")
    for i, z in enumerate((h * 0.45, h * 0.6, h * 0.75)):
        cylinder(x, y, z, z + 0.012, 0.045, M["gold"], n=10, name="phalera")
    box(x - 0.08, y - 0.01, h - 0.02, x + 0.08, y + 0.01, h + 0.0, M["gold"], bevel=0)
    blob(x, y, h + 0.06, 0.035, 0.03, 0.05, M["gold"], segs=8, rings=4, name="eagle")
    box(x - 0.09, y - 0.01, h + 0.04, x + 0.09, y + 0.0, h + 0.09, M["gold"], bevel=0)
    box(x - 0.07, y - 0.02, h * 0.28, x + 0.07, y - 0.008, h * 0.42, M["red"], bevel=0)


def shield(x, y, z, M, face="y", s=1.0):
    if face == "y":
        box(x - 0.055 * s, y - 0.012, z, x + 0.055 * s, y, z + 0.16 * s, M["red"], bevel=0.008)
        blob(x, y - 0.014, z + 0.08 * s, 0.02 * s, 0.01, 0.02 * s, M["gold"], segs=6, rings=4,
             name="boss")
    else:
        box(x, y - 0.055 * s, z, x + 0.012, y + 0.055 * s, z + 0.16 * s, M["red"], bevel=0.008)
        blob(x + 0.014, y, z + 0.08 * s, 0.01, 0.02 * s, 0.02 * s, M["gold"], segs=6, rings=4,
             name="boss")


def spear_rack(x, y, M, n=5, w=0.4):
    box(x - w / 2, y - 0.02, 0.12, x + w / 2, y + 0.02, 0.14, M["timber"], bevel=0)
    for px in (x - w / 2, x + w / 2):
        box(px - 0.015, y - 0.015, 0, px + 0.015, y + 0.015, 0.16, M["timber"], bevel=0)
    for i in range(n):
        sx = x - w / 2 + 0.05 + i * (w - 0.1) / max(1, n - 1)
        cylinder(sx, y, 0, 0.42, 0.006, M["timber_light"], n=5, name="spear")
        cone(sx, y, 0.42, 0.012, 0.06, M["iron"], n=6)


def helmet(x, y, z, M):
    blob(x, y, z + 0.02, 0.035, 0.035, 0.035, M["iron"], segs=8, rings=4, top_only=True,
         name="helm")
    box(x - 0.004, y - 0.03, z + 0.05, x + 0.004, y + 0.03, z + 0.075, M["red"], bevel=0)


def bucket(x, y, M):
    cylinder(x, y, 0, 0.06, 0.03, M["bronze"], n=10, name="bucket")


def barrel(x, y, M, r=0.06, h=0.12):
    cylinder(x, y, 0, h, r, M["timber_light"], n=12, name="barrel")
    for z in (h * 0.2, h * 0.8):
        cylinder(x, y, z - 0.006, z + 0.006, r + 0.004, M["iron"], n=12, name="hoop")


def crate(x, y, z, M, s=0.09):
    box(x - s / 2, y - s / 2, z, x + s / 2, y + s / 2, z + s, M["timber_light"], bevel=0.005)


def tent(x, y, M, w=0.32, d=0.4, h=0.26, mat=None):
    mat = mat or M["leather"]
    v = [(x - w / 2, y - d / 2, 0), (x + w / 2, y - d / 2, 0), (x, y - d / 2, h),
         (x - w / 2, y + d / 2, 0), (x + w / 2, y + d / 2, 0), (x, y + d / 2, h)]
    f = [(0, 1, 2), (3, 5, 4), (0, 2, 5, 3), (1, 4, 5, 2)]
    mesh("tent", v, f, mat)
    box(x - 0.03, y - d / 2 - 0.004, 0, x + 0.03, y - d / 2 + 0.002, h * 0.6, M["dark"], bevel=0)


def horse(x, y, M, ang=0.0, dark=False):
    """A small horse standing at (x, y) facing direction ang (0 = +x)."""
    mat = M["horse_dark"] if dark else M["horse"]
    c, s = math.cos(ang), math.sin(ang)

    def p(dx, dy):
        return x + dx * c - dy * s, y + dx * s + dy * c

    bx, by = p(0, 0)
    blob(bx, by, 0.2, 0.13 if abs(c) > 0.7 else 0.07, 0.07 if abs(c) > 0.7 else 0.13, 0.06, mat,
         segs=10, rings=6, name="horse_body")
    for dx in (-0.09, 0.09):
        for dy in (-0.035, 0.035):
            lx, ly = p(dx, dy)
            cylinder(lx, ly, 0, 0.17, 0.014, mat, n=6, name="leg")
    nx, ny = p(0.14, 0)
    blob(nx, ny, 0.28, 0.035, 0.035, 0.07, mat, segs=8, rings=4, name="neck")
    hx, hy = p(0.18, 0)
    blob(hx, hy, 0.33, 0.05 if abs(c) > 0.7 else 0.03, 0.03 if abs(c) > 0.7 else 0.05, 0.03,
         mat, segs=8, rings=4, name="head")
    tx, ty = p(-0.15, 0)
    blob(tx, ty, 0.18, 0.02, 0.02, 0.06, M["horse_dark"], segs=6, rings=4, name="tail")


def palm(x, y, h, M):
    cylinder(x, y, 0, h, 0.025, M["trunk"], n=6, name="palm_trunk")
    for i in range(7):
        a = 2 * math.pi * i / 7
        obox(x + math.cos(a) * 0.12, y + math.sin(a) * 0.12, h - 0.05, h - 0.02, 0.26, 0.06, a,
             M["palm"], name="frond")
    blob(x, y, h, 0.06, 0.06, 0.05, M["palm"], segs=8, rings=4, name="palm_crown")


def pine(x, y, h, M):
    cylinder(x, y, 0, h * 0.25, 0.03, M["trunk"], n=6)
    cone(x, y, h * 0.15, 0.17, h * 0.55, M["pine"])
    cone(x, y, h * 0.45, 0.12, h * 0.55, M["pine"])


def statue(x, y, z0, M, h=0.32, mat=None):
    mat = mat or M["marble"]
    box(x - 0.07, y - 0.07, z0, x + 0.07, y + 0.07, z0 + 0.14, M["marble"], bevel=0.006)
    blob(x, y, z0 + 0.14 + h * 0.4, 0.04, 0.035, h * 0.4, mat, segs=8, rings=5, name="statue")
    blob(x, y, z0 + 0.14 + h * 0.86, 0.025, 0.025, 0.03, mat, segs=6, rings=4, name="head")


def quadriga(x, y, z0, M, s=1.0):
    """Gilded four-horse chariot group facing -y (toward the viewer's lower left)."""
    g = M["gold"]
    box(x - 0.1 * s, y + 0.02 * s, z0, x + 0.1 * s, y + 0.16 * s, z0 + 0.1 * s, g, bevel=0.01)
    for i in range(4):
        hx = x - 0.15 * s + i * 0.1 * s
        blob(hx, y - 0.08 * s, z0 + 0.12 * s, 0.03 * s, 0.09 * s, 0.045 * s, g, segs=8, rings=4,
             name="qhorse")
        blob(hx, y - 0.18 * s, z0 + 0.18 * s, 0.022 * s, 0.035 * s, 0.05 * s, g, segs=6, rings=4,
             name="qhead")
        for dy in (-0.14, -0.02):
            cylinder(hx, y + dy * s, z0, z0 + 0.09 * s, 0.008 * s, g, n=5)
    blob(x, y + 0.09 * s, z0 + 0.2 * s, 0.035 * s, 0.03 * s, 0.1 * s, g, segs=8, rings=4,
         name="driver")


# ---------------------------------------------------------------- regional styles

def style(M, region):
    """Wall/roof kit per region: plain Roman, east (stucco, flat), egypt (mudbrick, flat),
    north (timber frame, thatch)."""
    if region == "east":
        return dict(wall=M["stucco"], roof=None, door=M["blue"], trim=M["blue"], tree="cypress",
                    ground=M["paving"], flat=M["stucco_roof"])
    if region == "egypt":
        return dict(wall=M["mudbrick"], roof=None, door=M["timber"], trim=M["sandstone"],
                    tree="palm", ground=M["sand"], flat=M["mud_roof"])
    if region == "north":
        return dict(wall=M["plaster_humble"], roof=M["thatch"], door=M["timber"],
                    trim=M["timber"], tree="pine", ground=M["dirt"], frame=True, pitch=1.6)
    return dict(wall=M["plaster"], roof=M["roof"], door=M["red"], trim=M["band_red"],
                tree="cypress", ground=M["dirt"])


def tree(S, x, y, M, h=0.55):
    t = S["tree"]
    if t == "palm":
        palm(x, y, h, M)
    elif t == "pine":
        pine(x, y, h * 1.1, M)
    else:
        cypress(x, y, h, M)


def block(x0, y0, x1, y1, h, S, M, axis="x", roof_h=None):
    """Walls + roof for one building block in the given regional style."""
    box(x0, y0, 0, x1, y1, h, S["wall"])
    box(x0 - 0.01, y0 - 0.01, 0, x1 + 0.01, y1 + 0.01, 0.06, S["trim"], bevel=0.004)
    if S.get("frame"):
        for x in [x0 + (x1 - x0) * i / 4 for i in range(5)]:
            box(x - 0.015, y0 - 0.012, 0.06, x + 0.015, y0 + 0.002, h, M["timber"], bevel=0)
        for y in [y0 + (y1 - y0) * i / 3 for i in range(4)]:
            box(x1 - 0.002, y - 0.015, 0.06, x1 + 0.012, y + 0.015, h, M["timber"], bevel=0)
        box(x0, y0 - 0.012, h * 0.5, x1, y0 + 0.002, h * 0.5 + 0.025, M["timber"], bevel=0)
    if S["roof"] is None:
        # flat roof with a low parapet
        box(x0 - 0.02, y0 - 0.02, h, x1 + 0.02, y1 + 0.02, h + 0.06, S["wall"], bevel=0.004)
        box(x0 + 0.04, y0 + 0.04, h + 0.02, x1 - 0.04, y1 - 0.04, h + 0.05, S["flat"], bevel=0)
    else:
        span = (y1 - y0) if axis == "x" else (x1 - x0)
        rh = roof_h if roof_h is not None else span * 0.3 * S.get("pitch", 1.0)
        gable(x0, y0, x1, y1, h, rh, S["roof"], axis=axis, over=0.06, wall=S["wall"])


def door(x, y, z0, w, h, mat):
    box(x - w / 2, y - 0.02, z0, x + w / 2, y + 0.004, z0 + h, mat, bevel=0.003)


# ---------------------------------------------------------------- walls (line pieces)

WALL_H = 0.74
WALL_T = 0.26
PIER = 0.36


def wall_centre(M, holdout=False):
    p = PIER / 2
    before = set(o.name for o in kit.bpy.data.objects)
    box(-p, -p, 0, p, p, WALL_H + 0.068, M["wall_stone"], bevel=0.006)
    box(-p - 0.02, -p - 0.02, WALL_H + 0.03, p + 0.02, p + 0.02, WALL_H + 0.08, M["wall_stone"],
        bevel=0.004)
    box(-p - 0.015, -p - 0.015, 0, p + 0.015, p + 0.015, 0.08, M["stone_dark"], bevel=0.004)
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = sx * (p - 0.03), sy * (p - 0.03)
            box(cx - 0.035, cy - 0.035, WALL_H + 0.08, cx + 0.035, cy + 0.035, WALL_H + 0.16,
                M["wall_stone"], bevel=0.003)
    if holdout:
        for o in kit.bpy.data.objects:
            if o.name not in before:
                o.is_holdout = True


def wall_arm(M, side):
    """Wall arm from the centre pier to the tile edge in grid direction side (n/e/s/w)."""
    p, t = PIER / 2, WALL_T / 2
    mat = M["wall_stone"]
    if side in ("e", "w"):
        x0, x1 = (p - 0.01, 0.5) if side == "e" else (-0.5, -p + 0.01)
        box(x0, -t, 0, x1, t, WALL_H, mat, bevel=0)
        box(x0, -t - 0.015, 0, x1, t + 0.015, 0.08, M["stone_dark"], bevel=0)
        box(x0, -t - 0.012, WALL_H - 0.05, x1, -t + 0.02, WALL_H, mat, bevel=0)
        merlons_x(x0, x1, -t + 0.025, WALL_H, M, mat)
        merlons_x(x0, x1, t - 0.025, WALL_H, M, mat)
    else:
        # grid north = Blender +y
        y0, y1 = (p - 0.01, 0.5) if side == "n" else (-0.5, -p + 0.01)
        box(-t, y0, 0, t, y1, WALL_H, mat, bevel=0)
        box(-t - 0.015, y0, 0, t + 0.015, y1, 0.08, M["stone_dark"], bevel=0)
        box(t - 0.02, y0, WALL_H - 0.05, t + 0.012, y1, WALL_H, mat, bevel=0)
        merlons_y(y0, y1, -t + 0.025, WALL_H, M, mat)
        merlons_y(y0, y1, t - 0.025, WALL_H, M, mat)
    if side in ("n", "w"):
        # Back arms: the centre pier hides their inner end, whatever order the game draws in.
        wall_centre(M, holdout=True)


def wall_c(M):
    mats(M)
    wall_centre(M)


def wall_piece(side):
    def build(M):
        mats(M)
        wall_arm(M, side)
    return build


# ---------------------------------------------------------------- small posts

def vigiles_post(M, region=None):
    mats(M)
    S = style(M, region)
    kit.lot_rect(1, 1, M["paving"] if region != "egypt" else M["sand"])
    block(-0.4, -0.08, 0.38, 0.42, 0.5, S, M, axis="x")
    door(-0.12, -0.08, 0.04, 0.16, 0.3, M["red"])
    box(-0.22, -0.1, 0.36, -0.02, -0.08, 0.44, M["red"], bevel=0)  # red sign board
    kit.windows_y(-0.08, [0.18], 0.24, 0.36, M, w=0.07)
    # buckets on a rack, ladder, water barrel
    box(0.05, -0.32, 0.0, 0.42, -0.28, 0.1, M["timber"], bevel=0)
    for i in range(4):
        bucket(0.1 + i * 0.09, -0.36, M)
    barrel(-0.36, -0.32, M, r=0.07, h=0.15)
    # ladder lying along the east wall
    for y in (-0.06, 0.06):
        box(0.4, y - 0.012 + 0.12, 0, 0.44, y + 0.012 + 0.12, 0.03, M["timber_light"], bevel=0)
    for k in range(4):
        box(0.4, 0.04 + k * 0.05, 0, 0.44, 0.05 + k * 0.05, 0.035, M["timber_light"], bevel=0)


def guard_post(M, region=None):
    mats(M)
    S = style(M, region)
    kit.lot_rect(2, 2, S["ground"])
    block(-0.9, -0.05, 0.25, 0.85, 0.55, S, M, axis="x")
    door(-0.35, -0.05, 0.06, 0.2, 0.34, S["door"])
    kit.windows_y(-0.05, [-0.72, 0.05], 0.3, 0.44, M, w=0.08)
    # watchtower
    tw = S["wall"] if region in ("east", "egypt") else M["wall_stone"]
    box(0.35, 0.05, 0, 0.88, 0.6, 1.05, tw, bevel=0.006)
    kit.windows_y(0.05, [0.62], 0.7, 0.85, M, w=0.06)
    if S["roof"] is None:
        crenel_block(0.33, 0.03, 0.9, 0.62, 1.0, 1.08, M, tw)
    else:
        hip(0.33, 0.03, 0.9, 0.62, 1.05, 0.28, S["roof"], over=0.06)
    banner(0.86, 0.04, 1.0, 0.5, M)
    shield(-0.62, -0.06, 0.08, M)
    shield(-0.08, -0.06, 0.08, M)
    spear_rack(0.45, -0.45, M, n=4, w=0.3)
    # brazier
    cylinder(-0.55, -0.6, 0, 0.16, 0.012, M["iron"], n=5)
    cylinder(-0.55, -0.6, 0.16, 0.2, 0.06, M["iron"], n=10)
    blob(-0.55, -0.6, 0.22, 0.045, 0.045, 0.05, M["fire"], segs=8, rings=4)
    tree(S, -0.85, -0.75, M, 0.5)


def militia_post(M, region=None):
    mats(M)
    S = style(M, region)
    kit.lot_rect(2, 2, S["ground"])
    block(-0.85, 0.15, 0.85, 0.85, 0.55, S, M, axis="x")
    door(0.0, 0.15, 0.06, 0.24, 0.34, S["door"])
    kit.windows_y(0.15, [-0.55, -0.25, 0.4, 0.65], 0.3, 0.44, M, w=0.08)
    # palisade yard in front
    pal = M["timber"]
    for i in range(16):
        x = -0.92 + i * 0.123
        if -0.2 < x < 0.2:
            continue
        cylinder(x, -0.9, 0, 0.3, 0.035, pal, n=6, name="stake")
        cone(x, -0.9, 0.3, 0.035, 0.05, pal, n=6)
    for j in range(8):
        y = -0.9 + j * 0.13
        for x in (-0.92, 0.92):
            cylinder(x, y, 0, 0.3, 0.035, pal, n=6, name="stake")
            cone(x, y, 0.3, 0.035, 0.05, pal, n=6)
    spear_rack(-0.5, -0.35, M, n=5, w=0.4)
    for i in range(3):
        shield(0.35 + i * 0.13, -0.3, 0.0, M)
    banner(0.75, -0.6, 0, 0.8, M)
    helmet(0.55, -0.6, 0.0, M)
    crate(0.2, -0.65, 0, M)


def armory(M):
    mats(M)
    kit.lot_rect(3, 2, M["soot"])
    x0, y0, x1, y1 = -1.35, -0.15, 1.35, 0.9
    box(x0, y0, 0, x1, y1, 0.78, M["wall_stone"])
    box(x0 - 0.01, y0 - 0.01, 0, x1 + 0.01, y1 + 0.01, 0.1, M["stone_dark"], bevel=0.004)
    box(x0 - 0.01, y0 - 0.012, 0.66, x1 + 0.01, y0, 0.71, M["red"], bevel=0)  # red frieze
    gable(x0, y0, x1, y1, 0.78, 0.36, M["roof"], axis="x", over=0.08, wall=M["wall_stone"])
    # big red double doors with iron studs
    box(-0.32, y0 - 0.03, 0.1, 0.32, y0 + 0.004, 0.62, M["wall_stone"], bevel=0.004)
    box(-0.27, y0 - 0.045, 0.1, -0.01, y0, 0.56, M["red"], bevel=0.004)
    box(0.01, y0 - 0.045, 0.1, 0.27, y0, 0.56, M["red"], bevel=0.004)
    for x in (-0.14, 0.14):
        box(x - 0.1, y0 - 0.05, 0.32, x + 0.1, y0 - 0.044, 0.34, M["iron"], bevel=0)
    kit.windows_y(y0, [-1.05, -0.7, 0.7, 1.05], 0.4, 0.56, M, w=0.08)
    kit.windows_x(x1, [0.15, 0.6], 0.4, 0.56, M, w=0.08)
    # yard: spear racks, shield stacks, helmets, crates
    spear_rack(-0.95, -0.6, M, n=6, w=0.5)
    for i in range(4):
        shield(0.45 + i * 0.13, -0.55, 0.0, M)
    for i in range(3):
        helmet(0.55 + i * 0.12, -0.8, 0.0, M)
    crate(1.15, -0.55, 0, M)
    crate(1.15, -0.75, 0, M)
    crate(1.15, -0.65, 0.09, M)
    banner(-1.35, -0.85, 0, 0.85, M)


def tower(M):
    mats(M)
    kit.lot_rect(2, 2, M["dirt"])
    S = M["wall_stone"]
    box(-0.62, -0.62, 0, 0.62, 0.62, 0.12, M["stone_dark"], bevel=0.006)
    crenel_block(-0.55, -0.55, 0.55, 0.55, 0.12, 1.45, M, S)
    box(-0.56, -0.56, 0.95, 0.56, 0.56, 1.0, S, bevel=0.004)
    door(0.0, -0.55, 0.12, 0.2, 0.34, M["timber"])
    kit.windows_y(-0.55, [-0.25, 0.25], 0.65, 0.85, M, w=0.05)
    kit.windows_y(-0.55, [0.0], 1.1, 1.3, M, w=0.05)
    kit.windows_x(0.55, [-0.25, 0.25], 0.65, 0.85, M, w=0.05)
    kit.windows_x(0.55, [0.0], 1.1, 1.3, M, w=0.05)
    banner(0.0, 0.0, 1.45, 0.55, M, w=0.17, ch=0.28)
    shield(-0.3, -0.56, 0.15, M)


def gate(M):
    mats(M)
    kit.lot_rect(2, 1, M["dirt"])
    S = M["wall_stone"]
    # road through the gate
    box(-0.3, -0.5, 0, 0.3, 0.5, 0.012, M["road"], bevel=0)
    for sx in (-1, 1):
        x0, x1 = (-0.98, -0.42) if sx < 0 else (0.42, 0.98)
        crenel_block(x0, -0.42, x1, 0.42, 0.0, 1.28, M, S)
        kit.windows_y(-0.42, [(x0 + x1) / 2], 0.75, 0.95, M, w=0.06)
        kit.windows_x(x1, [0.0], 0.75, 0.95, M, w=0.06)
        banner((x0 + x1) / 2, -0.43, 1.28, 0.45, M)
    # arch over the gateway
    arch_bay(-0.42, 0.42, -0.34, 0.34, 0.5, 0.98, S, segs=10)
    box(-0.44, -0.36, 0.98, 0.44, 0.36, 1.04, S, bevel=0.004)
    merlons_x(-0.42, 0.42, -0.31, 1.04, M, S)
    merlons_x(-0.42, 0.42, 0.31, 1.04, M, S)
    # open gate leaves and a red-and-gold plaque
    box(-0.4, 0.2, 0, -0.37, 0.34, 0.55, M["timber"], bevel=0)
    box(0.37, 0.2, 0, 0.4, 0.34, 0.55, M["timber"], bevel=0)
    box(-0.2, -0.36, 0.84, 0.2, -0.34, 0.94, M["red"], bevel=0)
    box(-0.17, -0.365, 0.87, 0.17, -0.35, 0.91, M["gold"], bevel=0)


def barracks(M, region=None):
    mats(M)
    S = style(M, region)
    kit.lot_rect(4, 4, M["dirt"] if region != "east" else M["paving"])
    # drill yard
    box(-1.05, -1.55, 0, 1.05, 0.75, 0.01, M["paving"] if region is None else S["ground"],
        bevel=0)
    # back block and two wings (U opening to the front)
    block(-1.85, 0.85, 1.85, 1.85, 0.62, S, M, axis="x")
    block(-1.85, -1.6, -1.15, 0.85, 0.62, S, M, axis="y")
    block(1.15, -1.6, 1.85, 0.85, 0.62, S, M, axis="y")
    for x in (-1.3, -0.65, 0.0, 0.65, 1.3):
        door(x, 0.85, 0.05, 0.13, 0.3, S["door"])
        kit.windows_y(0.85, [x + 0.22], 0.32, 0.46, M, w=0.07)
    for y in (-1.25, -0.6, 0.05, 0.6):
        box(-1.15 - 0.004, y - 0.06, 0.05, -1.15 + 0.02, y + 0.06, 0.35, S["door"], bevel=0)
        kit.windows_x(1.85, [y + 0.2], 0.32, 0.46, M, w=0.07)
        box(1.13, y - 0.06, 0.05, 1.152, y + 0.06, 0.35, S["door"], bevel=0)
    kit.windows_y(-1.6, [-1.5, 1.5], 0.32, 0.46, M, w=0.08)
    kit.windows_y(0.85, [-1.6, 1.6], 0.32, 0.46, M, w=0.08)
    for x in (-1.0, -0.35, 0.3, 0.95):
        shield(x, 0.84, 0.08, M)
    aquila(0.0, -0.35, M, h=0.75)
    spear_rack(-0.55, -1.1, M, n=5, w=0.4)
    spear_rack(0.55, -1.1, M, n=5, w=0.4)
    banner(-1.5, -1.62, 0.62, 0.45, M)
    banner(1.5, -1.62, 0.62, 0.45, M)
    tree(S, -1.75, -1.85, M, 0.45)
    tree(S, 1.8, -1.85, M, 0.45)


def training_ground(M):
    mats(M)
    kit.lot_rect(5, 5, M["sand"])
    # fence around the field
    kit.fence([(-2.4, -2.4), (2.4, -2.4)], M["timber"], h=0.14)
    kit.fence([(-2.4, -2.4), (-2.4, 2.4)], M["timber"], h=0.14)
    kit.fence([(2.4, -2.4), (2.4, 2.4)], M["timber"], h=0.14)
    kit.fence([(-2.4, 2.4), (2.4, 2.4)], M["timber"], h=0.14)
    # wooden practice posts (pali)
    for i in range(4):
        for j in range(3):
            x, y = -1.6 + i * 0.55, -1.5 + j * 0.65
            cylinder(x, y, 0, 0.36, 0.035, M["timber_light"], n=8, name="palus")
    # archery butts with red bullseyes on the east side
    for j in range(3):
        y = -1.4 + j * 0.75
        box(1.75, y - 0.18, 0, 1.95, y + 0.18, 0.32, M["hay"], bevel=0.02)
        cylinder(0.12, y, 1.73, 1.75, 0.12, M["target"], n=14, axis="x", name="butt")
        cylinder(0.12, y, 1.72, 1.735, 0.06, M["red"], n=12, axis="x", name="eye")
    # shed and racks at the back
    box(-2.2, 1.35, 0, -0.7, 2.2, 0.5, M["timber"], bevel=0.006)
    gable(-2.2, 1.35, -0.7, 2.2, 0.5, 0.26, M["roof"], axis="x", over=0.06, wall=M["timber"])
    door(-1.45, 1.35, 0.0, 0.18, 0.3, M["dark"])
    spear_rack(0.2, 1.7, M, n=6, w=0.5)
    for i in range(5):
        shield(0.8 + i * 0.13, 1.75, 0.0, M)
    banner(2.2, 2.1, 0, 0.9, M)
    banner(-2.2, -2.15, 0, 0.9, M)
    # timber watch platform and a sand ring for sparring
    for x in (-2.0, -1.5):
        for y in (-2.0, -1.5):
            box(x - 0.03, y - 0.03, 0, x + 0.03, y + 0.03, 0.75, M["timber"], bevel=0)
    box(-2.08, -2.08, 0.72, -1.42, -1.42, 0.78, M["timber_light"], bevel=0.004)
    banner(-1.5, -2.06, 0.78, 0.4, M)
    v = []
    for i in range(24):
        t = 2 * math.pi * i / 24
        v.append((0.95 + 0.55 * math.cos(t), 0.75 + 0.55 * math.sin(t), 0.012))
    mesh("ring", v, [tuple(range(24))], M["dirt"])
    for i in range(12):
        t = 2 * math.pi * i / 12
        cylinder(0.95 + 0.58 * math.cos(t), 0.75 + 0.58 * math.sin(t), 0, 0.06, 0.02,
                 M["stone_dark"], n=6)
    for i in range(3):
        shield(-0.9 + i * 0.13, -2.15, 0.0, M)
    for i in range(3):
        helmet(-0.85 + i * 0.12, -1.95, 0.0, M)
    # straw dummies
    for x in (-0.3, 0.4):
        cylinder(x, 0.8, 0, 0.2, 0.015, M["timber"], n=6)
        blob(x, 0.8, 0.3, 0.06, 0.05, 0.11, M["hay"], segs=8, rings=5, name="dummy")
        box(x - 0.12, 0.79, 0.3, x + 0.12, 0.81, 0.33, M["timber"], bevel=0)


def stables(M):
    mats(M)
    kit.lot_rect(4, 3, M["dirt"])
    x0, y0, x1, y1 = -1.85, 0.3, 1.85, 1.35
    box(x0, y0, 0, x1, y1, 0.55, M["timber_light"])
    box(x0 - 0.01, y0 - 0.01, 0, x1 + 0.01, y1 + 0.01, 0.08, M["stone_dark"], bevel=0.004)
    for i in range(9):
        x = x0 + i * (x1 - x0) / 8
        box(x - 0.02, y0 - 0.02, 0.0, x + 0.02, y0 + 0.002, 0.55, M["timber"], bevel=0)
    for i in range(8):
        x = x0 + (i + 0.5) * (x1 - x0) / 8
        box(x - 0.12, y0 - 0.025, 0.08, x + 0.12, y0 + 0.002, 0.3, M["timber"], bevel=0.003)
        box(x - 0.12, y0 - 0.01, 0.3, x + 0.12, y0 + 0.002, 0.46, M["dark"], bevel=0)
    gable(x0, y0, x1, y1, 0.55, 0.32, M["roof_timber"], axis="x", over=0.09,
          wall=M["timber_light"])
    # paddock with horses
    pts = [(-1.85, 0.2), (-1.85, -1.4), (1.2, -1.4), (1.2, 0.2)]
    kit.fence(pts, M["timber"], h=0.16)
    horse(-1.1, -0.5, M, ang=0.3)
    horse(-0.3, -0.95, M, ang=2.6, dark=True)
    horse(0.55, -0.35, M, ang=-0.6)
    # hay, trough, water
    for (hx, hy) in ((1.5, -0.4), (1.5, -0.75), (1.72, -0.58)):
        box(hx - 0.12, hy - 0.09, 0, hx + 0.12, hy + 0.09, 0.14, M["hay"], bevel=0.02)
    box(1.42, -0.62, 0.14, 1.66, -0.44, 0.27, M["hay"], bevel=0.02)
    box(-1.6, 0.0, 0, -0.9, 0.12, 0.1, M["timber"], bevel=0.005)
    box(-1.57, 0.02, 0.06, -0.93, 0.1, 0.095, M["water"], bevel=0)
    banner(1.75, -1.25, 0, 0.75, M)


def onager(x, y, M, s=1.0):
    t = M["timber_light"]
    box(x - 0.25 * s, y - 0.12 * s, 0.03, x + 0.25 * s, y - 0.09 * s, 0.07, t, bevel=0)
    box(x - 0.25 * s, y + 0.09 * s, 0.03, x + 0.25 * s, y + 0.12 * s, 0.07, t, bevel=0)
    for dx in (-0.2, 0.2):
        box(x + dx * s - 0.03, y - 0.12 * s, 0.03, x + dx * s + 0.03, y + 0.12 * s, 0.06, t,
            bevel=0)
    for dy in (-0.12, 0.12):
        cylinder(0.04, x - 0.18 * s, y + dy * s - 0.01, y + dy * s + 0.01, 0.045, M["timber"],
                 n=10, axis="y")
        cylinder(0.04, x + 0.18 * s, y + dy * s - 0.01, y + dy * s + 0.01, 0.045, M["timber"],
                 n=10, axis="y")
    for dy in (-0.1, 0.1):
        obox(x, y + dy * s, 0.05, 0.3 * s, 0.03, 0.03, 0, t)
    box(x - 0.02, y - 0.11 * s, 0.26 * s, x + 0.02, y + 0.11 * s, 0.3 * s, t, bevel=0)
    # throwing arm leaning back with a sling cup
    v = [(x + 0.02, y - 0.015, 0.1), (x + 0.02, y + 0.015, 0.1), (x - 0.22 * s, y + 0.015, 0.38 * s),
         (x - 0.22 * s, y - 0.015, 0.38 * s)]
    mesh("arm", v, [(0, 1, 2, 3)], t, solidify=0.03)
    blob(x - 0.23 * s, y, 0.39 * s, 0.04, 0.04, 0.02, M["iron"], segs=6, rings=4)


def ballista(x, y, M, s=1.0):
    t = M["timber_light"]
    for dx in (-0.06, 0.06):
        obox(x + dx, y, 0, 0.2 * s, 0.025, 0.025, 0, t)
    box(x - 0.09, y - 0.06, 0.18 * s, x + 0.09, y + 0.06, 0.24 * s, t, bevel=0)
    box(x - 0.02, y - 0.28 * s, 0.22 * s, x + 0.02, y + 0.2 * s, 0.25 * s, t, bevel=0)
    box(x - 0.2 * s, y - 0.2 * s, 0.22 * s, x + 0.2 * s, y - 0.17 * s, 0.27 * s, t, bevel=0)
    for dx in (-0.2, 0.2):
        cylinder(x + dx * s, y - 0.185 * s, 0.2 * s, 0.3 * s, 0.025, M["iron"], n=8)


def siege_workshop(M):
    mats(M)
    kit.lot_rect(4, 3, M["dirt"])
    # open timber shed on posts at the back
    x0, y0, x1, y1 = -1.85, 0.15, 0.9, 1.35
    for x in (x0, (x0 + x1) / 2, x1):
        for y in (y0, y1):
            box(x - 0.04, y - 0.04, 0, x + 0.04, y + 0.04, 0.7, M["timber"], bevel=0.004)
    box(x0, y1 - 0.02, 0, x1, y1 + 0.02, 0.7, M["timber"], bevel=0)  # back wall
    gable(x0, y0, x1, y1, 0.7, 0.36, M["roof_timber"], axis="x", over=0.1, wall=M["timber"])
    ballista(-1.0, 0.75, M, 1.3)
    onager(0.0, 0.7, M, 1.2)
    # finished onager in the yard, timber stack, wheel, tools
    onager(0.95, -0.7, M, 1.6)
    for k in range(4):
        cylinder(0.05, -1.5, -0.95 + k * 0.08 - 0.04, -0.95 + k * 0.08 + 0.04, 0.04, M["timber_light"],
                 n=8, axis="y")
    for k in range(5):
        obox(-0.9 + k * 0.0, -0.55, 0.0 + k * 0.05, 0.05 + k * 0.05, 0.8, 0.07,
             0.0 if k % 2 == 0 else 0.05, M["timber_light"])
    cylinder(0.12, 1.5, 0.4, 0.44, 0.12, M["timber"], n=14, axis="y", name="wheel")
    for i in range(3):
        box(-1.55 + i * 0.14, -1.25, 0, -1.45 + i * 0.14, -1.15, 0.12, M["stone_dark"],
            bevel=0.02)
        blob(-1.5 + i * 0.14, -1.2, 0.12, 0.05, 0.05, 0.045, M["stone_dark"], segs=8, rings=4)
    banner(1.75, 1.2, 0, 0.9, M)


def fort(M):
    mats(M)
    kit.lot_rect(6, 6, M["dirt"])
    S = M["wall_stone"]
    lo, hi = -2.85, 2.85
    t = 0.24
    wh = 0.7
    # curtain walls (front wall has a gateway)
    crenel_block(lo, hi - t, hi, hi, 0, wh, M, S)  # back
    crenel_block(lo, lo + t, lo + t, hi - t, 0, wh, M, S)  # west
    crenel_block(hi - t, lo + t, hi, hi - t, 0, wh, M, S)  # east
    crenel_block(lo, lo, -0.55, lo + t, 0, wh, M, S)  # front left
    crenel_block(0.55, lo, hi, lo + t, 0, wh, M, S)  # front right
    for cx in (lo, hi):
        for cy in (lo, hi):
            crenel_block(cx - 0.32, cy - 0.32, cx + 0.32, cy + 0.32, 0, 1.05, M, S)
            kit.windows_y(cy - 0.32, [cx], 0.6, 0.78, M, w=0.05)
    for gx in (-0.75, 0.75):
        crenel_block(gx - 0.22, lo - 0.08, gx + 0.22, lo + 0.36, 0, 1.15, M, S)
        banner(gx, lo - 0.09, 1.15, 0.45, M)
    arch_bay(-0.53, 0.53, lo, lo + t, 0.42, 0.85, S, segs=10)
    box(-0.55, lo - 0.01, 0.85, 0.55, lo + t + 0.01, 0.9, S, bevel=0)
    box(-0.5, lo - 0.05, 0, 0.5, lo + 0.3, 0.012, M["road"], bevel=0)
    # principia (headquarters) at the back
    box(-1.3, 0.85, 0, 1.3, 2.35, 0.75, M["plaster"])
    gable(-1.3, 0.85, 1.3, 2.35, 0.75, 0.42, M["roof"], axis="x", over=0.07, wall=M["plaster"])
    for x in [-1.15 + i * 2.3 / 5 for i in range(6)]:
        column(x, 0.7, 0, 0.7, 0.04, M["marble"])
    box(-1.3, 0.6, 0.7, 1.3, 0.85, 0.78, M["marble"], bevel=0.004)
    box(-1.3, 0.55, 0.0, 1.3, 0.85, 0.04, M["marble"], bevel=0.004)
    door(0.0, 0.85, 0.04, 0.26, 0.42, M["red"])
    kit.windows_y(0.85, [-0.8, -0.45, 0.45, 0.8], 0.35, 0.5, M, w=0.08)
    aquila(0.0, 0.25, M, 0.8)
    # rows of leather tents (contubernia) on both sides of the via praetoria
    for side in (-1, 1):
        for i in range(3):
            for j in range(3):
                x = side * (0.95 + i * 0.55)
                y = -2.2 + j * 0.62
                tent(x, y, M)
    banner(-2.3, 2.2, 0, 0.8, M)
    banner(2.3, 2.2, 0, 0.8, M)
    spear_rack(-2.05, 0.95, M, n=5, w=0.4)
    spear_rack(2.05, 0.95, M, n=5, w=0.4)


# ---------------------------------------------------------------- monuments

def triumphal_arch(M):
    mats(M)
    kit.lot_rect(3, 2, M["paving"], curb=M["marble"])
    box(-0.42, -1.0, 0, 0.42, 1.0, 0.012, M["road"], bevel=0)
    mb = M["marble"]
    y0, y1 = -0.38, 0.38
    # piers
    box(-1.3, y0, 0, -0.42, y1, 1.42, mb, bevel=0.006)
    box(0.42, y0, 0, 1.3, y1, 1.42, mb, bevel=0.006)
    arch_bay(-0.42, 0.42, y0, y1, 0.78, 1.42, mb, segs=12)
    box(-1.33, y0 - 0.03, 0, 1.33, y1 + 0.03, 0.14, mb, bevel=0.006)
    # side passages (small arches as dark recesses)
    for x in (-0.86, 0.86):
        box(x - 0.17, y0 - 0.012, 0.14, x + 0.17, y0, 0.62, M["dark"], bevel=0)
        cylinder(0.62, x, y0 - 0.012, y0, 0.17, M["dark"], n=12, axis="y")
        box(x - 0.2, y0 - 0.02, 0.85, x + 0.2, y0, 1.15, M["marble"], bevel=0.004)  # relief panel
        blob(x, y0 - 0.025, 1.0, 0.13, 0.01, 0.1, M["gold"], segs=8, rings=4, name="relief")
    # engaged columns on the front and right faces
    for x in (-1.2, -0.52, 0.52, 1.2):
        column(x, y0 - 0.06, 0.14, 1.12, 0.05, mb)
    for y in (-0.25, 0.25):
        column(1.36, y, 0.14, 1.12, 0.05, mb)
    # entablature and attic with inscription, quadriga on top
    box(-1.36, y0 - 0.1, 1.26, 1.36, y1 + 0.06, 1.42, mb, bevel=0.006)
    box(-1.22, y0 - 0.02, 1.42, 1.22, y1, 1.85, mb, bevel=0.006)
    box(-0.8, y0 - 0.03, 1.5, 0.8, y0 - 0.01, 1.77, M["marble"], bevel=0)
    for k in range(3):
        box(-0.7, y0 - 0.035, 1.56 + k * 0.07, 0.7, y0 - 0.025, 1.59 + k * 0.07, M["gold"], bevel=0)
    box(-1.25, y0 - 0.05, 1.85, 1.25, y1 + 0.03, 1.9, mb, bevel=0.004)
    quadriga(0.0, 0.0, 1.9, M, s=1.8)
    for x in (-1.05, 1.05):
        statue(x, -0.1, 1.9, M, h=0.3, mat=M["gold"])


def grand_aqueduct(M):
    mats(M)
    kit.lot_rect(10, 2, M["grass"])
    S = M["travertine"]
    y0, y1 = -0.25, 0.25
    xs = [-4.85 + i * 0.97 for i in range(11)]
    arcade(xs, y0, y1, 0.0, 0.95, 1.45, 0.26, S, cap=S)
    xs2 = [-4.85 + i * 0.485 for i in range(21)]
    arcade(xs2, y0 + 0.02, y1 - 0.02, 1.5, 1.82, 2.08, 0.14, S, cap=S)
    # water channel (specus) on top
    box(-4.95, y0 - 0.02, 2.13, 4.95, y1 + 0.02, 2.3, M["stone_dark"], bevel=0.006)
    box(-4.95, -0.12, 2.27, 4.95, 0.12, 2.305, M["water"], bevel=0)
    box(-4.95, y0 - 0.02, 2.13, 4.95, -0.12, 2.36, M["stone_dark"], bevel=0)
    box(-4.95, 0.12, 2.13, 4.95, y1 + 0.02, 2.36, M["stone_dark"], bevel=0)
    # pier footings and cutwaters
    for x in xs:
        box(x - 0.17, y0 - 0.06, 0, x + 0.17, y1 + 0.06, 0.18, M["stone_dark"], bevel=0.008)
    for x in (-4.4, -2.0, 1.5, 3.9):
        cypress(x, 0.75, 0.6, M)
    for x in (-3.3, 0.2, 2.6):
        kit.olive(x, -0.7, M, 0.9)


def lighthouse(M):
    mats(M)
    kit.lot_rect(4, 4, M["rock"])
    S = M["travertine"]
    # quay steps
    box(-1.75, -1.75, 0, 1.75, 1.75, 0.18, M["stone_dark"], bevel=0.01)
    steps(-0.5, 0.5, -1.95, -1.75, 0.18, 3, M["stone_dark"])
    # tier 1: square tower
    box(-1.25, -1.25, 0.18, 1.25, 1.25, 2.1, S, bevel=0.01)
    box(-1.3, -1.3, 2.05, 1.3, 1.3, 2.18, M["marble"], bevel=0.006)
    for z in (0.7, 1.25, 1.75):
        kit.windows_y(-1.25, [-0.75, -0.25, 0.25, 0.75], z, z + 0.18, M, w=0.08)
        kit.windows_x(1.25, [-0.75, -0.25, 0.25, 0.75], z, z + 0.18, M, w=0.08)
    door(0.0, -1.25, 0.18, 0.3, 0.42, M["timber"])
    for sx in (-1, 1):
        for sy in (-1, 1):
            statue(sx * 1.18, sy * 1.18, 2.18, M, h=0.3, mat=M["bronze"])
    # tier 2: octagon
    cylinder(0, 0, 2.18, 3.35, 0.8, S, n=8, smooth=False, name="octagon")
    cylinder(0, 0, 3.3, 3.42, 0.88, M["marble"], n=8, smooth=False)
    for i in range(8):
        a = 2 * math.pi * (i + 0.5) / 8
        if math.cos(a) * 0.7 + math.sin(a) * -0.7 < -0.2:
            continue
        x, y = math.cos(a) * 0.79, math.sin(a) * 0.79
        obox(x, y, 2.6, 2.85, 0.1, 0.03, a + math.pi / 2, M["window"])
    # tier 3: round lantern with columns and the fire
    cylinder(0, 0, 3.42, 3.95, 0.42, S, n=16, name="lantern_base")
    for i in range(8):
        a = 2 * math.pi * i / 8
        column(math.cos(a) * 0.36, math.sin(a) * 0.36, 3.95, 0.45, 0.035, M["marble"])
    blob(0, 0, 4.12, 0.25, 0.25, 0.2, M["fire"], segs=10, rings=6, name="beacon")
    cylinder(0, 0, 4.4, 4.48, 0.44, M["marble"], n=16)
    blob(0, 0, 4.48, 0.4, 0.4, 0.18, M["bronze"], segs=12, rings=6, top_only=True, name="cupola")
    statue(0, 0, 4.62, M, h=0.42, mat=M["gold"])


def pantheon(M):
    mats(M)
    kit.lot_rect(8, 8, M["marble_paving"], curb=M["marble"])
    S = M["travertine"]
    cx, cy, r = 0.0, 1.0, 2.35
    # rotunda drum with cornices
    cylinder(cx, cy, 0, 1.65, r, S, n=48, name="drum")
    for z in (0.62, 1.15, 1.65):
        cylinder(cx, cy, z - 0.04, z + 0.03, r + 0.04, M["marble"], n=48)
    # stepped rings and dome
    cylinder(cx, cy, 1.68, 1.8, r - 0.05, S, n=48)
    cylinder(cx, cy, 1.8, 1.9, r - 0.25, S, n=48)
    cylinder(cx, cy, 1.9, 2.0, r - 0.45, S, n=48)
    blob(cx, cy, 1.65, r - 0.08, r - 0.08, 1.3, M["dome"], segs=40, rings=16, top_only=True,
         name="dome")
    cylinder(cx, cy, 2.93, 2.97, 0.32, M["marble"], n=24)
    cylinder(cx, cy, 2.95, 2.975, 0.24, M["dark"], n=24, name="oculus")
    # intermediate block
    box(-1.55, -1.75, 0, 1.55, -0.75, 2.05, S, bevel=0.008)
    gable(-1.55, -1.75, 1.55, -0.75, 2.05, 0.5, M["roof_marble"], axis="y", over=0.05, wall=S)
    # portico: 8 front columns + 2 inner rows, pediment
    pz = 0.14
    box(-1.85, -3.25, 0, 1.85, -1.7, pz, M["marble"], bevel=0.008)
    steps(-1.6, 1.6, -3.55, -3.25, pz, 3, M["marble"])
    xs = [-1.7 + i * 3.4 / 7 for i in range(8)]
    for x in xs:
        column(x, -3.1, pz, 1.45, 0.1, M["granite"])
    for y in (-2.5, -1.95):
        for x in (xs[0], xs[2], xs[5], xs[7]):
            column(x, y, pz, 1.45, 0.1, M["granite"])
    box(-1.2, -1.76, pz, 1.2, -1.72, pz + 1.2, M["window"], bevel=0)  # bronze doors (lit)
    box(-0.5, -1.78, pz, 0.5, -1.74, pz + 0.95, M["bronze"], bevel=0.004)
    ez = pz + 1.45
    box(-1.85, -3.28, ez, 1.85, -1.7, ez + 0.3, M["marble"], bevel=0.006)
    box(-1.3, -3.29, ez + 0.08, 1.3, -3.27, ez + 0.2, M["marble"], bevel=0)
    for k in range(2):
        box(-1.1, -3.30, ez + 0.11 + k * 0.05, 1.1, -3.285, ez + 0.13 + k * 0.05, M["dark"],
            bevel=0)  # M AGRIPPA inscription
    gable(-1.85, -3.28, 1.85, -1.7, ez + 0.3, 0.55, M["dome"], axis="y", over=0.05,
          wall=M["marble"])
    # piazza: fountain with an obelisk, statues
    cylinder(0, -3.95, 0, 0.18, 0.5, M["marble"], n=24, name="fountain")
    cylinder(0, -3.95, 0.12, 0.19, 0.44, M["water"], n=24)
    obelisk(0, -3.95, 0.18, 1.1, 0.12, M)
    for x in (-2.9, 2.9):
        statue(x, -3.0, 0, M, h=0.35, mat=M["bronze"])
        cypress(x, 2.9, 0.75, M)


def temple_of_jupiter(M):
    mats(M)
    kit.lot_rect(8, 6, M["marble_paving"], curb=M["marble"])
    mb = M["marble"]
    # high podium with front stairs
    pz = 0.75
    box(-3.15, -1.55, 0, 3.15, 2.75, pz, mb, bevel=0.01)
    box(-3.2, -1.6, pz - 0.06, 3.2, 2.8, pz, mb, bevel=0.006)
    steps(-2.4, 2.4, -2.65, -1.55, pz, 9, mb)
    for x in (-2.75, 2.75):
        box(x - 0.35, -2.65, 0, x + 0.35, -1.55, pz + 0.05, mb, bevel=0.008)
        statue(x, -2.3, pz + 0.05, M, h=0.38, mat=M["gold"])
    # cella
    cz = pz + 2.0
    box(-2.3, 0.1, pz, 2.3, 2.6, cz, mb)
    box(-0.45, 0.08, pz, 0.45, 0.11, pz + 1.3, M["window"], bevel=0)
    box(-0.4, 0.06, pz, 0.4, 0.09, pz + 1.2, M["bronze"], bevel=0.004)
    # three rows of six front columns + side colonnades
    xs = [-2.85 + i * 5.7 / 5 for i in range(6)]
    for y in (-1.35, -0.8, -0.25):
        for x in xs:
            column(x, y, pz, cz - pz, 0.13, mb)
    for y in (0.35, 0.95, 1.55, 2.15):
        column(-2.85, y, pz, cz - pz, 0.13, mb)
        column(2.85, y, pz, cz - pz, 0.13, mb)
    # entablature, gold frieze, gilded roof
    ez = cz + 0.32
    box(-3.05, -1.55, cz, 3.05, 2.65, ez, mb, bevel=0.008)
    box(-3.07, -1.57, ez - 0.17, 3.07, 2.67, ez - 0.08, M["gold"], bevel=0)
    gable(-3.05, -1.55, 3.05, 2.65, ez, 0.95, M["gold_roof"], axis="y", over=0.08, wall=mb)
    # pediment sculpture (gold) and acroteria, quadriga on the apex
    blob(0, -1.65, ez + 0.32, 0.6, 0.03, 0.18, M["gold"], segs=12, rings=4, name="pediment")
    for x in (-1.3, 1.3):
        blob(x, -1.64, ez + 0.15, 0.32, 0.03, 0.09, M["gold"], segs=10, rings=4, name="relief")
    quadriga(0, -1.4, ez + 0.95, M, s=2.2)
    for x in (-3.1, 3.1):
        statue(x, -1.6, ez, M, h=0.45, mat=M["gold"])
    # great altar and attendants on the piazza
    box(-0.7, -2.95, 0, 0.7, -2.7, 0.28, mb, bevel=0.01)
    box(-0.72, -2.97, 0.27, 0.72, -2.68, 0.32, M["gold"], bevel=0.004)
    blob(0, -2.82, 0.36, 0.14, 0.08, 0.1, M["fire"], segs=8, rings=4)
    for x in (-3.6, 3.6):
        cypress(x, -0.5, 0.9, M)
        cypress(x, 1.5, 0.9, M)
    for x in (-1.9, 1.9):
        statue(x, -2.85, 0, M, h=0.38, mat=M["bronze"])


def colosseum(M):
    mats(M)
    kit.lot_rect(12, 10, M["marble_paving"], curb=M["marble"])
    S = M["travertine"]
    a, b = 5.55, 4.55        # outer ellipse semi-axes
    H = 2.75
    n = 64

    def pt(ra, rb, i):
        t = 2 * math.pi * i / n
        return ra * math.cos(t), rb * math.sin(t)

    # outer wall surface (thin shell) with four storeys
    v, f = [], []
    for i in range(n):
        x, y = pt(a, b, i)
        v += [(x, y, 0), (x, y, H)]
    for i in range(n):
        j = (i + 1) % n
        f.append((2 * i, 2 * j, 2 * j + 1, 2 * i + 1))
    mesh("outer_wall", v, f, S, solidify=0.12)
    # arches (windows glow at night) and pilasters on the three arcaded storeys
    tiers = [(0.1, 0.62), (0.78, 1.28), (1.44, 1.94)]
    for i in range(n):
        t = 2 * math.pi * (i + 0.5) / n
        nx, ny = b * math.cos(t), a * math.sin(t)
        L = math.hypot(nx, ny)
        nx, ny = nx / L, ny / L
        if nx * 0.707 - ny * 0.707 < -0.35:
            continue  # back side: hidden from the camera
        x, y = a * math.cos(t) + nx * 0.012, b * math.sin(t) + ny * 0.012
        ang = math.atan2(ny, nx) + math.pi / 2
        seg = 2 * math.pi * math.hypot(a, b) / math.sqrt(2) / n
        for (z0, z1) in tiers:
            obox(x, y, z0, z1 - 0.12, seg * 0.55, 0.02, ang, M["window"], name="arch")
            # round arch head
            obox(x, y, z1 - 0.12, z1 - 0.06, seg * 0.42, 0.02, ang, M["window"], name="arch")
            obox(x, y, z1 - 0.06, z1 - 0.03, seg * 0.26, 0.02, ang, M["window"], name="arch")
        # top storey: small square windows
        obox(x, y, 2.2, 2.38, seg * 0.22, 0.02, ang, M["window"], name="win")
        # pilasters between bays
        t2 = 2 * math.pi * i / n
        px, py = a * math.cos(t2) + nx * 0.02, b * math.sin(t2) + ny * 0.02
        obox(px, py, 0, H, 0.06, 0.035, ang, S, name="pilaster")
    for z in (0.7, 1.36, 2.02, H):
        v, f = [], []
        for i in range(n):
            x0, y0 = pt(a + 0.05, b + 0.05, i)
            v += [(x0, y0, z - 0.05), (x0, y0, z + 0.02)]
        for i in range(n):
            j = (i + 1) % n
            f.append((2 * i, 2 * j, 2 * j + 1, 2 * i + 1))
        mesh("cornice", v, f, M["marble"], solidify=0.08)
    # cavea: stepped seating from the rim down to the arena wall
    rings = 9
    ai, bi = 2.55, 1.6       # arena wall
    zt, zb = H - 0.12, 0.62
    for k in range(rings):
        u0, u1 = k / rings, (k + 1) / rings
        ra0, rb0 = a - 0.15 - (a - 0.15 - ai) * u0, b - 0.15 - (b - 0.15 - bi) * u0
        ra1, rb1 = a - 0.15 - (a - 0.15 - ai) * u1, b - 0.15 - (b - 0.15 - bi) * u1
        z = zt - (zt - zb) * u0
        z1 = zt - (zt - zb) * u1
        mat = M["seat"] if k % 3 != 2 else M["marble"]
        v, f = [], []
        for i in range(n):
            xo, yo = pt(ra0, rb0, i)
            xi, yi = pt(ra1, rb1, i)
            v += [(xo, yo, z), (xi, yi, z), (xi, yi, z1)]
        for i in range(n):
            j = (i + 1) % n
            f.append((3 * i, 3 * j, 3 * j + 1, 3 * i + 1))       # tread
            f.append((3 * i + 1, 3 * j + 1, 3 * j + 2, 3 * i + 2))  # riser
        mesh("cavea", v, f, mat)
    # aisles (vomitoria) as dark radial stripes
    for i in range(0, n, 8):
        t = 2 * math.pi * i / n
        for k in range(1, rings, 3):
            u = (k + 0.5) / rings
            ra, rb = a - 0.15 - (a - 0.15 - ai) * u, b - 0.15 - (b - 0.15 - bi) * u
            z = zt - (zt - zb) * u
            obox(ra * math.cos(t), rb * math.sin(t), z - 0.1, z + 0.005, 0.14, 0.12,
                 math.atan2(rb * math.sin(t), ra * math.cos(t)), M["dark"])
    # arena: podium wall and sand
    v, f = [], []
    for i in range(n):
        x, y = pt(ai, bi, i)
        v += [(x, y, 0.15), (x, y, zb)]
    for i in range(n):
        j = (i + 1) % n
        f.append((2 * i + 1, 2 * j + 1, 2 * j, 2 * i))
    mesh("podium", v, f, M["red"])
    v = [(0, 0, 0.15)] + [(pt(ai, bi, i)[0], pt(ai, bi, i)[1], 0.15) for i in range(n)]
    f = [(0, 1 + i, 1 + (i + 1) % n) for i in range(n)]
    mesh("arena", v, f, M["sand"])
    # velarium masts and awning segments on the rim
    for i in range(0, n, 4):
        x, y = pt(a - 0.05, b - 0.05, i)
        cylinder(x, y, H, H + 0.35, 0.02, M["timber"], n=6, name="mast")
    # statues and plaza trees
    for x, y in ((-5.6, -4.6), (5.6, -4.6), (-5.6, 4.6), (5.6, 4.6)):
        statue(x * 0.98, y * 0.98, 0, M, h=0.4, mat=M["bronze"])


def circus_maximus(M):
    mats(M)
    kit.lot_rect(24, 7, M["paving"])
    S = M["travertine"]
    X0, X1 = -10.9, 9.2           # straight stands from the carceres to the curved end
    ty = 2.05                      # half width of the track
    oy = 3.4                       # outer wall
    H = 1.15
    # track
    box(X0 + 0.4, -ty, 0, X1, ty, 0.03, M["sand"], bevel=0)
    v = [(X1, 0, 0.03)]
    n = 24
    for i in range(n + 1):
        t = -math.pi / 2 + math.pi * i / n
        v.append((X1 + ty * math.cos(t), ty * math.sin(t), 0.03))
    mesh("track_end", v, [(0, i + 1, i + 2) for i in range(n)], M["sand"])

    def stand(sign):
        """Straight stand along x on one side (sign = +1 north/back, -1 south/front)."""
        yi, yo = sign * ty, sign * oy
        # seating slope (treads) from track wall up to the outer wall
        steps_n = 7
        for k in range(steps_n):
            u0, u1 = k / steps_n, (k + 1) / steps_n
            ya, yb = yi + (yo - yi) * u0, yi + (yo - yi) * u1
            z = 0.25 + (H - 0.3) * u1
            mat = M["seat"] if k % 3 != 2 else M["marble"]
            y_lo, y_hi = min(ya, yb), max(ya, yb)
            box(X0 + 0.4, y_lo, 0, X1, y_hi, z, mat, bevel=0)
        # outer arcaded wall
        y_lo, y_hi = (yo, yo + sign * 0.12) if sign > 0 else (yo - 0.12, yo)
        box(X0 + 0.4, min(y_lo, y_hi), 0, X1, max(y_lo, y_hi), H, S, bevel=0.004)
        if sign < 0:
            k = 0
            x = X0 + 0.7
            while x < X1 - 0.2:
                for (z0, z1) in ((0.08, 0.5), (0.62, 0.98)):
                    box(x - 0.1, yo - 0.135, z0, x + 0.1, yo - 0.12, z1, M["window"], bevel=0)
                x += 0.36
                k += 1
        box(X0 + 0.4, min(yi, yi + sign * 0.06) - 0.0, 0, X1, max(yi, yi + sign * 0.06), 0.3,
            M["red"], bevel=0)
        # faction banners on the rim
        for i, x in enumerate([X0 + 1.5 + j * 1.6 for j in range(12)]):
            cloth = ("red", "white_cloth", "green_cloth", "blue_cloth")[i % 4]
            banner(x, yo, H, 0.4, M, cloth=cloth, w=0.14, ch=0.2)

    stand(+1)
    stand(-1)
    # curved end (sphendone): stepped half-rings
    n = 28
    steps_n = 7
    for k in range(steps_n):
        r0 = ty + (oy - ty) * k / steps_n
        r1 = ty + (oy - ty) * (k + 1) / steps_n
        z = 0.25 + (H - 0.3) * (k + 1) / steps_n
        v, f = [], []
        for i in range(n + 1):
            t = -math.pi / 2 + math.pi * i / n
            c, s = math.cos(t), math.sin(t)
            v += [(X1 + r0 * c, r0 * s, z), (X1 + r1 * c, r1 * s, z), (X1 + r0 * c, r0 * s, 0)]
        for i in range(n):
            a_, b_ = 3 * i, 3 * (i + 1)
            f.append((a_, b_, b_ + 1, a_ + 1))
            f.append((a_ + 2, b_ + 2, b_, a_))
        mesh("sphendone", v, f, M["seat"] if k % 3 != 2 else M["marble"])
    v, f = [], []
    for i in range(n + 1):
        t = -math.pi / 2 + math.pi * i / n
        v += [(X1 + oy * math.cos(t), oy * math.sin(t), 0), (X1 + oy * math.cos(t), oy * math.sin(t), H)]
    for i in range(n):
        f.append((2 * i, 2 * (i + 1), 2 * (i + 1) + 1, 2 * i + 1))
    mesh("sphendone_wall", v, f, S, solidify=0.12)
    # carceres: twelve starting gates between two towers at the west end
    cx0 = X0
    box(cx0, -oy, 0, cx0 + 0.45, oy, 0.75, S, bevel=0.006)
    for i in range(12):
        y = -ty + 0.15 + i * (2 * ty - 0.3) / 11
        box(cx0 + 0.44, y - 0.12, 0.05, cx0 + 0.46, y + 0.12, 0.5, M["window"], bevel=0)
        box(cx0 + 0.45, y - 0.14, 0.5, cx0 + 0.47, y + 0.14, 0.56, M["red"], bevel=0)
    for y in (-oy + 0.35, oy - 0.35):
        crenel_block(cx0 - 0.1, y - 0.4, cx0 + 0.6, y + 0.4, 0, 1.55, M, S)
    box(cx0 + 0.05, -0.6, 0.75, cx0 + 0.4, 0.6, 1.2, S, bevel=0.006)  # magistrate's box
    quadriga(cx0 + 0.25, 0, 1.2, M, s=1.2)
    # spina: central barrier with obelisk, metae and lap counters
    box(-7.4, -0.22, 0, 7.4, 0.22, 0.3, M["marble"], bevel=0.008)
    box(-7.3, -0.15, 0.3, 7.3, 0.15, 0.33, M["water"], bevel=0)
    obelisk(0, 0, 0.3, 2.3, 0.22, M)
    for x in (-7.7, 7.7):
        box(x - 0.3, -0.3, 0, x + 0.3, 0.3, 0.32, M["marble"], bevel=0.008)
        for dy in (-0.15, 0.0, 0.15):
            cone(x, dy, 0.32, 0.07, 0.65, M["gold"], n=10)
    for x in (-4.6, 4.6):
        for k in range(4):
            column(x - 0.3 + k * 0.2, 0, 0.3, 0.55, 0.03, M["marble"])
        box(x - 0.42, -0.06, 0.85, x + 0.42, 0.06, 0.9, M["marble"], bevel=0)
        for k in range(7):
            blob(x - 0.35 + k * 0.117, 0, 0.94, 0.04, 0.025, 0.035, M["gold"], segs=6, rings=4,
                 name="egg")
    for x in (-2.4, 2.4, -6.2, 6.2):
        statue(x, 0, 0.3, M, h=0.38, mat=M["gold"])
    # imperial box (pulvinar) on the north stand
    box(-0.75, oy - 0.9, H - 0.2, 0.75, oy + 0.1, H + 0.55, M["marble"], bevel=0.006)
    for x in (-0.6, -0.2, 0.2, 0.6):
        column(x, oy - 0.95, 0.55, H - 0.6, 0.04, M["marble"])
    gable(-0.8, oy - 0.95, 0.8, oy + 0.15, H + 0.55, 0.3, M["roof"], axis="y", over=0.05,
          wall=M["marble"])
    box(-0.5, oy - 0.92, H - 0.05, 0.5, oy - 0.9, H + 0.25, M["red"], bevel=0)


# ---------------------------------------------------------------- placeholders

def placeholder(fw, fh):
    def build(M):
        mats(M)
        kit.lot_rect(fw, fh, M["dirt"])
        hx, hy = fw / 2 - 0.08, fh / 2 - 0.08
        corners = [(-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy)]
        for (x, y) in corners:
            cylinder(x, y, 0, 0.22, 0.018, M["timber_light"], n=6, name="stake")
            box(x - 0.02, y - 0.02, 0.2, x + 0.02, y + 0.02, 0.24, M["red"], bevel=0)
        for (x0, y0), (x1, y1) in zip(corners, corners[1:] + corners[:1]):
            if abs(y1 - y0) < 1e-6:
                box(min(x0, x1), y0 - 0.004, 0.15, max(x0, x1), y0 + 0.004, 0.158,
                    M["white_cloth"], bevel=0)
            else:
                box(x0 - 0.004, min(y0, y1), 0.15, x0 + 0.004, max(y0, y1), 0.158,
                    M["white_cloth"], bevel=0)
        # a few dressed stones and a plank stack
        sx, sy = -hx + 0.25, -hy + 0.22
        for i in range(min(3, fw + fh - 1)):
            box(sx + i * 0.16 - 0.06, sy - 0.05, 0, sx + i * 0.16 + 0.06, sy + 0.05, 0.08,
                M["travertine"], bevel=0.01)
        if fw * fh >= 2:
            px, py = hx - 0.3, hy - 0.25
            for k in range(3):
                box(px - 0.22, py - 0.05 + k * 0.0, 0.02 * k, px + 0.22, py + 0.05, 0.02 * (k + 1),
                    M["timber_light"], bevel=0)
    return build


# ---------------------------------------------------------------- registry

def _r(fn, region):
    return lambda M: fn(M, region)


SPRITES = {}
for name, fp, fn, regional, lit in (
        ("vigiles_post", (1, 1), vigiles_post, True, False),
        ("guard_post", (2, 2), guard_post, True, False),
        ("militia_post", (2, 2), militia_post, True, True),
        ("barracks", (4, 4), barracks, True, True)):
    SPRITES["bld/" + name] = dict(fp=fp, build=_r(fn, None), lit=lit)
    if regional:
        for reg in ("east", "egypt", "north"):
            SPRITES["bld/%s@%s" % (name, reg)] = dict(fp=fp, build=_r(fn, reg))

SPRITES.update({
    "bld/armory": dict(fp=(3, 2), build=armory, lit=True),
    "bld/tower": dict(fp=(2, 2), build=tower),
    "bld/gate": dict(fp=(2, 1), build=gate),
    "bld/wall_c": dict(fp=(1, 1), build=wall_c),
    "bld/wall_n": dict(fp=(1, 1), build=wall_piece("n")),
    "bld/wall_e": dict(fp=(1, 1), build=wall_piece("e")),
    "bld/wall_s": dict(fp=(1, 1), build=wall_piece("s")),
    "bld/wall_w": dict(fp=(1, 1), build=wall_piece("w")),
    "bld/training_ground": dict(fp=(5, 5), build=training_ground),
    "bld/stables": dict(fp=(4, 3), build=stables),
    "bld/siege_workshop": dict(fp=(4, 3), build=siege_workshop),
    "bld/fort": dict(fp=(6, 6), build=fort),
    "bld/triumphal_arch": dict(fp=(3, 2), build=triumphal_arch),
    "bld/grand_aqueduct": dict(fp=(10, 2), build=grand_aqueduct),
    "bld/lighthouse": dict(fp=(4, 4), build=lighthouse, lit=True),
    "bld/pantheon": dict(fp=(8, 8), build=pantheon, lit=True),
    "bld/temple_of_jupiter": dict(fp=(8, 6), build=temple_of_jupiter),
    "bld/colosseum": dict(fp=(12, 10), build=colosseum, lit=True),
    "bld/circus_maximus": dict(fp=(24, 7), build=circus_maximus),
})
for w in range(1, 5):
    for h in range(1, 5):
        SPRITES["bld/placeholder_%dx%d" % (w, h)] = dict(fp=(w, h), build=placeholder(w, h))
