#!/usr/bin/env python3
"""Pack assets/sprites/**/*.png into assets/atlas/main.png + main.json.

- Deterministic: sprites sorted by name, then stably sorted tallest-first
  (ties: wider first, then name), placed with a shelf packer.
- 1 px transparent padding between sprites.
- Atlas is power-of-two in both dimensions, max 4096; the smallest-area
  layout among candidate widths wins (ties: squarer, then narrower).
- main.json: {"width", "height", "sprites": {name: {x, y, w, h, ax, ay}}}
  with ax/ay from assets/sprites/meta.json (default 0, 0).

Run from anywhere:  python3 tools/atlas/pack.py
"""
import json
import os
import sys

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SPRITES = os.path.join(ROOT, "assets", "sprites")
OUT_DIR = os.path.join(ROOT, "assets", "atlas")
PAD = 1
MAX = 4096


def load_sprites():
    items = []
    for dirpath, _, files in os.walk(SPRITES):
        for fn in files:
            if not fn.endswith(".png"):
                continue
            path = os.path.join(dirpath, fn)
            name = os.path.relpath(path, SPRITES)[:-4].replace(os.sep, "/")
            img = Image.open(path).convert("RGBA")
            img.load()
            items.append((name, img))
    items.sort(key=lambda it: it[0])
    return items


def shelf_pack(sizes, width):
    """sizes: list of (name, w, h) already in packing order.
    Returns ({name: (x, y)}, used_height) or None if a sprite is too wide."""
    pos = {}
    x = y = shelf_h = 0
    for name, w, h in sizes:
        if w > width:
            return None
        if x > 0 and x + w > width:
            y += shelf_h + PAD
            x = shelf_h = 0
        pos[name] = (x, y)
        x += w + PAD
        shelf_h = max(shelf_h, h)
    return pos, y + shelf_h


def pow2(n):
    p = 1
    while p < n:
        p *= 2
    return p


def main():
    items = load_sprites()
    if not items:
        sys.exit("no sprites under %s -- run tools/artgen/build_all.py first" % SPRITES)
    meta_path = os.path.join(SPRITES, "meta.json")
    meta = {}
    if os.path.exists(meta_path):
        with open(meta_path) as f:
            meta = json.load(f)

    order = sorted(items, key=lambda it: (-it[1].height, -it[1].width, it[0]))
    sizes = [(n, im.width, im.height) for n, im in order]

    best = None
    w = 64
    while w <= MAX:
        res = shelf_pack(sizes, w)
        if res is not None:
            pos, used_h = res
            h = pow2(used_h)
            if h <= MAX:
                score = (w * h, abs(w - h), w)
                if best is None or score < best[0]:
                    best = (score, w, h, pos)
        w *= 2
    if best is None:
        sys.exit("sprites do not fit in a %dx%d atlas" % (MAX, MAX))
    _, W, H, pos = best

    atlas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sprites = {}
    for name, im in items:
        x, y = pos[name]
        atlas.paste(im, (x, y))
        m = meta.get(name, {})
        sprites[name] = {"x": x, "y": y, "w": im.width, "h": im.height,
                         "ax": int(m.get("ax", 0)), "ay": int(m.get("ay", 0))}

    os.makedirs(OUT_DIR, exist_ok=True)
    atlas.save(os.path.join(OUT_DIR, "main.png"), optimize=False)
    with open(os.path.join(OUT_DIR, "main.json"), "w") as f:
        json.dump({"width": W, "height": H, "sprites": sprites}, f, indent=1, sort_keys=True)
        f.write("\n")
    print("packed %d sprites into %dx%d atlas (%s)" % (len(items), W, H,
                                                     os.path.relpath(OUT_DIR, os.getcwd())))


if __name__ == "__main__":
    main()
