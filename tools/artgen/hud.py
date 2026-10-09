"""HUD art: title logo, Greek-key ornaments and 16x16 HUD icons.

  ui/logo            ~200x60 "URBS" in chiselled Roman capitals (raised
                     stone, lit top-left facets, shaded bottom-right facets),
                     terracotta drop shadow, laurel branches beneath
  ui/panel_corner    8x8 Greek-key corner (top-left; renderer mirrors it)
  ui/divider         64x6 running meander band, period 8 (tiles cleanly)
  ui/icon_<name>     16x16 HUD icons, 1px dark outline, light from top-left

Letter shapes are geometric definitions rasterised from code (no fonts);
icons are symbolic templates or parametric shapes (no painted PNGs).
"""
import math

from PIL import Image, ImageDraw

from palette import C
from primitives import new, px, get, outline, ellipse_mask, line, hline, vline, rect

G, M, W = C.gray, C.marble, C.white
TV, TR, OC, EA = C.travertine, C.roof_terracotta, C.ochre, C.earth
GR, OL, CY, SEA = C.grass, C.olive, C.cypress, C.sea
RD, TY, GOLD, SK = C.roman_red, C.tyrian, C.gold, C.skin
OUT = C.outline
CLEAR = (0, 0, 0, 0)

ICONS = ["population", "happiness", "culture", "prosperity", "security", "favor", "build",
         "rome", "advisors", "finance", "goods", "trade", "labor", "religion", "military",
         "research", "alerts", "help", "menu", "pause", "play", "fast", "faster", "road",
         "demolish", "overlay"]

# symbol -> color for icon templates
PAL = {
    "k": OUT, "W": W, "w": M[1], "m": M[0],
    "G": GOLD, "g": OC[3], "y": OC[2], "o": OC[0],
    "R": RD[2], "r": RD[1], "d": RD[0],
    "T": TR[2], "t": TR[1], "u": TR[3], "U": TR[0],
    "E": EA[2], "e": EA[1], "b": EA[3], "f": EA[0],
    "S": G[3], "s": G[2], "x": G[1], "X": G[0],
    "L": GR[3], "l": GR[2], "n": GR[1], "N": GR[0],
    "B": SEA[3], "v": SEA[2], "V": SEA[1], "F": SEA[4],
    "P": TY[1], "p": TY[0],
    "K": SK[2], "j": SK[1], "J": SK[0],
    "A": TV[2], "a": TV[1], "z": TV[0],
    "O": OL[2], "q": OL[1], "Q": OL[0],
}


def _grid(rows, img=None, ox=1, oy=1):
    img = img or new(16, 16)
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch != ".":
                px(img, ox + i, oy + j, PAL[ch])
    return img


def _finish(img):
    outline(img, OUT)
    return img


def _disc(img, cx, cy, r, ramp, ry=None):
    """Shaded disc lit from the top-left; ramp dark -> light."""
    ry = ry or r
    for (x, y) in ellipse_mask(cx, cy, r, ry):
        nx, ny = (x + 0.5 - cx) / r, (y + 0.5 - cy) / ry
        b = 0.5 - 0.45 * nx - 0.45 * ny
        px(img, x, y, ramp[max(0, min(len(ramp) - 1, int(b * len(ramp))))])


def _thick_line(img, x0, y0, x1, y1, c, c2=None):
    line(img, x0, y0, x1, y1, c)
    if c2 is not None:
        line(img, x0 + 1, y0, x1 + 1, y1, c2)


# ---------------------------------------------------------------- icons
def icon_population():
    return _finish(_grid([
        "..............",
        "..eeee........",
        ".eKKKje.......",
        "..KKKjj.......",
        "..jKjj....ee..",
        "...jj....eKKe.",
        ".RRRRRr...KKj.",
        "RuRRRRrr..jj..",
        "RuRRRRrr.BBBV.",
        "KuRRRRrK.BvvVj",
        ".uRRRRr.KBvvV.",
        ".uRRRRr..BvV..",
        "..j..J...j.J..",
        "..e..f...e.f..",
    ]))


def icon_happiness():
    """Comedy mask."""
    return _finish(_grid([
        "...wwwwwwww...",
        "..wWWWWWWWwm..",
        ".wWWWWWWWWwwm.",
        ".wWWWWWWWWwwm.",
        ".wWkkWWWkkwwm.",
        ".wkWWkWkWWkwm.",
        ".wWWWWWWWWwmm.",
        ".wWRWWWWWRwmm.",
        ".wkWWWWWWWkmm.",
        "..wkWWWWWkmm..",
        "..wwkRRRkmmm..",
        "...wwkkkmmm...",
        "....mwwwmm....",
        ".....mmmm.....",
    ]))


def icon_culture():
    """Golden lyre."""
    return _finish(_grid([
        ".G..........G.",
        ".Gg........go.",
        "..Gg......go..",
        "..Gg......go..",
        "..GGGGGGGGGo..",
        "..Gg.W.W.Wgo..",
        "..Gg.W.W.Wgo..",
        "..Gg.W.W.Wgo..",
        "...Gg.W.W.o...",
        "...Gg.W.Wgo...",
        "....GgggGo....",
        ".....GGGo.....",
        "....eeeeef....",
        "..............",
    ]))


def icon_prosperity():
    """Cornucopia overflowing with fruit and grain."""
    img = new(16, 16)
    # horn: discs along a curve, radius growing toward the mouth
    pts = []
    for i in range(24):
        t = i / 23.0
        x = (1 - t) ** 2 * 2.5 + 2 * (1 - t) * t * 3.0 + t * t * 10.0
        y = (1 - t) ** 2 * 13.5 + 2 * (1 - t) * t * 7.0 + t * t * 6.5
        pts.append((x, y, 0.6 + 2.6 * t))
    for (x, y, r) in pts:
        _disc(img, x, y, r, (OC[0], OC[1], GOLD, OC[3]))
    for k, (x, y, r) in enumerate(pts):
        if k in (8, 13, 18):                               # rings on the horn
            for (xx, yy) in ellipse_mask(x, y, r, r):
                if abs(xx + 0.5 - x) < 0.8:
                    px(img, xx, yy, OC[0])
    # mouth with fruit
    mx, my = 11.0, 5.5
    for (x, y) in ellipse_mask(mx, my, 3.4, 2.4):
        px(img, x, y, EA[0])
    _disc(img, 10.0, 4.2, 1.7, (RD[0], RD[1], RD[2], TR[3]))            # apple
    for (x, y) in ((12, 3), (13, 4), (12, 5), (13, 6), (14, 5)):          # grapes
        px(img, x, y, TY[1] if (x + y) % 2 else TY[0])
    px(img, 9, 2, GR[2])                                                # leaf
    px(img, 10, 1, GR[3])
    px(img, 13, 1, OC[3])                                               # wheat
    px(img, 12, 2, OC[2])
    px(img, 14, 2, OC[2])
    return _finish(img)


def icon_security():
    """Red scutum with gold rim, spine and boss."""
    img = new(16, 16)
    x0, x1, y0, y1 = 3, 12, 1, 14
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if (x in (x0, x1)) and (y in (y0, y1)):
                continue
            c = RD[2]
            if x == x0 or y == y0:
                c = OC[3]
            elif x == x1 or y == y1:
                c = OC[0]
            elif x == x0 + 1:
                c = TR[3]
            elif x == x1 - 1:
                c = RD[1]
            px(img, x, y, c)
    for y in range(y0 + 1, y1):
        px(img, 7, y, GOLD)
        px(img, 8, y, OC[0])
    _disc(img, 8.0, 7.5, 2.2, (OC[0], OC[1], GOLD, W))
    # lightning wings
    for (x, y) in ((5, 4), (6, 5), (5, 10), (6, 9), (10, 4), (9, 5), (10, 10), (9, 9)):
        px(img, x, y, GOLD)
    return _finish(img)


def icon_favor():
    """Altar with a sacrificial flame."""
    return _finish(_grid([
        "......y.......",
        ".....yG.......",
        ".....GGy..y...",
        "....yGWGyGy...",
        "...yGWWGGGy...",
        "...GWWWWGTG...",
        "..TGGWWGGGTt..",
        "..mWWWWWWWWa..",
        "...wWwwwwwa...",
        "...wwPPPPwa...",
        "...wwwwwwwa...",
        "...wwwwwwwa...",
        "..mWWWWWWWWa..",
        "..aaaaaaaaaa..",
    ]))


def icon_build():
    """Trowel over neat brick courses."""
    img = _grid([
        "..........Eb..",
        ".........Ebf..",
        "........Ebf...",
        "......SSef....",
        "....SWSSs.....",
        "..SWSSSsx.....",
        ".SWSSSsx......",
        "..ssssx.......",
        "..............",
        "uTTtwuTTTtwuTt",
        "wwwwwwwwwwwwww",
        "TtwuTTTtwuTTTt",
        "wwwwwwwwwwwwww",
        "uTTtwuTTTtwuTt",
    ])
    return _finish(img)


def icon_rome():
    """Aquila standard with an SPQR plaque."""
    return _finish(_grid([
        "......GG......",
        ".GG..gGGo..GG.",
        ".GgGGGGGGGGGo.",
        "..ogGGGGGGoo..",
        "....ogGGgo....",
        ".....oGGo.....",
        "......bf......",
        "..GGGGGGGGGG..",
        "..GRRRRRRRRo..",
        "..GRgRgRgRgo..",
        "..GRRRRRRRRo..",
        "..ooooooooo...",
        "......bf......",
        "......bf......",
    ]))


def icon_advisors():
    """Two togate busts."""
    return _finish(_grid([
        ".......xxxx...",
        "......xKKKjx..",
        "......KKKKjj..",
        "..eeee.KKjj...",
        ".eKKKje.jj....",
        ".KKKKjjAAaAa..",
        "..KKjj.AAAAaa.",
        "...jj.AAAaaaa.",
        ".wwWWw.Aaaaaa.",
        "wWWPWWWwaaaaa.",
        "wWWWPWWWwaaaa.",
        "wWWWWPWWwm....",
        "wWWWWWPWWm....",
        "mmmmmmmmmm....",
    ]))


def icon_finance():
    """Stacks of gold denarii and a big coin."""
    img = new(16, 16)
    for (x0, n) in ((1, 4), (6, 6)):
        for k in range(n):
            y = 14 - k * 2
            hline(img, x0, x0 + 4, y, OC[0])
            hline(img, x0, x0 + 4, y - 1, GOLD if k % 2 == 0 else OC[3])
            px(img, x0, y - 1, OC[3])
    _disc(img, 11.5, 10.5, 3.6, (OC[0], OC[1], GOLD, OC[3]))
    for (x, y) in ((11, 9), (12, 9), (11, 10), (12, 11), (11, 12)):      # profile
        px(img, x, y, OC[0])
    px(img, 10, 8, OC[3])
    return _finish(img)


def icon_goods():
    """Amphora beside a crate."""
    return _finish(_grid([
        "...TT.........",
        "..tuTt........",
        "...uT.........",
        "..TuTTt.......",
        ".TuTTTTt......",
        ".TuTTTTt......",
        ".TuTTTTt.bbbbb",
        ".TuTTTtt.bEEEe",
        "..TTTTt..bbbbb",
        "..tTTtt..bEfEe",
        "...TTt...bEfEe",
        "...tTt...bEfEe",
        "....t....bbbbb",
        "...ttt...eeeee",
    ]))


def icon_trade():
    """Merchant ship under sail."""
    return _finish(_grid([
        "......e.......",
        "...WWWeWWm....",
        "...WWWWWWm....",
        "...RRRRRRd....",
        "...WWWWWWm....",
        "...WWWWWWm....",
        "....wwwwm.....",
        "......e.....b.",
        "bEEEEEEEEEEEEe",
        ".EbbbbbbbbbbE.",
        "..EEEEEEEEEe..",
        "...eeeeeeef...",
        ".vBvvBvvvBvv..",
        "..VvvVvvVvvV..",
    ]))


def icon_labor():
    """Crossed pickaxe and shovel."""
    img = new(16, 16)
    # shovel: handle top-left -> centre, blade bottom-right
    line(img, 2, 2, 9, 9, EA[3])
    line(img, 3, 2, 10, 9, EA[1])
    hline(img, 1, 3, 1, EA[2])
    for (x, y) in ((10, 10), (11, 10), (10, 11), (11, 11), (12, 11), (11, 12), (12, 12),
                   (13, 12), (12, 13), (13, 13), (12, 10), (13, 11)):
        px(img, x, y, G[3] if x + y < 23 else G[2])
    px(img, 9, 10, G[3])
    # pickaxe: handle bottom-left -> top-right, curved iron head
    line(img, 2, 13, 11, 4, EA[3])
    line(img, 3, 13, 12, 4, EA[1])
    for (x, y) in ((7, 2), (8, 2), (9, 2), (10, 3), (11, 3), (12, 3), (13, 4), (14, 5), (14, 6)):
        px(img, x, y, G[3])
    for (x, y) in ((6, 3), (13, 5), (12, 4)):
        px(img, x, y, G[2])
    return _finish(img)


def icon_religion():
    """Temple front."""
    return _finish(_grid([
        "......GG......",
        "....mWWWWa....",
        "..mWWWWWWWWa..",
        "mWWWwwwwwwWWWa",
        "aaaaaaaaaaaaaa",
        ".WmzWmzzWmzWm.",
        ".WmzWmzzWmzWm.",
        ".WmzWmzzWmzWm.",
        ".WmzWmzzWmzWm.",
        ".WmzWmzzWmzWm.",
        ".WmzWmzzWmzWm.",
        "WWWWWWWWWWWWWa",
        "mmmmmmmmmmmmma",
        "zzzzzzzzzzzzzz",
    ]))


def icon_military():
    """Legionary helmet (galea) with a red crest, front view."""
    return _finish(_grid([
        ".....RRRR.....",
        "...RRuuRRRd...",
        "..RuRRRRRRRd..",
        ".....dddd.....",
        "...SWSSSSsx...",
        "..SWSSSSSSsx..",
        ".SWSSSSSSSSsx.",
        ".GgGGGGGGGGGo.",
        ".SSkkkkkkkksx.",
        ".SSsk.kk.ksxx.",
        ".SWsk....ksx..",
        ".SSs......sx..",
        "..Ss......x...",
        "..............",
    ]))


def icon_research():
    """Open scroll with lines of writing."""
    return _finish(_grid([
        "..bEEEEEEEEe..",
        ".bEEEEEEEEEEf.",
        "..gAAAAAAAAz..",
        "..gAxxxAxxAz..",
        "..gAAAAAAAAz..",
        "..gAxxAxxxAz..",
        "..gAAAAAAAAz..",
        "..gAxxxxAxAz..",
        "..gAAAAAAAAz..",
        "..gAxxAxxxAz..",
        "..gAAAAAAAAz..",
        ".bEEEEEEEEEEf.",
        "..bEEEEEEEEe..",
        "..............",
    ]))


def icon_alerts():
    """Bronze bell with red exclamation."""
    return _finish(_grid([
        "......oo......",
        ".....GGGo.....",
        "....GgGGGo....",
        "...GgGRRGGo...",
        "...GgGRRGGo...",
        "...GgGRRGGo...",
        "..GgGGRRGGGo..",
        "..GgGGGGGGGo..",
        ".GgGGGRRGGGGo.",
        "GgGGGGGGGGGGGo",
        ".oooooooooooo.",
        ".....oGGo.....",
        "......oo......",
        "..............",
    ]))


def icon_help():
    """White question mark on a blue disc."""
    img = new(16, 16)
    _disc(img, 8, 8, 6.6, (SEA[0], SEA[1], SEA[2], SEA[3]))
    _grid([
        "....WWW...",
        "...Wm..Wm.",
        ".......Wm.",
        "......Wm..",
        ".....Wm...",
        ".....Wm...",
        "..........",
        ".....Wm...",
    ], img, ox=3, oy=4)
    return _finish(img)


def _bar(img, x, y, w, h):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            c = M[1]
            if yy == y or xx == x:
                c = W
            elif yy == y + h - 1 or xx == x + w - 1:
                c = G[3]
            px(img, xx, yy, c)


def icon_menu():
    img = new(16, 16)
    for y in (3, 7, 11):
        _bar(img, 2, y, 12, 2)
    return _finish(img)


def icon_pause():
    img = new(16, 16)
    _bar(img, 4, 2, 3, 12)
    _bar(img, 9, 2, 3, 12)
    return _finish(img)


def _tri(img, x0, y0, h):
    """Right-pointing triangle, apex at (x0 + h//2, y0 + h//2)."""
    for j in range(h):
        span = min(j, h - 1 - j)
        for i in range(span + 1):
            c = M[1]
            if i == 0 or j <= span and i == 0:
                c = W
            if j > h // 2 and i == span:
                c = G[3]
            elif i == span:
                c = W if j <= h // 2 else G[3]
            px(img, x0 + i, y0 + j, c)


def icon_play():
    img = new(16, 16)
    _tri(img, 4, 1, 14)
    return _finish(img)


def icon_fast():
    img = new(16, 16)
    _tri(img, 1, 3, 10)
    _tri(img, 7, 3, 10)
    return _finish(img)


def icon_faster():
    img = new(16, 16)
    _tri(img, 0, 4, 8)
    _tri(img, 5, 4, 8)
    _tri(img, 10, 4, 8)
    return _finish(img)


def icon_road():
    """Flagstone road between grass verges."""
    import roads
    tex = roads.TEX["paved"]
    img = new(16, 16)
    for y in range(1, 15):
        for x in range(1, 15):
            if x in (1, 2, 3) or x in (12, 13, 14):
                c = GR[2] if (x * 7 + y * 3) % 5 else GR[3]
                if x == 3 or x == 12:
                    c = GR[0]
            else:
                c = tex[y][x]
            px(img, x, y, c)
    return _finish(img)


def icon_demolish():
    """Sledgehammer smashing a cracked brick wall."""
    img = _grid([
        "........SSSs..",
        ".......SWSSsx.",
        "........sSssx.",
        ".......bfsxx..",
        "......bf......",
        ".....bf.......",
        "....bf...t....",
        "..........u.T.",
        "uTTkwuT..t....",
        "wwwkwwww.kTt..",
        "TtwukTTtwkTTTt",
        "wwwwkwwwwkwwww",
        "uTTtwkTTkwuTTt",
        "wwwwwwkkwwwwww",
    ])
    return _finish(img)


def icon_overlay():
    """Three stacked map layers."""
    img = new(16, 16)
    layers = [(10, (GR[1], GR[2], GR[3])), (7, (SEA[1], SEA[2], SEA[3])), (4, (OC[0], GOLD, OC[3]))]
    for (cy, ramp) in layers:
        for y in range(cy - 3, cy + 4):
            half = 7 - abs(y - cy) * 2
            for x in range(8 - half, 8 + half):
                c = ramp[1]
                if y < cy and x < 8:
                    c = ramp[2]
                elif y > cy or x >= 8 + half - 1:
                    c = ramp[0]
                px(img, x, y, c)
        # dark edge under each layer so they separate
        for x in range(2, 14):
            if get(img, x, cy + 3)[3] == 0 and get(img, x, cy + 2)[3]:
                pass
    # draw layer rims
    out = new(16, 16)
    for (cy, ramp) in reversed(layers):
        lay = new(16, 16)
        for y in range(cy - 3, cy + 4):
            half = 7 - abs(y - cy) * 2
            for x in range(8 - half, 8 + half):
                c = ramp[1]
                if y < cy and x < 8:
                    c = ramp[2]
                elif y > cy:
                    c = ramp[0]
                px(lay, x, y, c)
        outline(lay, OUT)
        out.alpha_composite(lay)
    return out


# ---------------------------------------------------------------- ornaments
def panel_corner():
    """Top-left corner: gold border lines on the top row and left column
    with a square Greek-key spiral on a dark ground."""
    rows = [
        "gggggggg",
        "gkkkkkkk",
        "gkGGGGGk",
        "gkGkkkGk",
        "gkGkGkGk",
        "gkGkGGGk",
        "gkGkkkkk",
        "gkkkkkkk",
    ]
    img = new(8, 8)
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            px(img, i, j, {"g": OC[3], "G": GOLD, "k": OUT}[ch])
    return img


def divider():
    """Running meander between two gold rules; period 8 px."""
    cell = [
        "gggggggg",
        "kGGGGGkk",
        "kkkkGkGk",
        "kGGkGkGk",
        "kGkkkkGk",
        "oooooooo",
    ]
    img = new(64, 6)
    for x in range(64):
        for y in range(6):
            ch = cell[y][x % 8]
            px(img, x, y, {"g": OC[3], "G": GOLD, "k": OUT, "o": OC[0]}[ch])
    return img


# ---------------------------------------------------------------- logo
SS = 4          # supersampling factor for letter shapes


def _letter_shapes(d, ch, x, y, h):
    """Draw a Roman capital into ImageDraw d (supersampled coordinates).
    x, y = top-left of the letter box, h = cap height.  Returns width."""
    th = h * 0.20        # thick stroke
    tn = h * 0.085       # thin stroke
    sf = h * 0.07        # serif height
    fill = 255

    def bar(x0, y0, x1, y1):
        d.rectangle([x0, y0, x1, y1], fill=fill)

    def serif(cx, yy, w, top=True):
        # bracketed slab serif
        bar(cx - w / 2, yy if top else yy - sf, cx + w / 2, yy + sf if top else yy)

    if ch == "U":
        w = h * 0.86
        sx0 = x + h * 0.06
        bar(sx0, y, sx0 + th, y + h * 0.62)
        sx1 = x + w - h * 0.14
        bar(sx1, y, sx1 + tn, y + h * 0.62)
        # bowl: thick left/bottom, thin right
        bx0, bx1 = sx0, sx1 + tn
        by0, by1 = y + h * 0.25, y + h
        d.ellipse([bx0, by0, bx1, by1], fill=fill)
        d.ellipse([bx0 + th, by0 - h * 0.02, bx1 - tn, by1 - th * 0.75], fill=0)
        bar(sx0 + th, y, sx1, y + h * 0.6)
        d.rectangle([sx0 + th, y, sx1 - 1, y + h * 0.6], fill=0)
        serif(sx0 + th / 2, y, th + h * 0.22)
        serif(sx1 + tn / 2, y, tn + h * 0.2)
        return w
    if ch == "R":
        w = h * 0.84
        sx = x + h * 0.08
        bar(sx, y, sx + th, y + h)
        # bowl
        bw = w * 0.62
        d.ellipse([sx - th * 0.2, y, sx + bw, y + h * 0.55], fill=fill)
        d.ellipse([sx + th, y + tn, sx + bw - th * 0.85, y + h * 0.55 - tn], fill=0)
        d.rectangle([sx - th * 0.2, y, sx + th * 0.5, y + h * 0.55], fill=fill)
        d.rectangle([sx - th, y - 2, sx - 1, y + h + 2], fill=0)
        # leg: thick diagonal to the bottom right
        lx0 = sx + th * 1.2
        d.polygon([(lx0, y + h * 0.5), (lx0 + th * 1.05, y + h * 0.5),
                   (x + w + h * 0.02, y + h), (x + w - th * 1.15, y + h)], fill=fill)
        serif(sx + th / 2, y, th + h * 0.22)
        serif(sx + th / 2, y + h, th + h * 0.24, top=False)
        bar(x + w - th * 1.3, y + h - sf, x + w + h * 0.1, y + h)
        return w + h * 0.08
    if ch == "B":
        w = h * 0.74
        sx = x + h * 0.08
        bar(sx, y, sx + th, y + h)
        d.ellipse([sx, y, sx + w * 0.78, y + h * 0.5], fill=fill)
        d.ellipse([sx + th, y + tn, sx + w * 0.78 - th * 0.85, y + h * 0.5 - tn], fill=0)
        d.ellipse([sx, y + h * 0.44, sx + w * 0.98, y + h], fill=fill)
        d.ellipse([sx + th, y + h * 0.44 + tn, sx + w * 0.98 - th, y + h - tn], fill=0)
        bar(sx, y, sx + th, y + h)
        bar(sx, y, sx + w * 0.4, y + tn)
        bar(sx, y + h - tn, sx + w * 0.5, y + h)
        bar(sx, y + h * 0.46, sx + w * 0.45, y + h * 0.46 + tn)
        d.rectangle([sx - th, y - 2, sx - 1, y + h + 2], fill=0)
        serif(sx + th / 2, y, th + h * 0.22)
        serif(sx + th / 2, y + h, th + h * 0.22, top=False)
        return w
    if ch == "S":
        w = h * 0.66
        cx = x + w / 2
        # upper bowl (thin) and lower bowl (thick), joined by a thick spine
        d.ellipse([x + h * 0.04, y, x + w - h * 0.02, y + h * 0.54], fill=fill)
        d.ellipse([x + h * 0.04 + th, y + tn, x + w - h * 0.02 - tn * 1.2, y + h * 0.54 - th * 0.8], fill=0)
        d.ellipse([x, y + h * 0.46, x + w, y + h], fill=fill)
        d.ellipse([x + tn * 1.2, y + h * 0.46 + th * 0.8, x + w - th, y + h - tn], fill=0)
        # open the counters: cut away the right of the top bowl, left of the bottom one
        d.rectangle([cx + h * 0.02, y + h * 0.17, x + w + 2, y + h * 0.43], fill=0)
        d.rectangle([x - 2, y + h * 0.57, cx - h * 0.06, y + h * 0.82], fill=0)
        # terminal serifs
        bar(x + w - h * 0.09, y, x + w + h * 0.01, y + h * 0.2)
        bar(x - h * 0.02, y + h * 0.8, x + h * 0.08, y + h)
        return w
    raise KeyError(ch)


def _mask_word(word, h, gap):
    W_, H_ = 260 * SS, (h + 8) * SS
    m = Image.new("L", (W_, H_), 0)
    d = ImageDraw.Draw(m)
    x = 4 * SS
    for ch in word:
        w = _letter_shapes(d, ch, x, 4 * SS, h * SS)
        x += w + gap * SS
    width = int(x / SS) + 4
    small = m.resize((W_ // SS, H_ // SS), Image.BOX)
    pts = set()
    for yy in range(small.height):
        for xx in range(min(width, small.width)):
            if small.getpixel((xx, yy)) >= 128:
                pts.add((xx, yy))
    return pts


def _chisel(img, pts, ox, oy):
    """Raised chiselled letters: facets facing up-left lit, down-right shaded,
    a crisp ridge where they meet."""
    def run(x, y, dx, dy):
        n = 0
        while (x + dx * (n + 1), y + dy * (n + 1)) in pts:
            n += 1
        return n
    for (x, y) in pts:
        dl, du = run(x, y, -1, 0), run(x, y, 0, -1)
        dr, dd = run(x, y, 1, 0), run(x, y, 0, 1)
        a = min(dl, du)        # distance to an up/left edge
        b = min(dr, dd)        # distance to a down/right edge
        if a == 0 and b > 0:
            c = W
        elif b == 0:
            c = G[1]
        elif a < b:
            c = M[1] if a < 2 else M[0]
        elif a > b:
            c = TV[1] if b < 2 else TV[0]
        else:
            c = TV[2]
        px(img, ox + x, oy + y, c)


def _laurel_branch(layer, pts_curve, flip=False):
    """Leaves alternating along a polyline; each leaf lit on its upper side."""
    stem = []
    for i in range(len(pts_curve) - 1):
        (x0, y0), (x1, y1) = pts_curve[i], pts_curve[i + 1]
        line(layer, int(round(x0)), int(round(y0)), int(round(x1)), int(round(y1)), OL[0])
        stem.append(((x0 + x1) / 2, (y0 + y1) / 2, x1 - x0, y1 - y0))
    for k, (sx, sy, dx, dy) in enumerate(stem):
        ln = math.hypot(dx, dy) or 1
        ux, uy = dx / ln, dy / ln
        side = 1 if k % 2 == 0 else -1
        # leaf axis: 45 degrees off the stem, forward
        ax = ux * 0.7 + (-uy) * side * 0.7
        ay = uy * 0.7 + (ux) * side * 0.7
        L, Wd = 6.5, 2.4
        cx, cy = sx + ax * L / 2, sy + ay * L / 2
        for yy in range(int(cy - L), int(cy + L) + 1):
            for xx in range(int(cx - L), int(cx + L) + 1):
                rx, ry = xx + 0.5 - cx, yy + 0.5 - cy
                along = rx * ax + ry * ay
                perp = -rx * ay + ry * ax
                if abs(along) <= L / 2 and abs(perp) <= Wd / 2 * (1 - (2 * along / L) ** 2) + 0.35:
                    up = perp * (-1 if ax < 0 else 1)
                    c = GR[3] if (perp < 0) == (ay > 0) else GR[1]
                    if abs(perp) < 0.5:
                        c = GR[2]
                    px(layer, xx, yy, c)
    # berries
    for k, (sx, sy, dx, dy) in enumerate(stem):
        if k % 3 == 1:
            px(layer, int(sx), int(sy) + 1, OC[3])


def logo():
    Wd, Hd = 200, 60
    img = new(Wd, Hd)
    h = 38
    pts = _mask_word("URBS", h, 5)
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    pw = max(xs) - min(xs) + 1
    ox = (Wd - pw) // 2 - min(xs)
    oy = 5 - min(ys)

    # laurel branches beneath the word, crossing at the centre
    lay = new(Wd, Hd)
    for side in (-1, 1):
        curve = []
        for i in range(13):
            t = i / 12.0
            x = Wd / 2 + side * (6 + t * 88)
            y = 55 - 26 * t * t - 4 * t
            curve.append((x, y))
        _laurel_branch(lay, curve)
    # ribbon knot
    for (x, y) in ((97, 53), (98, 54), (99, 55), (100, 55), (101, 54), (102, 53),
                   (98, 56), (97, 57), (101, 56), (102, 57), (99, 54), (100, 54)):
        px(lay, x, y, RD[2] if y < 56 else RD[1])
    outline(lay, OUT)
    img.alpha_composite(lay)

    # terracotta drop shadow, offset down-right
    sh = new(Wd, Hd)
    for (x, y) in pts:
        for (dx, dy, c) in ((3, 3, TR[0]), (2, 2, TR[1]), (1, 1, TR[1])):
            px(sh, ox + x + dx, oy + y + dy, c)
    outline(sh, OUT)
    img.alpha_composite(sh)

    let = new(Wd, Hd)
    _chisel(let, pts, ox, oy)
    outline(let, OUT)
    img.alpha_composite(let)
    return img


# ---------------------------------------------------------------- export
def generate():
    out = [("ui/logo", logo(), None),
           ("ui/panel_corner", panel_corner(), None),
           ("ui/divider", divider(), None)]
    g = globals()
    for name in ICONS:
        out.append(("ui/icon_" + name, g["icon_" + name](), None))
    return out
