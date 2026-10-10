"""Regional building variants.

Provinces build with their own materials.  A variant is the base sprite with
palette ramps swapped, so shapes (and anchors) stay identical and the C++
renderer can pick ``<name>@<region>`` when a scenario sets ``"region"``:

  north  Gaul, Germania, Britannia: slate roofs, timber walls
  east   Greece, Syria: whitewashed walls, pale stone roofs
  egypt  Egypt: mudbrick walls, earthen roofs

Only housing and the most common civic buildings get variants; everything else
falls back to the Italian sprite.
"""
from palette import COLORS, ROLES

REGIONS = ("north", "east", "egypt")

# Base sprites that get regional variants.
BASES = ["bld/house_plot_t%d" % t for t in range(5)] + ["bld/domus_plot_t%d" % t for t in range(3)] + [
    "bld/" + b for b in (
        "market", "granary", "warehouse", "stockpile", "shrine", "temple", "tavern", "latrina", "balneum",
        "bakery", "mill", "potter", "weaver", "forum", "vigiles_post", "engineers_post", "physician", "school",
        "guard_post", "trade_post", "barracks", "militia_post", "library", "basilica", "theater")]


def _ramp(role):
    idx = ROLES[role]
    return (idx,) if isinstance(idx, int) else tuple(idx)


def _map_ramp(src, dst):
    """Map each colour of ramp src onto ramp dst, keeping relative lightness."""
    s, d = _ramp(src), _ramp(dst)
    return {COLORS[s[i]][:3]: COLORS[d[min(len(d) - 1, round(i * (len(d) - 1) / max(1, len(s) - 1)))]][:3]
            for i in range(len(s))}


def _remap_for(region):
    m = {}
    if region == "north":
        m.update(_map_ramp("roof_terracotta", "gray"))     # slate
        m.update(_map_ramp("travertine", "earth"))         # timber and wattle
    elif region == "east":
        m.update(_map_ramp("roof_terracotta", "travertine"))  # pale stone, flat roofs
        m.update(_map_ramp("travertine", "marble"))           # whitewash
    elif region == "egypt":
        m.update(_map_ramp("roof_terracotta", "earth"))    # earthen roofs
        m.update(_map_ramp("travertine", "ochre"))         # mudbrick
    return m


def variants(sprites):
    """sprites: list of (name, image, anchor) -> list of regional variants."""
    by_name = {n: (img, a) for n, img, a in sprites}
    out = []
    for region in REGIONS:
        remap = _remap_for(region)
        for base in BASES:
            if base not in by_name:
                continue
            img, anchor = by_name[base]
            v = img.copy()
            px = v.load()
            for y in range(v.height):
                for x in range(v.width):
                    c = px[x, y]
                    if c[3] and c[:3] in remap:
                        px[x, y] = remap[c[:3]] + (c[3],)  # keep the ground flag
            out.append(("%s@%s" % (base, region), v, anchor))
    return out
