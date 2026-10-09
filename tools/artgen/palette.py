"""URBS locked palette.

Loads assets/palettes/urbs.pal (JASC-PAL, 48 colors) and exposes named color
roles.  Every ramp is ordered dark -> light, so ``ramp[0]`` is the shadow and
``ramp[-1]`` the highlight.  Colors are RGBA tuples ready for Pillow.

    from palette import C
    C.grass[0]            # darkest grass
    C.roof_terracotta[2]  # lit roof tile
    C.outline             # sprite outline
"""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PAL_PATH = os.path.join(ROOT, "assets", "palettes", "urbs.pal")

TRANSPARENT = (0, 0, 0, 0)


def load_pal(path=PAL_PATH):
    with open(path, "r") as f:
        lines = [ln.strip() for ln in f.read().splitlines() if ln.strip()]
    if lines[0] != "JASC-PAL" or lines[1] != "0100":
        raise ValueError("%s: not a JASC-PAL file" % path)
    n = int(lines[2])
    cols = []
    for ln in lines[3:3 + n]:
        r, g, b = (int(v) for v in ln.split())
        cols.append((r, g, b, 255))
    if len(cols) != n:
        raise ValueError("%s: expected %d colors, got %d" % (path, n, len(cols)))
    return cols


COLORS = load_pal()
assert len(COLORS) == 48, "URBS palette must have exactly 48 colors"
PALETTE_SET = frozenset(COLORS)

# role name -> palette indices (dark -> light)
ROLES = {
    "outline": 0,
    "gray": (1, 2, 3, 4),
    "marble": (5, 6, 7),
    "white": 8,
    "travertine": (9, 10, 11),
    "roof_terracotta": (12, 13, 14, 15),
    "ochre": (16, 17, 18, 19),          # also sand
    "earth": (20, 21, 22, 23),          # sun-baked earth, wood, soil
    "grass": (24, 25, 26, 27),
    "olive": (28, 29, 30),
    "cypress": (31, 32, 33),            # dark conifer / forest floor
    "sea": (34, 35, 36, 37, 38),        # deep -> shallow -> foam
    "roman_red": (39, 40, 41),
    "tyrian": (42, 43),
    "skin": (44, 45, 46),
    "gold": 47,
}


class _Roles:
    def __init__(self):
        for name, idx in ROLES.items():
            if isinstance(idx, int):
                setattr(self, name, COLORS[idx])
            else:
                setattr(self, name, tuple(COLORS[i] for i in idx))
        # convenience aliases
        self.sand = self.ochre
        self.terracotta = self.roof_terracotta
        self.stucco = (self.travertine[1], self.travertine[2], self.marble[0], self.marble[1])
        self.wood = self.earth
        self.metal = self.gray
        self.water_deep = (self.sea[0], self.sea[1])
        self.water_shallow = (self.sea[2], self.sea[3])
        self.foam = self.sea[4]
        self.transparent = TRANSPARENT


C = _Roles()


def bad_pixels(img, limit=10):
    """Return up to `limit` (x, y, rgba) pixels that are neither fully
    transparent nor exact palette colors."""
    img = img.convert("RGBA")
    w, _ = img.size
    out = []
    for i, p in enumerate(img.getdata()):
        if p[3] == 0:
            continue
        if p not in PALETTE_SET:
            out.append((i % w, i // w, p))
            if len(out) >= limit:
                break
    return out


def check_image(img, name="<image>"):
    """Raise ValueError if any pixel is off-palette or semi-transparent."""
    bad = bad_pixels(img)
    if bad:
        raise ValueError("%s: %d+ off-palette pixels, e.g. %s" % (name, len(bad), bad[:3]))
    return True


if __name__ == "__main__":
    for name in ROLES:
        print(name, getattr(C, name))
