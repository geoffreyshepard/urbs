"""Goods icons.

  goods/<id>      7x7  carried-goods icon (drawn above a porter's head)
  goods/<id>_16   16x16 UI icon

Both sizes are designed separately (the 7x7 ones are never downscaled).
7x7 icons are symbolic 5x5 templates (code, not painted PNGs) placed at
(1, 1) and outlined, so the dark outline stays inside the 7x7 canvas.
16x16 icons are built from parametric families (lathe-turned vessels,
mounds, sacks, ingots, blocks, rock piles, logs) plus a few bespoke shapes.
Each separate object is outlined on its own layer, so stacked objects keep a
dark line between them.  Light comes from the top-left.  Anchor (0, 0).
"""
import math

from palette import C
from primitives import (new, px, get, outline, ellipse_mask, line, rng, hline,
                        vline, rect, shade_blob)

GOODS = ["wheat", "vegetables", "fish", "meat", "olives", "grapes", "flour", "bread",
         "olive_oil", "wine", "salt", "garum", "wood", "planks", "clay", "bricks",
         "stone", "cut_stone", "limestone", "lime", "pozzolana", "concrete", "marble",
         "sand", "iron_ore", "iron", "silver_ore", "silver", "gold", "tools", "weapons",
         "armor", "pottery", "amphorae", "wool", "clothing", "dye", "dyed_clothing",
         "furniture", "glassware", "papyrus", "scrolls", "jewelry", "statues", "denarii"]

G, M, W = C.gray, C.marble, C.white
TV, TR, OC, EA = C.travertine, C.roof_terracotta, C.ochre, C.earth
GR, OL, CY, SEA = C.grass, C.olive, C.cypress, C.sea
RD, TY, GOLD = C.roman_red, C.tyrian, C.gold
OUT = C.outline

# ramps used for metals and stones (dark -> light)
R_IRON = (G[0], G[1], G[2], G[3])
R_SILVER = (G[2], G[3], M[1], W)
R_GOLD = (OC[0], OC[1], GOLD, OC[3])
R_TERRA = (TR[0], TR[1], TR[2], TR[3])
R_WOOD = (EA[1], EA[2], EA[3], OC[1])


# ======================================================================
# 7x7 carried icons: 5x5 templates.  L/M/D = light/mid/dark of the main
# ramp; other letters are per-good accents.
# ======================================================================
SMALL = {
    "wheat": (["L.L.L",
               "LMLML",
               ".MMM.",
               ".ddd.",
               ".M.M."], dict(L=GOLD, M=OC[1], d=EA[1])),
    "vegetables": (["....o",
                    ".gGoo",
                    "gGgGo",
                    "bBbBb",
                    ".bBb."], dict(g=GR[1], G=GR[3], o=TR[3], b=EA[3], B=EA[2])),
    "fish": ([".....",
              "D.LL.",
              "DMMeM",
              "D.MM.",
              "....."], dict(L=SEA[3], M=M[1], D=SEA[2], e=OUT)),
    "meat": (["..RR.",
              ".RLRR",
              "RRRRD",
              ".RDD.",
              "ab..."], dict(R=RD[1], L=TR[3], D=RD[0], a=W, b=M[0])),
    "olives": (["..l..",
                ".oOo.",
                "oOoOo",
                "bBBBd",
                ".ddd."], dict(o=OL[2], O=CY[0], l=GR[2], b=TR[3], B=TR[2], d=TR[1])),
    "grapes": (["..gG.",
                ".PhP.",
                "PPPPD",
                ".PPD.",
                "..D.."], dict(P=TY[1], D=TY[0], h=G[3], g=EA[2], G=GR[2])),
    "flour": ([".www.",
               "LMMMD",
               "LMaMD",
               "LMMMD",
               ".DDD."], dict(w=W, L=TV[2], M=TV[1], D=TV[0], a=EA[2])),
    "bread": (["....,",
               ".LLL.",
               "LsLsM",
               "MMMMD",
               ".DDD."], {",": None, "L": OC[2], "M": OC[1], "s": EA[2], "D": EA[2]}),
    "olive_oil": (["..a..",
                   ".LMD.",
                   "LLMMD",
                   "LMMDD",
                   ".DDD."], dict(a=EA[2], L=GOLD, M=OL[2], D=OL[1])),
    "wine": ([".pMp.",
              ".LMD.",
              "LLMDD",
              ".LMD.",
              "..D.."], dict(p=TY[1], L=TY[1], M=TY[1], D=TY[0])),
    "garum": (["..a..",
               ".aaa.",
               "LMfMD",
               "LMMDD",
               ".DDD."], dict(a=G[3], L=EA[3], M=EA[2], D=EA[1], f=M[1])),
    "salt": (["..W..",
              ".WLs.",
              "WLMLM",
              "LMMMD",
              "....."], dict(W=W, L=M[1], M=M[0], D=G[3], s=SEA[4])),
    "wood": ([".....",
              "MMMMe",
              "DDDDf",
              "MMMMe",
              "DDDDf"], dict(M=EA[2], D=EA[1], e=OC[2], f=OC[1])),
    "planks": ([".....",
                "LLLLa",
                "MMMMb",
                "LLLLa",
                "MMMMb"], dict(L=OC[3], M=OC[1], a=OC[2], b=OC[0])),
    "clay": ([".....",
              ".hMM.",
              "LMMMD",
              "MMMDD",
              ".DDD."], dict(h=TR[3], L=TR[2], M=TR[1], D=TR[0])),
    "bricks": ([".....",
                "LLjLL",
                "jjjjj",
                "MjMMM",
                "DDjDD"], dict(L=TR[3], M=TR[2], D=TR[1], j=TV[1])),
    "stone": ([".....",
               ".LM..",
               "LMMML",
               "MMDMD",
               ".DD.."], dict(L=G[3], M=G[2], D=G[1])),
    "cut_stone": ([".....",
                   "LLLLM",
                   "MMMMD",
                   "MMMMD",
                   "DDDDD"], dict(L=G[3], M=G[2], D=G[1])),
    "limestone": ([".....",
                   "..LL.",
                   ".LMpD",
                   "LMpMD",
                   "MMDD."], dict(L=TV[2], M=TV[1], D=TV[0], p=OC[0])),
    "lime": ([".....",
              ".www.",
              "bBBBc",
              "gggggg"[:5],
              ".bBc."], dict(w=W, b=EA[3], B=EA[2], c=EA[1], g=G[1])),
    "pozzolana": ([".....",
                   "..L..",
                   ".LMs.",
                   "LMsMD",
                   "MMMDD"], dict(L=G[2], M=TY[0], s=G[1], D=G[0])),
    "concrete": ([".....",
                  ".....",
                  "gGgGg",
                  "wWWWv",
                  ".wWv."], dict(g=G[2], G=G[1], w=EA[3], W=EA[2], v=EA[1])),
    "marble": (["LLLLM",
                "MvMMD",
                "MMvMD",
                "MMMvD",
                "....."], dict(L=W, M=M[1], D=M[0], v=G[2])),
    "sand": ([".....",
              "..L..",
              ".LMs.",
              "LMsMD",
              "MMMDD"], dict(L=OC[3], M=OC[2], s=OC[1], D=OC[1])),
    "iron_ore": ([".....",
                  ".Mr..",
                  "MDMrM",
                  "DMrDD",
                  ".DD.."], dict(M=G[1], D=G[0], r=TR[1])),
    "iron": ([".....",
              ".LLL.",
              "LMMMD",
              "MMMDD",
              "....."], dict(L=G[2], M=G[1], D=G[0])),
    "silver_ore": ([".....",
                    ".Ms..",
                    "MDMsM",
                    "DMsDD",
                    ".DD.."], dict(M=G[1], D=G[0], s=W)),
    "silver": ([".....",
                ".LLL.",
                "LMMMD",
                "MMMDD",
                "....."], dict(L=W, M=G[3], D=G[2])),
    "gold": ([".....",
              ".LLL.",
              "LMMMD",
              "MMMDD",
              "....."], dict(L=OC[3], M=GOLD, D=OC[0])),
    "tools": ([".LLM.",
               ".MMD.",
               "..h..",
               "..h..",
               "..h.."], dict(L=G[3], M=G[2], D=G[1], h=EA[2])),
    "weapons": (["....L",
                 "...L.",
                 ".gL..",
                 ".hg..",
                 "h...."], dict(L=G[3], g=GOLD, h=EA[1])),
    "armor": ([".rrr.",
               ".LMM.",
               "LLMMD",
               "L...D",
               "L...D"], dict(r=RD[2], L=G[3], M=G[2], D=G[1])),
    "pottery": ([".....",
                 "LMMMD",
                 "kkkkk",
                 ".MMD.",
                 "..D.."], dict(L=TR[3], M=TR[2], D=TR[1], k=OUT)),
    "amphorae": ([".hMh.",
                  ".LMD.",
                  "LLMDD",
                  ".LMD.",
                  "..D.."], dict(h=TR[1], L=TR[3], M=TR[2], D=TR[1])),
    "wool": ([".....",
              ".WLW.",
              "WLWLM",
              "LWLMM",
              ".MMM."], dict(W=M[1], L=TV[2], M=TV[1])),
    "clothing": (["LL.MD",
                  "LLMMD",
                  ".LMD.",
                  ".LMD.",
                  ".LDD."], dict(L=TV[2], M=TV[1], D=TV[0])),
    "dye": ([".....",
             "ppppp",
             "LMpMD",
             ".MMD.",
             "..D.."], dict(p=TY[1], L=G[3], M=G[2], D=G[1])),
    "dyed_clothing": (["LL.MD",
                       "LLMMD",
                       ".LMD.",
                       ".LMD.",
                       ".ggg."], dict(L=RD[2], M=TY[1], D=TY[0], g=GOLD)),
    "furniture": ([".....",
                   "LLLLM",
                   "MMMMD",
                   "L...D",
                   "L...D"], dict(L=EA[3], M=EA[2], D=EA[1])),
    "glassware": (["..L..",
                   "..M..",
                   ".LMM.",
                   "LWLMM",
                   ".MMM."], dict(L=SEA[4], M=SEA[3], W=W)),
    "papyrus": (["g.g.g",
                 "GgGgG",
                 ".GGG.",
                 ".aaa.",
                 ".G.G."], dict(g=GR[3], G=GR[1], a=OC[2])),
    "scrolls": (["eeeee",
                 ".LLM.",
                 ".kLM.",
                 ".LkM.",
                 "eeeee"], dict(e=EA[1], L=OC[3], M=OC[2], k=EA[2])),
    "jewelry": ([".GrG.",
                 "L...G",
                 "L...G",
                 "L...G",
                 ".GGG."], dict(G=GOLD, L=OC[3], r=RD[2])),
    "statues": (["..L..",
                 ".LMD.",
                 "..M..",
                 ".LMD.",
                 "aaaab"], dict(L=W, M=M[1], D=M[0], a=G[3], b=G[2])),
    "denarii": ([".LLM.",
                 "LMaMD",
                 "LaaMD",
                 "LMMDD",
                 ".DDD."], dict(L=W, M=G[3], D=G[2], a=G[2])),
}


def _tpl(rows, cmap, size, ox, oy, do_outline=True):
    img = new(size, size)
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch in ". ":
                continue
            c = cmap.get(ch)
            if c is None:
                continue
            px(img, ox + i, oy + j, c)
    if do_outline:
        outline(img, OUT)
    return img


def small_icon(gid):
    rows, cmap = SMALL[gid]
    return _tpl(rows, cmap, 7, 1, 1)


# ======================================================================
# 16x16 helpers
# ======================================================================
def _ramp_i(n, b):
    return max(0, min(n - 1, int(b * n)))


def lathe(img, cx, top, hws, ramp, spec=True, dim=0.18, bias=0.0):
    """Turned vessel: one half-width per row, cylinder shading lit from the
    left, optional specular streak on the upper body."""
    n = len(ramp)
    H = len(hws)
    for j, hw in enumerate(hws):
        if hw <= 0:
            continue
        y = top + j
        for x in range(int(math.floor(cx - hw - 1)), int(math.ceil(cx + hw + 1))):
            xc = x + 0.5
            if abs(xc - cx) > hw:
                continue
            nx = (xc - cx) / hw
            b = 0.62 - 0.55 * nx - dim * j / H + bias
            i = _ramp_i(n, b)
            if spec and -0.78 < nx < -0.3 and 0.25 < j / float(H) < 0.65:
                i = n - 1
            px(img, x, y, ramp[i])


def mound(img, cx, base_y, height, half, ramp, key, grains=(), density=0.18, shape=0.85):
    """Conical heap (sand, salt, ash) lit from the top-left, with grains."""
    r = rng(key)
    n = len(ramp)
    for j in range(height):
        y = base_y - j
        t = j / float(height)
        hw = half * (1 - t) ** shape + 0.5
        for x in range(int(cx - hw - 1), int(cx + hw + 2)):
            xc = x + 0.5
            if abs(xc - cx) > hw:
                continue
            nx = (xc - cx) / max(hw, 1.0)
            b = 0.55 - 0.45 * nx + 0.25 * t
            px(img, x, y, ramp[_ramp_i(n, b)])
            if grains and r.random() < density:
                px(img, x, y, r.choice(grains))


def blob(img, cx, cy, rx, ry, ramp, key, noise=0.15):
    shade_blob(img, ellipse_mask(cx, cy, rx, ry), ramp, key=key, noise=noise,
               cx=cx, cy=cy, rx=rx, ry=ry)


def box(img, x, y, w, top_h, front_h, top, front, side, edge=None):
    """3/4 block: top face (lit), front face, darker right column."""
    for yy in range(y, y + top_h):
        for xx in range(x, x + w):
            px(img, xx, yy, top)
    for yy in range(y + top_h, y + top_h + front_h):
        for xx in range(x, x + w):
            px(img, xx, yy, front)
        px(img, x + w - 1, yy, side)
    if edge is not None:
        hline(img, x, x + w - 2, y, edge)
        vline(img, x, y, y + top_h - 1, edge)


def ingot(img, x, y, w, ramp):
    """Trapezoid bar: narrower lit top face, front face, dark ends."""
    d, m, l, h = ramp
    hline(img, x + 1, x + w - 2, y, h)
    hline(img, x + 1, x + w - 2, y + 1, l)
    for yy in (y + 2, y + 3):
        hline(img, x, x + w - 1, yy, m)
        px(img, x + w - 1, yy, d)
    hline(img, x, x + w - 1, y + 4, d)
    px(img, x + 1, y + 2, l)


def layered(parts, size=16):
    """parts: list of draw callbacks (img) -> None, back to front.  Each is
    drawn on its own layer and outlined, then composited."""
    img = new(size, size)
    for fn in parts:
        layer = new(size, size)
        fn(layer)
        outline(layer, OUT)
        img.paste(layer, (0, 0), layer)
    return img


def single(fn):
    return layered([fn])


def amphora_hws():
    return [1.5, 1.0, 1.0, 2.5, 3.5, 4.0, 4.0, 3.5, 3.0, 2.5, 2.0, 1.5, 1.0, 0.5]


def draw_amphora(img, cx, top, ramp):
    lathe(img, cx, top, amphora_hws(), ramp)
    # loop handles from the neck down to the shoulder
    neck = [x for x in range(16) if get(img, x, top + 1)[3]]
    nl, nr = min(neck), max(neck)
    d = ramp[0]
    for (x, y) in ((nl - 1, top + 1), (nl - 2, top + 1), (nl - 2, top + 2),
                   (nr + 1, top + 1), (nr + 2, top + 1), (nr + 2, top + 2)):
        px(img, x, y, d)


def rock_pile(img_parts, ramp, key, flecks=(), fleck_n=0, pebbles=True):
    """Three rough rocks (back one first)."""
    def mk(cx, cy, rx, ry, k):
        def f(img):
            blob(img, cx, cy, rx, ry, ramp, key + k, noise=0.22)
            r = rng(key + k + "f")
            pts = sorted(ellipse_mask(cx, cy, rx - 0.8, ry - 0.8))
            for _ in range(fleck_n):
                if pts and flecks:
                    x, y = r.choice(pts)
                    px(img, x, y, r.choice(flecks))
                    if r.random() < 0.5:
                        px(img, x + 1, y, r.choice(flecks))
        return f
    return [mk(8, 6.5, 3.3, 2.8, "a"), mk(4.8, 10.3, 3.6, 3.1, "b"), mk(11.2, 10.6, 3.4, 3.0, "c")]


# ======================================================================
# 16x16 icons
# ======================================================================
def i_wheat():
    """Sheaf: five ears on separate layers (so each keeps its outline),
    stalks gathered by a twine tie, spread below it."""
    def stalks(img):
        for k in (-2, -1, 0, 1, 2):
            line(img, 7 + (k > 0), 10, int(7.5 + k * 2.6), 7 if abs(k) < 2 else 8, OC[1])
            line(img, 7 + (k > 0), 11, int(7.5 + k * 1.4), 14, OC[1] if k <= 0 else OC[0])

    def ear(x, y0, h):
        def f(img):
            px(img, x, y0 - 1, OC[3])
            for i in range(h):
                px(img, x, y0 + i, GOLD if i % 2 == 0 else OC[3])
                px(img, x + 1, y0 + i, OC[1] if i % 2 == 0 else GOLD)
        return f

    def tie(img):
        hline(img, 6, 9, 10, EA[1])
        hline(img, 6, 9, 11, EA[2])
        px(img, 6, 10, EA[2])
    return layered([stalks, ear(2, 4, 4), ear(12, 4, 4), ear(4, 2, 5), ear(10, 2, 5),
                    ear(7, 1, 6), tie])


def i_vegetables():
    def contents(img):
        blob(img, 5.5, 7.0, 3.4, 3.0, (GR[0], GR[1], GR[2], GR[3]), "veg/cab")
        px(img, 5, 6, GR[0])
        px(img, 6, 7, GR[0])
        px(img, 4, 8, GR[0])

    def carrot(img):
        for i in range(5):
            px(img, 9 + i, 5 + i, TR[3])
            px(img, 10 + i, 5 + i, TR[2])
            if i < 4:
                px(img, 9 + i, 6 + i, TR[2])
        px(img, 14, 10, TR[1])
        px(img, 9, 4, GR[2])
        px(img, 8, 3, GR[3])
        px(img, 10, 3, GR[2])

    def radish(img):
        blob(img, 9.5, 8.5, 2.0, 1.8, (RD[0], RD[1], RD[2]), "veg/rad")
        px(img, 9, 7, M[1])

    def basket(img):
        for y in range(9, 15):
            inset = {9: 1, 10: 1, 11: 2, 12: 2, 13: 3, 14: 4}[y]
            for x in range(1 + inset, 15 - inset):
                c = EA[3] if (x + y) % 2 == 0 else EA[2]
                if y == 9:
                    c = OC[1] if x % 2 else EA[3]
                if x == 14 - inset:
                    c = EA[1]
                px(img, x, y, c)
    return layered([contents, carrot, radish, basket])


def i_fish():
    def f(img):
        cx, cy, rx, ry = 8.5, 8.0, 5.5, 2.8
        for (x, y) in ellipse_mask(cx, cy, rx, ry):
            dy = (y + 0.5 - cy) / ry
            if dy < -0.6:
                c = SEA[1]
            elif dy < -0.15:
                c = SEA[3]
            elif dy < 0.35:
                c = G[3]
            elif dy < 0.75:
                c = M[1]
            else:
                c = M[0]
            px(img, x, y, c)
        for x, hw in ((1, 2), (2, 1), (3, 0)):
            for y in range(8 - hw - 1, 8 + hw + 1):
                px(img, x, y, SEA[2])
        hline(img, 7, 9, 4, SEA[2])
        px(img, 8, 11, SEA[2])
        px(img, 12, 7, OUT)
        vline(img, 10, 7, 9, SEA[2])
        px(img, 5, 7, M[1])
    return single(f)


def i_meat():
    def ham(img):
        m = ellipse_mask(9.5, 6.5, 4.8, 4.3) | ellipse_mask(6.5, 10, 2.8, 2.3)
        shade_blob(img, m, (RD[0], RD[1], RD[2], TR[3]), key="meat", noise=0.1)
        for (x, y) in ellipse_mask(10.5, 5.5, 2.4, 2.0):
            px(img, x, y, TR[3])
        for (x, y) in ellipse_mask(10.5, 5.5, 1.4, 1.1):
            px(img, x, y, RD[2])

    def bone(img):
        line(img, 5, 11, 3, 13, M[1])
        line(img, 6, 11, 4, 13, M[0])
        px(img, 2, 13, W)
        px(img, 2, 14, M[1])
        px(img, 3, 14, M[0])
    return layered([bone, ham])


def i_olives():
    def bowl(img):
        lathe(img, 8, 10, [6.0, 5.5, 5.0, 4.0, 2.5], R_TERRA, spec=False)
        hline(img, 3, 12, 10, TR[3])

    def olive(cx, cy, dark):
        def f(img):
            ramp = (CY[0], G[0], G[1]) if dark else (OL[0], OL[1], OL[2])
            blob(img, cx, cy, 1.7, 1.3, ramp, "ol%d%d" % (cx, cy), noise=0.05)
        return f

    def leaf(img):
        line(img, 10, 3, 14, 1, OL[2])
        line(img, 10, 4, 13, 3, OL[1])
        line(img, 9, 4, 12, 6, OL[0])
        px(img, 9, 5, EA[2])
    parts = [leaf, bowl]
    for i, (cx, cy) in enumerate([(5, 8.5), (8, 8), (11, 8.5), (6.5, 6.2), (9.5, 6)]):
        parts.insert(1 + i, olive(cx, cy, i % 2 == 1))
    # olives sit in the bowl: draw bowl last only on its rim rows
    img = layered([leaf] + [olive(cx, cy, i % 2 == 1) for i, (cx, cy) in
                            enumerate([(6.5, 6.6), (9.5, 6.4), (5, 9), (8, 8.8), (11, 9)])] + [bowl])
    return img


def i_grapes():
    def stem(img):
        line(img, 8, 1, 8, 4, EA[2])
        blob(img, 11, 2.5, 2.6, 1.6, (GR[0], GR[1], GR[2]), "gr/leaf", noise=0.05)
        px(img, 10, 2, GR[0])

    def grape(cx, cy):
        def f(img):
            for (x, y) in ellipse_mask(cx, cy, 1.6, 1.6):
                nx, ny = x + 0.5 - cx, y + 0.5 - cy
                c = TY[1] if nx + ny < 0.6 else TY[0]
                px(img, x, y, c)
            px(img, int(cx) - 1, int(cy) - 1, G[3])
        return f
    centers = [(5, 5.5), (8, 5.5), (11, 5.5), (6.5, 8), (9.5, 8), (8, 10.5), (5, 8.2), (11, 8.2),
               (6.5, 10.5), (9.5, 10.5), (8, 13)]
    return layered([stem] + [grape(cx, cy) for cx, cy in sorted(centers, key=lambda c: c[1])])


SACK_HWS = [2.0, 2.5, 1.8, 4.0, 5.0, 5.5, 6.0, 6.0, 6.0, 5.5, 5.0, 4.0]


def i_flour():
    def f(img):
        lathe(img, 8, 3, SACK_HWS, (TV[0], TV[1], TV[2], M[0]), spec=False)
        for (x, y) in ellipse_mask(8, 2.7, 3.2, 1.4):
            px(img, x, y, W if x < 9 else M[1])
        hline(img, 6, 9, 5, EA[2])
        px(img, 10, 5, EA[1])
        # wheat stamp
        for y in range(8, 12):
            px(img, 8, y, EA[2] if y > 9 else OC[0])
        px(img, 7, 9, OC[0])
        px(img, 9, 9, OC[0])
        px(img, 7, 10, OC[0])
        px(img, 9, 10, OC[0])
    return single(f)


def i_bread():
    def f(img):
        blob(img, 8, 8.5, 6.2, 4.6, (EA[2], EA[3], OC[1], OC[2]), "bread", noise=0.08)
        for y in range(5, 13):
            px(img, 8, y, EA[1])
        for x in range(3, 14):
            px(img, x, 8, EA[1])
        line(img, 5, 6, 11, 11, EA[1])
        line(img, 11, 6, 5, 11, EA[1])
        for (x, y) in ((4, 6), (6, 5), (5, 7)):
            px(img, x, y, TV[2])
    return single(f)


def i_olive_oil():
    def jug(img):
        lathe(img, 8, 2, [1.6, 1.0, 1.0, 1.5, 2.5, 3.5, 4.0, 4.5, 4.5, 4.5, 4.0, 3.5, 3.0],
              (OL[1], OL[2], GOLD, OC[3]))
        px(img, 6, 2, OL[1])
        for (x, y) in ((10, 3), (11, 4), (11, 5), (11, 6), (10, 7)):
            px(img, x, y, OL[0])

    def leaf(img):
        line(img, 2, 5, 5, 4, OL[2])
        line(img, 3, 6, 6, 5, OL[1])
        px(img, 1, 6, OL[0])
    return layered([jug])


def i_wine():
    def amph(img):
        draw_amphora(img, 6, 1, R_TERRA)
        hline(img, 5, 6, 1, TY[1])
        for (x, y) in ellipse_mask(6, 6.5, 3.7, 0.6):
            px(img, x, y, TY[0])

    def cup(img):
        lathe(img, 11.5, 10, [3.0, 2.6, 2.0, 1.0, 2.0], R_GOLD, spec=False)
        hline(img, 9, 14, 10, TY[1])
        px(img, 9, 10, RD[2])
    return layered([amph, cup])


def i_garum():
    def jar(img):
        lathe(img, 8, 4, [3.0, 2.5, 3.5, 5.0, 5.5, 5.5, 5.5, 5.0, 4.5, 3.5], R_WOOD)
        # cloth cover tied with a red string
        for (x, y) in ellipse_mask(8, 3.6, 3.6, 1.6):
            px(img, x, y, G[3] if x < 9 else G[2])
        hline(img, 5, 10, 5, RD[1])
        # painted fish
        hline(img, 6, 9, 9, M[1])
        px(img, 5, 8, M[1])
        px(img, 5, 10, M[1])
        px(img, 9, 9, M[0])
    return single(jar)


def i_salt():
    def dish(img):
        for (x, y) in ellipse_mask(8, 13, 6.5, 1.6):
            px(img, x, y, EA[2] if y >= 13 else EA[3])

    def heap(img):
        mound(img, 8, 12, 9, 6.0, (M[0], M[1], W, W), "salt",
              grains=(SEA[4], G[3], W), density=0.12)
    return layered([dish, heap])


def i_wood():
    def log(cx, cy, r):
        def f(img):
            for k in range(3, 0, -1):          # bark body receding up-left
                for (x, y) in ellipse_mask(cx - k, cy - k, r, r):
                    px(img, x, y, EA[1] if y > cy - k else EA[2])
            for (x, y) in ellipse_mask(cx, cy, r, r):
                d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
                c = OC[2] if d < r - 1 else OC[1]
                if d < 1.0:
                    c = EA[3]
                px(img, x, y, c)
            px(img, int(cx) - 1, int(cy) - 1, OC[3])
        return f
    return layered([log(9.5, 6.5, 2.8), log(6.5, 11.5, 2.8), log(12.5, 11.5, 2.8)])


def i_planks():
    def plank(x, y, w):
        def f(img):
            hline(img, x, x + w - 1, y, OC[3])
            hline(img, x, x + w - 1, y + 1, OC[2])
            hline(img, x, x + w - 1, y + 2, EA[3])
            for yy in (y, y + 1, y + 2):
                px(img, x + w - 1, yy, OC[1])
            for xx in range(x + 2, x + w - 2, 4):
                px(img, xx, y + 1, OC[1])
        return f
    return layered([plank(4, 3, 10), plank(2, 6, 12), plank(3, 9, 11), plank(1, 12, 12)])


def i_clay():
    def board(img):
        hline(img, 2, 13, 13, EA[3])
        hline(img, 2, 13, 14, EA[1])

    def lump(img):
        m = ellipse_mask(8, 9.5, 5.5, 3.6) | ellipse_mask(7, 6.5, 3.4, 2.8)
        shade_blob(img, m, (TR[0], TR[1], TR[2], TR[3]), key="clay", noise=0.08,
                   cx=7.5, cy=8, rx=5.5, ry=5)
        line(img, 6, 6, 8, 9, TR[0])
        line(img, 9, 7, 11, 10, TR[0])
        px(img, 5, 5, TR[3])
    return layered([board, lump])


def i_bricks():
    def brick(x, y):
        def f(img):
            box(img, x, y, 7, 2, 3, TR[3], TR[2], TR[1])
            hline(img, x, x + 6, y + 4, TR[0])
            px(img, x + 2, y + 3, TR[1])
        return f
    return layered([brick(1, 8), brick(8, 8), brick(4, 3)])


def i_stone():
    return layered(rock_pile([], R_IRON, "stone", flecks=(G[1],), fleck_n=2))


def i_cut_stone():
    def f(img):
        box(img, 2, 3, 12, 4, 7, G[3], G[2], G[1], edge=M[0])
        for (x, y) in ((4, 8), (6, 10), (9, 9), (11, 11), (5, 12)):
            px(img, x, y, G[1])
        hline(img, 2, 13, 7, G[1])
    return single(f)


def i_limestone():
    def chunk(x, y, w, th, fh, key):
        def f(img):
            box(img, x, y, w, th, fh, TV[2], TV[1], TV[0])
            r = rng(key)
            # chipped corners and pits
            px(img, x, y, (0, 0, 0, 0))
            px(img, x + w - 1, y + th + fh - 1, (0, 0, 0, 0))
            for _ in range(4):
                px(img, x + r.randrange(1, w - 1), y + r.randrange(1, th + fh - 1), OC[0])
        return f
    return layered([chunk(6, 2, 8, 3, 4, "ls/a"), chunk(1, 6, 9, 3, 5, "ls/b")])


def i_lime():
    def bucket(img):
        for j, y in enumerate(range(6, 15)):
            hw = 5.5 - j * 0.25
            for x in range(int(8 - hw), int(8 + hw + 0.5)):
                xc = x + 0.5
                nx = (xc - 8) / hw
                c = EA[3] if nx < -0.3 else (EA[2] if nx < 0.5 else EA[1])
                px(img, x, y, c)
        hline(img, 3, 12, 8, G[1])
        hline(img, 3, 12, 12, G[1])
        for (x, y) in ellipse_mask(8, 5.5, 5.2, 1.6):
            px(img, x, y, W if x < 10 else M[1])
        mound(img, 8, 5, 3, 3.5, (M[0], M[1], W), "lime")
    return single(bucket)


def i_pozzolana():
    def f(img):
        mound(img, 8, 13, 10, 6.5, (G[0], TY[0], TY[1], G[2]), "pozz",
              grains=(G[1], RD[0], G[2]), density=0.25)
    return single(f)


def i_concrete():
    def trough(img):
        box(img, 1, 8, 14, 0, 6, EA[3], EA[2], EA[1])
        hline(img, 1, 14, 13, EA[1])
        r = rng("concrete")
        for x in range(2, 14):
            for y in (8, 9):
                c = G[2] if r.random() > 0.3 else G[1]
                if y == 8 and r.random() < 0.3:
                    c = G[3]
                px(img, x, y, c)

    def trowel(img):
        line(img, 9, 7, 12, 2, EA[2])
        px(img, 12, 1, EA[1])
        for (x, y) in ((6, 9), (7, 9), (8, 9), (7, 8), (8, 8), (8, 7)):
            px(img, x, y, G[3] if y < 9 else G[2])
    return layered([trough, trowel])


def i_marble():
    def f(img):
        box(img, 2, 3, 12, 4, 7, W, M[1], M[0], edge=W)
        hline(img, 2, 13, 7, M[0])
        r = rng("marble/vein")
        x, y = 3, 4
        for _ in range(14):
            px(img, x, y, G[2] if y >= 7 else G[3])
            x += 1
            y += r.choice((0, 1, 1, -1)) if y < 12 else -1
            y = max(4, min(12, y))
            if x > 12:
                break
        px(img, 10, 11, G[3])
        px(img, 11, 12, G[3])
    return single(f)


def i_sand():
    def f(img):
        mound(img, 8, 13, 9, 6.5, (OC[0], OC[1], OC[2], OC[3]), "sand",
              grains=(OC[1], OC[3], TV[2]), density=0.15)
    return single(f)


def i_iron_ore():
    return layered(rock_pile([], (G[0], G[0], G[1], G[2]), "iore",
                             flecks=(TR[1], TR[0], RD[1]), fleck_n=3))


def i_silver_ore():
    return layered(rock_pile([], (CY[0], G[0], G[1], G[1]), "sore",
                             flecks=(W, G[3], M[1]), fleck_n=3))


def _ingots(ramp):
    return layered([lambda im: ingot(im, 1, 9, 7, ramp), lambda im: ingot(im, 8, 9, 7, ramp),
                    lambda im: ingot(im, 4, 5, 8, ramp)])


def i_iron():
    return _ingots((G[0], G[1], G[2], G[3]))


def i_silver():
    return _ingots((G[2], G[3], M[1], W))


def i_gold():
    return _ingots((OC[0], GOLD, OC[3], W))


def i_tools():
    def hammer(img):
        for i in range(8):
            px(img, 3 + i, 13 - i, EA[3])
            px(img, 4 + i, 13 - i, EA[1])
        for i in range(6):
            px(img, 8 + i, 3 + i, G[3])
            px(img, 9 + i, 3 + i, G[2])
        px(img, 14, 8, G[1])
        px(img, 13, 8, G[1])

    def chisel(img):
        for i in range(4):
            px(img, 9 + i, 9 + i, EA[2])
            px(img, 10 + i, 9 + i, EA[1])
        for i in range(5):
            px(img, 4 + i, 4 + i, G[3])
        px(img, 3, 3, W)
    return layered([chisel, hammer])


def i_weapons():
    def spear(img):
        line(img, 3, 3, 13, 13, EA[2])
        for (x, y, c) in ((2, 2, G[3]), (1, 1, W), (3, 2, G[2]), (2, 3, G[2])):
            px(img, x, y, c)

    def sword(img):
        for i in range(7):
            px(img, 13 - i, 2 + i, W if i < 6 else G[3])
            px(img, 14 - i, 2 + i, G[2])
        px(img, 14, 1, G[3])
        for (x, y) in ((5, 8), (6, 9), (7, 10), (8, 11)):
            px(img, x, y, GOLD)
        px(img, 5, 10, EA[1])
        px(img, 4, 11, EA[1])
        px(img, 3, 12, GOLD)
        px(img, 2, 13, OC[0])
    return layered([spear, sword])


def i_armor():
    """Galea: round dome, gold brow band, cheek guards, curved red crest."""
    def crest(img):
        for x in range(4, 12):
            top = 1 if 5 <= x <= 10 else 2
            for y in range(top, 5):
                c = RD[2] if (y == top or x < 6) else RD[1]
                if y >= 4:
                    c = RD[0]
                px(img, x, y, c)
        for x in range(6, 12, 2):
            px(img, x, 2, RD[1])

    def helm(img):
        for (x, y) in ellipse_mask(8, 9, 4.6, 4.6):
            if y > 9:
                continue
            nx = (x + 0.5 - 8) / 4.6
            px(img, x, y, G[3] if nx < -0.35 else (G[2] if nx < 0.4 else G[1]))
        px(img, 5, 6, W)
        px(img, 6, 5, W)
        hline(img, 3, 12, 9, GOLD)
        px(img, 12, 9, OC[0])
        hline(img, 2, 13, 10, G[2])
        px(img, 13, 10, G[1])
        for y in range(11, 14):
            hline(img, 4, 5, y, G[2] if y < 13 else G[1])
            hline(img, 10, 11, y, G[1])
            px(img, 4, y, G[3])
        px(img, 2, 11, G[1])
        px(img, 13, 11, G[1])
    return layered([crest, helm])


def i_pottery():
    def vase(img):
        lathe(img, 6, 2, [3.0, 2.0, 1.5, 2.0, 3.0, 4.0, 4.5, 4.5, 4.0, 3.5, 2.5, 1.5, 2.5], R_TERRA)
        for x in range(2, 11):
            px(img, x, 7, OUT if get(img, x, 7)[3] else (0, 0, 0, 0))
            if x % 2 == 0 and get(img, x, 8)[3]:
                px(img, x, 8, OUT)
        px(img, 4, 7, TR[3])
        px(img, 7, 7, TR[3])

    def bowl(img):
        lathe(img, 11.5, 10, [3.0, 3.0, 2.5, 1.5], (OC[0], OC[1], OC[2], OC[3]), spec=False)
        hline(img, 9, 14, 10, OUT)
        px(img, 10, 11, TR[1])
        px(img, 12, 11, TR[1])
        hline(img, 10, 13, 14, OC[0])
    return layered([vase, bowl])


def i_amphorae():
    return layered([lambda im: draw_amphora(im, 11, 1, R_TERRA),
                    lambda im: draw_amphora(im, 5.5, 2, R_TERRA)])


def i_wool():
    def f(img):
        ramp = (TV[1], TV[2], M[1], W)
        for (cx, cy, r_) in ((6, 6, 2.8), (10, 6.5, 2.8), (4.5, 9.5, 2.6), (8, 9.5, 3.2), (11.5, 10, 2.4)):
            blob(img, cx, cy, r_, r_, ramp, "wool%d" % cx, noise=0.12)
            px(img, int(cx), int(cy + r_) - 1, TV[1])
        line(img, 11, 12, 14, 14, TV[1])
        px(img, 14, 13, TV[0])
    return single(f)


def _tunic(img, ramp, trim=None):
    d, m, l, h = ramp
    for y in range(3, 15):
        for x in range(1, 15):
            sleeve = y <= 7 and (y - 3) <= x - 1 + 4 and (y - 3) <= 14 - x + 4
            body = 4 <= x <= 11
            arm = y <= 7 and (x <= 3 and x >= 1 + (y - 3)) or (y <= 7 and x >= 12 and x <= 14 - (y - 3))
            if not (body or arm):
                continue
            if y > 7 and not body:
                continue
            nx = (x - 7.5) / 7.0
            c = h if nx < -0.45 else (l if nx < 0.1 else (m if nx < 0.6 else d))
            px(img, x, y, c)
    for y in range(8, 14):
        px(img, 7, y, d)
    px(img, 9, 10, m)
    for x in (7, 8):
        px(img, x, 3, (0, 0, 0, 0))
    px(img, 6, 3, d)
    px(img, 9, 3, d)
    if trim is not None:
        hline(img, 4, 11, 14, trim)
        for y in range(4, 14):
            px(img, 5, y, trim)
            px(img, 10, y, trim)


def i_clothing():
    def f(img):
        _tunic(img, (TV[0], TV[1], TV[2], M[0]))
        hline(img, 4, 11, 14, EA[3])
    return single(f)


def i_dyed_clothing():
    return single(lambda img: _tunic(img, (TY[0], TY[1], RD[1], RD[2]), trim=GOLD))


def i_dye():
    def murex(img):
        m = ellipse_mask(4, 8, 2.4, 3.2)
        shade_blob(img, m, (TV[0], TV[1], TV[2], M[1]), key="murex", noise=0.1)
        for (x, y) in ((2, 6), (6, 7), (2, 9), (4, 4), (4, 12)):
            px(img, x, y, M[1])
        px(img, 4, 8, TY[0])

    def vat(img):
        lathe(img, 10, 7, [4.5, 4.5, 4.5, 4.0, 3.5, 3.0, 2.5, 3.0], (G[0], G[1], G[2], G[3]), spec=False)
        for (x, y) in ellipse_mask(10, 7.0, 4.2, 1.2):
            px(img, x, y, TY[1] if x < 11 else TY[0])
        px(img, 7, 8, TY[1])
        px(img, 7, 9, TY[0])
        px(img, 13, 8, TY[0])
    return layered([murex, vat])


def i_furniture():
    def legs(img):
        for i in range(8):
            px(img, 3 + i, 13 - i, EA[3])
            px(img, 4 + i, 13 - i, EA[1])
            px(img, 12 - i, 13 - i, EA[2])
            px(img, 11 - i, 13 - i, EA[3])
        hline(img, 2, 5, 14, EA[1])
        hline(img, 10, 13, 14, EA[1])

    def seat(img):
        hline(img, 2, 13, 6, EA[2])
        px(img, 13, 6, EA[1])
        for y in (4, 5):
            for x in range(3, 13):
                px(img, x, y, RD[2] if (y == 4 and x < 10) else RD[1])
        px(img, 12, 5, RD[0])
        hline(img, 4, 11, 3, RD[2])
        for x in (4, 7, 10):
            px(img, x, 5, GOLD)
    return layered([legs, seat])


def i_glassware():
    ramp = (SEA[2], SEA[3], SEA[4], W)

    def bottle(img):
        lathe(img, 6, 1, [1.6, 1.0, 1.0, 1.0, 1.5, 2.5, 3.5, 4.0, 4.0, 4.0, 3.5, 3.0], ramp)
        vline(img, 4, 7, 10, W)

    def cup(img):
        lathe(img, 11.5, 8, [3.0, 2.8, 2.6, 2.4, 2.2, 2.0, 2.5], ramp, spec=False)
        vline(img, 10, 9, 12, W)
        hline(img, 9, 14, 8, SEA[4])
    return layered([bottle, cup])


def i_papyrus():
    def sheets(img):
        for k, (x, y) in enumerate(((3, 8), (2, 10))):
            for yy in range(y, y + 4):
                for xx in range(x, x + 11):
                    c = OC[3] if (xx + yy) % 3 else OC[2]
                    if yy == y + 3:
                        c = OC[1]
                    px(img, xx, yy, c)

    def stalk(img):
        line(img, 3, 13, 12, 4, GR[1])
        line(img, 4, 13, 12, 5, GR[0])
        for (dx, dy) in ((-2, -1), (-1, -2), (0, -3), (1, -2), (2, -1), (-1, -1), (0, -2), (1, -1), (0, -1)):
            px(img, 12 + dx, 4 + dy, GR[3] if dy < -1 else GR[2])
    return layered([sheets, stalk])


def i_scrolls():
    def scroll(x, y, w):
        def f(img):
            for yy, c in ((y, OC[3]), (y + 1, OC[3]), (y + 2, OC[2]), (y + 3, OC[1])):
                hline(img, x + 1, x + w - 2, yy, c)
            for yy in range(y - 1, y + 5):
                px(img, x, yy, EA[2] if yy < y + 2 else EA[1])
                px(img, x + w - 1, yy, EA[2] if yy < y + 2 else EA[1])
            px(img, x + 2, y, W)
        return f

    def tag(img):
        hline(img, 7, 8, 13, RD[2])
        px(img, 7, 14, RD[1])
    return layered([scroll(3, 3, 11), scroll(1, 8, 12), tag])


def i_jewelry():
    def necklace(img):
        for i in range(15):
            a = math.pi * i / 14.0
            x = int(round(7.5 - 5.5 * math.cos(a)))
            y = int(round(3 + 6.0 * math.sin(a)))
            px(img, x, y, GOLD if i % 2 == 0 else OC[3])

    def gem(img):
        for (x, y, c) in ((7, 9, OC[3]), (8, 9, GOLD), (6, 10, GOLD), (9, 10, OC[0]),
                          (7, 10, RD[2]), (8, 10, RD[1]), (7, 11, RD[1]), (8, 11, RD[0]),
                          (7, 12, OC[0]), (8, 12, OC[0])):
            px(img, x, y, c)
        px(img, 7, 10, W)

    def ring(img):
        for (x, y) in ellipse_mask(12, 12.5, 2.4, 2.0):
            px(img, x, y, GOLD if x < 12 else OC[0])
        for (x, y) in ellipse_mask(12, 12.8, 1.0, 0.8):
            px(img, x, y, (0, 0, 0, 0))
        px(img, 12, 10, SEA[3])
    return layered([necklace, gem, ring])


def i_statues():
    def bust(img):
        ramp = (M[0], M[1], W)
        m = ellipse_mask(8, 10.5, 5.0, 2.6)
        m = {(x, y) for (x, y) in m if y <= 11}
        shade_blob(img, m, ramp, key="bust/b", noise=0.05)
        rect(img, 7, 7, 2, 2, M[1])
        px(img, 8, 8, M[0])
        shade_blob(img, ellipse_mask(8, 4.5, 2.6, 3.0), ramp, key="bust/h", noise=0.05)
        for (x, y) in ((6, 2), (7, 2), (8, 1), (9, 2), (6, 3)):
            px(img, x, y, M[0])
        px(img, 7, 4, G[3])
        px(img, 9, 4, G[3])

    def plinth(img):
        box(img, 5, 12, 6, 1, 1, G[3], G[2], G[1])
        hline(img, 4, 11, 14, G[2])
        px(img, 11, 14, G[1])
    return layered([plinth, bust])


def i_denarii():
    def stack(img):
        for k in range(4):
            cy = 13 - k * 1.5
            for (x, y) in ellipse_mask(5.5, cy, 4.0, 1.4):
                px(img, x, y, G[3] if y < cy else G[2])
            hline(img, 3, 7, int(cy), M[1])

    def coin(img):
        for (x, y) in ellipse_mask(11, 7.5, 3.6, 4.5):
            d = math.hypot((x + 0.5 - 11) / 3.6, (y + 0.5 - 7.5) / 4.5)
            nx = x + 0.5 - 11
            c = M[1] if nx < 0 else G[3]
            if d > 0.75:
                c = W if nx + (y - 7.5) < 0 else G[2]
            px(img, x, y, c)
        # profile head
        for (x, y) in ((10, 5), (11, 5), (10, 6), (11, 6), (12, 7), (10, 7), (11, 8), (10, 9), (11, 9)):
            px(img, x, y, G[2])
    return layered([stack, coin])


BIG = {
    "wheat": i_wheat, "vegetables": i_vegetables, "fish": i_fish, "meat": i_meat,
    "olives": i_olives, "grapes": i_grapes, "flour": i_flour, "bread": i_bread,
    "olive_oil": i_olive_oil, "wine": i_wine, "salt": i_salt, "garum": i_garum,
    "wood": i_wood, "planks": i_planks, "clay": i_clay, "bricks": i_bricks,
    "stone": i_stone, "cut_stone": i_cut_stone, "limestone": i_limestone, "lime": i_lime,
    "pozzolana": i_pozzolana, "concrete": i_concrete, "marble": i_marble, "sand": i_sand,
    "iron_ore": i_iron_ore, "iron": i_iron, "silver_ore": i_silver_ore, "silver": i_silver,
    "gold": i_gold, "tools": i_tools, "weapons": i_weapons, "armor": i_armor,
    "pottery": i_pottery, "amphorae": i_amphorae, "wool": i_wool, "clothing": i_clothing,
    "dye": i_dye, "dyed_clothing": i_dyed_clothing, "furniture": i_furniture,
    "glassware": i_glassware, "papyrus": i_papyrus, "scrolls": i_scrolls,
    "jewelry": i_jewelry, "statues": i_statues, "denarii": i_denarii,
}


def generate():
    out = []
    for gid in GOODS:
        out.append(("goods/%s" % gid, small_icon(gid), None))
        out.append(("goods/%s_16" % gid, BIG[gid](), None))
    return out
