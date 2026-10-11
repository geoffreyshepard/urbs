"""Build-menu category icons: 24x24, one bold symbol each, 1px dark outline, light from top-left.

  ui/cat_<id>   roads housing farms food industry storage water health
                religion culture civic military monument

Each symbol has its own dominant colour (road basalt, roof terracotta, wheat gold,
water blue, snake green, banner red, ...) so the rail reads by shape and colour
before any text. Symbols are drawn from code (no painted PNGs).
"""
import math

from palette import C
from primitives import new, px, get, outline, ellipse_mask, line, rect

N = 24
OUT = C.outline
G, M, W = C.gray, C.marble, C.white
TV, TR, OC, EA = C.travertine, C.roof_terracotta, C.ochre, C.earth
GR, OL, SEA, RD, GOLD = C.grass, C.olive, C.sea, C.roman_red, C.gold

CATEGORIES = ["roads", "housing", "farms", "food", "industry", "storage", "water", "health",
              "religion", "culture", "civic", "military", "monument"]


def _shade_rect(img, x0, y0, x1, y1, ramp, edge_hi=True):
    """Filled box, ramp dark -> light: lit top/left edge, shaded bottom/right edge."""
    lo, mid, hi = ramp[0], ramp[len(ramp) // 2], ramp[-1]
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            c = mid
            if edge_hi and (x == x0 or y == y0):
                c = hi
            if x == x1 or y == y1:
                c = lo
            px(img, x, y, c)


def _disc(img, cx, cy, r, ramp, ry=None, spec=None):
    ry = ry or r
    for (x, y) in ellipse_mask(cx, cy, r, ry):
        nx, ny = (x + 0.5 - cx) / r, (y + 0.5 - cy) / ry
        b = 0.55 - 0.42 * nx - 0.42 * ny
        px(img, x, y, ramp[max(0, min(len(ramp) - 1, int(b * len(ramp))))])
    if spec:
        px(img, int(cx - r * 0.45), int(cy - ry * 0.45), spec)


def _thick(img, x0, y0, x1, y1, c, w=2):
    for k in range(w):
        line(img, x0 + k, y0, x1 + k, y1, c)


def _finish(img):
    outline(img, OUT)
    return img


# ---------------------------------------------------------------- symbols
def cat_roads():
    """A paved road running corner to corner between grass verges."""
    import roads
    tex = roads.TEX["paved"]
    img = new(N, N)
    for y in range(1, N - 1):
        for x in range(1, N - 1):
            d = (x - y) / math.sqrt(2)          # signed distance from the NW-SE diagonal
            if abs(d) <= 4.6:
                c = tex[y % 16][x % 16]
            elif abs(d) <= 5.8:
                c = G[3] if d < 0 else G[1]     # kerb stones, lit on the upper side
            else:
                c = GR[2] if (x * 7 + y * 3) % 5 else GR[3]
                if (x * 5 + y * 11) % 13 == 0:
                    c = GR[1]
            px(img, x, y, c)
    return _finish(img)


def cat_housing():
    """A Roman house: terracotta gable roof over whitewashed walls, door and shuttered windows."""
    img = new(N, N)
    # Walls.
    _shade_rect(img, 3, 11, 20, 21, (M[0], M[1], W))
    # Roof: a gable from x=1..22, apex at y=2.
    for y in range(2, 11):
        half = int((y - 2) * 11 / 8) + 1
        for x in range(11 - half, 13 + half):
            if not (1 <= x <= 22):
                continue
            c = TR[2] if (y % 2) else TR[1]
            if x <= 11 - half + 1:
                c = TR[3]
            elif x >= 12 + half - 1:
                c = TR[0]
            px(img, x, y, c)
    for x in range(1, 23):
        px(img, x, 10, TR[0])                   # eave shadow line
    # Door with an arched head.
    rect(img, 10, 15, 4, 6, EA[0])
    px(img, 10, 15, M[1]); px(img, 13, 15, M[1])
    rect(img, 11, 16, 2, 5, EA[1])
    # Windows with shutters.
    for wx in (5, 16):
        rect(img, wx, 13, 3, 3, OUT)
        px(img, wx - 1, 13, EA[2]); px(img, wx - 1, 14, EA[2]); px(img, wx - 1, 15, EA[2])
        px(img, wx + 3, 13, EA[1]); px(img, wx + 3, 14, EA[1]); px(img, wx + 3, 15, EA[1])
        for k in range(3):
            px(img, wx + k, 16, M[0])
    for x in range(3, 21):
        px(img, x, 21, TV[0])                   # plinth
    return _finish(img)


def cat_farms():
    """A wheat sheaf: plump golden ears fanned out on top, a red cord, stalks splayed below."""
    img = new(N, N)
    cx, cy = 11.5, 14.0
    # Stalks below the cord.
    for k, a in enumerate((-24, -14, -5, 5, 14, 24)):
        r = math.radians(a)
        line(img, int(cx + 2 * math.sin(r)), int(cy + 1), int(cx + 9 * math.sin(r)), 21, OC[1] if k % 2 else OC[0])
    # Stems and ears, fanned from -55 to +55 degrees.
    for k, a in enumerate((-55, -33, -11, 11, 33, 55)):
        r = math.radians(a)
        dx, dy = math.sin(r), -math.cos(r)
        line(img, int(cx), int(cy - 1), int(cx + 5 * dx), int(cy - 1 + 5 * dy), OC[1])
        for t in range(5, 12):                    # the ear: grains either side of the axis
            x, y = cx + t * dx, cy - 1 + t * dy
            px(img, int(x), int(y), OC[3])
            if t < 11:
                px(img, int(x - dy * 1.2), int(y + dx * 1.2), GOLD if a < 0 else OC[2])
                px(img, int(x + dy * 1.2), int(y - dx * 1.2), OC[2] if a < 0 else OC[1])
    # Cord.
    for x in range(int(cx) - 3, int(cx) + 4):
        px(img, x, int(cy), RD[2])
        px(img, x, int(cy) + 1, RD[0])
    return _finish(img)


def cat_food():
    """A round Roman loaf (panis quadratus): a dark-baked crust scored into eight wedges."""
    img = new(N, N)
    cx, cy, r = 11.5, 12.5, 10
    ramp = (EA[0], EA[1], EA[2], OC[0], OC[1], OC[2])
    _disc(img, cx, cy, r, ramp, ry=9)
    for k in range(8):
        a = math.pi * k / 4 + math.pi / 8
        for t in range(3, 9):
            x, y = cx + t * math.cos(a), cy + t * 0.9 * math.sin(a)
            px(img, int(x), int(y), EA[0])
    for (x, y) in ellipse_mask(cx, cy, 2.6, 2.4):
        px(img, x, y, OC[1])
    for a in range(0, 360, 20):
        px(img, int(cx + 2.8 * math.cos(math.radians(a))), int(cy + 2.6 * math.sin(math.radians(a))), EA[0])
    px(img, 6, 7, OC[3]); px(img, 7, 6, OC[3]); px(img, 8, 5, OC[2])
    return _finish(img)


def cat_industry():
    """An iron anvil, a hammer raised over it, and sparks."""
    img = new(N, N)
    # Anvil face with the horn to the left.
    for x in range(3, 21):
        px(img, x, 12, G[3])
    for x in range(1, 21):
        if x < 5:
            if x >= 1 + (x < 3):
                px(img, x, 13, G[2])
        else:
            px(img, x, 13, G[2])
    _shade_rect(img, 5, 13, 20, 15, (G[0], G[1], G[2]))
    px(img, 2, 13, G[2]); px(img, 3, 13, G[2]); px(img, 4, 13, G[2]); px(img, 3, 14, G[1]); px(img, 4, 14, G[1])
    for x in range(5, 21):
        px(img, x, 12, G[3])
    # Waist and foot.
    _shade_rect(img, 9, 16, 16, 18, (G[0], G[1], G[2]))
    _shade_rect(img, 6, 19, 19, 21, (G[0], G[1], G[2]))
    # Hammer: handle down-left, head up-right.
    _thick(img, 7, 9, 14, 2, EA[2], 1)
    line(img, 8, 9, 15, 2, EA[1])
    _shade_rect(img, 13, 1, 18, 4, (G[0], G[1], G[3]))
    # Sparks.
    for (x, y) in ((4, 8), (3, 10), (6, 7), (19, 9), (21, 7), (20, 10)):
        px(img, x, y, GOLD)
    return _finish(img)


def cat_storage():
    """Goods for storage and trade: a terracotta amphora beside a wooden crate."""
    img = new(N, N)
    # Crate.
    _shade_rect(img, 10, 10, 21, 21, (EA[0], EA[1], EA[2], EA[3]))
    for y in (13, 17):
        for x in range(11, 21):
            px(img, x, y, EA[1])
    line(img, 11, 20, 20, 11, EA[3])
    line(img, 11, 19, 19, 11, EA[0])
    for (x, y) in ((11, 11), (20, 11), (11, 20), (20, 20)):
        px(img, x, y, G[2])
    # Amphora: rim, neck, looped handles, broad shoulders, body tapering to a toe.
    cx = 6
    widths = {5: 3, 6: 4, 7: 4, 8: 4, 9: 4, 10: 4, 11: 4, 12: 3, 13: 3, 14: 3, 15: 2, 16: 2, 17: 1, 18: 1, 19: 1, 20: 0, 21: 0}
    for y, h in widths.items():
        for x in range(cx - h, cx + h + 1):
            c = TR[2]
            if x == cx - h or (x == cx - h + 1 and h > 2):
                c = TR[3]
            if x == cx + h and h > 0:
                c = TR[0]
            px(img, x, y, c)
    for y in (2, 3, 4):                          # neck
        px(img, cx - 1, y, TR[3]); px(img, cx, y, TR[2]); px(img, cx + 1, y, TR[1])
    for x in range(cx - 2, cx + 3):              # rim
        px(img, x, 1, TR[2] if x < cx + 2 else TR[0])
    for (x, y) in ((cx - 2, 2), (cx - 3, 2), (cx - 4, 3), (cx - 4, 4), (cx - 4, 5)):
        px(img, x, y, TR[1])                     # left handle
    for (x, y) in ((cx + 2, 2), (cx + 3, 2), (cx + 4, 3), (cx + 4, 4), (cx + 4, 5)):
        px(img, x, y, TR[0])                     # right handle
    for x in range(cx - 4, cx + 5):              # painted band
        if get(img, x, 9)[3]:
            px(img, x, 9, OUT)
    return _finish(img)


def cat_water():
    """A large water drop with a white highlight, and a ripple beneath."""
    img = new(N, N)
    cx, cy, r = 11.5, 13.0, 7.0
    ramp = (SEA[0], SEA[1], SEA[2], SEA[3])
    for y in range(1, 22):
        for x in range(1, 23):
            fx, fy = x + 0.5, y + 0.5
            inside = (fx - cx) ** 2 + (fy - cy) ** 2 <= r * r
            if fy < cy:                          # the point: width shrinks linearly to the tip
                half = r * (fy - 1.5) / (cy - 1.5)
                inside = inside or abs(fx - cx) <= half * 0.95
            if not inside:
                continue
            nx, ny = (fx - cx) / r, (fy - cy) / r
            b = 0.6 - 0.45 * nx - 0.35 * ny
            px(img, x, y, ramp[max(0, min(3, int(b * 4)))])
    # Highlight.
    for (x, y) in ((8, 10), (8, 11), (8, 12), (9, 9), (9, 13)):
        px(img, x, y, SEA[4])
    px(img, 8, 9, W)
    return _finish(img)


def cat_health():
    """The rod of Asclepius: a wooden staff with a green serpent coiled around it."""
    img = new(N, N)
    for y in range(1, 23):
        px(img, 11, y, EA[2])
        px(img, 12, y, EA[1])
    # Serpent body: one and a half S-turns from the foot to the head, a 2px brush.
    body = new(N, N)
    pts = []
    steps = 200
    for t in range(steps + 1):
        f = t / steps
        y = 20.5 - f * 14.0
        x = 11.5 + 5.0 * math.sin(f * math.pi * 3.0)
        pts.append((x, y))
        for (bx, by) in ((0, 0), (1, 0), (0, 1), (1, 1)):
            px(body, int(x - 0.5) + bx, int(y - 0.5) + by, GR[1])
    # Shade: lit where the pixel above is empty, dark where the pixel below is empty.
    shaded = body.copy()
    for y in range(N):
        for x in range(N):
            if get(body, x, y)[3]:
                if not get(body, x, y - 1)[3]:
                    px(shaded, x, y, GR[3])
                elif not get(body, x, y + 1)[3]:
                    px(shaded, x, y, GR[0])
    # The staff passes in front of the coil on alternate crossings.
    img.alpha_composite(shaded)
    for y in range(1, 23):
        if 9 <= y <= 13:
            px(img, 11, y, EA[2]); px(img, 12, y, EA[1])
    # Head above the last turn, facing right, with an eye and a forked tongue.
    hx, hy = int(pts[-1][0]), int(pts[-1][1])
    for (x, y) in ((hx, hy - 1), (hx + 1, hy - 1), (hx + 2, hy - 1), (hx, hy - 2), (hx + 1, hy - 2), (hx + 2, hy - 2), (hx + 3, hy - 1)):
        px(img, x, y, GR[2])
    px(img, hx + 1, hy - 2, OUT)
    px(img, hx + 4, hy - 1, RD[2])
    return _finish(img)


def cat_religion():
    """A temple front: gilded pediment, four marble columns on three steps."""
    img = new(N, N)
    # Pediment.
    for y in range(2, 8):
        half = int((y - 2) * 10 / 5) + 1
        for x in range(11 - half, 13 + half):
            if 1 <= x <= 22:
                px(img, x, y, W if x < 12 else M[1])
    for x in range(1, 23):
        px(img, x, 8, M[0])
    px(img, 11, 1, GOLD); px(img, 12, 1, GOLD)
    for x in (9, 10, 11, 12, 13, 14):
        px(img, x, 5, OC[2])                     # relief in the tympanum
    # Architrave.
    for x in range(2, 22):
        px(img, x, 9, TV[2]); px(img, x, 10, TV[1])
    # Columns.
    for cx in (3, 8, 13, 18):
        for y in range(11, 18):
            px(img, cx, y, W); px(img, cx + 1, y, M[1]); px(img, cx + 2, y, M[0])
    # Steps.
    for k, y in enumerate((18, 19, 20, 21)):
        for x in range(1 + (3 - k) // 2 if k < 2 else 1, 23 - ((3 - k) // 2 if k < 2 else 1)):
            px(img, x, y, (W, M[1], M[1], M[0])[k])
    return _finish(img)


def cat_culture():
    """A golden lyre: two curving arms, a crossbar, white strings over a wooden sound box."""
    img = new(N, N)
    # Sound box.
    _disc(img, 11.5, 18, 6.5, (EA[0], EA[1], EA[2], EA[3]), ry=3.5)
    # Arms: curve out and back in, like horns.
    for t in range(0, 40):
        f = t / 39
        y = 17 - f * 15
        off = 4.5 + 3.2 * math.sin(f * math.pi * 0.9)
        for sgn, cols in ((-1, (GOLD, OC[3])), (1, (OC[2], OC[1]))):
            x = 11.5 + sgn * off
            px(img, int(x), int(y), cols[0])
            px(img, int(x) + 1, int(y), cols[1])
    # Crossbar.
    for x in range(6, 18):
        px(img, x, 4, OC[3]); px(img, x, 5, OC[1])
    # Strings.
    for sx in (9, 11, 13, 15):
        for y in range(6, 17):
            px(img, sx, y, W)
    for x in range(8, 16):
        px(img, x, 17, OC[1])                    # bridge
    return _finish(img)


_GLYPH = {  # 3x5
    "S": ["###", "#..", "###", "..#", "###"],
    "P": ["##.", "#.#", "##.", "#..", "#.."],
    "Q": ["###", "#.#", "#.#", "#.#", "###", "..#"],
    "R": ["##.", "#.#", "##.", "#.#", "#.#"],
}


def cat_civic():
    """A vexillum: a red standard lettered SPQR with a gold fringe, hung from a crossbar on a pole."""
    img = new(N, N)
    # Pole and finial.
    for y in range(2, 23):
        px(img, 11, y, EA[2]); px(img, 12, y, EA[1])
    px(img, 11, 0, GOLD); px(img, 12, 0, GOLD); px(img, 11, 1, GOLD); px(img, 12, 1, OC[2])
    # Crossbar.
    for x in range(2, 22):
        px(img, x, 3, OC[3] if x < 12 else OC[2])
    # Banner.
    _shade_rect(img, 3, 4, 20, 15, (RD[0], RD[1], RD[2]), edge_hi=False)
    for x in range(3, 21):
        px(img, x, 16, GOLD if x % 2 else OC[1])  # fringe
        if x % 2:
            px(img, x, 17, OC[1])
    # SPQR.
    x0 = 4
    for ch in "SPQR":
        for j, row in enumerate(_GLYPH[ch]):
            for i, c in enumerate(row):
                if c == "#":
                    px(img, x0 + i, 7 + j, GOLD)
        x0 += 4
    return _finish(img)


def cat_military():
    """A red legionary scutum with a gold boss and bands, a gladius crossed behind it."""
    img = new(N, N)
    # Gladius, diagonal behind the shield (hilt top-right, point bottom-left).
    line(img, 3, 21, 19, 5, G[3])
    line(img, 4, 21, 20, 5, G[1])
    for (x, y) in ((18, 3), (19, 4), (20, 5), (21, 6), (22, 7)):
        px(img, x, y, OC[2])                     # guard
    line(img, 21, 3, 22, 2, EA[2])
    px(img, 22, 1, GOLD)
    # Shield: a tall rounded rectangle.
    for y in range(2, 22):
        for x in range(5, 18):
            corner = (y in (2, 21)) and x in (5, 17)
            if corner:
                continue
            c = RD[1]
            if x == 5 or y == 2:
                c = RD[2]
            if x == 17 or y == 21:
                c = RD[0]
            if x in (6, 16) or y in (3, 20):
                c = GOLD if not (x in (6, 16) and y in (3, 20)) else OC[2]
            px(img, x, y, c)
    # Bands and boss.
    for y in range(4, 20):
        px(img, 11, y, OC[2])
    for x in range(7, 16):
        px(img, x, 11, OC[2]); px(img, x, 12, OC[2])
    _disc(img, 11.5, 11.5, 2.6, (G[0], G[1], G[2], G[3]))
    return _finish(img)


def cat_monument():
    """A triumphal arch: one tall central arch, an attic storey and a gilded quadriga on top."""
    img = new(N, N)
    _shade_rect(img, 2, 7, 21, 21, (TV[0], TV[1], TV[2]))
    # Attic with an inscription line.
    _shade_rect(img, 4, 3, 19, 6, (TV[0], TV[1], TV[2]))
    for x in range(6, 18):
        px(img, x, 5, TV[0] if x % 2 else TV[1])
    for x in range(2, 22):
        px(img, x, 7, M[1])                     # cornice
    # Quadriga.
    for (x, y) in ((8, 2), (9, 1), (10, 2), (11, 1), (12, 2), (13, 1), (14, 2), (15, 2), (10, 0), (13, 0)):
        px(img, x, y, GOLD)
    # Central arch: open (transparent) so the silhouette reads.
    cx, top, half = 11.5, 10, 4
    for y in range(top, 22):
        for x in range(2, 22):
            dx = x + 0.5 - cx
            if y >= top + half:
                ok = abs(dx) <= half
            else:
                ok = dx * dx + (y + 0.5 - (top + half)) ** 2 <= half * half
            if ok:
                px(img, x, y, (0, 0, 0, 0))
    # Side niches and engaged columns.
    for nx in (4, 18):
        rect(img, nx, 13, 2, 4, TV[0])
        px(img, nx, 12, TV[1]); px(img, nx + 1, 12, TV[1])
    for colx in (3, 7, 16, 20):
        for y in range(9, 21):
            px(img, colx, y, M[1])
    return _finish(img)


def generate():
    g = globals()
    return [("ui/cat_" + c, g["cat_" + c](), None) for c in CATEGORIES]


if __name__ == "__main__":
    import os
    from PIL import Image
    sheet = Image.new("RGBA", (len(CATEGORIES) * 26 * 8, 26 * 8), (232, 217, 181, 255))
    for k, (name, img, _) in enumerate(generate()):
        sheet.alpha_composite(img.resize((N * 8, N * 8), Image.NEAREST), (k * 26 * 8 + 8, 8))
    out = os.environ.get("OUT", "/tmp/cat_icons.png")
    sheet.save(out)
    print(out)
