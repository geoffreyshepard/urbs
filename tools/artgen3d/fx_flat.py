#!/usr/bin/env python3
"""Flat 2D marks for the iso world (family "fx_flat"), drawn with Pillow, no Blender.

    python3 tools/artgen3d/fx_flat.py

fx/white       16x16 solid white (tinted fills; anchor top-left)
fx/dot         4x4 soft white dot (anchor centre)
fx/tile        128x64 filled white tile diamond (ground-attached: anchor (64, 64))
fx/select_WxH  footprint outline for W, H in 1..4: white corner brackets with a dark edge,
               canvas (W + H) * 64 x (W + H) * 32, anchor (W * 64, canvas height)
fx/plague_mark 36x36 red disc with a white cross (anchor bottom centre)
fx/alert       36x36 gold disc with a dark "!" (anchor bottom centre)
"""
import json
import os

from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "assets", "sprites_iso")
SS = 4  # supersampling


def save(name, img, ax, ay, meta):
    path = os.path.join(OUT, name + ".png")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    meta[name] = {"ax": int(ax), "ay": int(ay)}


def down(img, w, h):
    return img.resize((w, h), Image.LANCZOS)


def diamond_pts(fw, fh, s=1.0, inset=0.0):
    """Footprint corners (top, right, bottom, left) in canvas px, scaled by s, pulled in by inset."""
    W, H = (fw + fh) * 64, (fw + fh) * 32
    pts = [(fh * 64, 0), (W, fw * 32), (fw * 64, H), (0, fh * 32)]
    cx = sum(p[0] for p in pts) / 4
    cy = sum(p[1] for p in pts) / 4
    out = []
    for (x, y) in pts:
        dx, dy = x - cx, y - cy
        ln = max(1e-6, (dx * dx + dy * dy) ** 0.5)
        k = max(0.0, (ln - inset) / ln)
        out.append(((cx + dx * k) * s, (cy + dy * k) * s))
    return out


def select_mark(fw, fh):
    W, H = (fw + fh) * 64, (fw + fh) * 32
    img = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pts = diamond_pts(fw, fh, SS, inset=5)
    arm = 0.22  # bracket arm, as a fraction of the shorter adjoining edge
    for i, p in enumerate(pts):
        for q in (pts[(i + 1) % 4], pts[(i - 1) % 4]):
            e = (q[0] - p[0], q[1] - p[1])
            ln = (e[0] ** 2 + e[1] ** 2) ** 0.5
            k = min(arm, 40 * SS / ln)
            end = (p[0] + e[0] * k, p[1] + e[1] * k)
            d.line([p, end], fill=(20, 16, 12, 200), width=7 * SS)
    for i, p in enumerate(pts):
        for q in (pts[(i + 1) % 4], pts[(i - 1) % 4]):
            e = (q[0] - p[0], q[1] - p[1])
            ln = (e[0] ** 2 + e[1] ** 2) ** 0.5
            k = min(arm, 40 * SS / ln)
            end = (p[0] + e[0] * k, p[1] + e[1] * k)
            d.line([p, end], fill=(255, 255, 255, 255), width=4 * SS)
    return down(img, W, H)


def disc_icon(fill, mark):
    S = 36
    img = Image.new("RGBA", (S * SS, S * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([2 * SS, 2 * SS, (S - 2) * SS, (S - 2) * SS], fill=(30, 20, 14, 230))
    d.ellipse([4 * SS, 4 * SS, (S - 4) * SS, (S - 4) * SS], fill=fill)
    d.ellipse([7 * SS, 6 * SS, (S - 12) * SS, 15 * SS], fill=(255, 255, 255, 60))  # gloss
    c = S * SS / 2
    if mark == "cross":
        a, b = 3.2 * SS, 10 * SS
        d.rectangle([c - a, c - b, c + a, c + b], fill=(255, 255, 255, 255))
        d.rectangle([c - b, c - a, c + b, c + a], fill=(255, 255, 255, 255))
    else:
        d.rounded_rectangle([c - 3 * SS, c - 11 * SS, c + 3 * SS, c + 4 * SS], radius=2 * SS, fill=(40, 26, 12, 255))
        d.ellipse([c - 3.2 * SS, c + 6 * SS, c + 3.2 * SS, c + 12 * SS], fill=(40, 26, 12, 255))
    return down(img, S, S)


def main():
    meta = {}
    save("fx/white", Image.new("RGBA", (16, 16), (255, 255, 255, 255)), 0, 0, meta)
    dot = Image.new("RGBA", (4 * SS, 4 * SS), (0, 0, 0, 0))
    ImageDraw.Draw(dot).ellipse([0, 0, 4 * SS - 1, 4 * SS - 1], fill=(255, 255, 255, 255))
    save("fx/dot", down(dot, 4, 4), 2, 2, meta)
    tile = Image.new("RGBA", (128 * SS, 64 * SS), (0, 0, 0, 0))
    ImageDraw.Draw(tile).polygon(diamond_pts(1, 1, SS), fill=(255, 255, 255, 255))
    save("fx/tile", down(tile, 128, 64), 64, 64, meta)
    for fw in range(1, 5):
        for fh in range(1, 5):
            save("fx/select_%dx%d" % (fw, fh), select_mark(fw, fh), fw * 64, (fw + fh) * 32, meta)
    save("fx/plague_mark", disc_icon((196, 40, 32, 255), "cross"), 18, 36, meta)
    save("fx/alert", disc_icon((232, 180, 60, 255), "bang"), 18, 36, meta)
    path = os.path.join(OUT, "meta", "fx_flat.json")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(meta, f, indent=1, sort_keys=True)
        f.write("\n")
    print("fx_flat: %d sprites" % len(meta))


if __name__ == "__main__":
    main()
