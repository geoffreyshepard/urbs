"""UI art: god emblems (16x16 medallions) and advisor portraits (32x32).

  ui/god_<name>       gold-rimmed medallion, domain-colored field, symbolic
                      emblem with its own dark outline (all share the frame)
  ui/advisor_<name>   framed bust portrait, light from the top-left

Emblems are symbolic templates (code, not painted PNGs); portraits are
composed from parametric parts (head, hair, beard, headgear, dress).
"""
import math

from palette import C
from primitives import new, px, get, outline, ellipse_mask, line, rng, hline, vline, rect

G, M, W = C.gray, C.marble, C.white
TV, TR, OC, EA = C.travertine, C.roof_terracotta, C.ochre, C.earth
GR, OL, CY, SEA = C.grass, C.olive, C.cypress, C.sea
RD, TY, GOLD, SK = C.roman_red, C.tyrian, C.gold, C.skin
OUT = C.outline
CLEAR = (0, 0, 0, 0)

GODS = ["jupiter", "juno", "mars", "venus", "ceres", "neptune", "mercury", "minerva",
        "vulcan", "vesta", "isis", "serapis", "divus"]
ADVISORS = ["engineer", "quaestor", "haruspex", "legate", "aedile"]

# ---------------------------------------------------------------- emblems
# 10x10 templates placed at (3, 3) inside the medallion field.
EMBLEMS = {
    "jupiter": (  # thunderbolt
        ["......YY..",
         ".....YW...",
         "....YW....",
         "...YWYYY..",
         "..YYYYW...",
         "....YW....",
         "...YW.....",
         "..YW......",
         ".YY.......",
         ".........."], dict(Y=GOLD, W=W), SEA[1]),
    "juno": (  # peacock feather eye
        ["...gggg...",
         "..gaaaag..",
         ".gaYYYYag.",
         ".gaYbbYag.",
         ".gaYbbYag.",
         "..gaYYag..",
         "...gaag...",
         "....ss....",
         "....s.....",
         "....s....."], dict(g=GR[2], a=SEA[3], Y=GOLD, b=SEA[0], s=OC[3]), TY[0]),
    "mars": (  # spear behind a round shield
        [".........L",
         "........LW",
         ".......s..",
         "..rrrrr...",
         ".rRRRRRr..",
         ".rRRYRRr..",
         ".rRYYYRr..",
         ".rRRYRRr..",
         "..rrrrr...",
         ".s........"], dict(L=G[3], W=W, s=EA[3], r=GOLD, R=RD[2], Y=OC[3]), RD[0]),
    "venus": (  # scallop shell
        ["..........",
         "...aBaBa..",
         "..aBaBaBa.",
         ".aBaBaBaBa",
         ".aBaBaBaBa",
         "..aBaBaBa.",
         "...aBaBa..",
         "....bbb...",
         "...bbbbb..",
         ".........."], dict(a=M[1], B=TR[3], b=TR[2]), SEA[2]),
    "ceres": (  # wheat sheaf
        ["..Y..Y..Y.",
         ".YoY.oY.Yo",
         "..Yo.Yo.Y.",
         ".YoY.oY.Yo",
         "..s..s..s.",
         "...s.s.s..",
         "....eee...",
         "...s.s.s..",
         "..s..s..s.",
         ".........."], dict(Y=GOLD, o=OC[1], s=OC[2], e=EA[1]), GR[0]),
    "neptune": (  # trident
        [".W..W..W..",
         ".LW.LW.LW.",
         ".L..L..L..",
         ".L..L..L..",
         ".LLLLLLL..",
         "....L.....",
         "....L.....",
         "....L.....",
         "....L.....",
         "....L....."], dict(L=G[3], W=W), SEA[0]),
    "mercury": (  # caduceus
        [".WW.Y.WW..",
         "..WWYWW...",
         "....Y.....",
         "...gYg....",
         "..g.Y.g...",
         "...gYg....",
         "..g.Y.g...",
         "...gYg....",
         "....Y.....",
         "....Y....."], dict(W=M[1], Y=GOLD, g=GR[2]), OL[0]),
    "minerva": (  # owl
        [".E......E.",
         ".EbbbbbbE.",
         ".bWWbbWWb.",
         ".bWkbbWkb.",
         ".bbbYYbbb.",
         ".bLbbbbLb.",
         ".bLLbbLLb.",
         "..bLLLLb..",
         "...bbbb...",
         "...Y..Y..."], dict(E=EA[1], b=EA[2], L=OC[2], W=GOLD, k=OUT, Y=OC[3]), G[0]),
    "vulcan": (  # hammer over anvil, sparks
        ["...LLLLW..",
         "...LLLLW..",
         ".....s....",
         ".y...s.y..",
         "...y.s....",
         "..AAAAAAAA",
         "...aAAAAa.",
         ".....Aa...",
         "....AAAa..",
         "...aaaaaa."], dict(L=G[3], W=W, s=EA[3], y=GOLD, A=G[2], a=G[1]), EA[0]),
    "vesta": (  # hearth flame in a bowl
        ["....y.....",
         "...yry....",
         "...yrry...",
         "..yrWrry..",
         "..yrWWry..",
         "...rrrr...",
         ".AAAAAAAA.",
         "..aAAAAa..",
         "....Aa....",
         "...AAAa..."], dict(y=TR[3], r=RD[2], W=GOLD, A=OC[1], a=OC[0]), TR[0]),
    "isis": (  # tyet / knot of Isis
        ["...RRR....",
         "..R...R...",
         "..R...R...",
         "...R.R....",
         ".RRRRRRR..",
         ".R..R..R..",
         "....R.....",
         "...RRR....",
         "...RrR....",
         "...RRR...."], dict(R=RD[2], r=TR[3]), CY[1]),
    "serapis": (  # modius (grain measure) with ears
        [".Y..Y..Y..",
         "..Y.Y.Y...",
         ".LLLLLLL..",
         ".LMMMMMD..",
         ".LMoMoMD..",
         "..LMMMD...",
         "..LMoMD...",
         "..LMMMD...",
         "...LMD....",
         ".........."], dict(Y=OC[3], L=GOLD, M=OC[1], D=OC[0], o=RD[2]), EA[1]),
    "divus": (  # laurel wreath with ribbon
        ["...g..g...",
         "..gG..Gg..",
         ".gG....Gg.",
         ".G......G.",
         "gG......Gg",
         ".G......G.",
         ".gG....Gg.",
         "..gGrrGg..",
         "...r..r...",
         "..r....r.."], dict(g=GR[1], G=GR[3], r=RD[2]), TY[1]),
}


def god_icon(name):
    rows, cmap, field = EMBLEMS[name]
    img = new(16, 16)
    cx = cy = 8.0
    inner = set()
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
            if d > 7.0:
                continue
            if d > 5.8:
                # gold rim lit from the top-left
                s = (x + 0.5 - cx) + (y + 0.5 - cy)
                c = OC[3] if s < -4 else (OC[0] if s > 4 else GOLD)
                px(img, x, y, c)
            else:
                inner.add((x, y))
                px(img, x, y, field)
    # subtle shading of the field: shadow along the lower-right inside the rim
    for (x, y) in inner:
        d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
        if d > 4.9 and (x + y) > 17 and (x + y) % 2 == 0:
            px(img, x, y, OUT)
    outline(img, OUT)
    # emblem on its own layer, outlined inside the field only
    em = new(16, 16)
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch != ".":
                px(em, 3 + i, 3 + j, cmap[ch])
    ring = em.copy()
    outline(ring, OUT)
    for y in range(16):
        for x in range(16):
            if ring.getpixel((x, y))[3] and not em.getpixel((x, y))[3] and (x, y) in inner:
                px(img, x, y, OUT)
            if em.getpixel((x, y))[3]:
                px(img, x, y, em.getpixel((x, y)))
    return img


# ---------------------------------------------------------------- portraits
ADV = {
    "engineer": dict(bg=EA[1], hair=EA[0], hair_style="curly", beard="short", age=1,
                     dress="tunic", prop="groma"),
    "quaestor": dict(bg=SEA[1], hair=EA[1], hair_style="fringe", beard=None, age=0,
                     dress="toga", prop="scroll"),
    "haruspex": dict(bg=TY[0], hair=G[3], hair_style="none", beard="full", age=2,
                     dress="cloak", hat="cone", prop="liver"),
    "legate": dict(bg=RD[0], hair=EA[0], hair_style="none", beard=None, age=1,
                   dress="armor", hat="helmet", wide=True),
    "aedile": dict(bg=OL[0], hair=G[2], hair_style="bald", beard=None, age=2,
                   dress="praetexta", prop=None),
}


def _frame(img, bg):
    rect(img, 0, 0, 32, 32, bg)
    # faint vertical dither toward the bottom to give the backdrop depth
    for y in range(20, 31):
        for x in range(32):
            if (x + y) % 2 == 0 and y > 25:
                px(img, x, y, OUT)
    for i in range(32):
        for (x, y) in ((i, 0), (i, 31), (0, i), (31, i)):
            px(img, x, y, OUT)
    for i in range(1, 31):
        px(img, i, 1, OC[3])
        px(img, 1, i, OC[3])
        px(img, i, 30, OC[0])
        px(img, 30, i, OC[0])
    px(img, 30, 1, GOLD)
    px(img, 1, 30, GOLD)


def _shade(x, y, cx, cy, rx, ry):
    nx = (x + 0.5 - cx) / rx
    ny = (y + 0.5 - cy) / ry
    return -(nx * 0.75 + ny * 0.55)


def _head(layer, p):
    cx, cy = 16.0, 14.0
    rx = 6.4 if p.get("wide") else 5.9
    ry = 7.2
    head = ellipse_mask(cx, cy, rx, ry)
    # neck
    for y in range(19, 25):
        for x in range(13, 19):
            px(layer, x, y, SK[1] if x < 17 else SK[0])
    for (x, y) in head:
        b = _shade(x, y, cx, cy, rx, ry)
        c = SK[2] if b > 0.25 else (SK[1] if b > -0.45 else SK[0])
        px(layer, x, y, c)
    # ears
    for y in (13, 14, 15):
        px(layer, int(cx - rx) - 1, y, SK[1])
        px(layer, int(cx + rx) + 1, y, SK[0])
    # brows, eyes, nose, mouth
    hc = p["hair"] if p["hair_style"] not in ("none", "bald") else EA[1]
    if p["age"] >= 2:
        hc = G[2]
    hline(layer, 12, 14, 12, hc)
    hline(layer, 17, 19, 12, hc)
    px(layer, 12, 13, M[1])
    px(layer, 13, 13, OUT)
    px(layer, 18, 13, M[1])
    px(layer, 19, 13, OUT)
    px(layer, 14, 13, SK[1])
    for y in range(13, 17):
        px(layer, 16, y, SK[0])
    px(layer, 15, 14, SK[2])
    px(layer, 15, 16, SK[0])
    px(layer, 17, 16, SK[0])
    hline(layer, 14, 17, 18, RD[0])
    px(layer, 15, 19, SK[2])
    if p["age"] >= 1:
        px(layer, 13, 17, SK[0])
        px(layer, 18, 17, SK[0])
    if p["age"] >= 2:
        hline(layer, 13, 18, 9, SK[0])
        px(layer, 11, 14, SK[0])
        px(layer, 20, 14, SK[0])
        px(layer, 12, 15, SK[1])
    return cx, cy, rx, ry


def _hair(layer, p, cx, cy, rx, ry):
    style, hc = p["hair_style"], p["hair"]
    r = rng("adv/hair/" + style)
    dark = OUT if hc in (EA[0],) else (G[1] if hc in (G[2], G[3]) else EA[1])
    if style in ("short", "curly", "fringe"):
        for (x, y) in ellipse_mask(cx, cy - 1.2, rx + 0.8, ry):
            if y <= 10 or (y <= 13 and (x < cx - rx + 1.2 or x > cx + rx - 1.2)):
                if style == "fringe" and 10 <= y <= 11 and 12 <= x <= 19:
                    c = hc if x % 2 else dark
                else:
                    c = hc
                if x > cx + 2:
                    c = dark if (x + y) % 3 == 0 else hc
                px(layer, x, y, c)
        if style == "curly":
            for _ in range(14):
                x = r.randrange(int(cx - rx), int(cx + rx) + 1)
                y = r.randrange(int(cy - ry) - 1, 11)
                if get(layer, x, y)[3]:
                    px(layer, x, y, dark)
            for x in range(int(cx - rx) - 1, int(cx + rx) + 2, 2):
                y = int(cy - ry) - 1
                while not get(layer, x, y + 1)[3] and y < 12:
                    y += 1
                px(layer, x, y, hc)
        if style == "fringe":
            hline(layer, 11, 20, 10, hc)
            for x in range(11, 21, 2):
                px(layer, x, 11, hc)
    elif style == "bald":
        for y in range(10, 16):
            px(layer, int(cx - rx), y, hc)
            px(layer, int(cx - rx) + 1, y, hc if y > 11 else get(layer, int(cx - rx) + 1, y))
            px(layer, int(cx + rx), y, G[1])
            px(layer, int(cx + rx) - 1, y, hc if y > 11 else get(layer, int(cx + rx) - 1, y))
        px(layer, 13, 8, SK[2])
        px(layer, 14, 8, W)


def _beard(layer, p):
    kind = p.get("beard")
    if not kind:
        return
    hc = p["hair"]
    if kind == "stubble":
        for y in range(16, 21):
            for x in range(11, 21):
                if get(layer, x, y)[3] and (x + y) % 2 == 0 and not (14 <= x <= 17 and y == 18):
                    if get(layer, x, y) in (SK[1], SK[0], SK[2]):
                        px(layer, x, y, SK[0])
        return
    top = 16 if kind == "short" else 15
    for y in range(top, 23 if kind == "full" else 21):
        for x in range(10, 23):
            if not get(layer, x, y)[3] and y < 20:
                continue
            inner = 14 <= x <= 17 and 17 <= y <= 18
            if inner:
                continue
            if kind == "full" or y >= 17 or x < 12 or x > 19:
                if kind == "short" and (y > 20 or (y >= 19 and (x < 12 or x > 19))):
                    continue
                if kind == "full" and y >= 20 and abs(x - 16) > 5 - (y - 20) * 2:
                    continue
                if kind == "short" and y >= 20 and abs(x - 16) > 3:
                    continue
                c = hc if (x < 17 or (x + y) % 2) else (G[2] if hc == G[3] else OUT)
                px(layer, x, y, c)
    hline(layer, 14, 17, 18, RD[0])
    if kind == "full":
        hline(layer, 13, 18, 16, hc)


def _hat(layer, p, cx, cy, rx, ry):
    hat = p.get("hat")
    if hat == "helmet":
        for (x, y) in ellipse_mask(cx, cy - 1, rx + 1.2, ry + 0.5):
            if y <= 11:
                b = _shade(x, y, cx, cy - 1, rx + 1.2, ry)
                px(layer, x, y, G[3] if b > 0.3 else (G[2] if b > -0.3 else G[1]))
        hline(layer, int(cx - rx) - 1, int(cx + rx) + 1, 11, GOLD)
        px(layer, int(cx + rx) + 1, 11, OC[0])
        for y in range(12, 19):                     # cheek guards
            for x in (int(cx - rx) - 1, int(cx - rx)):
                px(layer, x, y, G[3] if x == int(cx - rx) - 1 else G[2])
            for x in (int(cx + rx), int(cx + rx) + 1):
                px(layer, x, y, G[1])
        # transverse crest
        for x in range(8, 25):
            top = 2 if 11 <= x <= 21 else 3
            for y in range(top, 6):
                c = RD[2] if (y == top or x < 12) else RD[1]
                px(layer, x, y, c)
        for x in range(9, 25, 2):
            px(layer, x, 4, RD[0])
        px(layer, 15, 6, GOLD)
        px(layer, 16, 6, GOLD)
    elif hat == "cone":
        # Etruscan tutulus: tall conical cap, tied under the chin
        for y in range(1, 11):
            t = (y - 1) / 9.0
            hw = 0.8 + 6.6 * t
            for x in range(int(cx - hw), int(cx + hw) + 1):
                if abs(x + 0.5 - cx) <= hw + 0.3:
                    nx = (x + 0.5 - cx) / max(hw, 1)
                    c = TV[2] if nx < -0.35 else (TV[1] if nx < 0.4 else TV[0])
                    px(layer, x, y, c)
        hline(layer, 9, 22, 10, GOLD)
        hline(layer, 9, 22, 11, OC[0])
        px(layer, 16, 0, GOLD)
        line(layer, 10, 12, 11, 18, OC[0])
        line(layer, 21, 12, 20, 18, OC[0])


def _dress(layer, p):
    kind = p["dress"]
    r = rng("adv/dress/" + kind)
    sh = ellipse_mask(16, 33, 14.5, 9.5)
    sh = {(x, y) for (x, y) in sh if 2 <= x <= 29 and y <= 29}
    if kind == "tunic":
        ramp = (EA[2], EA[3], OC[1])
    elif kind in ("toga", "praetexta"):
        ramp = (M[0], M[1], W)
    elif kind == "cloak":
        ramp = (EA[0], RD[0], TR[0])
    else:
        ramp = (G[1], G[2], G[3])
    for (x, y) in sh:
        b = _shade(x, y, 16, 33, 14.5, 9.5)
        c = ramp[2] if b > 0.15 else (ramp[1] if b > -0.35 else ramp[0])
        px(layer, x, y, c)
    if kind == "tunic":
        hline(layer, 12, 19, 24, EA[1])                     # neckline
        line(layer, 8, 25, 22, 29, EA[1])                   # tool strap
        line(layer, 8, 24, 22, 28, EA[0])
    elif kind in ("toga", "praetexta"):
        # sinus folds sweeping over the left shoulder
        for k in range(3):
            line(layer, 6 + k * 4, 29, 20 + k * 3, 23 + k, M[0])
        if kind == "praetexta":
            line(layer, 9, 24, 21, 29, TY[1])
            line(layer, 9, 25, 20, 29, TY[1])
            line(layer, 10, 23, 22, 29, TY[0])
    elif kind == "cloak":
        for x in range(4, 28, 2):                           # fringe
            px(layer, x, 29, OC[2])
        for (x, y) in ellipse_mask(16, 25, 1.6, 1.4):       # fibula
            px(layer, x, y, GOLD)
        px(layer, 15, 24, OC[3])
    elif kind == "armor":
        # bronze muscle cuirass with a red paludamentum over the left shoulder
        for (x, y) in sh:
            if 9 <= x <= 22:
                b = _shade(x, y, 16, 33, 14.5, 9.5)
                px(layer, x, y, OC[3] if b > 0.25 else (GOLD if b > -0.25 else OC[0]))
        line(layer, 12, 26, 15, 27, OC[0])
        line(layer, 20, 26, 17, 27, OC[0])
        for (x, y) in sh:
            if x <= 9 or (x >= 23):
                b = _shade(x, y, 16, 33, 14.5, 9.5)
                px(layer, x, y, RD[2] if b > 0.2 else (RD[1] if b > -0.3 else RD[0]))
        for (x, y) in ellipse_mask(9.5, 24.5, 1.5, 1.4):
            px(layer, x, y, GOLD)


def _prop(layer, p):
    prop = p.get("prop")
    if prop == "groma":
        vline(layer, 4, 6, 29, EA[2])
        hline(layer, 2, 9, 6, EA[3])
        for x in (2, 9):
            vline(layer, x, 7, 10, G[2])
            px(layer, x, 11, G[1])
    elif prop == "scroll":
        for y in range(22, 30):
            hline(layer, 23, 27, y, OC[3] if y % 3 else OC[2])
        hline(layer, 22, 28, 21, EA[1])
        hline(layer, 22, 28, 29, EA[1])
        for y in (24, 26):
            hline(layer, 24, 26, y, EA[2])
    elif prop == "liver":
        # bronze model liver used for divination
        for (x, y) in ellipse_mask(25, 26, 3.4, 2.4):
            px(layer, x, y, GOLD if x < 25 else OC[0])
        line(layer, 23, 25, 27, 27, OC[0])


def advisor(name):
    p = ADV[name]
    img = new(32, 32)
    _frame(img, p["bg"])
    fig = new(32, 32)
    _dress(fig, p)
    cx, cy, rx, ry = _head(fig, p)
    _hair(fig, p, cx, cy, rx, ry)
    _beard(fig, p)
    _hat(fig, p, cx, cy, rx, ry)
    outline(fig, OUT)
    _prop(fig, p)
    # keep the figure inside the frame
    for y in range(32):
        for x in range(32):
            if 2 <= x <= 29 and 2 <= y <= 29 and fig.getpixel((x, y))[3]:
                img.putpixel((x, y), fig.getpixel((x, y)))
    return img


def generate():
    out = []
    for g in GODS:
        out.append(("ui/god_%s" % g, god_icon(g), None))
    for a in ADVISORS:
        out.append(("ui/advisor_%s" % a, advisor(a), None))
    return out
