#!/usr/bin/env python3
"""Preview of the iso ground layer on a synthetic map, mirroring render/terrain.cpp build_chunk.

    python3 tools/artgen3d/terrain_preview.py [out.png] [--scale 0.5]

Uses assets/sprites_iso + meta/terrain.json.  Draw order emulates the depth slots of build_chunk:
(slot, layer) ascending: dual cell = middle diagonal / 0, road = diagonal + 1 / 1,
quarter = diagonal / 2, cliff = diagonal / 3, so nearer and higher things cover what is behind them.
"""
import json
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SPR = os.path.join(ROOT, "assets", "sprites_iso")
META = json.load(open(os.path.join(SPR, "meta", "terrain.json")))
TERR = ["grass", "fertile", "sand", "rock", "forest", "marsh", "shallow", "deep"]
PRIO = {"deep": 0, "shallow": 1, "marsh": 2, "sand": 3, "rock": 4, "grass": 5, "fertile": 6, "forest": 7}
LEVEL = 16
_cache = {}


def spr(name):
    if name not in _cache:
        _cache[name] = Image.open(os.path.join(SPR, name + ".png")).convert("RGBA")
    return _cache[name]


def make_map(w=24, h=24, seed=3):
    rng = np.random.default_rng(seed)
    t = np.full((h, w), "grass", dtype=object)
    hgt = np.zeros((h, w), dtype=int)
    yy, xx = np.mgrid[0:h, 0:w]
    t[(xx + yy) > w + h - 12] = "shallow"
    t[(xx + yy) > w + h - 7] = "deep"
    t[((xx + yy) > w + h - 14) & ((xx + yy) <= w + h - 12)] = "sand"
    t[(xx < 6) & (yy < 9)] = "forest"
    t[(xx > 14) & (yy < 6)] = "rock"
    t[(xx >= 6) & (xx < 12) & (yy > 12) & (yy < 18)] = "fertile"
    t[(xx < 4) & (yy > 14) & (yy < 20)] = "marsh"
    for _ in range(40):  # ragged edges
        x, y = rng.integers(0, w), rng.integers(0, h)
        t[y, x] = t[min(h - 1, y + 1), x]
    hgt[(xx > 12) & (yy < 9)] = 1
    hgt[(xx > 15) & (yy < 6)] = 2
    hgt[(xx < 7) & (yy < 7)] = 1
    hgt[(xx + 2 * yy > 30) & (xx + 2 * yy < 40)] += 1  # a terrace rising toward the viewer
    road = np.zeros((h, w), dtype=object)
    road[:] = None
    for x in range(2, 20):
        road[10, x] = "dirt"
    for y in range(3, 18):
        road[y, 9] = "paved"
    for k in range(5):
        road[11 + k, 10 + k] = "dirt"
    return t, hgt, road


def anchored(canvas, name, X, Y, ox, oy, scale):
    im = spr(name)
    m = META[name]
    if scale != 1:
        im = im.resize((max(1, round(im.width * scale)), max(1, round(im.height * scale))), Image.LANCZOS)
    canvas.alpha_composite(im, (int(round((X + ox) * scale - m["ax"] * scale)),
                                int(round((Y + oy) * scale - m["ay"] * scale))))


def from_tile(tx, ty, lv=0):
    return (tx - ty) * 64, (tx + ty) * 32 - lv * LEVEL


def main():
    out = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else \
        os.path.join(ROOT, "build", "terrain_preview.png")
    scale = float(sys.argv[sys.argv.index("--scale") + 1]) if "--scale" in sys.argv else 1.0
    t, hgt, road = make_map()
    h, w = t.shape
    items = []  # (slot, layer, seq, sprite, X, Y)

    def T(x, y):
        return t[min(max(y, 0), h - 1), min(max(x, 0), w - 1)]

    def H(x, y):
        return hgt[min(max(y, 0), h - 1), min(max(x, 0), w - 1)]

    seq = 0
    for y in range(h + 1):
        for x in range(w + 1):
            p = (x & 1) + 2 * (y & 1)
            tiles = [(x - 1, y - 1), (x, y - 1), (x - 1, y), (x, y)]
            inb = [0 <= tx < w and 0 <= ty < h for tx, ty in tiles]
            lv = [H(*tt) for tt in tiles]
            if all(inb) and len(set(lv)) == 1:
                c = [T(*tt) for tt in tiles]
                pr = [PRIO[k] for k in c]
                low = min(pr)
                base = c[max(k for k in range(4) if pr[k] == low)]
                X, Y = from_tile(x + 0.5, y + 0.5, lv[0])
                slot = x + y - 1
                items.append((slot, 0, seq, "terrain/%s_%d" % (base, p), X, Y)); seq += 1
                for pp in range(low + 1, 8):
                    mask, which = 0, None
                    for k in range(4):
                        if pr[k] >= pp:
                            mask |= 1 << k
                            if pr[k] == pp:
                                which = c[k]
                    if which:
                        items.append((slot, 0, seq, "terrain/dg_%s_%d_%d" % (which, mask, p), X, Y)); seq += 1
            else:
                for k, (tx, ty) in enumerate(tiles):
                    if not inb[k]:
                        continue
                    X, Y = from_tile(x + 0.5, y + 0.5, lv[k])
                    items.append((tx + ty, 2, seq, "terrain/q_%s_%d_%d" % (T(tx, ty), k, p), X, Y)); seq += 1
    for y in range(h):
        for x in range(w):
            L = hgt[y, x]
            for side, (nx, ny) in (("s", (x, y + 1)), ("e", (x + 1, y))):
                nl = hgt[ny, nx] if 0 <= nx < w and 0 <= ny < h else 0
                for lv in range(nl, L):
                    X, Y = from_tile(x + 1, y + 1, lv)
                    v = (x * 7 + y * 3 + lv) & 1
                    items.append((x + y, 3, seq, "terrain/cliff_%s_%d" % (side, v), X, Y)); seq += 1
            if road[y, x]:
                X, Y = from_tile(x + 1, y + 1, L)
                kind = road[y, x]
                items.append((x + y + 1, 1, seq, "road/%s_c" % kind, X, Y)); seq += 1
                dirs = [("n", 0, -1), ("ne", 1, -1), ("e", 1, 0), ("se", 1, 1), ("s", 0, 1), ("sw", -1, 1),
                        ("w", -1, 0), ("nw", -1, -1)]
                orth = {d: 0 <= x + dx < w and 0 <= y + dy < h and road[y + dy, x + dx] is not None
                        for d, dx, dy in dirs if len(d) == 1}
                for d, dx, dy in dirs:
                    if not (0 <= x + dx < w and 0 <= y + dy < h) or road[y + dy, x + dx] is None:
                        continue
                    if len(d) == 2 and (orth[d[0]] or orth[d[1]]):
                        continue
                    items.append((x + y + 1, 1, seq, "road/%s_%s" % (kind, d), X, Y)); seq += 1
    items.sort(key=lambda it: (it[0], it[1], it[2]))
    W = (w + h) * 64 + 64
    Hh = (w + h) * 32 + 8 * LEVEL + 64
    ox, oy = h * 64 + 32, 8 * LEVEL + 32
    canvas = Image.new("RGBA", (int(W * scale), int(Hh * scale)), (24, 26, 22, 255))
    for _, _, _, name, X, Y in items:
        anchored(canvas, name, X, Y, ox, oy, scale)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    canvas.save(out)
    print(out, canvas.size, len(items), "items")


if __name__ == "__main__":
    main()
