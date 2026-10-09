"""Generic Roman building blocks for every footprint 1x1 .. 4x4.

Sprite: width = w*16, height = h*16 + extra, extra = 8 + 4*min(w, h).
Anchor bottom-left (0, H-1): the sprite's bottom row is the footprint's
south edge.  Front (south) wall visible, roof above it (3/4 view).

- w >= h: hipped roof with the ridge running east-west.
- h >  w: gable roof with the ridge running north-south and a pediment on
  the front wall.
- w >= 3 and h >= 3: atrium house with an open compluvium showing the
  impluvium pool.
"""
from palette import C
from primitives import (new, px, rect, hline, vline, rng, outline, roof)

TILE = 16


def extra_height(w, h):
    return 8 + 4 * min(w, h)


def wall_height(w, h):
    return 6 + 2 * min(w, h)


def _wall(img, x0, x1, y0, y1, key, w, h):
    """Stucco front wall between rows y0..y1 (inclusive)."""
    r = rng(key + "/wall")
    st = C.stucco      # trav1, trav2, marble0, marble1
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            c = st[2]
            if y == y0:
                c = C.travertine[0]                # shadow under the eaves
            elif y == y0 + 1:
                c = st[1]
            elif y >= y1 - 1:
                c = C.gray[2] if y == y1 else C.gray[3]   # plinth / base course
            elif r.random() < 0.06:
                c = st[1]                          # weathering
            if x == x0 and y0 < y < y1 - 1:
                c = st[3]                          # lit left corner
            if x == x1 and y0 < y < y1 - 1:
                c = st[1]                          # shaded right corner
            px(img, x, y, c)
    # stain streaks below the eaves
    for _ in range(max(1, (x1 - x0) // 12)):
        sx = r.randrange(x0 + 2, x1 - 1)
        for y in range(y0 + 1, y0 + 1 + r.randrange(2, 4)):
            px(img, sx, y, st[1])


def _door(img, cx, y_bottom, hgt):
    """Arched doorway, 4px wide, centered on cx (left pixel cx-2)."""
    x = cx - 2
    top = y_bottom - hgt + 1
    for yy in range(top, y_bottom + 1):
        for xx in range(x, x + 4):
            c = C.earth[0]
            if xx == x + 3 or yy == top + 1:
                c = C.outline
            if xx in (x + 1, x + 2) and yy > top + 1:
                c = C.earth[1] if xx == x + 1 else C.earth[0]   # wooden leaves
            px(img, xx, yy, c)
    # rounded top: knock out corners back to wall, lintel/arch stones above
    hline(img, x - 1, x + 4, top - 1, C.travertine[1])
    px(img, x, top, C.travertine[0])
    px(img, x + 3, top, C.travertine[0])
    px(img, x - 1, top - 1, C.travertine[0])


def _window(img, x, y):
    px(img, x, y, C.outline)
    px(img, x + 1, y, C.outline)
    px(img, x, y + 1, C.earth[0])
    px(img, x + 1, y + 1, C.earth[0])
    px(img, x, y - 1, C.marble[1])       # lit sill above (lintel)
    px(img, x + 1, y - 1, C.marble[1])


def _shop(img, x, y_floor):
    """Taberna opening: dark interior, wooden shutter post, stone counter,
    goods (amphorae) on the counter."""
    top = y_floor - 4
    rect(img, x, top, 6, 5, C.earth[0])
    hline(img, x, x + 5, top, C.outline)
    hline(img, x - 1, x + 6, top - 1, C.travertine[1])      # lintel
    vline(img, x + 5, top, y_floor, C.earth[2])           # shutter post
    hline(img, x, x + 4, y_floor, C.travertine[2])        # counter
    hline(img, x, x + 4, y_floor - 1, C.travertine[1])
    px(img, x + 1, y_floor - 2, C.roof_terracotta[2])     # amphorae
    px(img, x + 3, y_floor - 2, C.roof_terracotta[1])


def _compluvium(img, x0, y0, x1, y1):
    """Opening in the roof: inner roof rim, atrium floor, impluvium pool."""
    rect(img, x0, y0, x1 - x0 + 1, y1 - y0 + 1, C.travertine[2])
    hline(img, x0, x1, y0, C.outline)
    hline(img, x0, x1, y0 + 1, C.travertine[0])         # shadow from north roof
    vline(img, x0, y0, y1, C.outline)
    vline(img, x0 + 1, y0, y1, C.travertine[1])
    vline(img, x1, y0, y1, C.roof_terracotta[3])       # lit inner roof face
    hline(img, x0, x1, y1, C.roof_terracotta[3])
    # pool
    pw, ph = max(2, (x1 - x0) // 2), max(2, (y1 - y0) // 2)
    pxs = x0 + (x1 - x0 - pw) // 2 + 1
    pys = y0 + (y1 - y0 - ph) // 2 + 1
    rect(img, pxs - 1, pys - 1, pw + 2, ph + 2, C.marble[1])
    rect(img, pxs, pys, pw, ph, C.sea[2])
    hline(img, pxs, pxs + pw - 1, pys, C.sea[1])
    px(img, pxs + pw - 1, pys + ph - 1, C.sea[3])


def placeholder(w, h):
    key = "bld/placeholder_%dx%d" % (w, h)
    W = w * TILE
    H = h * TILE + extra_height(w, h)
    img = new(W, H)
    wall_h = wall_height(w, h)
    x0, x1 = 1, W - 2                        # 1px gutter for the outline
    wall_top = H - wall_h
    roof_top = 1
    roof_bot = wall_top - 1
    r = rng(key)

    if w >= h:
        roof(img, x0, roof_top, x1 - x0 + 1, roof_bot - roof_top + 1, ridge="h", key=key)
        _wall(img, x0, x1, wall_top, H - 2, key, w, h)
    else:
        roof(img, x0, roof_top, x1 - x0 + 1, roof_bot - roof_top + 1, ridge="v", key=key)
        _wall(img, x0, x1, wall_top, H - 2, key, w, h)
        # pediment: the gable end faces the viewer -- a stucco triangle whose
        # apex sits under the ridge, framed by a dark raking eave
        mid_x = x0 + (x1 - x0 + 1) // 2
        ph = max(3, min(roof_bot - roof_top - 3, (x1 - x0) // 3))
        for k in range(ph + 1):
            yrow = wall_top - 1 - k
            span = int(round((x1 - x0 + 1) / 2.0 * (1 - k / float(ph + 1))))
            xa, xb = mid_x - span, mid_x + span - 1
            for xx in range(max(x0, xa), min(x1, xb) + 1):
                c = C.marble[0] if xx < mid_x else C.travertine[2]
                px(img, xx, yrow, c)
            px(img, max(x0, xa), yrow, C.roof_terracotta[0])
            px(img, min(x1, xb), yrow, C.roof_terracotta[0])
            if k == ph:
                hline(img, max(x0, xa), min(x1, xb), yrow, C.roof_terracotta[0])
        hline(img, x0, x1, wall_top - 1, C.marble[1])      # cornice
        # a small round window (oculus) in larger pediments
        if ph >= 5:
            px(img, mid_x - 1, wall_top - 3, C.outline)
            px(img, mid_x, wall_top - 3, C.outline)

    if w >= 3 and h >= 3:
        rh = roof_bot - roof_top
        cy0 = roof_top + rh // 3
        cy1 = roof_top + rh * 2 // 3
        cw = (x1 - x0) // 3
        cx0 = x0 + (x1 - x0 - cw) // 2
        _compluvium(img, cx0, cy0, cx0 + cw, cy1)

    # ground-floor details
    floor_y = H - 3                          # last wall row above the plinth
    if w >= 2:
        door_cx = W // 2 if w % 2 == 0 else W // 2 + 1
        _door(img, door_cx, floor_y + 1, min(wall_h - 3, 7))
        # windows either side
        win_y = wall_top + 3
        slots = list(range(x0 + 3, x1 - 3, 7))
        for sx in slots:
            if abs(sx + 1 - door_cx) > 4:
                _window(img, sx, win_y)
        if w >= 3 and h <= 2:
            # tabernae: wide shop openings with a counter either side
            for sx in (x0 + 3, x1 - 8):
                if abs(sx + 3 - door_cx) > 6:
                    _shop(img, sx, floor_y)
    else:
        _window(img, W // 2 - 1, wall_top + 3)
        # narrow door
        dx = W // 2 - 1
        for y in range(H - 2 - 4, H - 2):
            px(img, dx, y, C.earth[0])
            px(img, dx + 1, y, C.outline)

    outline(img, C.outline)
    # outline between roof and wall (eave line)
    hline(img, x0, x1, roof_bot, C.roof_terracotta[0])
    return img


def generate():
    out = []
    for w in range(1, 5):
        for h in range(1, 5):
            img = placeholder(w, h)
            out.append(("bld/placeholder_%dx%d" % (w, h), img, (0, img.height - 1)))
    return out
