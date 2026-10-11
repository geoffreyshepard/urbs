"""Compose the shared test district (layout.py) from isometric sprites.

    python3 tools/arttest/iso_compose.py <sprite_dir> <out_prefix> [terrain_dir]

Sprite conventions (near scale): a tile diamond is 128x64 px; an NxN building
sprite is N*128 px wide with the lot's bottom corner on the bottom-centre pixel.
Terrain (from terrain_dir, default sprite_dir): tile_grass[_i_j].png and tile_road[_i_j].png (128x64).  When the 4x4
variants exist, tile (x, y) uses variant (x % 4, y % 4) so the noise stitches.

Writes <out_prefix>_near.png (128 px tiles), _2x.png (64 px, default zoom) and
_1x.png (32 px, far zoom).  Missing building sprites are skipped with a warning.
Optional variants <kind>@1.png, @2 ... are picked per building by position.
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import layout  # noqa: E402

TW, TH = 128, 64


def load(d, name):
    p = os.path.join(d, name + ".png")
    return Image.open(p).convert("RGBA") if os.path.exists(p) else None


def tile(d, kind, x, y, cache):
    key = (kind, x % 4, y % 4)
    if key not in cache:
        cache[key] = load(d, "tile_%s_%d_%d" % key) or load(d, "tile_" + kind)
    return cache[key]


def variants(d, kind):
    """<kind>.png plus optional <kind>@1.png, <kind>@2.png ... picked per building."""
    out, i = [], 1
    while True:
        im = load(d, "%s@%d" % (kind, i))
        if im is None:
            return out
        out.append(im)
        i += 1


def compose(d, ground=None):
    sprites = {k: load(d, k) for k in layout.FOOT}
    alts = {k: ([sprites[k]] if sprites[k] else []) + variants(d, k) for k in layout.FOOT}
    for k, im in sprites.items():
        if im is None:
            print("warning: no sprite for %s in %s, skipped" % (k, d))
        elif im.width != layout.FOOT[k] * TW:
            print("warning: %s is %d px wide, expected %d" % (k, im.width, layout.FOOT[k] * TW))
    tdir = ground or d
    top = max([im.height - layout.FOOT[k] * TH for k, im in sprites.items() if im] + [0]) + 8
    W, H = layout.W, layout.H
    cw, ch = (W + H) * TW // 2, (W + H) * TH // 2 + top

    def scr(lx, ly):  # screen position of layout point (lx, ly)
        return (lx - ly) * TW // 2 + H * TW // 2, (lx + ly) * TH // 2 + top

    canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
    under = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
    cache = {}
    for y in range(H):
        for x in range(W):
            t = tile(tdir, "road" if layout.is_road(x, y) else "grass", x, y, cache)
            if t is None:
                continue
            px, py = scr(x, y)
            canvas.alpha_composite(t, (px - TW // 2, py))
    # fill hairline seams between anti-aliased diamonds with the ground's own colour
    for y in range(H):
        for x in range(W):
            t = tile(tdir, "road" if layout.is_road(x, y) else "grass", x, y, cache)
            if t is None:
                continue
            px, py = scr(x, y)
            big = t.resize((TW + 4, TH + 2), Image.BILINEAR)
            under.alpha_composite(big, (px - TW // 2 - 2, py - 1))
    base = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
    base.alpha_composite(under)
    base.alpha_composite(canvas)
    canvas = base
    order = sorted(layout.BUILDINGS, key=lambda b: (b[1] + b[2] + layout.FOOT[b[0]], b[1]))
    for kind, x0, y0 in order:
        if not alts[kind]:
            continue
        im = alts[kind][(x0 * 7 + y0 * 13 + x0 * y0) % len(alts[kind])]
        n = layout.FOOT[kind]
        bx, by = scr(x0 + n, y0 + n)
        canvas.alpha_composite(im, (bx - im.width // 2, by - im.height))
    # opaque background behind the map edges (outside the diamond)
    bg = Image.new("RGBA", canvas.size, (32, 36, 30, 255))
    bg.alpha_composite(canvas)
    return bg


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    d, prefix = sys.argv[1], sys.argv[2]
    ground = sys.argv[3] if len(sys.argv) > 3 else None
    near = compose(d, ground)
    os.makedirs(os.path.dirname(os.path.abspath(prefix)), exist_ok=True)
    near.convert("RGB").save(prefix + "_near.png")
    for suffix, k in (("_2x", 2), ("_1x", 4)):
        near.resize((near.width // k, near.height // k), Image.LANCZOS).convert("RGB").save(
            prefix + suffix + ".png")
    print("wrote %s_{near,2x,1x}.png (%dx%d near)" % (prefix, near.width, near.height))


if __name__ == "__main__":
    main()
