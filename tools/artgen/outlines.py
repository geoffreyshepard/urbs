"""Selective outlines ("sel-out").

Generators outline every solid part with the near-black C.outline.  As a last step, the outer
silhouette of each sprite is recoloured with the dark partner of the material it borders (warm
for terracotta and red, green for foliage, slate for stone, a soft brown-grey for white walls...),
so sprites keep a clear edge without the hard black line of older pixel art.  Interior lines and
details drawn in C.outline (windows, doorways, arches) are left as they are.
"""
from palette import C, COLORS, ROLES

FAMILY = {
    "roof_terracotta": "ol_warm", "roman_red": "ol_warm", "skin": "ol_warm",
    "earth": "ol_earth", "grass": "ol_green", "olive": "ol_green", "cypress": "ol_green",
    "gray": "ol_stone", "marble": "ol_light", "white": "ol_light", "travertine": "ol_light",
    "sea": "ol_blue", "tyrian": "ol_purple", "gold": "ol_gold",
}
OUTLINED_FAMILIES = ("bld/", "people/", "obj/tree", "obj/bush", "obj/boulder", "fx/cart", "fx/ship")


def _partner_table():
    t = {}
    for role, ol in FAMILY.items():
        idx = ROLES[role]
        for i in ((idx,) if isinstance(idx, int) else idx):
            t[COLORS[i][:3]] = getattr(C, ol)
    for i in ROLES["ochre"]:                      # dark ochre reads as earth, light ochre as gold
        t[COLORS[i][:3]] = C.ol_earth if i in ROLES["ochre"][:2] else C.ol_gold
    return t


PARTNER = _partner_table()
OUT = C.outline[:3]


def selout(img):
    """Recolour outline pixels on the sprite's silhouette (in place); returns img."""
    w, h = img.size
    p = img.load()
    src = img.copy().load()
    for y in range(h):
        for x in range(w):
            c = src[x, y]
            if c[3] == 0 or c[:3] != OUT:
                continue
            exterior = False
            votes = {}
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, -1), (-1, 1), (1, 1)):
                xx, yy = x + dx, y + dy
                if not (0 <= xx < w and 0 <= yy < h) or src[xx, yy][3] == 0:
                    if abs(dx) + abs(dy) == 1:
                        exterior = True
                    continue
                n = src[xx, yy][:3]
                if n == OUT or n not in PARTNER:
                    continue
                k = PARTNER[n]
                votes[k] = votes.get(k, 0) + (2 if abs(dx) + abs(dy) == 1 else 1)
            if exterior and votes:
                best = max(sorted(votes), key=lambda k: votes[k])
                p[x, y] = best[:3] + (c[3],)
    return img


def apply(sprites):
    """sprites: list of (name, image, anchor); recolours matching families in place."""
    for name, img, _ in sprites:
        if name.startswith(OUTLINED_FAMILIES) and "_tile_" not in name:
            selout(img)
    return sprites
