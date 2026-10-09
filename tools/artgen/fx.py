"""Small effect / UI helper sprites: tintable dot and white block, selection
outlines for every footprint size."""
from palette import C
from primitives import new, rect, hline, vline

TILE = 16


def select(w, h):
    W, H = w * TILE, h * TILE
    img = new(W, H)
    c, hi = C.marble[1], C.white
    hline(img, 0, W - 1, 0, c)
    hline(img, 0, W - 1, H - 1, c)
    vline(img, 0, 0, H - 1, c)
    vline(img, W - 1, 0, H - 1, c)
    L = 4 if min(w, h) == 1 else 6
    # emphasized corners: 2px thick L-brackets in pure white
    for (x, y, sx, sy) in ((0, 0, 1, 1), (W - 1, 0, -1, 1), (0, H - 1, 1, -1), (W - 1, H - 1, -1, -1)):
        for i in range(L):
            for t in (0, 1):
                img.putpixel((x + sx * i, y + sy * t), hi)
                img.putpixel((x + sx * t, y + sy * i), hi)
    return img


def generate():
    out = []
    dot = new(2, 2, C.white)
    out.append(("fx/dot", dot, None))
    out.append(("fx/white", new(4, 4, C.white), None))
    for w in range(1, 5):
        for h in range(1, 5):
            out.append(("fx/select_%dx%d" % (w, h), select(w, h), None))
    return out
