"""Housing evolution sprites.

  bld/house_plot          2x2 surveyed empty plot (32 x 48)
  bld/house_plot_t0..t4   tent, hut, casa, small insula, large insula
                          (32 x 48; t4 is taller: 32 x 64)
  bld/domus_plot          3x3 surveyed empty plot (48 x 68)
  bld/domus_plot_t0..t2   domus, grand domus, villa urbana (48 x 68)

Size convention as placeholders: width = w*16, height = h*16 + 8 + 4*min(w,h)
(t4 adds 16 px for its extra storeys).  Anchor bottom-left (0, H-1).

Every sprite is built from two layers: a ground layer (yard earth, lawns,
paths -- no outline, like terrain) and a structure layer that gets the dark
outline, then the structure is pasted over the ground.
"""
from palette import C
from primitives import new, px, rect, hline, vline, rng, outline, roof, ellipse_mask, line, flat, paste_exact
from buildings.placeholder import _wall, _door, _window, _compluvium, extra_height
import objects

TILE = 16


# ------------------------------------------------------------ helpers
def _canvas(w, h, more=0):
    W = w * TILE
    H = h * TILE + extra_height(w, h) + more
    return W, H


def _merge(ground, struct, c=None):
    """Outline the structure layer and paste it over the ground layer."""
    outline(struct, c or C.outline)
    flat(ground)  # yards and lawns cast no shadow
    paste_exact(ground, struct)
    return ground


def _yard(img, x0, y0, x1, y1, key, base=None, specks=None, ragged=True):
    """Packed-earth yard patch (ground layer, no outline)."""
    r = rng(key + "/yard")
    base = base or C.earth[3]
    specks = specks or (C.ochre[1], C.earth[2], C.ochre[2])
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            edge = x in (x0, x1) or y in (y0, y1)
            if ragged and edge and r.random() < 0.45:
                continue
            c = base
            q = r.random()
            if q < 0.12:
                c = specks[0]
            elif q < 0.20:
                c = specks[1]
            elif q < 0.24:
                c = specks[2]
            px(img, x, y, c)


def _lawn(img, x0, y0, x1, y1, key):
    r = rng(key + "/lawn")
    g = C.grass
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            q = r.random()
            c = g[2]
            if q < 0.08:
                c = g[1]
            elif q < 0.16:
                c = g[3]
            px(img, x, y, c)


def thatch_roof(img, x, y, w, h, key):
    """Straw thatch, hipped, lit from the top-left: lit back slope and west
    hip, mid front slope, shaded east hip, ragged hanging eave."""
    r = rng(key + "/thatch")
    o = C.ochre
    back = max(2, h // 3)
    ry = y + back
    L = max(2, min(w // 2 - 2, (h * 2) // 3))
    streak = [r.randrange(4) for _ in range(w)]
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            lx, rx = xx - x, x + w - 1 - xx
            t = (yy - y) / float(max(1, back)) if yy <= ry else \
                (y + h - 1 - yy) / float(max(1, h - 1 - back))
            hip = int(round(L * t))
            k = (yy + streak[xx - x]) % 4
            if lx < hip:
                c = o[3] if k else o[2]
            elif rx < hip:
                c = o[1] if k else o[0]
            elif yy < ry:
                c = o[3] if k else o[2]
            elif yy == ry:
                c = C.earth[2] if (xx % 3) else C.earth[1]          # ridge binding
            else:
                c = o[2] if k else o[1]
                if r.random() < 0.08:
                    c = o[3]
            px(img, xx, yy, c)
    # ragged eave: straws hanging 0..2 px below the roof edge
    for xx in range(x, x + w):
        hang = r.choice((0, 1, 1, 2))
        for d in range(hang):
            px(img, xx, y + h + d, o[1] if d == 0 else o[0])
    hline(img, x, x + w - 1, y + h - 1, o[0])


def wattle_wall(img, x0, x1, y0, y1, key):
    """Woven wattle-and-daub wall with corner posts."""
    r = rng(key + "/wattle")
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            c = C.earth[2]
            if (x + 2 * y) % 4 == 0:
                c = C.earth[3]
            elif (x - 2 * y) % 4 == 0:
                c = C.earth[1]
            if r.random() < 0.18:
                c = C.ochre[1]                         # daub patches
            if y == y0:
                c = C.earth[0]                         # eave shadow
            px(img, x, y, c)
    for x in (x0, x1) + tuple(range(x0 + 6, x1 - 2, 7)):
        vline(img, x, y0 + 1, y1, C.earth[1] if x != x0 else C.earth[3])


def _stucco_floor(img, x0, x1, y0, y1, key, lit=True):
    """Upper-storey stucco band (no plinth)."""
    r = rng(key)
    st = C.stucco
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            c = st[2]
            if r.random() < 0.05:
                c = st[1]
            if x == x0:
                c = st[3]
            if x == x1:
                c = st[1]
            px(img, x, y, c)


def _cornice(img, x0, x1, y):
    hline(img, x0, x1, y, C.marble[1])
    hline(img, x0, x1, y + 1, C.travertine[0])


def _shutter_window(img, x, y, h=3):
    """Tall window, dark opening with wooden shutters either side."""
    for yy in range(y, y + h):
        px(img, x, yy, C.earth[2])
        px(img, x + 1, yy, C.outline)
        px(img, x + 2, yy, C.earth[0])
        px(img, x + 3, yy, C.earth[1])
    hline(img, x, x + 3, y - 1, C.marble[1])


def _taberna(img, x, y_floor, w=6, h=6):
    """Shop opening: dark interior, counter with amphorae, lintel."""
    top = y_floor - h + 1
    rect(img, x, top, w, h, C.earth[0])
    hline(img, x, x + w - 1, top, C.outline)
    hline(img, x - 1, x + w, top - 1, C.travertine[1])
    hline(img, x, x + w - 2, y_floor, C.travertine[2])
    hline(img, x, x + w - 2, y_floor - 1, C.travertine[1])
    px(img, x + 1, y_floor - 2, C.roof_terracotta[2])
    px(img, x + 3, y_floor - 2, C.roof_terracotta[1])
    if w > 5:
        px(img, x + 4, y_floor - 2, C.ochre[2])
    vline(img, x + w - 1, top + 1, y_floor, C.earth[2])


def _balcony(img, x0, x1, y):
    """Wooden balcony (maenianum): plank floor, rail, balusters."""
    hline(img, x0, x1, y, C.earth[3])                # rail (lit)
    for x in range(x0, x1 + 1):
        px(img, x, y + 1, C.earth[1] if x % 2 else C.outline)
    hline(img, x0, x1, y + 2, C.earth[2])            # floor edge
    hline(img, x0, x1, y + 3, C.earth[0])            # shadow below


def _stakes(img, pts):
    for (x, y) in pts:
        px(img, x, y - 2, C.earth[3])
        px(img, x, y - 1, C.earth[2])
        px(img, x, y, C.earth[1])


def _rope(img, x0, y0, x1, y1):
    for (x, y) in _line_pts(x0, y0, x1, y1):
        if (x + y) % 3:
            px(img, x, y, C.travertine[2])


def _line_pts(x0, y0, x1, y1):
    pts = []
    n = max(abs(x1 - x0), abs(y1 - y0))
    for i in range(n + 1):
        t = i / float(max(1, n))
        pts.append((int(round(x0 + (x1 - x0) * t)), int(round(y0 + (y1 - y0) * t))))
    return pts


# ------------------------------------------------------------ plots
def empty_plot(w, h, key):
    W, H = _canvas(w, h)
    g = new(W, H)
    s = new(W, H)
    top = H - h * TILE
    _yard(g, 1, top + 1, W - 2, H - 2, key, base=C.earth[3])
    # survey rope between corner stakes
    xa, xb, ya, yb = 2, W - 3, top + 3, H - 3
    _rope(g, xa, ya, xb, ya)
    _rope(g, xa, yb, xb, yb)
    _rope(g, xa, ya, xa, yb)
    _rope(g, xb, ya, xb, yb)
    _stakes(s, [(xa, ya), (xb, ya), (xa, yb), (xb, yb)])
    # a little pile of marker stones
    for (x, y) in ((W // 2 - 1, top + h * 8), (W // 2, top + h * 8), (W // 2, top + h * 8 - 1)):
        px(s, x, y, C.travertine[2] if y % 2 else C.travertine[1])
    return _merge(g, s)


# ------------------------------------------------------------ house tiers
def tent(key="bld/house_plot_t0"):
    W, H = _canvas(2, 2)
    g = new(W, H)
    s = new(W, H)
    top = H - 32
    _yard(g, 2, top + 6, W - 3, H - 3, key)
    o = C.ochre
    # tent: ridge north-south, gable (entrance) facing the viewer
    x0, x1 = 4, 22
    cx = (x0 + x1) // 2
    roof_top, base = top + 2, H - 7
    apex = base - 11
    for y in range(roof_top, base + 1):
        for x in range(x0, x1 + 1):
            c = o[3] if x < cx else (o[1] if x > cx else o[2])
            if (y - roof_top) % 4 == 3 and x != cx:
                c = o[2] if x < cx else o[0]         # seams between hides
            px(s, x, y, c)
    # front gable triangle with an open door flap
    for y in range(apex, base + 1):
        span = int(round((y - apex) * (x1 - x0) / 2.0 / (base - apex)))
        for x in range(cx - span, cx + span + 1):
            c = o[2] if x < cx else o[1]
            px(s, x, y, c)
        px(s, cx - span, y, o[0])
        px(s, cx + span, y, o[0])
    for y in range(apex + 4, base + 1):
        span = (y - apex - 4) // 2
        for x in range(cx - span, cx + span + 1):
            px(s, x, y, C.earth[0])
        px(s, cx - span - 1, y, o[3])                # pulled-back flap (lit)
    # guy ropes to pegs (ground layer)
    _rope(g, x0 - 2, base + 2, x0, base - 3)
    _rope(g, x1 + 2, base + 2, x1, base - 3)
    _stakes(s, [(x0 - 2, base + 3), (x1 + 2, base + 3)])
    # campfire ring with flames
    fx, fy = 26, H - 9
    for (dx, dy) in ((-2, 0), (2, 0), (-1, -1), (1, -1), (-1, 1), (1, 1), (0, -1), (0, 1)):
        px(s, fx + dx, fy + dy, C.gray[3] if dy <= 0 else C.gray[2])
    px(s, fx, fy, C.roman_red[2])
    px(s, fx, fy - 1, C.gold)
    px(s, fx - 1, fy, C.roof_terracotta[3])
    px(s, fx + 1, fy, C.earth[1])
    # bedroll
    rect(s, 24, top + 10, 4, 2, C.roman_red[1])
    hline(s, 24, 27, top + 10, C.roman_red[2])
    return _merge(g, s)


def hut(key="bld/house_plot_t1"):
    W, H = _canvas(2, 2)
    g = new(W, H)
    s = new(W, H)
    top = H - 32
    _yard(g, 1, top + 4, W - 2, H - 2, key)
    x0, x1 = 3, 22
    wall_h = 8
    wall_top = H - 3 - wall_h
    wattle_wall(s, x0, x1, wall_top, H - 4, key)
    thatch_roof(s, x0 - 1, top + 1, x1 - x0 + 3, wall_top - top - 1, key)
    # door with a hide curtain
    dx = 10
    rect(s, dx, H - 9, 4, 6, C.earth[0])
    vline(s, dx, H - 9, H - 4, C.earth[3])
    vline(s, dx + 1, H - 8, H - 4, C.earth[2])
    # tiny window
    px(s, 17, wall_top + 3, C.outline)
    px(s, 18, wall_top + 3, C.outline)
    # wattle fence and woodpile in the yard
    for x in range(24, 30):
        px(s, x, H - 6, C.earth[2] if x % 2 else C.earth[3])
        if x % 3 == 0:
            px(s, x, H - 7, C.earth[1])
    for (x, y) in ((25, top + 12), (27, top + 12), (26, top + 11)):
        px(s, x, y, C.earth[3])
        px(s, x + 1, y, C.earth[1])
    return _merge(g, s)


def casa(key="bld/house_plot_t2"):
    W, H = _canvas(2, 2)
    g = new(W, H)
    s = new(W, H)
    top = H - 32
    _yard(g, 22, top + 6, W - 2, H - 2, key, base=C.ochre[2], specks=(C.ochre[1], C.ochre[3], C.earth[3]))
    x0, x1 = 1, 21
    wall_h = 9
    wall_top = H - 1 - wall_h
    roof(s, x0, top - 2, x1 - x0 + 1, wall_top - top + 1, ridge="h", key=key)
    _wall(s, x0, x1, wall_top, H - 2, key, 2, 2)
    _door(s, 13, H - 3, 6)
    _window(s, 5, wall_top + 3)
    hline(s, x0, x1, wall_top - 1, C.roof_terracotta[0])
    # low garden wall around the yard
    for x in range(22, W - 1):
        px(s, x, H - 3, C.marble[0])
        px(s, x, H - 2, C.travertine[1])
    for y in range(top + 8, H - 1):
        px(s, W - 3, y, C.marble[0])
        px(s, W - 2, y, C.travertine[1])
    hline(s, 22, W - 2, top + 8, C.marble[1])
    # a fig bush and an amphora in the yard
    b = objects.bush(0)
    s.paste(b, (22, top + 10), b)
    px(s, 25, H - 6, C.roof_terracotta[2])
    px(s, 25, H - 5, C.roof_terracotta[1])
    px(s, 26, H - 6, C.roof_terracotta[1])
    return _merge(g, s)


def _insula(key, W, H, storeys, roof_h):
    s = new(W, H)
    x0, x1 = 1, W - 2
    wall_top = 1 + roof_h
    roof(s, x0, 1, x1 - x0 + 1, roof_h, ridge="h", key=key)
    ground_h = 10
    upper_h = (H - 2 - wall_top - ground_h) // (storeys - 1)
    # ground floor with shops
    gtop = H - 2 - ground_h + 1
    _wall(s, x0, x1, gtop, H - 2, key + "/g", 2, 2)
    # upper floors
    y = wall_top
    for i in range(storeys - 1):
        y1 = y + upper_h - 1
        _stucco_floor(s, x0, x1, y, y1, key + "/f%d" % i)
        if i == 0:
            hline(s, x0, x1, y, C.travertine[0])      # eave shadow
        for wx in range(x0 + 3, x1 - 3, 6):
            _shutter_window(s, wx, y + 2, min(4, upper_h - 4))
        y = y1 + 1
        if y < gtop:
            _cornice(s, x0, x1, y - 2 if i < storeys - 2 else gtop - 2)
    _cornice(s, x0, x1, gtop - 2)
    _balcony(s, x0 + 2, x1 - 2, gtop - 4)
    # shops and central stair entrance
    _taberna(s, x0 + 2, H - 3, 6, 6)
    _taberna(s, x1 - 7, H - 3, 6, 6)
    _door(s, W // 2, H - 3, 7)
    hline(s, x0, x1, wall_top - 1, C.roof_terracotta[0])
    return s


def small_insula(key="bld/house_plot_t3"):
    W, H = _canvas(2, 2)
    g = new(W, H)
    s = _insula(key, W, H, storeys=2, roof_h=22)
    return _merge(g, s)


def large_insula(key="bld/house_plot_t4"):
    W, H = _canvas(2, 2, more=16)
    g = new(W, H)
    s = _insula(key, W, H, storeys=4, roof_h=22)
    return _merge(g, s)


# ------------------------------------------------------------ domus tiers
def _red_dado(img, x0, x1, y0, y1):
    """Painted Pompeian-red socle on the lower wall of rich houses."""
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            c = C.roman_red[1]
            if x == x0:
                c = C.roman_red[2]
            if x == x1 or y == y1:
                c = C.roman_red[0]
            px(img, x, y, c)


def _portal(img, cx, y_bottom, hgt):
    """Columned entrance (fauces) with a tiny pediment."""
    _door(img, cx, y_bottom, hgt)
    for x in (cx - 4, cx + 3):
        vline(img, x, y_bottom - hgt + 1, y_bottom, C.marble[1] if x < cx else C.marble[0])
    top = y_bottom - hgt - 1
    hline(img, cx - 5, cx + 4, top, C.marble[1])
    hline(img, cx - 3, cx + 2, top - 1, C.marble[0])
    hline(img, cx - 1, cx, top - 2, C.marble[0])


def domus(key="bld/domus_plot_t0"):
    W, H = _canvas(3, 3)
    g = new(W, H)
    s = new(W, H)
    x0, x1 = 1, W - 2
    wall_h = 12
    wall_top = H - 1 - wall_h
    roof(s, x0, 1, x1 - x0 + 1, wall_top - 1, ridge="h", key=key)
    rh = wall_top - 2
    _compluvium(s, 17, 1 + rh // 3, 31, 1 + rh * 2 // 3)
    _wall(s, x0, x1, wall_top, H - 2, key, 3, 3)
    _red_dado(s, x0, x1, H - 6, H - 4)
    _portal(s, W // 2, H - 3, 8)
    _taberna(s, x0 + 3, H - 3, 7, 7)
    _taberna(s, x1 - 9, H - 3, 7, 7)
    hline(s, x0, x1, wall_top - 1, C.roof_terracotta[0])
    return _merge(g, s)


def _peristyle(s, g, x0, y0, x1, y1, key):
    """Open garden court surrounded by a colonnade (ground in g)."""
    # portico floor ring + lawn
    rect(g, x0, y0, x1 - x0 + 1, y1 - y0 + 1, C.travertine[2])
    _lawn(g, x0 + 3, y0 + 3, x1 - 3, y1 - 2, key)
    # clear the structure layer inside the court
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            s.putpixel((x, y), (0, 0, 0, 0))
    # shadow of the roof on the north portico
    hline(g, x0, x1, y0, C.travertine[0])
    hline(g, x0, x1, y0 + 1, C.travertine[1])
    vline(g, x0, y0, y1, C.travertine[1])
    # columns: 1x2 marble with shaded base, every 3 px along the inner edge
    for x in range(x0 + 2, x1 - 1, 3):
        px(s, x, y0 + 2, C.white)
        px(s, x, y0 + 3, C.marble[0])
        px(s, x, y1 - 1, C.white)
        px(s, x, y1, C.marble[0])
    for y in range(y0 + 5, y1 - 2, 3):
        px(s, x0 + 2, y, C.white)
        px(s, x0 + 2, y + 1, C.marble[0])
        px(s, x1 - 2, y, C.white)
        px(s, x1 - 2, y + 1, C.marble[0])
    # garden: cross paths, low hedges, central fountain
    cx, cy = (x0 + x1) // 2, (y0 + y1 + 1) // 2
    for x in range(x0 + 3, x1 - 2):
        px(g, x, cy, C.ochre[3])
    for y in range(y0 + 3, y1 - 1):
        px(g, cx, y, C.ochre[3])
    for (hx, hy) in ((x0 + 5, y0 + 5), (cx + 3, y0 + 5), (x0 + 5, cy + 2), (cx + 3, cy + 2)):
        for x in range(hx, min(hx + 5, x1 - 3)):
            px(s, x, hy, C.grass[1])
            px(s, x, hy + 1, C.grass[0])
    for (x, y) in ellipse_mask(cx + 0.5, cy + 0.5, 2.5, 2.0):
        px(s, x, y, C.marble[1])
    for (x, y) in ellipse_mask(cx + 0.5, cy + 0.5, 1.5, 1.0):
        px(s, x, y, C.sea[2])
    px(s, cx, cy, C.sea[3])


def grand_domus(key="bld/domus_plot_t1"):
    W, H = _canvas(3, 3)
    g = new(W, H)
    s = new(W, H)
    x0, x1 = 1, W - 2
    wall_h = 12
    wall_top = H - 1 - wall_h
    # back wings (U around the peristyle) + front atrium block
    roof(s, x0, 1, x1 - x0 + 1, wall_top - 1, ridge="h", key=key + "/back")
    front_top = wall_top - 22
    roof(s, x0, front_top, x1 - x0 + 1, wall_top - front_top, ridge="h", key=key)
    _compluvium(s, 18, front_top + 7, 29, front_top + 14)
    hline(s, x0, x1, front_top, C.roof_terracotta[3])
    hline(s, x0, x1, front_top - 1, C.roof_terracotta[0])     # step between roofs
    _peristyle(s, g, 9, 6, 38, front_top - 3, key)
    _wall(s, x0, x1, wall_top, H - 2, key, 3, 3)
    _red_dado(s, x0, x1, H - 6, H - 4)
    _portal(s, W // 2, H - 3, 8)
    for wx in (6, 12, 33, 39):
        _window(s, wx, wall_top + 3)
    hline(s, x0, x1, wall_top - 1, C.roof_terracotta[0])
    return _merge(g, s)


def villa(key="bld/domus_plot_t2"):
    W, H = _canvas(3, 3)
    g = new(W, H)
    s = new(W, H)
    x0, x1 = 1, W - 2
    # main house at the back
    wall_top = 27
    wall_bot = 37
    roof(s, x0, 1, x1 - x0 + 1, wall_top - 1, ridge="h", key=key)
    rh = wall_top - 2
    _compluvium(s, 18, 1 + rh // 3, 29, 1 + rh * 2 // 3)
    # portico: shaded back wall with marble columns
    for y in range(wall_top, wall_bot + 1):
        for x in range(x0, x1 + 1):
            c = C.travertine[1] if y > wall_top else C.travertine[0]
            if y == wall_bot:
                c = C.gray[3]
            px(s, x, y, c)
    for x in range(x0 + 1, x1, 4):
        vline(s, x, wall_top + 1, wall_bot - 1, C.white)
        vline(s, x + 1, wall_top + 1, wall_bot - 1, C.marble[0])
    _door(s, W // 2, wall_bot - 1, 7)
    hline(s, x0, x1, wall_top - 1, C.roof_terracotta[0])
    # garden (ground layer): lawn, gravel walks, long pool
    _lawn(g, 1, wall_bot + 1, W - 2, H - 2, key)
    for y in range(wall_bot + 1, H - 2):
        for x in (W // 2 - 1, W // 2):
            px(g, x, y, C.ochre[3] if (x + y) % 3 else C.travertine[2])
    for x in range(1, W - 1):
        px(g, x, wall_bot + 1, C.ochre[3])
        px(g, x, wall_bot + 2, C.travertine[2])
    # pool (euripus) with marble rim
    px0, py0, px1, py1 = 9, wall_bot + 8, W - 10, wall_bot + 16
    rect(s, px0, py0, px1 - px0 + 1, py1 - py0 + 1, C.marble[1])
    rect(s, px0 + 1, py0 + 1, px1 - px0 - 1, py1 - py0 - 1, C.sea[2])
    hline(s, px0 + 1, px1 - 1, py0 + 1, C.sea[1])
    for x in range(px0 + 3, px1 - 2, 5):
        px(s, x, py0 + 3, C.sea[3])
        px(s, x + 1, py0 + 3, C.sea[4])
    vline(s, px1, py0, py1, C.marble[0])
    hline(s, px0, px1, py1, C.marble[0])
    # box hedges along the sides
    for y in range(wall_bot + 4, H - 4, 1):
        for x in (3, 4, W - 5, W - 4):
            px(s, x, y, C.grass[1] if (y + x) % 3 else C.grass[0])
    # low garden wall at the front with a gate gap
    for x in range(1, W - 1):
        if abs(x - W // 2 + 0.5) < 3:
            continue
        px(s, x, H - 3, C.marble[0])
        px(s, x, H - 2, C.travertine[1])
    # flowers
    r = rng(key + "/flowers")
    for _ in range(14):
        x, y = r.randrange(6, W - 7), r.randrange(wall_bot + 4, H - 5)
        if g.getpixel((x, y)) in (C.grass[2], C.grass[1], C.grass[3]) and s.getpixel((x, y))[3] == 0:
            px(g, x, y, r.choice((C.roman_red[2], C.tyrian[1], C.white, C.gold)))
    m = _merge(g, s)
    # cypresses flanking the pool, already outlined
    for cxp in (6, W - 7):
        t = objects.tree_cypress("villa/cypress/%d" % cxp, tall=False)
        m.paste(t, (cxp - t.width // 2, py1 - t.height + 4), t)
    return m


def generate():
    out = []

    def add(name, img):
        out.append((name, img, (0, img.height - 1)))

    add("bld/house_plot", empty_plot(2, 2, "bld/house_plot"))
    add("bld/house_plot_t0", tent())
    add("bld/house_plot_t1", hut())
    add("bld/house_plot_t2", casa())
    add("bld/house_plot_t3", small_insula())
    add("bld/house_plot_t4", large_insula())
    add("bld/domus_plot", empty_plot(3, 3, "bld/domus_plot"))
    add("bld/domus_plot_t0", domus())
    add("bld/domus_plot_t1", grand_domus())
    add("bld/domus_plot_t2", villa())
    return out
