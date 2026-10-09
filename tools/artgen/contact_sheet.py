#!/usr/bin/env python3
"""Review sheets for the generated sprites.

  build/contact_sheet.png      every sprite at 1x, 2x, 4x, grouped by family,
                               each on a grass and on a paving background
  build/contact/<family>.png   the same, one sheet per family (easier to view)
  build/scene_test.png         24x16-tile test scene at 3x composited the way
                               the renderer will (terrain blends, river with
                               shores, cliffs, roads incl. diagonals, trees,
                               buildings, citizens)

Run after build_all.py:  python3 tools/artgen/contact_sheet.py
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from palette import ROOT                         # noqa: E402
from primitives import hash2                     # noqa: E402

SPRITES = os.path.join(ROOT, "assets", "sprites")
BUILD = os.path.join(ROOT, "build")
TILE = 16
SCALES = (1, 2, 4)
FAMILY_ORDER = ["terrain", "road", "obj", "bld", "people", "fx"]
BG = (34, 30, 28, 255)
FG = (230, 222, 205, 255)


def load_all():
    spr = {}
    for dirpath, _, files in os.walk(SPRITES):
        for fn in sorted(files):
            if fn.endswith(".png"):
                p = os.path.join(dirpath, fn)
                name = os.path.relpath(p, SPRITES)[:-4].replace(os.sep, "/")
                im = Image.open(p).convert("RGBA")
                im.load()
                spr[name] = im
    with open(os.path.join(SPRITES, "meta.json")) as f:
        meta = json.load(f)
    return spr, meta


# ------------------------------------------------------------ contact sheet
def _bg_tile(spr, kind):
    """16x16 background tile list for 'grass' or 'paving'."""
    if kind == "grass":
        return [spr["terrain/grass_%d" % v] for v in range(4)]
    t = Image.new("RGBA", (TILE, TILE))
    for p in ("c", "n", "e", "s", "w", "fill_ne", "fill_nw", "fill_se", "fill_sw",
              "ne", "nw", "se", "sw"):
        im = spr["road/paved_%s" % p]
        t.paste(im, (0, 0), im)
    return [t]


def _tiled(tiles, w, h, k):
    out = Image.new("RGBA", (w, h))
    tw = TILE * k
    for y in range(0, h, tw):
        for x in range(0, w, tw):
            t = tiles[hash2(x // tw, y // tw, 1) % len(tiles)]
            out.paste(t.resize((tw, tw), Image.NEAREST), (x, y))
    return out


def _render_cell(im, name, bgs, font):
    pad = 2
    blocks = []
    for bg in bgs:
        for k in SCALES:
            w, h = im.width * k + 2 * pad * k, im.height * k + 2 * pad * k
            b = _tiled(bg, w, h, k)
            s = im.resize((im.width * k, im.height * k), Image.NEAREST)
            b.paste(s, (pad * k, pad * k), s)
            blocks.append(b)
    gap = 4
    label_h = 12
    cw = sum(b.width for b in blocks) + gap * (len(blocks) - 1) + gap * 2
    ch = max(b.height for b in blocks) + label_h
    lw = int(ImageDraw.Draw(Image.new("RGBA", (1, 1))).textlength(name, font=font)) + 4
    cw = max(cw, lw)
    cell = Image.new("RGBA", (cw, ch), BG)
    d = ImageDraw.Draw(cell)
    d.text((0, 0), name, fill=FG, font=font)
    x = 0
    for i, b in enumerate(blocks):
        cell.paste(b, (x, label_h))
        x += b.width + gap + (gap * 2 if i == len(SCALES) - 1 else 0)
    return cell


def _flow(cells, max_w, title, font):
    rows, row, rw = [], [], 0
    for c in cells:
        if row and rw + c.width + 8 > max_w:
            rows.append(row)
            row, rw = [], 0
        row.append(c)
        rw += c.width + 8
    if row:
        rows.append(row)
    total_h = 20 + sum(max(c.height for c in r) + 8 for r in rows)
    sheet = Image.new("RGBA", (max_w, total_h), BG)
    d = ImageDraw.Draw(sheet)
    d.text((4, 4), title, fill=(255, 210, 120, 255), font=font)
    y = 20
    for r in rows:
        x = 4
        for c in r:
            sheet.paste(c, (x, y))
            x += c.width + 8
        y += max(c.height for c in r) + 8
    return sheet


def contact_sheets(spr, font):
    bgs = [_bg_tile(spr, "grass"), _bg_tile(spr, "paving")]
    fams = {}
    for name in sorted(spr):
        fams.setdefault(name.split("/")[0], []).append(name)
    os.makedirs(os.path.join(BUILD, "contact"), exist_ok=True)
    sheets = []
    max_w = 2400
    for fam in FAMILY_ORDER + sorted(set(fams) - set(FAMILY_ORDER)):
        if fam not in fams:
            continue
        cells = [_render_cell(spr[n], n, bgs, font) for n in fams[fam]]
        sh = _flow(cells, max_w, "%s  (%d sprites)" % (fam, len(cells)), font)
        sh.save(os.path.join(BUILD, "contact", fam + ".png"))
        sheets.append(sh)
    H = sum(s.height for s in sheets)
    full = Image.new("RGBA", (max_w, H), BG)
    y = 0
    for s in sheets:
        full.paste(s, (0, y))
        y += s.height
    full.save(os.path.join(BUILD, "contact_sheet.png"))
    return full


# ------------------------------------------------------------ test scene
PRIORITY = ["deep", "shallow", "marsh", "sand", "rock", "grass", "fertile", "forest"]
PRIO = {t: i for i, t in enumerate(PRIORITY)}
NB = {"n": (0, -1), "e": (1, 0), "s": (0, 1), "w": (-1, 0)}
CORNER_NB = {"ne": ("n", "e"), "se": ("s", "e"), "sw": ("s", "w"), "nw": ("n", "w")}
DIRV = {"n": (0, -1), "ne": (1, -1), "e": (1, 0), "se": (1, 1),
        "s": (0, 1), "sw": (-1, 1), "w": (-1, 0), "nw": (-1, -1)}


def scene_map(W, H):
    import math
    t = [["grass"] * W for _ in range(H)]
    elev = [[0] * W for _ in range(H)]
    # forest top-left
    for y in range(H):
        for x in range(W):
            if (x - 1.5) ** 2 / 20 + (y - 1.5) ** 2 / 9 < 1:
                t[y][x] = "forest"
    # fertile fields bottom-left
    for y in range(H):
        for x in range(W):
            if (x - 3.5) ** 2 / 12 + (y - 13) ** 2 / 6 < 1:
                t[y][x] = "fertile"
    # rocky hill top-right (raised)
    for y in range(H):
        for x in range(W):
            if (x - 22) ** 2 / 9 + (y - 1) ** 2 / 10 < 1:
                t[y][x] = "rock"
                elev[y][x] = 1
    # river: meanders north to south
    for y in range(H):
        cx = 15 + int(math.floor(1.5 * math.sin(y / 3.2) + 0.5))
        for x in range(cx - 1, cx + 3):
            t[y][x] = "deep" if x in (cx, cx + 1) else "shallow"
        if y < 6:
            t[y][cx + 3] = "sand"
            if y < 3:
                t[y][cx + 4] = "sand"
        if 9 <= y <= 13:
            t[y][cx - 2] = "marsh"
            if y in (10, 11, 12):
                t[y][cx - 3] = "marsh"
    # a lake bay of shallow water
    for (x, y) in ((19, 13), (20, 13), (19, 14), (20, 14), (21, 14), (19, 15), (20, 15), (21, 15), (22, 15)):
        t[y][x] = "shallow"
    t[14][19] = "deep"
    t[15][20] = "deep"
    # a sand spit
    t[12][19] = "sand"
    t[12][20] = "sand"
    t[13][21] = "sand"
    return t, elev


def scene_roads(W, H, terr):
    roads = {}
    for x in range(0, 13):
        roads[(x, 8)] = "paved"
    for y in range(2, 8):
        roads[(9, y)] = "paved"
    # diagonal dirt track from the south-west fields to the paved road
    for i, (x, y) in enumerate([(2, 15), (3, 14), (4, 13), (5, 12), (6, 11), (7, 10), (8, 9)]):
        roads[(x, y)] = "dirt"
    # dirt lane east of the river ford area
    for x in range(19, 24):
        roads[(x, 9)] = "dirt"
    roads[(18, 10)] = "dirt"
    roads[(19, 10)] = "dirt"
    return roads


def road_links(roads, x, y):
    out = []
    for d, (dx, dy) in DIRV.items():
        q = (x + dx, y + dy)
        if q not in roads:
            continue
        if dx and dy:
            # diagonal only if no orthogonal path already joins them
            if (x + dx, y) in roads or (x, y + dy) in roads:
                continue
        out.append(d)
    return out


def scene(spr, meta, W=24, H=16, k=3):
    terr, elev = scene_map(W, H)
    roads = scene_roads(W, H, terr)
    img = Image.new("RGBA", (W * TILE, H * TILE))

    def at(x, y):
        if 0 <= x < W and 0 <= y < H:
            return terr[y][x]
        return None

    def put(name, x, y):
        im = spr[name]
        img.paste(im, (x, y), im)

    # terrain + blends
    for y in range(H):
        for x in range(W):
            t = terr[y][x]
            put("terrain/%s_%d" % (t, hash2(x, y, 42) % 4), x * TILE, y * TILE)
            for T in PRIORITY[PRIO[t] + 1:]:
                for s, (dx, dy) in NB.items():
                    if at(x + dx, y + dy) == T:
                        put("terrain/edge_%s_%s" % (T, s), x * TILE, y * TILE)
                for c, (a, b) in CORNER_NB.items():
                    dx = NB[a][0] + NB[b][0]
                    dy = NB[a][1] + NB[b][1]
                    if at(x + dx, y + dy) == T and at(x + NB[a][0], y + NB[a][1]) != T \
                            and at(x + NB[b][0], y + NB[b][1]) != T:
                        put("terrain/corner_%s_%s" % (T, c), x * TILE, y * TILE)
            if t in ("shallow", "deep"):
                put("terrain/water_shimmer_%d" % (hash2(x, y, 5) % 4), x * TILE, y * TILE)
    # cliffs
    for y in range(H):
        for x in range(W):
            e = elev[y][x]
            for s in ("s", "e", "w"):
                dx, dy = NB[s]
                if 0 <= x + dx < W and 0 <= y + dy < H and elev[y + dy][x + dx] < e:
                    put("terrain/cliff_%s" % s, x * TILE, y * TILE)
    # resources
    for (x, y, r) in ((21, 1, "marble"), (22, 3, "stone"), (16, 2, "fish"), (19, 0, "clay"),
                      (12, 13, "iron"), (23, 0, "silver"), (13, 4, "limestone"), (11, 14, "pozzolana")):
        put("obj/res_%s" % r, x * TILE, y * TILE)
    # roads
    for (x, y), R in sorted(roads.items()):
        put("road/%s_c" % R, x * TILE, y * TILE)
        for d in road_links(roads, x, y):
            put("road/%s_%s" % (R, d), x * TILE, y * TILE)
    # diagonal fill wedges on the tiles beside diagonal joins
    for y in range(H):
        for x in range(W):
            for c, (a, b) in CORNER_NB.items():
                pa = (x + NB[a][0], y + NB[a][1])
                pb = (x + NB[b][0], y + NB[b][1])
                if pa in roads and pb in roads:
                    dxy = (pb[0] - pa[0], pb[1] - pa[1])
                    dname = [d for d, v in DIRV.items() if v == dxy][0]
                    if dname in road_links(roads, *pa):
                        put("road/%s_fill_%s" % (roads[pa], c), x * TILE, y * TILE)

    # objects / buildings / people, depth-sorted by anchor y
    drawables = []

    def add(name, px_, py_):
        m = meta.get(name, {"ax": 0, "ay": 0})
        drawables.append((py_, px_, name, px_ - m["ax"], py_ - m["ay"]))

    trees_forest = [0, 1, 3, 4, 0, 3]
    for y in range(H):
        for x in range(W):
            t = terr[y][x]
            if (x, y) in roads:
                continue
            h = hash2(x, y, 99)
            if t == "forest":
                n = 1 + h % 2
                for i in range(n):
                    hh = hash2(x, y, 100 + i)
                    add("obj/tree_%d" % trees_forest[hh % 6], x * TILE + 3 + hh % 10,
                        y * TILE + 6 + (hh >> 8) % 9)
            elif t == "marsh" and h % 2 == 0:
                add("obj/reeds_%d" % (h % 3), x * TILE + 4 + (h >> 4) % 8, y * TILE + 10)
            elif t == "rock" and h % 3 == 0:
                add("obj/boulder_%d" % (h % 3), x * TILE + 4 + (h >> 4) % 8, y * TILE + 9)
            elif t == "grass" and h % 13 == 0:
                add("obj/tree_%d" % (2 if h % 2 else 5), x * TILE + 8, y * TILE + 12)
            elif t == "grass" and h % 11 == 1:
                add("obj/bush_%d" % (h % 3), x * TILE + 6 + (h >> 4) % 5, y * TILE + 11)
            elif t == "sand" and h % 5 == 0:
                add("obj/bush_2", x * TILE + 8, y * TILE + 12)

    # buildings: (tile x, tile y of top-left of footprint, w, h)
    blds = [(10, 5, 2, 2), (12, 6, 1, 1), (5, 6, 3, 2), (1, 9, 1, 2), (3, 9, 2, 1),
            (10, 9, 3, 3), (12, 1, 2, 3), (0, 12, 2, 2), (10, 2, 1, 2)]
    taken = set()
    for (bx, by, bw, bh) in blds:
        for yy in range(by, by + bh):
            for xx in range(bx, bx + bw):
                taken.add((xx, yy))
        add("bld/placeholder_%dx%d" % (bw, bh), bx * TILE, (by + bh) * TILE - 1)
    drawables = [d for d in drawables if not (d[2].startswith("obj/") and
                                              (d[1] // TILE, d[0] // TILE) in taken)]

    people = [("patrician", "e", 1, 2, 8), ("soldier", "w", 2, 6, 8), ("pleb", "s", 0, 9, 4),
              ("slave", "n", 3, 9, 6), ("liberti", "e", 2, 11, 8), ("eques", "w", 0, 4, 8),
              ("pleb", "e", 3, 20, 9), ("soldier", "e", 0, 22, 9), ("slave", "e", 1, 5, 12)]
    for (k_, d, f, tx, ty) in people:
        add("people/%s_%s_%d" % (k_, d, f), tx * TILE + 8, ty * TILE + 11)
    add("people/child_e_1", 2 * TILE + 13, 8 * TILE + 13)
    add("people/patrician_idle_s", 9 * TILE + 5, 7 * TILE + 13)
    add("people/eques_idle_s", 9 * TILE + 11, 7 * TILE + 13)

    for (_, _, name, x, y) in sorted(drawables):
        put(name, x, y)
    big = img.resize((img.width * k, img.height * k), Image.NEAREST)
    big.save(os.path.join(BUILD, "scene_test.png"))
    return img


def main():
    os.makedirs(BUILD, exist_ok=True)
    spr, meta = load_all()
    try:
        font = ImageFont.load_default(size=10)
    except TypeError:
        font = ImageFont.load_default()
    full = contact_sheets(spr, font)
    scene(spr, meta)
    print("wrote build/contact_sheet.png (%dx%d), build/contact/*.png, build/scene_test.png"
          % full.size)


if __name__ == "__main__":
    main()
