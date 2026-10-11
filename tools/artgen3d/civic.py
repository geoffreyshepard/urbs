"""Civic, education, entertainment and health buildings (family "civic").

    Blender -b --python tools/artgen3d/run.py -- civic [names] --samples 48

Lots are centred on the origin; fronts face grid south (Blender -y).  Category accents
(docs/13-isometric.md): civic = travertine, green-patina roofs, red banners; entertainment =
red/cream striped awnings, banners, curved seating; health = white plaster, teal water, lead
domes; education = pale limestone, blue-grey roofs, porches with benches and scrolls.
Regional variants (@east, @egypt, @north) swap walls, roofs, ground and trees via style().
"""
import math

import kit
from kit import blob, box, column, cylinder, cypress, gable, hip, mesh, olive, pot_plant, steps

# ---------------------------------------------------------------- materials


def mats(M):
    mt = kit.material
    M["patina"] = mt("patina", "#5f9e88", "#4c8672", kind="tiles", scale=7, brick=(0.085, 0.07),
                     bump=0.5, detail=(2.0, "#7fae98"))
    M["bronze"] = mt("bronze", "#8c6a3c", "#6d512b", scale=18, rough=0.4, metal=0.6, bump=0.05)
    M["banner"] = mt("banner", "#b0261d", "#951e17", scale=10, rough=0.9, bump=0.05)
    M["banner_y"] = mt("banner_y", "#e2b23a", "#c99a2a", scale=10, rough=0.9, bump=0.05)
    M["banner_b"] = mt("banner_b", "#2f5f9e", "#244c80", scale=10, rough=0.9, bump=0.05)
    M["awning_red"] = mt("awning_red", "#bd2e26", "#f3e9d2", kind="stripes", stripe=0.1, bump=0.0,
                         rough=0.9)
    M["pool"] = mt("pool", "#3fb7b4", "#2c9fa2", scale=5, rough=0.08, bump=0.02)
    M["lead"] = mt("lead", "#a3b0b6", "#8e9ba1", scale=9, rough=0.55, bump=0.06)
    M["teal"] = mt("teal", "#2e928e", "#257d7a", scale=12, bump=0.05)
    M["stucco"] = mt("stucco", "#f4f0e7", "#e5dfd2", scale=9, bump=0.06, detail=(1.5, "#e2dbcc"))
    M["white_plaster"] = mt("white_plaster", "#f2eee4", "#e3dccd", scale=9, bump=0.06,
                            detail=(1.5, "#ddd5c4"))
    M["blue"] = mt("blue", "#3f71ab", "#335f93", scale=12, bump=0.05)
    M["mudbrick"] = mt("mudbrick", "#bc9062", "#a97f54", kind="brick", scale=12, brick=(0.16, 0.07),
                       mortar="#93714f", bump=0.25)
    M["sandstone"] = mt("sandstone", "#dfc28a", "#ccaf74", kind="brick", scale=12,
                        brick=(0.3, 0.12), mortar="#b8996a", bump=0.2)
    M["thatch"] = mt("thatch", "#a88c55", "#8b7243", scale=22, bump=0.6, detail=(2.5, "#7d6a45"))
    M["shingle"] = mt("shingle", "#62503f", "#4d3e31", kind="tiles", scale=7, brick=(0.1, 0.06),
                      bump=0.5)
    M["grass_dark"] = mt("grass_dark", "#5a783b", "#4b6830", scale=11, bump=0.25,
                         detail=(0.9, "#7d8a58"))
    M["sand"] = mt("sand", "#dcc492", "#cbb07c", scale=9, bump=0.2, detail=(1.2, "#e6d3a8"))
    M["limestone"] = mt("limestone", "#e8e0cf", "#d6ccb6", kind="brick", scale=12,
                        brick=(0.3, 0.12), mortar="#bfb39a", bump=0.15)
    M["roof_edu"] = mt("roof_edu", "#6e83a0", "#5b6e89", kind="tiles", scale=7,
                       brick=(0.085, 0.07), bump=0.5, detail=(2.0, "#8a9ab0"))
    M["seat"] = mt("seat", "#ddd0b0", "#cbbd9a", scale=14, bump=0.1)
    M["riser"] = mt("riser", "#9e8d70", "#8a7a5e", scale=14, bump=0.1)
    M["arena"] = mt("arena", "#e3c890", "#d1b47c", scale=10, bump=0.15, detail=(1.3, "#c9a86e"))
    M["warm_stone"] = mt("warm_stone", "#d8bf93", "#c5aa7c", kind="brick", scale=12,
                         brick=(0.32, 0.14), mortar="#a99273", bump=0.2)
    M["scroll"] = mt("scroll", "#efe2bf", "#ddcca2", scale=20, bump=0.05)
    M["herb"] = mt("herb", "#6f9a45", "#5a8236", scale=22, bump=0.3)
    M["coin"] = mt("coin", "#f0c450", "#d6a632", scale=25, rough=0.3, metal=0.9, bump=0.0)
    return M


# ---------------------------------------------------------------- regional style

CAT = {
    "civic": dict(wall="travertine", stone="travertine", roof="patina", ground="paving"),
    "ent": dict(wall="plaster_warm", stone="warm_stone", roof="roof", ground="dirt"),
    "health": dict(wall="white_plaster", stone="marble", roof="roof_faded", ground="paving"),
    "edu": dict(wall="limestone", stone="limestone", roof="roof_edu", ground="garden"),
}


def style(M, cat, reg=None):
    c = CAT[cat]
    S = dict(wall=M[c["wall"]], stone=M[c["stone"]], roof=M[c["roof"]], ground=M[c["ground"]],
             kind="hip", door=M["dark"], tree="cypress", round_cols=True, reg=reg, M=M)
    if reg == "east":
        S.update(wall=M["stucco"], stone=M["marble"], roof=M["stucco"], kind="flat",
                 door=M["blue"], trim=M["blue"], tree="olive")
        if cat == "civic":
            S.update(roof=M["patina"], kind="hip_low")
    elif reg == "egypt":
        S.update(wall=M["mudbrick"] if cat in ("ent", "health") else M["sandstone"],
                 stone=M["sandstone"], roof=M["sandstone"], kind="flat", ground=M["sand"],
                 tree="palm")
    elif reg == "north":
        S.update(wall=M["plaster_humble"], stone=M["timber"], kind="steep", ground=M["grass_dark"],
                 tree="pine", round_cols=False, frame=M["timber"])
        S["roof"] = M["shingle"] if cat in ("civic", "edu") else M["thatch"]
    if cat == "ent" and reg is None:
        S["ground"] = M["dirt"]
    return S


def roof(S, x0, y0, x1, y1, z, h, axis="x", wall=None):
    """Roof in the style's form.  Returns the top z of the eaves level."""
    k = S["kind"]
    if k == "flat":
        box(x0 - 0.03, y0 - 0.03, z, x1 + 0.03, y1 + 0.03, z + 0.05, S["roof"], bevel=0.006)
        t = 0.04
        for (a, b, c, d) in ((x0 - 0.03, y0 - 0.03, x1 + 0.03, y0 - 0.03 + t),
                             (x0 - 0.03, y1 + 0.03 - t, x1 + 0.03, y1 + 0.03),
                             (x0 - 0.03, y0 - 0.03, x0 - 0.03 + t, y1 + 0.03),
                             (x1 + 0.03 - t, y0 - 0.03, x1 + 0.03, y1 + 0.03)):
            box(a, b, z + 0.05, c, d, z + 0.1, S["wall"], bevel=0.004)
        return
    if k == "steep":
        gable(x0, y0, x1, y1, z, h * 1.9, S["roof"], axis=axis, over=0.08, wall=wall or S["wall"])
        return
    if k == "hip_low":
        hip(x0, y0, x1, y1, z, h * 0.6, S["roof"])
        return
    if k == "gable":
        gable(x0, y0, x1, y1, z, h, S["roof"], axis=axis, wall=wall or S["wall"])
        return
    hip(x0, y0, x1, y1, z, h, S["roof"])


def block(S, x0, y0, x1, y1, z1, rh=0.25, axis="x", z0=0.0, wall=None):
    """Walls plus roof; in the north style the corners get timber posts."""
    w = wall or S["wall"]
    box(x0, y0, z0, x1, y1, z1, w)
    if S["reg"] == "north":
        f = S["frame"]
        for x in (x0, x1):
            for y in (y0, y1):
                box(x - 0.025, y - 0.025, z0, x + 0.025, y + 0.025, z1, f, bevel=0)
        box(x0 - 0.012, y0 - 0.012, z1 - 0.04, x1 + 0.012, y1 + 0.012, z1, f, bevel=0)
    if S["reg"] == "east" and "trim" in S:
        box(x0 - 0.01, y0 - 0.01, z1 - 0.035, x1 + 0.01, y1 + 0.01, z1, S["trim"], bevel=0)
    roof(S, x0, y0, x1, y1, z1, rh, axis=axis, wall=w)


def col(S, x, y, z0, h, r=0.045):
    if S["round_cols"]:
        column(x, y, z0, h, r, S["stone"])
    else:
        box(x - r, y - r, z0, x + r, y + r, z0 + h, S["stone"], bevel=0.004)


def tree(S, x, y, s=1.0):
    t, M = S["tree"], S["M"]
    if t == "palm":
        palm(x, y, M, 0.75 * s)
    elif t == "pine":
        pine(x, y, M, 0.7 * s)
    elif t == "olive":
        olive(x, y, M, s)
    else:
        cypress(x, y, 0.55 * s, M)


def lot(S, fw, fh, mat=None, curb=None):
    kit.lot_rect(fw, fh, mat or S["ground"], curb=curb)


# ---------------------------------------------------------------- extra parts


def palm(x, y, M, h=0.75):
    n = 6
    for i in range(n):
        z0, z1 = h * i / n, h * (i + 1) / n
        dx = 0.04 * math.sin(i * 0.5)
        cylinder(x + dx, y, z0, z1 + 0.01, 0.024 - 0.002 * i, M["trunk"], n=7, name="palm")
    tx, ty, tz = x + 0.04 * math.sin(n * 0.5), y, h
    for k in range(8):
        a = 2 * math.pi * k / 8 + 0.3
        ca, sa = math.cos(a), math.sin(a)
        L = 0.26
        v = [(tx, ty, tz + 0.02),
             (tx + ca * L * 0.5 - sa * 0.05, ty + sa * L * 0.5 + ca * 0.05, tz + 0.05),
             (tx + ca * L, ty + sa * L, tz - 0.07),
             (tx + ca * L * 0.5 + sa * 0.05, ty + sa * L * 0.5 - ca * 0.05, tz + 0.05)]
        mesh("frond", v, [(0, 1, 2, 3)], M["olive_leaf"], solidify=0.01)


def cone(x, y, z0, h, r, mat, n=12, name="cone"):
    v = [(x + r * math.cos(2 * math.pi * i / n), y + r * math.sin(2 * math.pi * i / n), z0)
         for i in range(n)] + [(x, y, z0 + h)]
    f = [(i, (i + 1) % n, n) for i in range(n)] + [tuple(reversed(range(n)))]
    return mesh(name, v, f, mat)


def pine(x, y, M, h=0.7):
    cylinder(x, y, 0, h * 0.25, 0.03, M["trunk"], n=6)
    for i, (z, r) in enumerate(((0.18, 0.2), (0.38, 0.16), (0.56, 0.11))):
        cone(x, y, h * z, h * 0.35, r * h / 0.7, M["cypress"], n=10, name="pine")


def banner(x, y, M, h=0.55, mat="banner", face="y"):
    """Pole with a hanging cloth banner and a gold finial."""
    cylinder(x, y, 0, h, 0.012, M["timber"], n=6)
    blob(x, y, h + 0.02, 0.022, 0.022, 0.022, M["gold"], segs=6, rings=4, name="finial")
    if face == "y":
        box(x - 0.012, y - 0.07, h - 0.24, x + 0.012, y + 0.07, h - 0.02, M[mat], bevel=0)
        box(x - 0.013, y - 0.071, h - 0.05, x + 0.013, y + 0.071, h - 0.02, M["gold"], bevel=0)
    else:
        box(x - 0.07, y - 0.012, h - 0.24, x + 0.07, y + 0.012, h - 0.02, M[mat], bevel=0)
        box(x - 0.071, y - 0.013, h - 0.05, x + 0.071, y + 0.013, h - 0.02, M["gold"], bevel=0)


def wall_banner_y(x, face_y, z_top, M, mat="banner", w=0.09, h=0.28):
    """Cloth banner hanging on a wall that faces -y."""
    box(x - w / 2, face_y - 0.02, z_top - h, x + w / 2, face_y - 0.005, z_top, M[mat], bevel=0)
    box(x - w / 2 - 0.005, face_y - 0.022, z_top - 0.03, x + w / 2 + 0.005, face_y - 0.004, z_top,
        M["gold"], bevel=0)


def wall_banner_x(face_x, y, z_top, M, mat="banner", w=0.09, h=0.28):
    box(face_x + 0.005, y - w / 2, z_top - h, face_x + 0.02, y + w / 2, z_top, M[mat], bevel=0)
    box(face_x + 0.004, y - w / 2 - 0.005, z_top - 0.03, face_x + 0.022, y + w / 2 + 0.005, z_top,
        M["gold"], bevel=0)


def shed(x0, y0, x1, y1, z_lo, z_hi, mat, high="y1"):
    """Lean-to roof sloping down toward the opposite side of `high`."""
    if high == "y1":
        v = [(x0, y0, z_lo), (x1, y0, z_lo), (x1, y1, z_hi), (x0, y1, z_hi)]
    elif high == "y0":
        v = [(x0, y0, z_hi), (x1, y0, z_hi), (x1, y1, z_lo), (x0, y1, z_lo)]
    elif high == "x0":
        v = [(x0, y0, z_hi), (x1, y0, z_lo), (x1, y1, z_lo), (x0, y1, z_hi)]
    else:
        v = [(x0, y0, z_lo), (x1, y0, z_hi), (x1, y1, z_hi), (x0, y1, z_lo)]
    return mesh("shed", v, [(0, 1, 2, 3)], mat, solidify=0.03)


def dome(x, y, z, r, M, mat="lead", drum=0.0, drum_mat=None, rz=None):
    if drum:
        cylinder(x, y, z, z + drum, r * 1.02, drum_mat or M["white_plaster"], n=20, name="drum")
        z += drum
    blob(x, y, z, r, r, rz or r * 0.85, M[mat], segs=20, rings=10, top_only=True, name="dome")
    blob(x, y, z + (rz or r * 0.85) + 0.02, 0.025, 0.025, 0.03, M["gold"], segs=6, rings=4)


def pool(x0, y0, x1, y1, M, rim="marble", depth=0.03):
    t = 0.05
    for a, b, c, d in ((x0, y0, x1, y0 + t), (x0, y1 - t, x1, y1), (x0, y0, x0 + t, y1),
                       (x1 - t, y0, x1, y1)):
        box(a, b, 0, c, d, depth + 0.03, M[rim], bevel=0.004)
    box(x0 + t, y0 + t, 0, x1 - t, y1 - t, depth, M["pool"], bevel=0)


def ring_mesh(cx, cy, rx, ry, a0, a1, segs, prof, mat, name="ring", closed=False):
    """Sweep a 2D profile [(dr, z), ...] (dr = radial offset, + outward) around an ellipse arc."""
    v, f = [], []
    n = len(prof)
    cnt = segs if closed else segs + 1
    for i in range(cnt):
        a = a0 + (a1 - a0) * i / segs
        ca, sa = math.cos(a), math.sin(a)
        for dr, z in prof:
            v.append((cx + (rx + dr) * ca, cy + (ry + dr) * sa, z))
    for i in range(segs):
        j = (i + 1) % cnt
        for k in range(n - 1):
            f.append((i * n + k, j * n + k, j * n + k + 1, i * n + k + 1))
    if not closed and n >= 3:
        f.append(tuple(range(n)))
        f.append(tuple(segs * n + k for k in reversed(range(n))))
    return mesh(name, v, f, mat)


def tiers(cx, cy, rx, ry, a0, a1, depth, rows, z0, z1, M, segs=40):
    """Stepped seating from (rx, ry) outward by `depth`, rising from z0 to z1: treads in light
    stone, risers in shadowed stone so the rows read as stripes."""
    for k in range(rows):
        r0 = depth * k / rows
        r1 = depth * (k + 1) / rows
        zt = z0 + (z1 - z0) * (k + 1) / rows
        ring_mesh(cx, cy, rx, ry, a0, a1, segs, [(r0, 0.0), (r0, zt)], M["riser"], name="riser")
        ring_mesh(cx, cy, rx, ry, a0, a1, segs, [(r0, zt), (r1, zt)], M["seat"], name="tread")


def arcade(cx, cy, rx, ry, a0, a1, z0, z1, n, M, mat="window", w=0.6):
    """Arch openings on a curved outer wall: n dark (night-lit) panels between a0 and a1."""
    for i in range(n):
        a = a0 + (a1 - a0) * (i + 0.5) / n
        da = (a1 - a0) / n * w / 2
        ring_mesh(cx, cy, rx + 0.005, ry + 0.005, a - da, a + da, 3, [(0.0, z0), (0.0, z1)],
                  M[mat], name="arch")


def statue(x, y, M, h=0.32, mat="bronze", base="marble"):
    box(x - 0.06, y - 0.06, 0, x + 0.06, y + 0.06, h * 0.45, M[base], bevel=0.006)
    blob(x, y, h * 0.45 + h * 0.22, 0.035, 0.03, h * 0.22, M[mat], segs=8, rings=6, name="fig")
    blob(x, y, h * 0.45 + h * 0.5, 0.022, 0.022, 0.026, M[mat], segs=8, rings=4, name="head")


def bench(x0, y0, x1, y1, M, mat="seat"):
    box(x0, y0, 0.04, x1, y1, 0.065, M[mat], bevel=0.004)
    for x in (x0 + 0.02, x1 - 0.03):
        box(x, y0, 0, x + 0.012, y1, 0.04, M["stone_dark"], bevel=0)


def table(x, y, M):
    cylinder(x, y, 0, 0.07, 0.012, M["timber"], n=6)
    cylinder(x, y, 0.07, 0.085, 0.07, M["timber_light"], n=12)
    for a in (0.3, 2.4, 4.4):
        box(x + 0.11 * math.cos(a) - 0.025, y + 0.11 * math.sin(a) - 0.025, 0,
            x + 0.11 * math.cos(a) + 0.025, y + 0.11 * math.sin(a) + 0.025, 0.05, M["timber"],
            bevel=0.004)


def chimney(x, y, z0, z1, M, smoke=True):
    box(x - 0.05, y - 0.05, z0, x + 0.05, y + 0.05, z1, M["brick"], bevel=0.005)
    if smoke:
        blob(x + 0.03, y + 0.03, z1 + 0.09, 0.07, 0.07, 0.06, M["smoke"], name="smoke")
        blob(x + 0.08, y + 0.08, z1 + 0.2, 0.09, 0.09, 0.07, M["smoke"], name="smoke")


def band(x0, y0, x1, y1, z0, z1, mat, t=0.03):
    """Thin band around the outside of a footprint (stays hollow over courtyards)."""
    box(x0 - 0.012, y0 - 0.012, z0, x1 + 0.012, y0 + t, z1, mat, bevel=0)
    box(x0 - 0.012, y1 - t, z0, x1 + 0.012, y1 + 0.012, z1, mat, bevel=0)
    box(x0 - 0.012, y0 - 0.012, z0, x0 + t, y1 + 0.012, z1, mat, bevel=0)
    box(x1 - t, y0 - 0.012, z0, x1 + 0.012, y1 + 0.012, z1, mat, bevel=0)


def door_y(x, face_y, w, h, mat):
    box(x - w / 2, face_y - 0.015, 0, x + w / 2, face_y + 0.005, h, mat, bevel=0)


def wins_y(face_y, xs, z0, z1, M, w=0.07):
    kit.windows_y(face_y, xs, z0, z1, M, w=w)


def wins_x(face_x, ys, z0, z1, M, w=0.07):
    kit.windows_x(face_x, ys, z0, z1, M, w=w)


def portico(S, x0, x1, y_front, y_back, z_top, n, pediment=True, roof_mat=None, steps_n=0):
    """Row of n columns on y_front carrying an entablature and a gable (axis y) back to y_back."""
    M = S["M"]
    for i in range(n):
        x = x0 + (x1 - x0) * i / (n - 1)
        col(S, x, y_front, 0.0 if not steps_n else steps_n * 0.03, z_top - (steps_n * 0.03))
    ez = z_top + 0.08
    box(x0 - 0.07, y_front - 0.07, z_top, x1 + 0.07, y_back, ez, S["stone"], bevel=0.005)
    if pediment and S["kind"] not in ("flat",):
        gable(x0 - 0.07, y_front - 0.07, x1 + 0.07, y_back, ez, 0.22 if S["kind"] != "steep" else 0.4,
              roof_mat or S["roof"], axis="y", over=0.03, wall=S["stone"])
    else:
        box(x0 - 0.09, y_front - 0.09, ez, x1 + 0.09, y_back, ez + 0.05, S["stone"], bevel=0.005)


# ---------------------------------------------------------------- civic


def forum(M, reg=None):
    mats(M)
    S = style(M, "civic", reg)
    lot(S, 3, 3, curb=S["stone"])
    zc = 0.62
    # back portico (along x) and two side porticos (along y), open to the front
    box(-1.42, 1.3, 0, 1.42, 1.42, zc + 0.12, S["wall"])
    for i in range(9):
        col(S, -1.3 + 2.6 * i / 8, 0.85, 0.0, zc)
    box(-1.38, 0.79, zc, 1.38, 0.91, zc + 0.06, S["stone"], bevel=0.004)
    roof_mat = S["roof"] if S["kind"] != "flat" else S["wall"]
    shed(-1.42, 0.76, 1.42, 1.44, zc + 0.04, zc + 0.24, roof_mat, high="y1")
    for sx in (-1, 1):
        xw = sx * 1.36
        box(min(xw, xw + sx * 0.06), -1.15, 0, max(xw, xw + sx * 0.06), 0.8, zc + 0.12, S["wall"])
        for i in range(6):
            col(S, sx * 0.9, -1.05 + 1.7 * i / 5, 0.0, zc)
        box(min(sx * 0.84, sx * 0.96), -1.12, zc, max(sx * 0.84, sx * 0.96), 0.8, zc + 0.06,
            S["stone"], bevel=0.004)
        shed(min(sx * 0.82, sx * 1.42), -1.18, max(sx * 0.82, sx * 1.42), 0.78, zc + 0.04,
             zc + 0.24, roof_mat, high="x1" if sx > 0 else "x0")
    # rostra with banners, a central statue and a well-head
    box(-0.42, 0.3, 0, 0.42, 0.66, 0.16, S["stone"], bevel=0.006)
    steps(-0.25, 0.25, 0.14, 0.31, 0.16, 3, S["stone"])
    for x in (-0.36, 0.36):
        banner(x, 0.5, M, h=0.62)
    statue(0.0, -0.45, M, h=0.42)
    for x in (-0.55, 0.55):
        banner(x, -1.25, M, h=0.55)
    if reg in ("egypt", "north", "east"):
        tree(S, -1.2, -1.3, 0.8)
        tree(S, 1.25, -1.3, 0.8)


def governor_palace(M):
    mats(M)
    S = style(M, "civic")
    lot(S, 5, 5, M["garden"])
    box(-1.4, -2.45, 0, 1.4, -1.2, 0.012, M["paving"], bevel=0)  # forecourt
    x0, y0, x1, y1, z = -1.85, -1.2, 1.85, 2.2, 0.95
    # hollow square: four wings round an open peristyle garden
    t = 0.95
    box(x0, y0, 0, x1, y0 + t, z, S["wall"])
    box(x0, y1 - t, 0, x1, y1, z, S["wall"])
    box(x0, y0, 0, x0 + t, y1, z, S["wall"])
    box(x1 - t, y0, 0, x1, y1, z, S["wall"])
    band(x0, y0, x1, y1, 0, 0.14, M["stone_dark"])
    band(x0, y0, x1, y1, z * 0.5, z * 0.5 + 0.04, M["travertine"])
    kit.ring_roof(x0, y0, x1, y1, z, 0.4, 0.78, S["roof"])
    # courtyard garden with a pool
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    box(cx - 0.75, cy - 0.75, 0, cx + 0.75, cy + 0.75, 0.01, M["garden"], bevel=0)
    pool(cx - 0.3, cy - 0.2, cx + 0.3, cy + 0.2, M)
    for dx, dy in ((-0.55, -0.55), (0.55, 0.55), (-0.55, 0.55), (0.55, -0.55)):
        cypress(cx + dx, cy + dy, 0.4, M)
    # windows on the two visible faces, two storeys
    xs = [x0 + 0.25 + i * (x1 - x0 - 0.5) / 9 for i in range(10)]
    xs = [x for x in xs if abs(x) > 0.55]
    for zz in (0.22, 0.6):
        wins_y(y0, xs, zz, zz + 0.16, M)
        wins_x(x1, [y0 + 0.3 + i * (y1 - y0 - 0.6) / 8 for i in range(9)], zz, zz + 0.16, M)
    # grand portico with pediment, stairs and banners
    portico(S, -0.5, 0.5, -1.62, y0 + 0.05, 0.85, 6, pediment=True, steps_n=4)
    steps(-0.55, 0.55, -2.0, -1.68, 0.12, 4, M["travertine"])
    box(-0.6, -1.72, 0, 0.6, y0, 0.12, M["travertine"], bevel=0.004)
    box(-0.13, y0 - 0.02, 0, 0.13, y0 + 0.01, 0.5, M["bronze"], bevel=0)
    for x in (-0.75, 0.75):
        wall_banner_y(x, y0, 0.88, M, h=0.42, w=0.13)
    for x in (-1.25, 1.25):
        banner(x, -2.1, M, h=0.7)
    statue(-0.9, -2.0, M, h=0.38)
    statue(0.9, -2.0, M, h=0.38)
    for x in (-2.2, 2.2):
        for y in (-1.9, -0.6, 0.7, 1.9):
            cypress(x, y, 0.5, M)


def basilica(M, reg=None):
    mats(M)
    S = style(M, "civic", reg)
    lot(S, 6, 4, curb=S["stone"])
    x0, x1, y0, y1 = -2.65, 2.65, -0.85, 1.75
    za, zn = 0.62, 1.12
    # aisles (low) and nave (high, clerestory)
    box(x0, y0, 0, x1, y1, za, S["wall"])
    if S["kind"] == "flat":
        roof(S, x0, y0, x1, y1, za, 0.2)
    else:
        hip(x0, y0, x1, y1, za, 0.2, S["roof"])
    box(x0 + 0.15, y0 + 0.5, za, x1 - 0.15, y1 - 0.5, zn, S["wall"])
    wins_y(y0 + 0.5, [x0 + 0.4 + i * (x1 - x0 - 0.8) / 11 for i in range(12)], za + 0.2, zn - 0.08,
           M, w=0.09)
    wins_x(x1 - 0.15, [y0 + 0.75, y0 + 1.05, y0 + 1.35], za + 0.2, zn - 0.08, M)
    wins_y(y0, [x0 + 0.3 + i * (x1 - x0 - 0.6) / 13 for i in range(14)], 0.28, 0.48, M, w=0.08)
    roof(S, x0 + 0.15, y0 + 0.5, x1 - 0.15, y1 - 0.5, zn, 0.36, axis="x")
    # long front colonnade with stairs
    yc = -1.38
    for i in range(14):
        col(S, x0 + 0.15 + (x1 - x0 - 0.3) * i / 13, yc, 0.1, za - 0.1)
    box(x0, yc - 0.12, 0, x1, y0, 0.1, S["stone"], bevel=0.004)
    box(x0, yc - 0.08, za, x1, y0, za + 0.07, S["stone"], bevel=0.004)
    shed(x0, yc - 0.1, x1, y0 + 0.02, za + 0.05, za + 0.2,
         S["roof"] if S["kind"] != "flat" else S["wall"], high="y1")
    steps(-1.2, 1.2, -1.75, yc - 0.1, 0.1, 3, S["stone"])
    for x in (-2.0, -0.7, 0.7, 2.0):
        wall_banner_y(x, y0, za - 0.05, M, h=0.3, w=0.12)
    for x in (-2.5, 2.5):
        banner(x, -1.7, M, h=0.65)
    if reg:
        tree(S, -2.75, -1.75, 0.9)
        tree(S, 2.75, 1.85, 0.9)


def curia(M):
    mats(M)
    S = style(M, "civic")
    lot(S, 4, 3, curb=M["travertine"])
    x0, x1, y0, y1, z = -1.0, 1.0, -0.45, 1.3, 1.3
    box(x0, y0, 0, x1, y1, z, M["brick"])
    box(x0 - 0.02, y0 - 0.03, 0, x1 + 0.02, y0 + 0.02, z, M["travertine"])  # facade revetment
    box(x0 - 0.04, y0 - 0.05, z - 0.12, x1 + 0.04, y1 + 0.04, z, M["travertine"], bevel=0.006)
    gable(x0 - 0.04, y0 - 0.05, x1 + 0.04, y1 + 0.04, z, 0.42, M["patina"], axis="y",
          wall=M["travertine"])
    # tall bronze doors, three windows, banners
    box(-0.2, y0 - 0.05, 0.12, 0.2, y0 - 0.03, 0.78, M["bronze"], bevel=0)
    box(-0.012, y0 - 0.055, 0.12, 0.012, y0 - 0.03, 0.78, M["dark"], bevel=0)
    wins_y(y0 - 0.03, [-0.55, 0.0, 0.55], 0.88, 1.1, M, w=0.13)
    wins_x(x1, [0.0, 0.45, 0.9], 0.6, 1.05, M, w=0.12)
    steps(-0.45, 0.45, -0.85, y0 - 0.04, 0.12, 4, M["travertine"])
    for x in (-0.7, 0.7):
        wall_banner_y(x, y0 - 0.03, 0.82, M, h=0.5, w=0.16)
    for x in (-1.6, 1.6):
        banner(x, -1.15, M, h=0.7)
    statue(-1.15, -0.9, M, h=0.42)
    statue(1.15, -0.9, M, h=0.42)
    cypress(-1.7, 1.1, 0.55, M)
    cypress(1.7, 1.1, 0.55, M)


def mint(M):
    mats(M)
    S = style(M, "civic")
    lot(S, 3, 3, curb=M["stone_dark"])
    x0, x1, y0, y1, z = -1.1, 0.95, -0.35, 1.25, 0.82
    box(x0, y0, 0, x1, y1, 0.2, M["stone_dark"])
    box(x0 + 0.02, y0 + 0.02, 0.2, x1 - 0.02, y1 - 0.02, z, M["travertine"])
    hip(x0, y0, x1, y1, z, 0.3, M["patina"])
    for xx in (-0.7, -0.3, 0.3, 0.6):
        box(xx - 0.04, y0 - 0.01, 0.45, xx + 0.04, y0 + 0.005, 0.62, M["window"], bevel=0)
        for k in range(3):
            box(xx - 0.035 + k * 0.03, y0 - 0.015, 0.45, xx - 0.03 + k * 0.03, y0, 0.62,
                M["iron"], bevel=0)
    box(-0.15, y0 - 0.03, 0, 0.15, y0 + 0.005, 0.48, M["bronze"], bevel=0)
    for k in range(4):
        box(-0.15, y0 - 0.035, 0.08 + k * 0.11, 0.15, y0 - 0.025, 0.1 + k * 0.11, M["iron"],
            bevel=0)
    wall_banner_y(-0.5, y0, 0.78, M, h=0.26)
    wall_banner_y(0.5, y0, 0.78, M, h=0.26)
    # smelting furnace with glow and smoke at the side
    box(1.05, 0.4, 0, 1.4, 0.85, 0.32, M["brick"], bevel=0.01)
    box(1.12, 0.38, 0.06, 1.33, 0.4, 0.2, M["glow"], bevel=0)
    chimney(1.22, 0.75, 0.32, 0.8, M)
    # coin chests and gold stacks in the yard
    for i, (x, y) in enumerate(((-0.9, -0.95), (-0.55, -1.0), (0.45, -0.95))):
        box(x - 0.12, y - 0.08, 0, x + 0.12, y + 0.08, 0.12, M["timber"], bevel=0.008)
        box(x - 0.125, y - 0.085, 0.05, x + 0.125, y + 0.085, 0.065, M["iron"], bevel=0)
        for k in range(3):
            cylinder(x - 0.06 + k * 0.06, y, 0.12, 0.12 + 0.03 * (k + 1), 0.025, M["coin"], n=10)
    banner(-1.3, -1.3, M, h=0.55)
    banner(1.3, -1.3, M, h=0.55)


# ---------------------------------------------------------------- health


def latrina(M, reg=None):
    mats(M)
    S = style(M, "health", reg)
    lot(S, 2, 2, curb=M["marble"] if not reg else None)
    x0, x1, y0, y1, z = -0.8, 0.8, -0.25, 0.85, 0.5
    box(x0, y0 + 0.2, 0, x1, y1, z, S["wall"])
    box(x0, y0, 0, x0 + 0.08, y0 + 0.2, z, S["wall"])
    box(x1 - 0.08, y0, 0, x1, y0 + 0.2, z, S["wall"])
    for x in (-0.4, 0.0, 0.4):
        col(S, x, y0 + 0.02, 0, z, r=0.035)
    box(x0, y0 - 0.02, z - 0.06, x1, y0 + 0.2, z, S["wall"], bevel=0.004)
    box(x0 + 0.08, y0 + 0.2, 0.0, x1 - 0.08, y0 + 0.24, 0.16, M["marble"], bevel=0)  # bench
    box(x0 - 0.005, y0 - 0.025, 0.3, x1 + 0.005, y0 - 0.01, 0.36, M["teal"], bevel=0)
    roof(S, x0, y0 - 0.02, x1, y1, z, 0.22, axis="x")
    # water channel in front and a basin
    box(x0, -0.62, 0, x1, -0.42, 0.05, M["marble"], bevel=0.004)
    box(x0 + 0.03, -0.59, 0.0, x1 - 0.03, -0.45, 0.055, M["pool"], bevel=0)
    box(-0.12, -0.9, 0, 0.12, -0.72, 0.12, M["marble"], bevel=0.006)
    box(-0.09, -0.87, 0, 0.09, -0.75, 0.125, M["pool"], bevel=0)
    if reg:
        tree(S, 0.82, -0.82, 0.7)


def physician(M, reg=None):
    mats(M)
    S = style(M, "health", reg)
    lot(S, 1, 1)
    x0, x1, y0, y1, z = -0.36, 0.36, -0.12, 0.4, 0.42
    block(S, x0, y0, x1, y1, z, rh=0.18, axis="x")
    door_y(-0.1, y0, 0.12, 0.26, M["dark"] if reg != "east" else M["blue"])
    wins_y(y0, [0.17], 0.18, 0.3, M)
    wins_x(x1, [0.05, 0.25], 0.18, 0.3, M)
    box(x0 - 0.005, y0 - 0.012, 0.0, x1 + 0.005, y0 + 0.002, 0.05, M["teal"], bevel=0)
    # teal awning and the staff of Asclepius
    shed(-0.32, -0.33, 0.12, y0 + 0.01, 0.3, 0.38, M["teal"], high="y1")
    for x in (-0.3, 0.1):
        box(x - 0.01, -0.33, 0, x + 0.01, -0.31, 0.3, M["timber"], bevel=0)
    cylinder(0.3, -0.3, 0, 0.36, 0.012, M["gold"], n=6)
    blob(0.3, -0.3, 0.2, 0.03, 0.03, 0.12, M["teal"], segs=8, rings=5, name="snake")
    pot_plant(0.22, -0.35, M)
    pot_plant(-0.38, -0.38, M)


def hospital(M):
    mats(M)
    S = style(M, "health")
    lot(S, 4, 4, M["garden"])
    x0, x1, y0, y1, z, t = -1.6, 1.6, -1.0, 1.75, 0.7, 0.75
    for a, b, c, d in ((x0, y0, x1, y0 + t), (x0, y1 - t, x1, y1), (x0, y0, x0 + t, y1),
                       (x1 - t, y0, x1, y1)):
        box(a, b, 0, c, d, z, M["white_plaster"])
    band(x0, y0, x1, y1, 0.0, 0.07, M["teal"])
    band(x0, y0, x1, y1, z - 0.06, z, M["teal"])
    kit.ring_roof(x0, y0, x1, y1, z, 0.32, 0.62, M["roof_faded"])
    cx, cy = 0.0, (y0 + y1) / 2
    box(cx - 0.6, cy - 0.6, 0, cx + 0.6, cy + 0.6, 0.01, M["garden"], bevel=0)
    for k in range(4):
        box(cx - 0.45, cy - 0.45 + k * 0.25, 0, cx + 0.45, cy - 0.35 + k * 0.25, 0.04, M["herb"],
            bevel=0.004)
    pool(cx - 0.15, cy - 0.15, cx + 0.15, cy + 0.15, M)
    xs = [x0 + 0.25 + i * 0.3 for i in range(11)]
    xs = [x for x in xs if abs(x) > 0.3]
    wins_y(y0, xs, 0.3, 0.48, M)
    wins_x(x1, [y0 + 0.25 + i * 0.3 for i in range(9)], 0.3, 0.48, M)
    # entrance with teal awning, the Asclepius staff, benches and herb beds out front
    door_y(0.0, y0, 0.22, 0.4, M["dark"])
    shed(-0.4, -1.35, 0.4, y0 + 0.01, 0.42, 0.55, M["teal"], high="y1")
    for x in (-0.38, 0.38):
        box(x - 0.012, -1.35, 0, x + 0.012, -1.33, 0.42, M["timber"], bevel=0)
    cylinder(-0.75, -1.5, 0, 0.5, 0.016, M["gold"], n=6)
    blob(-0.75, -1.5, 0.3, 0.045, 0.045, 0.16, M["teal"], segs=8, rings=5, name="snake")
    bench(0.6, -1.6, 1.2, -1.5, M)
    for x in (-1.5, -1.1, 1.1, 1.5):
        box(x - 0.15, -1.9, 0, x + 0.15, -1.55, 0.04, M["herb"], bevel=0.004)
    cypress(-1.8, 1.8, 0.5, M)
    cypress(1.8, -1.8, 0.5, M)


def balneum(M, reg=None):
    mats(M)
    S = style(M, "health", reg)
    lot(S, 3, 3, M["marble_paving"] if not reg else None)
    x0, x1, y0, y1, z = -1.15, 1.15, 0.1, 1.35, 0.6
    block(S, x0, y0, x1, y1, z, rh=0.22, axis="x")
    box(x0 - 0.005, y0 - 0.012, 0.0, x1 + 0.005, y0 + 0.002, 0.06, M["teal"], bevel=0)
    wins_y(y0, [-0.85, -0.55, 0.55, 0.85], 0.28, 0.44, M)
    wins_x(x1, [0.4, 0.75, 1.05], 0.28, 0.44, M)
    door_y(0.0, y0, 0.2, 0.36, S["door"])
    if reg != "north":
        dome(0.0, 0.75, z + 0.08, 0.45, M, mat="lead" if reg != "egypt" else "sandstone",
             drum=0.1, drum_mat=S["wall"])
    else:
        box(-0.4, 0.4, z, 0.4, 1.1, z + 0.25, S["wall"])
        gable(-0.4, 0.4, 0.4, 1.1, z + 0.25, 0.3, S["roof"], axis="y", wall=S["wall"])
    chimney(-0.95, 1.15, z, z + 0.45, M)
    chimney(0.95, 1.2, z, z + 0.35, M)
    # outdoor plunge pool with a marble rim and a colonnade
    pool(-1.0, -1.25, 1.0, -0.05, M)
    for x in (-1.0, -0.5, 0.0, 0.5, 1.0):
        col(S, x, -0.02, 0, 0.42, r=0.035)
    box(-1.05, -0.06, 0.42, 1.05, 0.12, 0.47, S["stone"], bevel=0.004)
    statue(-1.3, -1.3, M, h=0.32, mat="marble", base="marble")
    if reg:
        tree(S, 1.3, -1.3, 0.8)


def thermae(M):
    mats(M)
    S = style(M, "health")
    lot(S, 6, 6, M["marble_paving"])
    # main bath block behind the natatio
    x0, x1, y0, y1, z = -2.6, 2.6, 0.2, 2.75, 0.8
    box(x0, y0, 0, x1, y1, z, M["white_plaster"])
    box(x0 - 0.01, y0 - 0.012, 0, x1 + 0.01, y1 + 0.01, 0.08, M["teal"], bevel=0)
    box(x0 - 0.01, y0 - 0.012, z - 0.06, x1 + 0.01, y1 + 0.01, z, M["teal"], bevel=0)
    hip(x0, y0, x1, y1, z, 0.22, M["roof_faded"])
    # tall central caldarium with a big dome and flanking barrel vaults
    box(-1.0, 0.5, z, 1.0, 2.4, z + 0.5, M["white_plaster"])
    dome(0.0, 1.45, z + 0.5, 0.95, M, drum=0.12)
    for sx in (-1, 1):
        cx = sx * 1.8
        box(cx - 0.55, 0.45, z, cx + 0.55, 2.45, z + 0.3, M["white_plaster"])
        cylinder(z + 0.3, cx, 0.42, 2.48, 0.55, M["lead"], n=20, axis="y", name="vault")
        wins_y(0.45, [cx - 0.3, cx, cx + 0.3], z + 0.08, z + 0.26, M)
    for xx in (-0.7, -0.35, 0.35, 0.7):
        box(xx - 0.05, 0.48, z + 0.12, xx + 0.05, 0.5, z + 0.4, M["window"], bevel=0)
    wins_y(y0, [x0 + 0.3 + i * 0.32 for i in range(16)], 0.32, 0.55, M)
    wins_x(x1, [y0 + 0.3 + i * 0.3 for i in range(8)], 0.32, 0.55, M)
    for x in (-0.6, 0.6):
        door_y(x, y0, 0.22, 0.45, M["dark"])
    for x, y in ((-2.3, 2.5), (2.3, 2.5), (-1.4, 2.6)):
        chimney(x, y, z, z + 0.5, M)
    # natatio and its colonnades
    pool(-2.0, -2.45, 2.0, -0.2, M)
    for sx in (-1, 1):
        for i in range(7):
            col(S, sx * 2.35, -2.5 + i * 0.4, 0, 0.5, r=0.04)
        box(min(sx * 2.25, sx * 2.45), -2.6, 0.5, max(sx * 2.25, sx * 2.45), 0.1, 0.56,
            M["marble"], bevel=0.004)
        shed(min(sx * 2.2, sx * 2.9), -2.65, max(sx * 2.2, sx * 2.9), 0.15, 0.55, 0.68,
             M["roof_faded"], high="x1" if sx > 0 else "x0")
        box(min(sx * 2.8, sx * 2.9), -2.65, 0, max(sx * 2.8, sx * 2.9), 0.15, 0.6,
            M["white_plaster"])
    for x in (-1.0, 0.0, 1.0):
        statue(x, -2.75, M, h=0.34, mat="marble")
    for x, y in ((-2.75, 2.75), (2.75, 2.75), (-2.75, -2.85), (2.75, -2.85)):
        cypress(x, y, 0.6, M)


# ---------------------------------------------------------------- entertainment


def tavern(M, reg=None):
    mats(M)
    S = style(M, "ent", reg)
    lot(S, 2, 2)
    x0, x1, y0, y1, z = -0.75, 0.75, -0.15, 0.85, 0.55
    if S["kind"] == "hip":
        S = dict(S, kind="gable")
    block(S, x0, y0, x1, y1, z, rh=0.26, axis="x")
    box(x0 - 0.005, y0 - 0.012, 0.0, x1 + 0.005, y0 + 0.002, 0.07, M["band_red"], bevel=0)
    door_y(-0.35, y0, 0.16, 0.34, S["door"])
    box(0.0, y0 - 0.06, 0, 0.55, y0, 0.2, M["marble"], bevel=0.005)  # counter
    for k in range(3):
        amphora_at = 0.1 + k * 0.16
        blob(amphora_at, y0 - 0.03, 0.24, 0.035, 0.035, 0.045, M["amphora"], segs=8, rings=5)
    wins_y(y0, [-0.6, 0.3], 0.3, 0.44, M)
    wins_x(x1, [0.2, 0.55], 0.28, 0.42, M)
    # striped awning, tables, amphorae stack, barrel and a hanging sign
    shed(-0.72, -0.62, 0.72, y0 + 0.01, 0.36, 0.46, M["awning_red"], high="y1")
    for x in (-0.7, 0.0, 0.7):
        box(x - 0.012, -0.62, 0, x + 0.012, -0.6, 0.36, M["timber"], bevel=0)
    table(-0.4, -0.78, M)
    table(0.3, -0.8, M)
    for i, (dx, dy) in enumerate(((0.0, 0.0), (0.09, 0.02), (0.045, 0.08))):
        kit.amphora(0.82 + dx - 0.1, 0.1 + dy, M)
    cylinder(-0.86, 0.3, 0, 0.16, 0.07, M["timber_light"], n=12, name="barrel")
    banner(0.86, -0.6, M, h=0.5, mat="banner_y")


def theater(M, reg=None):
    mats(M)
    S = style(M, "ent", reg)
    lot(S, 5, 4)
    cx, cy = 0.0, -0.75
    ro, ri = 2.3, 0.62
    # cavea: tiers rising to the back, closed by a curved outer wall
    ring_mesh(cx, cy, 0.0, 0.0, 0, math.pi, 24, [(0.0, 0.012), (ri, 0.012)], M["marble"],
              name="orchestra")
    tiers(cx, cy, ri, ri, 0, math.pi, ro - ri - 0.12, 9, 0.06, 0.9, M)
    ring_mesh(cx, cy, ro - 0.12, ro - 0.12, 0, math.pi, 40,
              [(0.0, 0.9), (0.0, 1.02), (0.14, 1.02), (0.14, 0.0)], S["wall"], name="outer")
    arcade(cx, cy, ro + 0.02, ro + 0.02, 0.05, math.pi - 0.05, 0.12, 0.42, 16, M)
    arcade(cx, cy, ro + 0.02, ro + 0.02, 0.05, math.pi - 0.05, 0.55, 0.82, 16, M)
    # striped velarium strips on the rim and banners
    for i in range(7):
        a = 0.2 + i * (math.pi - 0.4) / 6
        da = 0.12
        ring_mesh(cx, cy, ro - 0.12, ro - 0.12, a - da, a + da, 3,
                  [(0.02, 1.05), (-0.5, 0.92)], M["awning_red"], name="vel")
    for i in range(9):
        a = 0.1 + i * (math.pi - 0.2) / 8
        x, y = cx + (ro + 0.02) * math.cos(a), cy + (ro + 0.02) * math.sin(a)
        cylinder(x, y, 1.02, 1.3, 0.012, M["timber"], n=6)
        box(x - 0.012, y - 0.06, 1.12, x + 0.012, y + 0.06, 1.28,
            M["banner"] if i % 2 else M["banner_y"], bevel=0)
    # stage building (scaenae frons) at the front, its back to the street
    box(-1.9, -1.45, 0, 1.9, -1.05, 0.55, S["wall"])
    box(-1.95, -1.48, 0.5, 1.95, -1.02, 0.58, S["stone"], bevel=0.004)
    if S["kind"] == "flat":
        roof(S, -1.9, -1.45, 1.9, -1.05, 0.58, 0.1)
    else:
        shed(-1.95, -1.48, 1.95, -1.0, 0.58, 0.72, S["roof"], high="y1")
    for x in (-1.3, -0.45, 0.45, 1.3):
        box(x - 0.1, -1.47, 0, x + 0.1, -1.45, 0.34, M["window"], bevel=0)
    for x in (-1.6, -0.85, 0.0, 0.85, 1.6):
        wall_banner_y(x, -1.45, 0.48, M, mat="banner" if int(x * 10) % 2 == 0 else "banner_y",
                      h=0.2, w=0.1)
    for x in (-2.2, 2.2):
        banner(x, -1.7, M, h=0.6)
    if reg:
        tree(S, -2.25, 1.75, 0.8)
        tree(S, 2.25, 1.75, 0.8)


def odeon(M):
    mats(M)
    S = style(M, "ent")
    lot(S, 4, 4, M["paving"])
    x0, x1, y0, y1, z = -1.55, 1.55, -0.85, 1.6, 0.75
    box(x0, y0, 0, x1, y1, z, M["warm_stone"])
    ring_mesh(0.0, y1 - 0.05, 1.55, 0.6, math.pi, 2 * math.pi, 20, [(0.0, 0.0), (0.0, z)],
              M["warm_stone"], name="apse")  # rounded back (hidden mostly)
    # striped tent roof over the hall (hip) with a gold lyre disc on the front
    hip(x0, y0, x1, y1, z, 0.55, M["awning_red"], over=0.1)
    blob(0.0, y0 - 0.02, z - 0.22, 0.13, 0.02, 0.13, M["gold"], segs=12, rings=6, name="lyre")
    for x in (-1.2, -0.6, 0.6, 1.2):
        box(x - 0.08, y0 - 0.012, 0.1, x + 0.08, y0 + 0.002, 0.5, M["window"], bevel=0)
    wins_x(x1, [-0.4, 0.2, 0.8, 1.3], 0.3, 0.55, M, w=0.1)
    # entrance portico with banners
    portico(S, -0.6, 0.6, -1.3, y0 + 0.02, 0.55, 4, pediment=False)
    steps(-0.65, 0.65, -1.6, -1.36, 0.08, 3, M["warm_stone"])
    for x in (-1.75, -1.2, 1.2, 1.75):
        banner(x, -1.75, M, h=0.65, mat="banner" if x < 0 else "banner_y")
    for x, y in ((-1.85, 1.85), (1.85, 1.85)):
        cypress(x, y, 0.5, M)


def amphitheater(M):
    mats(M)
    S = style(M, "ent")
    lot(S, 7, 7, M["paving"])
    cx, cy = 0.0, 0.1
    ax_, ay_ = 1.35, 1.0  # arena
    depth = 1.65
    box(-ax_, cy - ay_, 0, ax_, cy + ay_, 0.005, M["arena"], bevel=0)
    # elliptical arena fill: sweep from the centre line to the arena edge
    ring_mesh(cx, cy, ax_, ay_, 0, 2 * math.pi, 48, [(-min(ax_, ay_), 0.02), (0.0, 0.02)],
              M["arena"], closed=True, name="arena2")
    ring_mesh(cx, cy, ax_, ay_, 0, 2 * math.pi, 48, [(0.0, 0.0), (0.0, 0.16), (0.06, 0.16)],
              M["marble"], closed=True, name="podium")
    for k in range(10):
        r0 = 0.06 + depth * k / 10
        r1 = 0.06 + depth * (k + 1) / 10
        zt = 0.16 + 0.95 * (k + 1) / 10
        ring_mesh(cx, cy, ax_, ay_, 0, 2 * math.pi, 56, [(r0, 0.0), (r0, zt)], M["riser"],
                  closed=True, name="riser")
        ring_mesh(cx, cy, ax_, ay_, 0, 2 * math.pi, 56, [(r0, zt), (r1, zt)], M["seat"],
                  closed=True, name="tread")
    ro = depth + 0.06
    ring_mesh(cx, cy, ax_ + ro, ay_ + ro, 0, 2 * math.pi, 64,
              [(0.0, 1.11), (0.0, 1.25), (0.22, 1.25), (0.22, 0.0)], M["warm_stone"], closed=True,
              name="outer")
    rx, ry = ax_ + ro + 0.22, ay_ + ro + 0.22
    for z0, z1 in ((0.1, 0.42), (0.55, 0.85)):
        arcade(cx, cy, rx, ry, 0, 2 * math.pi, z0, z1, 40, M)
    ring_mesh(cx, cy, rx, ry, 0, 2 * math.pi, 64, [(0.005, 0.47), (0.005, 0.5)], M["travertine"],
              closed=True, name="cornice")
    for i in range(14):
        a = 2 * math.pi * i / 14
        x, y = cx + (rx - 0.05) * math.cos(a), cy + (ry - 0.05) * math.sin(a)
        cylinder(x, y, 1.25, 1.6, 0.014, M["timber"], n=6)
        box(x - 0.06, y - 0.06, 1.42, x + 0.06, y + 0.06, 1.58,
            M["banner"] if i % 2 else M["banner_y"], bevel=0)
    for i in range(10):
        a = math.pi * 0.15 + i * 0.7 / 9 * math.pi
        da = 0.1
        ring_mesh(cx, cy, ax_ + ro, ay_ + ro, a - da, a + da, 3, [(0.02, 1.27), (-0.7, 1.05)],
                  M["awning_red"], name="vel")
    for x, y in ((-3.2, -3.2), (3.2, -3.2), (-3.2, 3.2), (3.2, 3.2)):
        cypress(x, y, 0.55, M)


def circus(M):
    mats(M)
    S = style(M, "ent")
    lot(S, 16, 5, M["dirt"])
    tx0, tx1, ty0, ty1 = -7.2, 5.6, -1.15, 1.25
    box(tx0, ty0, 0, tx1, ty1, 0.01, M["arena"], bevel=0)
    ring_mesh(tx1, 0.05, 0.0, 0.0, -math.pi / 2, math.pi / 2, 24, [(0.0, 0.01), (1.2, 0.01)],
              M["arena"], name="turn")
    # spina with obelisk, statues and the metae cones
    box(-5.0, -0.07, 0, 3.8, 0.17, 0.14, M["marble"], bevel=0.006)
    box(-4.9, -0.03, 0.14, 3.7, 0.13, 0.16, M["pool"], bevel=0)
    box(-0.16, -0.11, 0.14, 0.16, 0.21, 0.24, M["marble"], bevel=0.006)
    v = [(-0.1, -0.05, 0.24), (0.1, -0.05, 0.24), (0.1, 0.15, 0.24), (-0.1, 0.15, 0.24),
         (0.0, 0.05, 1.25)]
    mesh("obelisk", v, [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)], M["warm_stone"])
    blob(0.0, 0.05, 1.25, 0.04, 0.04, 0.05, M["gold"], segs=6, rings=4)
    for x in (-3.0, -1.6, 1.6, 3.0):
        statue(x, 0.05, M, h=0.38, mat="gold")
    for x in (-5.2, 4.0):
        for dy in (-0.05, 0.05, 0.15):
            cone(x, dy, 0.0, 0.42, 0.06, M["bronze"], n=10, name="meta")
    # seating: high stand at the back, low stand at the front, curved end
    ty_b = ty1 + 0.05
    for k in range(7):
        y0 = ty_b + 1.0 * k / 7
        zt = 0.08 + 0.68 * (k + 1) / 7
        box(tx0 + 0.6, y0, 0, tx1, y0 + 1.0 / 7, zt, M["seat"] if k % 2 else M["riser"], bevel=0)
    box(tx0 + 0.6, ty_b + 1.0, 0, tx1, ty_b + 1.12, 0.92, M["warm_stone"])
    for k in range(4):
        y1 = ty0 - 0.05 - 0.75 * k / 4
        zt = 0.06 + 0.32 * (k + 1) / 4
        box(tx0 + 0.6, y1 - 0.75 / 4, 0, tx1, y1, zt, M["seat"] if k % 2 else M["riser"], bevel=0)
    box(tx0 + 0.6, ty0 - 0.92, 0, tx1, ty0 - 0.8, 0.48, M["warm_stone"])
    for x in [tx0 + 1.0 + i * 0.6 for i in range(22)]:
        box(x - 0.08, ty0 - 0.925, 0.12, x + 0.08, ty0 - 0.915, 0.34, M["window"], bevel=0)
    tiers(tx1, 0.05, 1.25, 1.25, -math.pi / 2, math.pi / 2, 0.95, 6, 0.06, 0.75, M, segs=24)
    ring_mesh(tx1, 0.05, 2.2, 2.2, -math.pi / 2, math.pi / 2, 24,
              [(0.0, 0.75), (0.0, 0.92), (0.12, 0.92), (0.12, 0.0)], M["warm_stone"], name="end")
    # carceres (starting gates) with two towers
    box(-7.85, -1.2, 0, -7.25, 1.3, 0.45, M["warm_stone"])
    for k in range(6):
        y = -0.95 + k * 0.4
        box(-7.26, y - 0.13, 0.0, -7.24, y + 0.13, 0.3, M["window"], bevel=0)
    box(-7.88, -1.23, 0.45, -7.22, 1.33, 0.5, M["travertine"], bevel=0.004)
    for y in (-1.45, 1.55):
        box(-7.95, y - 0.25, 0, -7.25, y + 0.25, 0.72, M["warm_stone"])
        hip(-7.95, y - 0.25, -7.25, y + 0.25, 0.72, 0.2, M["roof"])
    # banners and awnings along the back stand
    for i, x in enumerate([tx0 + 1.0 + i * 0.9 for i in range(15)]):
        cylinder(x, ty_b + 1.06, 0.92, 1.25, 0.014, M["timber"], n=6)
        box(x - 0.012, ty_b + 0.99, 1.06, x + 0.012, ty_b + 1.13, 1.22,
            M["banner"] if i % 2 else M["banner_y"], bevel=0)
    for i in range(6):
        x = -4.5 + i * 1.8
        shed(x - 0.45, ty_b + 0.45, x + 0.45, ty_b + 1.06, 0.82, 0.95, M["awning_red"], high="y1")
    box(-1.0, -2.5, 0, 1.0, -1.97, 0.02, M["paving"], bevel=0)


# ---------------------------------------------------------------- education


def school(M, reg=None):
    mats(M)
    S = style(M, "edu", reg)
    lot(S, 2, 2)
    x0, x1, y0, y1, z = -0.8, 0.8, 0.25, 0.88, 0.5
    block(S, x0, y0, x1, y1, z, rh=0.22, axis="x")
    door_y(0.0, y0, 0.14, 0.3, S["door"])
    wins_y(y0, [-0.5, 0.5], 0.26, 0.4, M)
    wins_x(x1, [0.45, 0.7], 0.26, 0.4, M)
    # porch (pergola) with benches facing the master's chair
    for x in (-0.75, 0.0, 0.75):
        for y in (-0.6,):
            col(S, x, y, 0, 0.42, r=0.03)
    box(-0.8, -0.65, 0.42, 0.8, y0, 0.46, S["stone"], bevel=0.004)
    for k in range(7):
        x = -0.75 + k * 0.25
        box(x - 0.02, -0.66, 0.46, x + 0.02, y0, 0.5, M["timber"], bevel=0)
    for k in range(3):
        bench(-0.55, -0.4 + k * 0.17, 0.25, -0.34 + k * 0.17, M, mat="timber_light")
    box(0.45, -0.15, 0, 0.6, 0.0, 0.18, M["timber"], bevel=0.006)  # chair
    # scroll rack
    box(-0.78, -0.95, 0, -0.45, -0.85, 0.24, M["timber"], bevel=0.004)
    for k in range(3):
        for j in range(2):
            cylinder(0.04 + j * 0.08, -0.72 + k * 0.1, -0.97, -0.94, 0.03, M["scroll"], n=8,
                     axis="y", name="scroll")
    tree(S, 0.75, -0.8, 0.7)


def library(M, reg=None):
    mats(M)
    S = style(M, "edu", reg)
    lot(S, 3, 3, M["paving"] if not reg else None)
    x0, x1, y0, y1, z = -1.1, 1.1, -0.2, 1.3, 0.95
    block(S, x0, y0, x1, y1, z, rh=0.28, axis="x")
    box(x0 - 0.01, y0 - 0.012, z * 0.5, x1 + 0.01, y1 + 0.01, z * 0.5 + 0.035, S["stone"], bevel=0)
    for zz in (0.18, 0.6):
        wins_y(y0, [-0.85, -0.6, 0.6, 0.85], zz, zz + 0.2, M, w=0.08)
        wins_x(x1, [0.1, 0.45, 0.8, 1.1], zz, zz + 0.2, M, w=0.08)
    # portico of four columns with a pediment; scroll niches beside the door
    portico(S, -0.45, 0.45, -0.62, y0 + 0.02, 0.62, 4, pediment=True,
            roof_mat=S["roof"] if S["kind"] != "flat" else None)
    door_y(0.0, y0, 0.2, 0.42, S["door"])
    for x in (-0.3, 0.3):
        box(x - 0.06, y0 - 0.012, 0.15, x + 0.06, y0 + 0.002, 0.4, M["timber"], bevel=0)
        for k in range(3):
            cylinder(0.19 + k * 0.08, x, y0 - 0.02, y0 - 0.005, 0.03, M["scroll"], n=8, axis="y")
    steps(-0.5, 0.5, -0.85, -0.66, 0.06, 2, S["stone"])
    statue(-0.95, -0.95, M, h=0.38, mat="bronze", base="limestone")
    bench(0.6, -1.05, 1.15, -0.95, M)
    tree(S, 1.2, -0.45, 0.8)
    tree(S, -1.25, 1.25, 0.8)


def academy(M):
    mats(M)
    S = style(M, "edu")
    lot(S, 4, 4, M["garden"])
    x0, x1, y0, y1 = -1.75, 1.75, -1.6, 1.75
    # main hall at the back
    block(S, -1.3, 0.85, 1.3, 1.7, 0.95, rh=0.3, axis="x")
    wins_y(0.85, [-1.0, -0.7, 0.7, 1.0], 0.3, 0.6, M, w=0.09)
    portico(S, -0.55, 0.55, 0.45, 0.87, 0.75, 4, pediment=True)
    door_y(0.0, 0.85, 0.2, 0.45, M["dark"])
    # peristyle walks on the sides and front (blue-grey lean-to roofs)
    for sx in (-1, 1):
        for i in range(6):
            col(S, sx * 1.35, -1.25 + i * 0.4, 0, 0.45, r=0.035)
        shed(min(sx * 1.3, sx * 1.75), -1.45, max(sx * 1.3, sx * 1.75), 0.8, 0.45, 0.6, S["roof"],
             high="x1" if sx > 0 else "x0")
        box(min(sx * 1.7, sx * 1.75), -1.45, 0, max(sx * 1.7, sx * 1.75), 0.8, 0.6, S["wall"])
    for i in range(6):
        col(S, -1.0 + i * 0.4, -1.35, 0, 0.45, r=0.035)
    shed(-1.3, -1.6, 1.3, -1.3, 0.45, 0.6, S["roof"], high="y1")
    # garden: exedra bench, olives, fountain, scroll benches
    ring_mesh(0.0, -0.45, 0.55, 0.45, 0.15, math.pi - 0.15, 16,
              [(0.0, 0.0), (0.0, 0.1), (0.12, 0.1), (0.12, 0.0)], M["limestone"], name="exedra")
    ring_mesh(0.0, -0.45, 0.67, 0.57, 0.15, math.pi - 0.15, 16,
              [(0.0, 0.0), (0.0, 0.2), (0.04, 0.2), (0.04, 0.0)], M["limestone"], name="back")
    pool(-0.2, -0.95, 0.2, -0.65, M)
    for x, y in ((-0.85, 0.1), (0.85, 0.1), (-0.85, -0.9), (0.85, -0.9)):
        olive(x, y, M, 0.9)
    statue(0.0, -0.25, M, h=0.3, mat="marble", base="limestone")
    for x in (-1.85, 1.85):
        banner(x, -1.85, M, h=0.55, mat="banner_b")


# ---------------------------------------------------------------- registry

SPRITES = {}


def _add(name, fp, fn, lit=False, regions=False):
    SPRITES["bld/" + name] = dict(fp=fp, build=fn, lit=lit)
    if regions:
        for r in ("east", "egypt", "north"):
            SPRITES["bld/%s@%s" % (name, r)] = dict(fp=fp, build=(lambda f, rr: lambda M: f(M, rr))(fn, r))


_add("forum", (3, 3), forum, regions=True)
_add("governor_palace", (5, 5), governor_palace, lit=True)
_add("basilica", (6, 4), basilica, lit=True, regions=True)
_add("curia", (4, 3), curia)
_add("mint", (3, 3), mint)
_add("latrina", (2, 2), latrina, regions=True)
_add("physician", (1, 1), physician, regions=True)
_add("hospital", (4, 4), hospital, lit=True)
_add("balneum", (3, 3), balneum, lit=True, regions=True)
_add("thermae", (6, 6), thermae, lit=True)
_add("tavern", (2, 2), tavern, lit=True, regions=True)
_add("theater", (5, 4), theater, regions=True)
_add("amphitheater", (7, 7), amphitheater, lit=True)
_add("odeon", (4, 4), odeon)
_add("circus", (16, 5), circus)
_add("school", (2, 2), school, regions=True)
_add("library", (3, 3), library, regions=True)
_add("academy", (4, 4), academy)
