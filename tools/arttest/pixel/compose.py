"""Compose the shared test district (tools/arttest/layout.py) top-down, as the
game draws it: grass + dirt-road tiles from the game atlas, building sprites
anchored bottom-left on their footprint, drawn north to south.

    python3 tools/arttest/pixel/compose.py
      -> tools/arttest/out/current_{1,2,4}x.png  (the game's sprites today)
      -> tools/arttest/out/pixel_{1,2,4}x.png    (option A sprites)
"""
import json
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(HERE, ".."))
import layout  # noqa: E402

T = 16
OUT = os.path.join(HERE, "..", "out")

# The game's closest existing sprite for each test building.
CURRENT = {
    "house_small": "house_plot_t2",   # casa: one storey, tile roof (t1 is a thatch hut)
    "house_large": "house_plot_t3",   # small insula: two storeys
    "bakery": "bakery",
    "blacksmith": "blacksmith",
    "granary": "granary",
    "temple": "temple",
}
DIR8 = ["n", "ne", "e", "se", "s", "sw", "w", "nw"]
D8 = [(0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1)]


class Atlas:
    def __init__(self):
        self.img = Image.open(os.path.join(ROOT, "assets/atlas/main.png")).convert("RGBA")
        self.meta = json.load(open(os.path.join(ROOT, "assets/atlas/main.json")))["sprites"]

    def get(self, name):
        m = self.meta[name]
        return self.img.crop((m["x"], m["y"], m["x"] + m["w"], m["y"] + m["h"]))


def hash2(x, y):
    h = (x * 374761393 + y * 668265263) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return h ^ (h >> 16)


def terrain(atlas):
    W, H = layout.W, layout.H
    img = Image.new("RGBA", (W * T, H * T), (0, 0, 0, 255))
    grass = [atlas.get("terrain/grass_%d" % i) for i in range(4)]
    center = atlas.get("road/dirt_c")
    spokes = [atlas.get("road/dirt_" + d) for d in DIR8]
    for y in range(H):
        for x in range(W):
            img.alpha_composite(grass[hash2(x, y) & 3], (x * T, y * T))
            if not layout.is_road(x, y):
                continue
            img.alpha_composite(center, (x * T, y * T))
            orth = [layout.is_road(x + D8[s][0], y + D8[s][1]) if 0 <= x + D8[s][0] < W and 0 <= y + D8[s][1] < H
                    else False for s in (0, 2, 4, 6)]
            for d in range(8):
                nx, ny = x + D8[d][0], y + D8[d][1]
                if not (0 <= nx < W and 0 <= ny < H) or not layout.is_road(nx, ny):
                    continue
                if d & 1:
                    a, b = d // 2, ((d + 1) // 2) % 4
                    if orth[a] or orth[b]:
                        continue
                img.alpha_composite(spokes[d], (x * T, y * T))
    return img


def district(sprite_for, atlas):
    base = terrain(atlas)
    top = max(s.height - layout.FOOT[k] * T for k, s in ((k, sprite_for(k)) for k in layout.FOOT))
    img = Image.new("RGBA", (base.width, base.height + top), (0, 0, 0, 255))
    img.alpha_composite(base, (0, top))
    order = sorted(layout.BUILDINGS, key=lambda b: (b[2] + layout.FOOT[b[0]], b[1]))
    for kind, x, y in order:
        s = sprite_for(kind)
        n = layout.FOOT[kind]
        assert s.width == n * T, (kind, s.size)
        foot = (y + n) * T + top
        img.alpha_composite(s, (x * T, foot - s.height))
    return img


def save_scaled(img, name):
    os.makedirs(OUT, exist_ok=True)
    for k in (1, 2, 4):
        img.resize((img.width * k, img.height * k), Image.NEAREST).save(
            os.path.join(OUT, "%s_%dx.png" % (name, k)))


def main():
    atlas = Atlas()
    cur = {k: Image.open(os.path.join(ROOT, "assets/sprites/bld/%s.png" % v)).convert("RGBA")
           for k, v in CURRENT.items()}
    new = {k: Image.open(os.path.join(HERE, "sprites/%s.png" % k)).convert("RGBA") for k in layout.FOOT}
    save_scaled(district(cur.__getitem__, atlas), "current")
    save_scaled(district(new.__getitem__, atlas), "pixel")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
