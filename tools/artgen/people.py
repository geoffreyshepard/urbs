"""Citizens: body template x clothing palette x direction x frame.

Canvas 8x12, figure inside cols 1..6 / rows 1..10 (6x10 body), 1px dark
outline around it.  Anchor = feet bottom-center (4, 11).

Templates are tiny symbolic grids (code, not painted PNGs).  West-facing
sprites are mirrored east-facing ones.
  H hair       S skin lit   s skin shadow
  T cloth mid  L cloth light   D cloth dark   P accent (stripe, crest, cap)
  A armor lit  a armor dark    B belt         F footwear     . empty
"""
from palette import C
from primitives import new, px, outline

CLASSES = ["patrician", "eques", "pleb", "liberti", "slave", "soldier"]
DIRS = ["n", "e", "s", "w"]
ANCHOR = (4, 11)

HEAD = {
    "s": [".HHHH.",
          ".HSSs.",
          ".SSSs."],
    "n": [".HHHH.",
          ".HHHH.",
          "..sS.."],
    "e": ["..HHH.",
          ".HHSS.",
          "..HSs."],
}
HEADGEAR = {
    # helmet (galea) with red crest; cap (pileus) of freedmen
    "helmet": {"s": ["..PP..", ".HHHH.", ".HSsH."],
               "n": ["..PP..", ".HHHH.", ".HHHH."],
               "e": [".PP...", ".HHHH.", "..HSs."]},
    "cap":    {"s": ["..PP..", ".PPPP.", ".SSSs."],
               "n": ["..PP..", ".PPPP.", "..sS.."],
               "e": ["..PP..", ".PPPS.", "..HSs."]},
}

# torso + skirt, rows 3..7
BODY = {
    "tunic": {
        "s": ["LTTTTD",
              "SLTTDs",
              "SLBBDs",
              ".LTTD.",
              ".LTTD."],
        "n": ["LTTTTD",
              "STTTDs",
              "STBBDs",
              ".TTTD.",
              ".TTDD."],
        "e": [".LTTD.",
              ".LTSD.",
              ".LBBD.",
              ".LTTD.",
              ".LTD.."],
    },
    "striped": {   # equites: two narrow vertical stripes (angusticlavi)
        "s": ["LPTTPD",
              "SPTTPs",
              "SLBBDs",
              ".LTTD.",
              ".LTTD."],
        "n": ["LPTTPD",
              "SPTTPs",
              "STBBDs",
              ".TTTD.",
              ".TTDD."],
        "e": [".LPTD.",
              ".LPSD.",
              ".LBBD.",
              ".LTTD.",
              ".LTD.."],
    },
    "toga": {      # long white toga, purple border running diagonally
        "s": ["LLTTTD",
              "LPTTDs",
              "LTPTDs",
              ".LTPD.",
              ".LTTP."],
        "n": ["LTTTPD",
              "LTTPDs",
              "LTPTDs",
              ".PTTD.",
              ".LTTD."],
        "e": [".LTTD.",
              ".PTSD.",
              ".LPTD.",
              ".LTPD.",
              ".LTTD."],
    },
    "armor": {
        "s": ["AAAAAa",
              "SAaAas",
              "SBBBBs",
              ".TDTD.",
              ".LTTD."],
        "n": ["AAAAAa",
              "SAAAas",
              "SBBBBs",
              ".TDTD.",
              ".TTTD."],
        "e": [".AAAa.",
              ".AASa.",
              ".BBBB.",
              ".TDTD.",
              ".LTD.."],
    },
}

# legs (rows 8..9) per frame
LEGS = {
    # front/back walk: 0 stand, 1 left foot up, 2 stand, 3 right foot up
    "s": [[".S..s.", ".F..F."],
          [".F..s.", "....F."],
          [".S..s.", ".F..F."],
          [".S..F.", ".F...."]],
    # side walk: 0 stride, 1 passing, 2 stride (other leg front), 3 passing
    "e": [["..S.s.", ".F...F"],
          ["..Ss..", "..FF.."],
          ["..s.S.", ".F...F"],
          ["..sS..", "..FF.."]],
}
IDLE_LEGS = {"s": [".S..s.", ".F..F."], "e": ["..Ss..", "..FF.."]}
TOGA_HEM = {"s": ".LTTD.", "e": ".LTTD."}

OUTFITS = {
    "patrician": dict(style="toga", T=C.marble[1], L=C.white, D=C.marble[0], P=C.tyrian[1],
                      B=C.marble[0], H=C.earth[1], F=C.earth[1]),
    "eques": dict(style="striped", T=C.marble[0], L=C.marble[1], D=C.gray[3], P=C.tyrian[1],
                  B=C.earth[2], H=C.earth[0], F=C.earth[1]),
    "pleb": dict(style="tunic", T=C.earth[3], L=C.ochre[1], D=C.earth[2], B=C.earth[1],
                 H=C.earth[0], F=C.earth[1]),
    "liberti": dict(style="tunic", T=C.olive[1], L=C.olive[2], D=C.olive[0], B=C.sea[1],
                    H=C.earth[0], P=C.travertine[2], F=C.earth[1], gear="cap"),
    "slave": dict(style="tunic", T=C.gray[2], L=C.gray[3], D=C.gray[1], B=C.gray[1],
                  H=C.gray[0], F=C.gray[1]),
    "soldier": dict(style="armor", T=C.roman_red[1], L=C.roman_red[2], D=C.roman_red[0],
                    A=C.gray[3], a=C.gray[1], B=C.earth[1], H=C.gray[2], P=C.roman_red[2],
                    F=C.earth[0], gear="helmet"),
    "child": dict(style="tunic", T=C.ochre[1], L=C.ochre[2], D=C.ochre[0], B=C.earth[2],
                  H=C.earth[1], F=C.earth[1]),
}

# child: 4x7 figure
CHILD = {
    "s": ["HHHH", "HSSs", "LTTD", "LBBD", "LTTD"],
    "n": ["HHHH", "HHHH", "LTTD", "TBBD", "TTTD"],
    "e": [".HHH", "HHSS", "LTSD", "LBBD", "LTD."],
}
CHILD_LEGS = {
    "s": [["S..s", "F..F"], ["F..s", "...F"], ["S..s", "F..F"], ["S..F", "F..."]],
    "e": [["S.s.", "F..F"], [".Ss.", ".FF."], ["s.S.", "F..F"], [".sS.", ".FF."]],
}
CHILD_IDLE = {"s": ["S..s", "F..F"], "e": [".Ss.", ".FF."]}


def _color(o, ch, face):
    lit = face == "s"
    if ch == "S":
        return C.skin[2] if lit else C.skin[1]
    if ch == "s":
        return C.skin[1] if lit else C.skin[0]
    return o.get(ch, o["T"])


def _grid(o, d, frame, idle):
    face = "e" if d in ("e", "w") else d
    lk = "e" if face == "e" else "s"
    gear = o.get("gear")
    head = HEADGEAR[gear][face] if gear else HEAD[face]
    body = BODY[o["style"]][face]
    legs = IDLE_LEGS[lk] if idle else LEGS[lk][frame]
    if o["style"] == "toga":
        legs = [TOGA_HEM[lk], legs[1]]
    grid = list(head) + list(body) + list(legs)
    if d == "w":
        grid = [g[::-1] for g in grid]
    return grid, face


def _child_grid(d, frame, idle):
    face = "e" if d in ("e", "w") else d
    lk = "e" if face == "e" else "s"
    legs = CHILD_IDLE[lk] if idle else CHILD_LEGS[lk][frame]
    grid = list(CHILD[face]) + list(legs)
    if d == "w":
        grid = [g[::-1] for g in grid]
    return grid, face


def _draw(o, grid, face, x0, y0, bob=0):
    img = new(8, 12)
    n = len(grid)
    for j, row in enumerate(grid):
        for i, ch in enumerate(row):
            if ch == ".":
                continue
            yy = y0 + j - (bob if j < n - 2 else 0)
            px(img, x0 + i, yy, _color(o, ch, face))
    outline(img, C.outline)
    return img


def person(k, d, frame=0, idle=False):
    o = OUTFITS[k]
    grid, face = _grid(o, d, frame, idle)
    return _draw(o, grid, face, 1, 1)


def child(d, frame=0, idle=False):
    grid, face = _child_grid(d, frame, idle)
    return _draw(OUTFITS["child"], grid, face, 2, 4)


def generate():
    out = []
    for k in CLASSES:
        for d in DIRS:
            for f in range(4):
                out.append(("people/%s_%s_%d" % (k, d, f), person(k, d, f), ANCHOR))
            out.append(("people/%s_idle_%s" % (k, d), person(k, d, idle=True), ANCHOR))
    for d in DIRS:
        for f in range(4):
            out.append(("people/child_%s_%d" % (d, f), child(d, f), ANCHOR))
        out.append(("people/child_idle_%s" % d, child(d, idle=True), ANCHOR))
    return out
