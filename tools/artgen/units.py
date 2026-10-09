"""Military units: legionary, archer, barbarian (8x12, anchor (4, 11)),
cavalry (12x14, anchor (6, 13)) and the ballista (16x16, anchor (8, 15)).

  people/<unit>_<n|e|s|w>_<0..3>      walk cycle
  people/<unit>_atk_<n|e|s|w>_<0..1>  attack: 0 wind-up, 1 strike/release
  people/ballista_<n|e|s|w>

Bodies are small symbolic grids (code, not painted PNGs) in the same style
as people.py: 1px dark outline, light from the top-left.  West is the
mirror of east.  Thin weapons (pilum, spear, bow, lance) are drawn after
the outline so they stay 1px wide and readable.

Grid letters:
  H headgear/hair  h hair under helmet  P crest/plume  S skin  s skin shade
  b beard  A armor lit  a armor dark  M mail  m mail dark  T cloth  L cloth lit
  D cloth dark  B belt  U fur  u fur dark  R trousers  F foot
  Q shield lit  q shield mid  k shield dark  o shield boss
"""
from palette import C
from primitives import new, px, line, outline

DIRS = ["n", "e", "s", "w"]
UNITS = ["legionary", "archer", "cavalry", "barbarian"]
ANCHOR = (4, 11)
CAV_ANCHOR = (6, 13)

# ---------------------------------------------------------------- legs
LEGS = {
    "s": [[".S..s.", ".F..F."],
          [".F..s.", "....F."],
          [".S..s.", ".F..F."],
          [".S..F.", ".F...."]],
    "e": [["..S.s.", ".F...F"],
          ["..Ss..", "..FF.."],
          ["..s.S.", ".F...F"],
          ["..sS..", "..FF.."]],
}
ATK_LEGS = {"s": [".S..s.", "F....F"], "e": [".S..s.", "F....F"]}   # braced stance

# ---------------------------------------------------------------- templates
HEADS = {
    "legionary": {"s": ["..PP..", ".HHHH.", ".HSsH."],
                  "n": ["..PP..", ".HHHH.", ".HHHH."],
                  "e": [".PPP..", ".HHHH.", "..HSs."]},
    "archer":    {"s": ["..HH..", ".HHHH.", ".hSSs."],
                  "n": ["..HH..", ".HHHH.", ".hhhh."],
                  "e": ["..HH..", ".HHHH.", "..hSs."]},
    "barbarian": {"s": [".HHHH.", "HSSSsH", "HSbbsH"],
                  "n": [".HHHH.", "HHHHHH", "HHHHHH"],
                  "e": ["..HHH.", ".HHSS.", "HHHbb."]},
}
BODIES = {
    "legionary": {"s": ["AAAAAa", "SaAaas", "SBBBBs", ".TDTD.", ".LTTD."],
                  "n": ["AAAAAa", "SaaaAs", "SBBBBs", ".TDTD.", ".TTTD."],
                  "e": [".AAAa.", ".aaSa.", ".BBBB.", ".TDTD.", ".LTD.."]},
    "archer":    {"s": ["LMMMMm", "SMmMms", "SBBBBs", ".LTTD.", ".LTTD."],
                  "n": ["LMMMMm", "SMMMms", "SBBBBs", ".TTTD.", ".TTDD."],
                  "e": [".MMMm.", ".MMSm.", ".BBBB.", ".LTTD.", ".LTD.."]},
    "barbarian": {"s": ["UUuUUu", "STbbDs", "SBBBBs", ".LTTD.", ".RRRR."],
                  "n": ["UUUUUu", "UUUUUu", "SBBBBs", ".UUUu.", ".RRRR."],
                  "e": [".UUUu.", ".UTSu.", ".BBBB.", ".LTD..", ".RRR.."]},
}

COLORS = {
    "legionary": dict(H=C.gray[2], P=C.roman_red[2], A=C.gray[3], a=C.gray[1],
                      T=C.roman_red[1], L=C.roman_red[2], D=C.roman_red[0],
                      B=C.earth[1], F=C.earth[0],
                      Q=C.roman_red[2], q=C.roman_red[1], k=C.roman_red[0], o=C.gold),
    "archer": dict(H=C.gray[3], h=C.earth[0], M=C.gray[2], m=C.gray[1],
                   T=C.cypress[2], L=C.grass[0], D=C.cypress[1],
                   B=C.earth[1], F=C.earth[0]),
    "barbarian": dict(H=C.roof_terracotta[1], b=C.roof_terracotta[0], U=C.gray[3], u=C.gray[2],
                      T=C.olive[1], L=C.olive[2], D=C.olive[0], R=C.earth[1],
                      B=C.earth[0], F=C.earth[0],
                      Q=C.earth[3], q=C.earth[2], k=C.earth[1], o=C.gray[3]),
}

# shields: (template col, row) rectangles in the 6x10 body frame, per facing
SHIELDS = {
    "legionary": {"s": (3, 3, 3, 6), "n": (0, 3, 2, 6), "e": (4, 3, 2, 6)},
    "barbarian": {"s": (3, 3, 3, 4), "n": (0, 3, 2, 4), "e": (4, 3, 2, 4)},
}


def _skin(ch, face):
    lit = face == "s"
    if ch == "S":
        return C.skin[2] if lit else C.skin[1]
    return C.skin[1] if lit else C.skin[0]


def _shield(grid, unit, face):
    """Stamp the shield letters into the grid (rows of 6 chars)."""
    if unit not in SHIELDS:
        return grid
    x0, y0, w, h = SHIELDS[unit][face]
    rows = [list(r) for r in grid]
    round_ = unit == "barbarian"
    for j in range(h):
        for i in range(w):
            if round_ and (j in (0, h - 1)) and (i in (0, w - 1)) and w > 2:
                continue
            ch = "q"
            if i == 0:
                ch = "Q"
            if i == w - 1 and w > 1:
                ch = "k"
            if j == 0:
                ch = "Q"
            rows[y0 + j][x0 + i] = ch
    # boss
    bx, by = x0 + w // 2, y0 + h // 2
    if w >= 2:
        rows[by][min(x0 + w - 1, bx)] = "o"
    return ["".join(r) for r in rows]


def _render_body(unit, d, legs):
    face = "e" if d in ("e", "w") else d
    grid = list(HEADS[unit][face]) + list(BODIES[unit][face]) + list(legs)
    grid = _shield(grid, unit, face)
    if d == "w":
        grid = [g[::-1] for g in grid]
    cols = COLORS[unit]
    img = new(8, 12)
    for j, row in enumerate(grid):
        for i, ch in enumerate(row):
            if ch == ".":
                continue
            if ch in "Ss":
                # barbarian trousers cover the legs
                c = cols["R"] if (unit == "barbarian" and j >= 8) else _skin(ch, face)
            else:
                c = cols.get(ch, cols["T"])
            px(img, 1 + i, 1 + j, c)
    outline(img, C.outline)
    return img


# ---------------------------------------------------------------- weapons
def _mx(d, x, w=8):
    return w - 1 - x if d == "w" else x


def _pole(img, d, x, y0, y1, tip, shaft=None, tip_len=2, tip_at_top=True):
    shaft = shaft or C.ochre[1]
    for y in range(y0, y1 + 1):
        is_tip = (y < y0 + tip_len) if tip_at_top else (y > y1 - tip_len)
        px(img, _mx(d, x), y, tip if is_tip else shaft)


def _hline_w(img, d, x0, x1, y, tip, shaft=None, tip_len=2):
    """Horizontal weapon pointing toward the facing side (east unless w)."""
    shaft = shaft or C.ochre[1]
    for x in range(x0, x1 + 1):
        px(img, _mx(d, x), y, tip if x > x1 - tip_len else shaft)


def _legionary_weapon(img, d, frame, atk):
    tip = C.gray[3]
    if d == "s":
        if atk is None:
            _pole(img, d, 0, 0 + (frame % 2), 9, tip)
        elif atk == 0:
            _pole(img, d, 0, 0, 7, tip)
            px(img, 0, 8, C.skin[2])
        else:
            _pole(img, d, 1, 6, 11, C.white, tip_at_top=False)       # thrust at the viewer
    elif d == "n":
        if atk is None:
            _pole(img, d, 7, 0 + (frame % 2), 9, tip)
        elif atk == 0:
            _pole(img, d, 7, 0, 7, tip)
        else:
            _pole(img, d, 6, 0, 4, C.white)                          # thrust away
    else:
        if atk is None:
            _pole(img, d, 1, 0 + (frame % 2), 9, tip)
        elif atk == 0:
            line(img, _mx(d, 0), 7, _mx(d, 3), 0, C.ochre[1])       # pilum drawn back
            px(img, _mx(d, 3), 0, tip)
        else:
            _hline_w(img, d, 2, 7, 6, C.white)                       # thrust forward


def _archer_weapon(img, d, frame, atk):
    bow, string = C.earth[3], C.marble[0]
    quiver = C.earth[1]
    if d == "s":
        px(img, 6, 1, C.marble[1])                                   # fletching over shoulder
        if atk is None:
            for y in range(3, 9):
                px(img, 7, y, bow)
            px(img, 6, 2, bow)
            px(img, 6, 9, bow)
        else:
            # bow raised across the chest, aimed at the viewer
            line(img, 0, 5, 7, 5, bow)
            px(img, 0, 4, bow)
            px(img, 7, 4, bow)
            if atk == 0:
                px(img, 3, 6, C.white)                               # arrowhead nocked
                px(img, 4, 6, C.white)
            else:
                px(img, 3, 9, C.white)                               # arrow loosed
    elif d == "n":
        line(img, 2, 4, 5, 8, quiver)                                # quiver on the back
        px(img, 2, 3, C.marble[1])
        px(img, 1, 3, C.marble[1])
        if atk is None:
            for y in range(3, 9):
                px(img, 0, y, bow)
            px(img, 1, 2, bow)
            px(img, 1, 9, bow)
        else:
            line(img, 0, 3, 7, 3, bow)
            px(img, 0, 4, bow)
            px(img, 7, 4, bow)
    else:
        px(img, _mx(d, 1), 3, C.marble[1])                           # fletching
        if atk is None:
            for y in range(3, 9):
                px(img, _mx(d, 7), y, bow)
            px(img, _mx(d, 6), 2, bow)
            px(img, _mx(d, 6), 9, bow)
        elif atk == 0:
            for y in range(2, 10):
                px(img, _mx(d, 7), y, bow)
            line(img, _mx(d, 6), 2, _mx(d, 4), 5, string)            # string drawn back
            line(img, _mx(d, 4), 5, _mx(d, 6), 9, string)
            _hline_w(img, d, 4, 7, 5, C.white, shaft=C.earth[3], tip_len=1)
        else:
            for y in range(3, 9):
                px(img, _mx(d, 7), y, bow)
            px(img, _mx(d, 6), 2, bow)
            px(img, _mx(d, 6), 9, bow)
            px(img, _mx(d, 6), 5, C.white)                           # release flash


def _barbarian_weapon(img, d, frame, atk):
    head = C.gray[3]
    if d == "s":
        if atk is None:
            _pole(img, d, 0, 0 + (frame % 2), 9, head, tip_len=1)
        elif atk == 0:
            # axe raised overhead
            _pole(img, d, 0, 0, 6, head, tip_len=0)
            px(img, 1, 0, head)
            px(img, 1, 1, head)
        else:
            line(img, 0, 6, 2, 11, C.ochre[1])
            px(img, 3, 11, head)
            px(img, 2, 10, head)
    elif d == "n":
        if atk is None:
            _pole(img, d, 7, 0 + (frame % 2), 9, head, tip_len=1)
        elif atk == 0:
            _pole(img, d, 7, 0, 6, head, tip_len=0)
            px(img, 6, 0, head)
            px(img, 6, 1, head)
        else:
            line(img, 7, 6, 5, 1, C.ochre[1])
            px(img, 4, 0, head)
            px(img, 5, 0, head)
    else:
        if atk is None:
            _pole(img, d, 1, 0 + (frame % 2), 9, head, tip_len=1)
        elif atk == 0:
            line(img, _mx(d, 3), 4, _mx(d, 0), 0, C.ochre[1])        # axe swung back
            px(img, _mx(d, 0), 1, head)
            px(img, _mx(d, 1), 0, head)
        else:
            line(img, _mx(d, 3), 5, _mx(d, 7), 7, C.ochre[1])        # chop forward
            px(img, _mx(d, 7), 6, head)
            px(img, _mx(d, 7), 8, head)
            px(img, _mx(d, 6), 8, head)


WEAPONS = {"legionary": _legionary_weapon, "archer": _archer_weapon,
           "barbarian": _barbarian_weapon}


def foot(unit, d, frame=0, atk=None):
    face = "e" if d in ("e", "w") else d
    lk = "e" if face == "e" else "s"
    legs = ATK_LEGS[lk] if atk is not None else LEGS[lk][frame]
    img = _render_body(unit, d, legs)
    WEAPONS[unit](img, d, frame, atk)
    return img


# ---------------------------------------------------------------- cavalry
# 10x12 templates placed at (1, 1) in a 12x14 canvas.  Horse letters:
#   N coat  n coat dark  G coat lit  E ears/mane  z muzzle  t tail  l leg  f hoof
CAV = {
    "e": ["..PP......",
          "..HHH.....",
          "..hSs..EE.",
          ".AAAa..GNN",
          ".aSaa.EGNz",
          "tGBBGEGN..",
          "tNTTNNNN..",
          "tNNSNNNn..",
          ".nnFnnnn.."],
    "s": ["....PP....",
          "...HHHH...",
          "...HSSs...",
          "..SAAAAs..",
          "..SAaaAs..",
          "..TEGNET..",
          "..SNGNNs..",
          "..FNGNNF..",
          "...nzzn..."],
    "n": ["....PP....",
          "...HHHH...",
          "...HHHH...",
          "..AAAAAa..",
          "..AAAAAa..",
          ".TGGNNNnT.",
          ".SGNNNNnS.",
          ".FNNttNnF.",
          "..nntnnn.."],
}
CAV_LEGS = {
    "e": [[".l.l..l.l.", "l..l.l...l", "f..f.f...f"],
          [".l.l..l.l.", ".l.l..ll..", ".f.f..ff.."],
          [".l.l..l.l.", "..ll.l..l.", "..ff.f..f."],
          [".l.l..l.l.", ".ll...l.l.", ".ff...f.f."]],
    "s": [["...l..l...", "...l..l...", "...f..f..."],
          ["...l..l...", "...f..l...", "......f..."],
          ["...l..l...", "...l..l...", "...f..f..."],
          ["...l..l...", "...l..f...", "...f......"]],
    "n": [["..l....l..", "..l....l..", "..f....f.."],
          ["..l....l..", "..f....l..", ".......f.."],
          ["..l....l..", "..l....l..", "..f....f.."],
          ["..l....l..", "..l....f..", "..f......."]],
}
CAV_COLORS = dict(P=C.roman_red[2], H=C.gray[2], h=C.gray[2], A=C.gray[3], a=C.gray[1],
                  B=C.earth[1], T=C.roman_red[1], F=C.earth[0],
                  N=C.earth[2], n=C.earth[1], G=C.earth[3], E=C.earth[0],
                  z=C.earth[0], t=C.earth[0], l=C.earth[1], f=C.outline)


def cavalry(d, frame=0, atk=None):
    face = "e" if d in ("e", "w") else d
    legs = CAV_LEGS[face][0 if atk is not None else frame]
    grid = list(CAV[face]) + legs
    if d == "w":
        grid = [g[::-1] for g in grid]
    img = new(12, 14)
    bob = 1 if (atk is None and frame in (1, 3)) else 0
    for j, row in enumerate(grid):
        for i, ch in enumerate(row):
            if ch == ".":
                continue
            c = _skin(ch, face) if ch in "Ss" else CAV_COLORS[ch]
            yy = 1 + j - (bob if j < len(grid) - 3 else 0)
            px(img, 1 + i, yy, c)
    outline(img, C.outline)
    # lance (and its pennant) drawn over the outline
    tip = C.gray[3]
    if face == "e":
        if atk is None or atk == 0:
            # lance held upright behind the rider (atk 0: raised higher)
            x = _mx(d, 1, 12)
            top = 0
            bottom = 6 if atk is None else 4
            for y in range(top, bottom + 1):
                px(img, x, y, tip if y < top + 2 else C.ochre[1])
            if atk is None:
                px(img, _mx(d, 0, 12), 2, C.roman_red[2])           # pennant
        else:
            for x in range(3, 12):
                px(img, _mx(d, x, 12), 6, C.white if x >= 10 else C.ochre[1])
    elif face == "s":
        x = 9
        if atk is None:
            for y in range(0, 11):
                px(img, x, y, tip if y < 2 else C.ochre[1])
        elif atk == 0:
            for y in range(0, 8):
                px(img, x, y, tip if y < 2 else C.ochre[1])
        else:
            for y in range(7, 14):
                px(img, x - 1, y, C.white if y > 11 else C.ochre[1])
    else:
        x = 2
        if atk is None:
            for y in range(0, 11):
                px(img, x, y, tip if y < 2 else C.ochre[1])
        elif atk == 0:
            for y in range(0, 8):
                px(img, x, y, tip if y < 2 else C.ochre[1])
        else:
            for y in range(0, 6):
                px(img, x + 1, y, C.white if y < 2 else C.ochre[1])
    return img


# ---------------------------------------------------------------- ballista
def _ballista_materials():
    """Material map for a ballista pointing north on a 16x16 grid."""
    m = {}

    def put(x, y, k):
        if 0 <= x < 16 and 0 <= y < 16:
            m[(x, y)] = k

    for y in range(3, 14):                      # stock / slider
        put(7, y, "wood")
        put(8, y, "wood")
    for x in range(4, 12):                      # rear trestle
        for y in (12, 13):
            put(x, y, "wood")
    for y in range(10, 15):                     # wheels
        put(3, y, "iron")
        put(12, y, "iron")
    for x in range(4, 12):                      # torsion frame
        for y in range(5, 8):
            put(x, y, "wood")
    for x in (5, 10):                           # sinew springs
        for y in range(4, 9):
            put(x, y, "rope")
    for (x0, y0, x1, y1) in ((4, 6, 2, 9), (11, 6, 13, 9)):   # arms
        n = max(abs(x1 - x0), abs(y1 - y0))
        for i in range(n + 1):
            put(round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n), "wood")
    return m


def _ballista_overlays():
    """(x, y, color_key) drawn without shading: string and bolt."""
    o = []
    for (x0, y0, x1, y1) in ((2, 10, 7, 11), (13, 10, 8, 11)):
        n = max(abs(x1 - x0), abs(y1 - y0))
        for i in range(n + 1):
            o.append((round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n), "string"))
    for y in range(2, 11):
        o.append((7, y, "shaft"))
    o.append((7, 1, "tip"))
    o.append((7, 0, "tip"))
    return o


def _xform(x, y, d):
    if d == "n":
        return x, y
    if d == "s":
        return 15 - x, 15 - y
    if d == "e":
        return 15 - y, x
    return y, 15 - x                              # w


MATERIAL = {"wood": (C.earth[1], C.earth[2], C.earth[3]),
            "iron": (C.gray[0], C.gray[1], C.gray[2]),
            "rope": (C.roman_red[0], C.roof_terracotta[1], C.roof_terracotta[2])}
OVERLAY = {"string": C.marble[0], "shaft": C.earth[3], "tip": C.gray[3]}


def ballista(d):
    """Material map rotated to the facing, then auto-shaded so the light
    still comes from the top-left in every orientation."""
    mats = {_xform(x, y, d): k for (x, y), k in _ballista_materials().items()}
    img = new(16, 16)
    for (x, y), k in mats.items():
        dark, mid, lit = MATERIAL[k]
        c = mid
        if mats.get((x - 1, y)) != k or mats.get((x, y - 1)) != k:
            c = lit
        if mats.get((x + 1, y)) != k or mats.get((x, y + 1)) != k:
            c = dark
        px(img, x, y, c)
    for (x, y, k) in _ballista_overlays():
        xx, yy = _xform(x, y, d)
        px(img, xx, yy, OVERLAY[k])
    outline(img, C.outline)
    return img


# ---------------------------------------------------------------- export
def generate():
    out = []
    for unit in UNITS:
        for d in DIRS:
            for f in range(4):
                img = cavalry(d, f) if unit == "cavalry" else foot(unit, d, f)
                out.append(("people/%s_%s_%d" % (unit, d, f), img,
                            CAV_ANCHOR if unit == "cavalry" else ANCHOR))
            for f in range(2):
                img = cavalry(d, atk=f) if unit == "cavalry" else foot(unit, d, atk=f)
                out.append(("people/%s_atk_%s_%d" % (unit, d, f), img,
                            CAV_ANCHOR if unit == "cavalry" else ANCHOR))
    for d in DIRS:
        out.append(("people/ballista_%s" % d, ballista(d), (8, 15)))
    return out
