"""Road overlays: dirt and paved.

Every road piece paints the SAME 16-periodic texture through its own mask,
so pieces composite seamlessly in any draw order (no per-piece borders).

  road/<R>_c          center blob (8x8, rounded)
  road/<R>_<d>        spoke from the center to edge/corner d
  road/<R>_fill_<c>   (extra, optional) wedge for corner c of a tile whose two
                      orthogonal neighbours at that corner are joined by a
                      diagonal road.  Without it, diagonal roads pinch to a
                      single pixel at tile corners, because a 45-degree band
                      through a tile corner also crosses the two orthogonal
                      neighbour tiles.
"""
import math

from palette import C
from primitives import rng, new, px, hash2

T = 16
ROADS = ["dirt", "paved"]
DIRS = ["n", "ne", "e", "se", "s", "sw", "w", "nw"]
DVEC = {"n": (0, -1), "ne": (1, -1), "e": (1, 0), "se": (1, 1),
        "s": (0, 1), "sw": (-1, 1), "w": (-1, 0), "nw": (-1, -1)}
HALF = 4.0           # orthogonal half width (8 px road)
DHALF = 4.2          # diagonal half width, perpendicular


# ------------------------------------------------------------ textures
def _paved_texture():
    """16x16 periodic polygonal paving (Roman via): a toroidal Voronoi of
    jittered seeds gives irregular stones; joints sit on cell borders, each
    stone is lit on its top-left rim and shaded on its bottom-right rim."""
    r = rng("road/paved/tex")
    seeds = []
    for gy in range(4):
        for gx in range(3):
            seeds.append(((gx * 16 / 3.0 + (gy % 2) * 2.7 + r.uniform(-1.0, 1.0)) % T,
                          (gy * 4 + 2 + r.uniform(-0.8, 0.8)) % T))
    faces = [C.gray[3], C.gray[3], C.gray[2], C.travertine[1], C.gray[3]]
    face_of = [r.choice(faces) for _ in seeds]

    def cell(x, y):
        best, bi = 1e9, 0
        for i, (sx, sy) in enumerate(seeds):
            dx = min(abs(x + 0.5 - sx), T - abs(x + 0.5 - sx))
            dy = min(abs(y + 0.5 - sy), T - abs(y + 0.5 - sy))
            d = dx * dx + dy * dy * 1.25
            if d < best:
                best, bi = d, i
        return bi

    cid = [[cell(x, y) for x in range(T)] for y in range(T)]
    tex = [[None] * T for _ in range(T)]
    for y in range(T):
        for x in range(T):
            i = cid[y][x]
            right = cid[y][(x + 1) % T]
            down = cid[(y + 1) % T][x]
            left = cid[y][(x - 1) % T]
            up = cid[(y - 1) % T][x]
            f = face_of[i]
            if right != i or down != i:
                c = C.gray[1]                         # joint / shadow rim
            elif left != i or up != i:
                c = C.marble[0] if f != C.gray[2] else C.gray[3]   # lit rim
            else:
                c = f
                if hash2(x, y, 77) % 9 == 0:
                    c = C.gray[2] if f != C.gray[2] else C.gray[1]  # wear
            tex[y][x] = c
    return tex


def _dirt_texture():
    r = rng("road/dirt/tex")
    o = C.ochre
    tex = [[o[1]] * T for _ in range(T)]
    for y in range(T):
        for x in range(T):
            q = r.random()
            if q < 0.12:
                tex[y][x] = o[2]
            elif q < 0.22:
                tex[y][x] = C.earth[3]
            elif q < 0.25:
                tex[y][x] = o[0]
    for _ in range(5):            # pebble: light over dark
        x, y = r.randrange(T), r.randrange(T)
        tex[y][x] = o[3]
        tex[(y + 1) % T][x] = C.earth[2]
    return tex


TEX = {"paved": _paved_texture(), "dirt": _dirt_texture()}


def full_tile(R):
    img = new(T, T)
    for y in range(T):
        for x in range(T):
            px(img, x, y, TEX[R][y][x])
    return img


# ------------------------------------------------------------ masks
def _jag(R, x, y, salt):
    """Edge noise for dirt roads: boundary pixels randomly dropped."""
    if R != "dirt":
        return 0.0
    return ((hash2(x, y, salt) % 100) / 100.0) * 0.9


def center_mask(R):
    m = set()
    for y in range(4, 12):
        for x in range(4, 12):
            corner = (x in (4, 11)) and (y in (4, 11))
            if corner:
                continue
            if R == "dirt" and (x in (4, 11) or y in (4, 11)) and hash2(x, y, 3) % 4 == 0:
                continue
            m.add((x, y))
    return m


def spoke_mask(R, d):
    m = set()
    vx, vy = DVEC[d]
    for y in range(T):
        for x in range(T):
            cx, cy = x + 0.5 - 8, y + 0.5 - 8
            if vx == 0 or vy == 0:
                along = cx * vx + cy * vy
                perp = abs(cx * vy - cy * vx)
                if along < 0:
                    continue
                lim = HALF - _jag(R, x, y, 5) * (1 if perp > HALF - 1 else 0)
                if perp < lim:
                    m.add((x, y))
            else:
                n = math.sqrt(2)
                along = (cx * vx + cy * vy) / n
                perp = abs(cx * vy - cy * vx) / n
                if along < 0:
                    continue
                lim = DHALF - _jag(R, x, y, 9) * (1 if perp > DHALF - 1 else 0)
                if perp < lim:
                    m.add((x, y))
    return m


def fill_mask(R, c):
    """Wedge in corner c of the band joining the two orthogonal neighbours."""
    m = set()
    # the band runs through the tile corner point, perpendicular to the
    # corner's diagonal (e.g. for 'nw': between N and W neighbours, the line
    # x + y = 0 in tile coords)
    for y in range(T):
        for x in range(T):
            dx = (T - (x + 0.5)) if "e" in c else (x + 0.5)
            dy = (y + 0.5) if "n" in c else (T - (y + 0.5))
            perp = (dx + dy) / math.sqrt(2)
            lim = DHALF - _jag(R, x, y, 9) * (1 if perp > DHALF - 1 else 0)
            if perp < lim:
                m.add((x, y))
    return m


def _paint(R, mask):
    img = new(T, T)
    for (x, y) in mask:
        px(img, x, y, TEX[R][y][x])
    return img


def _ruts(img, R, d, mask):
    """Dirt cart ruts along the spoke direction (outside the center)."""
    if R != "dirt":
        return
    vx, vy = DVEC[d]
    for (x, y) in mask:
        cx, cy = x + 0.5 - 8, y + 0.5 - 8
        if vx == 0 or vy == 0:
            perp = cx * vy - cy * vx
            if abs(abs(perp) - 2.0) < 0.6 and hash2(x, y, 21) % 5:
                px(img, x, y, C.ochre[0])
        else:
            perp = (cx * vy - cy * vx) / math.sqrt(2)
            if abs(abs(perp) - 1.6) < 0.45 and hash2(x, y, 21) % 5:
                px(img, x, y, C.ochre[0])


def center(R):
    return _paint(R, center_mask(R))


def spoke(R, d):
    m = spoke_mask(R, d)
    img = _paint(R, m)
    _ruts(img, R, d, m)
    return img


def fill(R, c):
    return _paint(R, fill_mask(R, c))


def generate():
    out = []
    for R in ROADS:
        out.append(("road/%s_c" % R, center(R), None))
        for d in DIRS:
            out.append(("road/%s_%s" % (R, d), spoke(R, d), None))
        for c in ("ne", "se", "sw", "nw"):
            out.append(("road/%s_fill_%s" % (R, c), fill(R, c), None))
    return out
