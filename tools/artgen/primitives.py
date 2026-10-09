"""Pixel-level drawing helpers shared by all URBS generators.

All helpers write exact palette colors with alpha 0 or 255 -- never blend.
Randomness always comes from ``rng(key)``, a ``random.Random`` seeded from a
stable CRC of a string key, so output is byte-identical across runs.
"""
import math
import random
import zlib

from PIL import Image

from palette import C, TRANSPARENT


# ---------------------------------------------------------------- basics
def rng(key):
    """Deterministic RNG for a string key (independent of PYTHONHASHSEED)."""
    return random.Random(zlib.crc32(key.encode("utf-8")))


def hash2(x, y, salt=0):
    """Cheap deterministic integer hash of a coordinate (0..2^32-1)."""
    h = (x * 374761393 + y * 668265263 + salt * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return h ^ (h >> 16)


def new(w, h, fill=TRANSPARENT):
    return Image.new("RGBA", (w, h), fill)


def px(img, x, y, c):
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), c)


def get(img, x, y):
    if 0 <= x < img.width and 0 <= y < img.height:
        return img.getpixel((x, y))
    return TRANSPARENT


def opaque(img, x, y):
    return get(img, x, y)[3] != 0


def rect(img, x, y, w, h, c):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            px(img, xx, yy, c)


def hline(img, x0, x1, y, c):
    for x in range(min(x0, x1), max(x0, x1) + 1):
        px(img, x, y, c)


def vline(img, x, y0, y1, c):
    for y in range(min(y0, y1), max(y0, y1) + 1):
        px(img, x, y, c)


def line(img, x0, y0, x1, y1, c):
    """Bresenham line."""
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while True:
        px(img, x0, y0, c)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def ellipse_mask(cx, cy, rx, ry):
    """Set of pixels whose centers fall inside the ellipse."""
    out = set()
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            dx = (x + 0.5 - cx) / rx
            dy = (y + 0.5 - cy) / ry
            if dx * dx + dy * dy <= 1.0:
                out.add((x, y))
    return out


def fill_mask(img, mask, c):
    for (x, y) in mask:
        px(img, x, y, c)


# ---------------------------------------------------------------- shading
def shaded_box(img, x, y, w, h, ramp, outline=None):
    """Box lit from the top-left: ramp = (shadow, mid, highlight[, ...]).
    Top row + left column get the highlight, bottom row + right column the
    shadow.  Optional 1px outline drawn *outside* the box."""
    sh, mid, hi = ramp[0], ramp[len(ramp) // 2], ramp[-1]
    rect(img, x, y, w, h, mid)
    hline(img, x, x + w - 1, y, hi)
    vline(img, x, y, y + h - 1, hi)
    hline(img, x, x + w - 1, y + h - 1, sh)
    vline(img, x + w - 1, y, y + h - 1, sh)
    if outline is not None:
        hline(img, x - 1, x + w, y - 1, outline)
        hline(img, x - 1, x + w, y + h, outline)
        vline(img, x - 1, y - 1, y + h, outline)
        vline(img, x + w, y - 1, y + h, outline)


def shade_blob(img, mask, ramp, light=(-0.7, -0.7), key="blob", noise=0.18,
               cx=None, cy=None, rx=None, ry=None):
    """Fill a pixel set as a rounded volume lit from `light` (top-left).
    ramp is dark->light; brightness from the pseudo-normal plus seeded noise."""
    if not mask:
        return
    xs = [p[0] for p in mask]
    ys = [p[1] for p in mask]
    if cx is None:
        cx = (min(xs) + max(xs) + 1) / 2.0
        cy = (min(ys) + max(ys) + 1) / 2.0
        rx = max(1.0, (max(xs) - min(xs) + 1) / 2.0)
        ry = max(1.0, (max(ys) - min(ys) + 1) / 2.0)
    r = rng(key)
    ll = math.hypot(*light)
    lx, ly = light[0] / ll, light[1] / ll
    n = len(ramp)
    for (x, y) in sorted(mask):
        nx = (x + 0.5 - cx) / rx
        ny = (y + 0.5 - cy) / ry
        b = -(nx * lx + ny * ly)              # -1 .. 1, positive = lit
        b = 0.5 + 0.5 * b + r.uniform(-noise, noise)
        i = int(b * n)
        px(img, x, y, ramp[max(0, min(n - 1, i))])


# ---------------------------------------------------------------- outline
def outline(img, c=None, diagonal=False, inner_color_for=None):
    """Add a 1px outline outside all opaque pixels (in place).
    4-connected by default; diagonal=True also fills 8-connected corners."""
    c = c or C.outline
    w, h = img.size
    src = img.copy()
    sp = src.load()
    dp = img.load()
    nbrs = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    if diagonal:
        nbrs += [(-1, -1), (1, -1), (-1, 1), (1, 1)]
    for y in range(h):
        for x in range(w):
            if sp[x, y][3] != 0:
                continue
            for dx, dy in nbrs:
                xx, yy = x + dx, y + dy
                if 0 <= xx < w and 0 <= yy < h and sp[xx, yy][3] != 0:
                    dp[x, y] = c
                    break
    return img


def inner_outline(img, c=None):
    """Recolor opaque pixels touching transparency/border (outline inside the
    silhouette; keeps the sprite size)."""
    c = c or C.outline
    w, h = img.size
    src = img.copy()
    sp = src.load()
    dp = img.load()
    for y in range(h):
        for x in range(w):
            if sp[x, y][3] == 0:
                continue
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                xx, yy = x + dx, y + dy
                if not (0 <= xx < w and 0 <= yy < h) or sp[xx, yy][3] == 0:
                    dp[x, y] = c
                    break
    return img


# ---------------------------------------------------------------- texture
def dither(img, x, y, w, h, c, density=0.5, pattern="checker", r=None, mask=None):
    """Dither color c over a rect.  pattern: 'checker' (50%), 'sparse'
    (25% ordered), 'random' (seeded, uses density)."""
    bayer = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            if mask is not None and (xx, yy) not in mask:
                continue
            if pattern == "checker":
                on = (xx + yy) % 2 == 0
            elif pattern == "sparse":
                on = xx % 2 == 0 and yy % 2 == 0
            elif pattern == "bayer":
                on = bayer[yy % 4][xx % 4] < density * 16
            else:
                on = r.random() < density
            if on:
                px(img, xx, yy, c)


def speckle(img, r, colors, density, x=0, y=0, w=None, h=None, only_on=None):
    """Seeded noise speckle: each pixel becomes a random color from `colors`
    with probability `density`.  only_on: restrict to pixels of this color."""
    w = img.width if w is None else w
    h = img.height if h is None else h
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            if r.random() < density:
                if only_on is not None and get(img, xx, yy) != only_on:
                    continue
                px(img, xx, yy, r.choice(colors))


def column(img, x, y, h, ramp=None, capital=True, outline_c=None):
    """3px-wide fluted column, lit from the left, with base and capital."""
    ramp = ramp or C.marble
    sh, mid, hi = ramp[0], ramp[len(ramp) // 2], ramp[-1]
    for yy in range(y, y + h):
        px(img, x, yy, hi)
        px(img, x + 1, yy, mid)
        px(img, x + 2, yy, sh)
    if capital:
        hline(img, x - 1, x + 3, y, hi)
        px(img, x + 3, y, sh)
        hline(img, x - 1, x + 3, y + h - 1, mid)
        px(img, x + 3, y + h - 1, sh)
    if outline_c is not None:
        vline(img, x - 1, y + 1, y + h - 2, outline_c)
        vline(img, x + 3, y + 1, y + h - 2, outline_c)


def roof(img, x, y, w, h, ramp=None, ridge="h", key="roof"):
    """Terracotta tiled roof seen from above with a slight front tilt,
    lit from the top-left.  ramp = 4 shades, dark -> light.

    ridge='h': hipped roof, ridge east-west.  Back slope and west hip lit,
               front slope mid, east hip in shadow.
    ridge='v': gable roof, ridge north-south.  West slope lit, east slope in
               shadow (the caller draws the front pediment).
    Imbrex (cover tile) lines run down the slope; course lines across it."""
    ramp = ramp or C.roof_terracotta
    d0, d1, m, hi = ramp
    r = rng(key)
    if ridge == "h":
        back = max(2, int(round(h * 0.34)))
        ry = y + back
        L = max(2, min(w // 2 - 2, (h * 2) // 3))   # hip run in x
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                lx, rx = xx - x, x + w - 1 - xx
                if yy <= ry:
                    t = (yy - y) / float(max(1, back))      # 0 top .. 1 ridge
                else:
                    t = (y + h - 1 - yy) / float(max(1, h - 1 - back))
                hip = int(round(L * t))
                if lx < hip:
                    c = hi if (yy - y) % 3 != 2 else m          # west hip
                elif rx < hip:
                    c = d1 if (yy - y) % 3 != 2 else d0         # east hip
                elif yy < ry:
                    c = hi if (xx - x) % 3 != 2 else m          # back slope
                elif yy == ry:
                    c = hi                                      # ridge cap
                else:
                    k3 = (xx - x) % 3
                    c = m if k3 != 2 else d1                    # front slope
                    if (yy - ry) % 4 == 0 and k3 == 0:
                        c = hi                                  # imbrex ends
                    elif (yy - ry) % 4 == 0 and k3 == 1 and r.random() < 0.3:
                        c = d1
                # hip seams
                if hip > 0 and lx == hip:
                    c = hi
                if hip > 0 and rx == hip:
                    c = m if yy > ry else hi
                px(img, xx, yy, c)
        hline(img, x, x + w - 1, ry + 1, d1)          # shadow under the ridge
        for xx in range(x, x + w):                   # restore hips on that row
            lx, rx = xx - x, x + w - 1 - xx
            t = (y + h - 1 - (ry + 1)) / float(max(1, h - 1 - back))
            hip = int(round(L * t))
            if lx <= hip:
                px(img, xx, ry + 1, hi if lx < hip else hi)
            elif rx <= hip:
                px(img, xx, ry + 1, d1 if rx < hip else m)
        hline(img, x, x + w - 1, y + h - 1, d0)       # eave
    else:
        mid_x = x + w // 2
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                if xx < mid_x:
                    c = hi if (yy - y) % 3 != 2 else m
                    if (xx - x) % 4 == 3 and (yy - y) % 3 == 1 and r.random() < 0.5:
                        c = m
                elif xx == mid_x:
                    c = hi
                elif xx == mid_x + 1:
                    c = m
                else:
                    c = d1 if (yy - y) % 3 != 2 else d0
                px(img, xx, yy, c)
        hline(img, x, x + w - 1, y + h - 1, d0)
    return img


def scale(img, k):
    return img.resize((img.width * k, img.height * k), Image.NEAREST)
