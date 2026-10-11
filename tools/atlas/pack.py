#!/usr/bin/env python3
"""Pack the game's sprites into assets/atlas/main_<page>.png + main.json (see docs/13-isometric.md).

Sources:
- assets/sprites_iso/**/*.png: isometric world art at native scale (128 x 64 px per tile), with
  anchors from assets/sprites_iso/meta/*.json.
- assets/sprites/ui/** and goods/**: UI icons, packed unchanged (1 texel = 1 legacy px).  An icon
  of the same name in assets/sprites_iso/ui or goods (tools/artgen3d/icons.py: goods and build
  category icons) wins over the legacy one; it is packed unchanged too, never resampled by
  `scale` (the game draws icons at fixed on-screen sizes).
- Any other legacy sprite in assets/sprites/ with no iso version yet is converted so the game
  stays runnable mid-migration: flat tiles (terrain, roads, farm/garden/plaza tiles, selection
  marks) are re-projected onto the iso diamond; everything else is upscaled with nearest
  neighbour (x8 for buildings, x4 for people, objects and effects) and anchored at its bottom
  centre.

World sprites are resampled to `scale` atlas texels per iso px (--scale; "auto" picks the
sharpest of SCALES that fits PAGE_BUDGET pages, never below MIN_AUTO_SCALE).  Pages are
PAGE_W x PAGE_H; the game stacks them as layers of a texture array and encodes the page in the
high bits of a sprite's v coordinate (v = page * PAGE_H + y).

Deterministic: sprites sorted by name, then tallest-first (ties: wider, then name), shelf packed.
Transparent texels take the colour of their nearest opaque neighbour (alpha stays 0) so linear
filtering and mipmaps never pull in black fringes.

main.json: {"page_width", "page_height", "pages", "scale",
            "sprites": {name: {x, y, w, h, ax, ay, page, s}}}
x/y/w/h and ax/ay are in atlas texels; s = texels per iso px for that sprite.

Run from anywhere:  python3 tools/atlas/pack.py [--scale auto|0.5|...]
"""
import argparse
import glob
import json
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ISO = os.path.join(ROOT, "assets", "sprites_iso")
LEGACY = os.path.join(ROOT, "assets", "sprites")
OUT_DIR = os.path.join(ROOT, "assets", "atlas")
PAD = 2
PAGE_W, PAGE_H = 4096, 2048
MAX_PAGES = 65536 // PAGE_H  # page index lives in the high bits of a u16 v
SCALES = (1.0, 0.75, 0.625, 0.5, 0.375, 0.25)
MIN_AUTO_SCALE = 0.5
PAGE_BUDGET = 6  # 6 x 32 MB (+ mips): ~256 MB of GPU memory at most (4-direction buildings, 2026-10-11)
UI_GROUPS = ("ui", "goods")
UI_TEXELS_PER_ISO = 0.25  # a 16 px icon covers half a tile (64 iso px) when drawn in the world
ISO_ICON_TEXELS_PER_ISO = 1.0  # rendered icons: 1 texel = 1 iso px (the game sizes them explicitly)
TILE = 16  # legacy px per tile


def load_png(path):
    im = Image.open(path).convert("RGBA")
    im.load()
    return im


def walk(base):
    out = {}
    for dirpath, _, files in os.walk(base):
        if os.path.basename(dirpath) == "meta":
            continue
        for fn in files:
            if fn.endswith(".png"):
                path = os.path.join(dirpath, fn)
                out[os.path.relpath(path, base)[:-4].replace(os.sep, "/")] = path
    return out


def is_flat_legacy(name):
    group, rest = name.split("/", 1)
    if group in ("terrain", "road"):
        return True
    if group == "bld" and "_tile_" in rest:
        return True
    return group == "fx" and (rest.startswith("select_") or rest in ("foundation", "tile"))


def iso_from_flat(im):
    """Top-down image (16 px per tile) -> the same ground re-projected onto the iso diamond."""
    W, H = im.size
    ow, oh = (W + H) * 4, (W + H) * 2
    # iso(x, y) = (4 (x - y) + 4 H, 2 (x + y)); inverse for the PIL affine (output -> input).
    # x = (X - 4H) / 8 + Y / 4, y = Y / 4 - (X - 4H) / 8
    a, b, c = 1 / 8, 1 / 4, -H / 2
    d, e, f = -1 / 8, 1 / 4, H / 2
    big = im.resize((W * 4, H * 4), Image.NEAREST)  # sample a finer grid so edges stay crisp
    k = 4
    out = big.transform((ow, oh), Image.AFFINE, (a * k, b * k, c * k, d * k, e * k, f * k), resample=Image.BILINEAR)
    return out, (W * 4, oh)


def legacy_world(name, im, meta):
    """Legacy top-down sprite -> (native iso image, anchor)."""
    if is_flat_legacy(name):
        return iso_from_flat(im)
    group = name.split("/", 1)[0]
    k = 8 if group == "bld" else 4
    big = im.resize((im.width * k, im.height * k), Image.NEAREST)
    if group == "bld":
        return big, (big.width // 2, big.height)  # bottom centre on the footprint's bottom corner
    m = meta.get(name)
    if m:
        return big, (int(m.get("ax", 0)) * k + k // 2, (int(m.get("ay", 0)) + 1) * k)
    return big, (big.width // 2, big.height)


def tile_sprite():
    """fx/tile fallback: a white 128 x 64 diamond, anchor at the bottom corner."""
    a = np.zeros((64, 128, 4), np.uint8)
    ys, xs = np.mgrid[0:64, 0:128]
    inside = np.abs(xs + 0.5 - 64) / 64 + np.abs(ys + 0.5 - 32) / 32 <= 1.0
    a[inside] = (255, 255, 255, 255)
    return Image.fromarray(a, "RGBA"), (64, 64)


def gather():
    """{name: (native image, (ax, ay), texels-per-iso-px or None for world sprites)}"""
    iso_meta = {}
    for p in sorted(glob.glob(os.path.join(ISO, "meta", "*.json"))):
        with open(p) as f:
            iso_meta.update(json.load(f))
    legacy_meta = {}
    mp = os.path.join(LEGACY, "meta.json")
    if os.path.exists(mp):
        with open(mp) as f:
            legacy_meta = json.load(f)
    iso = walk(ISO) if os.path.isdir(ISO) else {}
    legacy = walk(LEGACY)
    items = {}
    converted = 0
    for name, path in legacy.items():
        group = name.split("/", 1)[0]
        if group in UI_GROUPS:
            if name in iso:
                continue  # a rendered iso icon replaces the pixel-art one
            m = legacy_meta.get(name, {})
            items[name] = (load_png(path), (int(m.get("ax", 0)), int(m.get("ay", 0))), UI_TEXELS_PER_ISO)
        elif group == "terrain":
            continue  # the iso terrain layer uses its own sprite scheme (see tools/artgen3d/terrain_tex.py)
        elif name not in iso:
            im, anchor = legacy_world(name, load_png(path), legacy_meta)
            items[name] = (im, anchor, None)
            converted += 1
    missing_meta = []
    for name, path in iso.items():
        im = load_png(path)
        m = iso_meta.get(name)
        if m is None:
            missing_meta.append(name)
            m = {"ax": im.width // 2, "ay": im.height}
        icon = name.split("/", 1)[0] in UI_GROUPS
        items[name] = (im, (float(m["ax"]), float(m["ay"])), ISO_ICON_TEXELS_PER_ISO if icon else None)
    if "fx/tile" not in items:
        im, anchor = tile_sprite()
        items["fx/tile"] = (im, anchor, None)
    if missing_meta:
        print("warning: %d iso sprites without meta (anchored bottom centre): %s" %
              (len(missing_meta), ", ".join(sorted(missing_meta)[:8])))
    return items, len(iso), converted


def resample(items, scale):
    """-> list of (name, image at atlas resolution, (ax, ay) texels, s)."""
    out = []
    for name in sorted(items):
        im, (ax, ay), s = items[name]
        if s is None:
            s = scale
            if scale != 1.0:
                w = max(1, int(round(im.width * scale)))
                h = max(1, int(round(im.height * scale)))
                im = im.resize((w, h), Image.LANCZOS if scale < 1.0 else Image.BILINEAR)
            ax, ay = ax * scale, ay * scale
            # Trim fully transparent margins (lit overlays, tall canvases); the anchor moves with the crop.
            box = im.getchannel("A").point(lambda a: 255 if a > 0 else 0).getbbox()
            if box is None:
                box = (0, 0, 1, 1)
            l, t = max(0, box[0] - 1), max(0, box[1] - 1)
            r, b = min(im.width, box[2] + 1), min(im.height, box[3] + 1)
            if (l, t, r, b) != (0, 0, im.width, im.height):
                im = im.crop((l, t, r, b))
                ax, ay = ax - l, ay - t
        out.append((name, im, (int(round(ax)), int(round(ay))), s))
    return out


def shelf_pack(sprites):
    """sprites: [(name, w, h)] in packing order -> ({name: (page, x, y)}, pages)."""
    pos = {}
    page = x = y = shelf_h = 0
    for name, w, h in sprites:
        if w > PAGE_W or h > PAGE_H:
            sys.exit("sprite %s (%dx%d) does not fit a %dx%d page" % (name, w, h, PAGE_W, PAGE_H))
        if x > 0 and x + w > PAGE_W:
            y += shelf_h + PAD
            x = shelf_h = 0
        if y + h > PAGE_H:
            page += 1
            x = y = shelf_h = 0
        pos[name] = (page, x, y)
        x += w + PAD
        shelf_h = max(shelf_h, h)
    return pos, page + 1


def order(sprites):
    return sorted(((n, im.width, im.height) for n, im, _, _ in sprites), key=lambda t: (-t[2], -t[1], t[0]))


def bleed(arr, iterations=6):
    """Spread opaque colours into transparent texels (alpha untouched)."""
    rgb = arr[..., :3].astype(np.float32)
    known = arr[..., 3] > 0
    for _ in range(iterations):
        if known.all():
            break
        acc = np.zeros_like(rgb)
        cnt = np.zeros(known.shape, np.float32)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            k = np.roll(known, (dy, dx), (0, 1))
            acc += np.roll(rgb, (dy, dx), (0, 1)) * k[..., None]
            cnt += k
        grow = (~known) & (cnt > 0)
        rgb[grow] = acc[grow] / cnt[grow][:, None]
        known = known | grow
    arr[..., :3] = np.clip(rgb + 0.5, 0, 255).astype(np.uint8)
    return arr


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--scale", default="auto", help="atlas texels per iso px, or 'auto'")
    args = ap.parse_args()
    items, n_iso, n_conv = gather()
    if args.scale == "auto":
        chosen = None
        for sc in SCALES:
            sprites = resample(items, sc)
            _, pages = shelf_pack(order(sprites))
            if pages <= PAGE_BUDGET or sc <= MIN_AUTO_SCALE:
                chosen = (sc, sprites)
                break
        scale, sprites = chosen
    else:
        scale = float(args.scale)
        sprites = resample(items, scale)
    pos, pages = shelf_pack(order(sprites))
    if pages > MAX_PAGES:
        sys.exit("%d pages exceed the %d the v encoding allows" % (pages, MAX_PAGES))

    os.makedirs(OUT_DIR, exist_ok=True)
    for old in glob.glob(os.path.join(OUT_DIR, "main*.png")):
        os.remove(old)
    canvases = [Image.new("RGBA", (PAGE_W, PAGE_H), (0, 0, 0, 0)) for _ in range(pages)]
    table = {}
    for name, im, (ax, ay), s in sprites:
        page, x, y = pos[name]
        canvases[page].paste(im, (x, y))
        table[name] = {"x": x, "y": y, "w": im.width, "h": im.height, "ax": ax, "ay": ay, "page": page,
                       "s": round(s, 6)}
    for i, c in enumerate(canvases):
        arr = bleed(np.array(c))
        Image.fromarray(arr, "RGBA").save(os.path.join(OUT_DIR, "main_%d.png" % i), optimize=False)
    with open(os.path.join(OUT_DIR, "main.json"), "w") as f:
        json.dump({"page_width": PAGE_W, "page_height": PAGE_H, "pages": pages, "scale": scale, "sprites": table}, f,
                  indent=1, sort_keys=True)
        f.write("\n")
    mb = pages * PAGE_W * PAGE_H * 4 / (1 << 20)
    print("packed %d sprites (%d iso, %d converted legacy) into %d page(s) of %dx%d at scale %g: %.0f MB "
          "(+ mips ~%.0f MB)" % (len(sprites), n_iso, n_conv, pages, PAGE_W, PAGE_H, scale, mb, mb * 4 / 3))


if __name__ == "__main__":
    main()
