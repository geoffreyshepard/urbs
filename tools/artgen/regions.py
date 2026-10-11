"""Regional building variants.

Provinces build with their own materials.  A variant is the base sprite with
palette ramps swapped, so shapes (and anchors) stay identical and the C++
renderer can pick ``<name>@<region>`` when a scenario sets ``"region"``:

  north  Gaul, Germania, Britannia: slate roofs; houses get half-timbered walls
  east   Greece, Syria: whitewashed walls; houses get flat pale roof terraces
  egypt  Egypt: mudbrick walls; houses get flat earthen roof terraces with parapets

Only housing and the most common civic buildings get variants; everything else
falls back to the Italian sprite.
"""
from palette import C, COLORS, ROLES

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
    m[C.ol_warm[:3]] = {"north": C.ol_stone, "east": C.ol_light, "egypt": C.ol_earth}[region][:3]
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


HOUSING = tuple(["bld/house_plot_t%d" % t for t in range(1, 5)] + ["bld/domus_plot_t%d" % t for t in range(3)])


def _roof_mask(img):
    roof = {COLORS[i][:3] for i in _ramp("roof_terracotta")}
    p = img.load()
    return {(x, y) for y in range(img.height) for x in range(img.width) if p[x, y][3] and p[x, y][:3] in roof}


def _flat_roof(img, mask, base, lit, shade, detail, key):
    """Repaint a pitched roof as a flat roof terrace: plain deck, a parapet lit on its top/left
    edge and shaded on its bottom/right, and a little clutter (stair hatch, jars, a drying mat)."""
    from primitives import hash2, rng
    p = img.load()
    for (x, y) in mask:
        c = base[0] if hash2(x, y, 31) % 9 else base[1]
        if (x, y - 1) not in mask or (x - 1, y) not in mask:
            c = lit
        elif (x, y + 1) not in mask or (x + 1, y) not in mask:
            c = shade
        elif (x, y - 2) not in mask or (x - 2, y) not in mask:
            c = shade                                  # inner foot of the parapet
        p[x, y] = c[:3] + (p[x, y][3],)
    inner = [(x, y) for (x, y) in mask if all((x + dx, y + dy) in mask for dx in (-3, 0, 3) for dy in (-3, 0, 3))]
    if not inner:
        return
    r = rng("flatroof/" + key)
    inner.sort()
    hx, hy = inner[r.randrange(len(inner))]
    for dx in range(2):                                # stair hatch
        for dy in range(2):
            p[hx + dx, hy + dy] = C.outline[:3] + (255,) if dy == 0 else C.earth[0][:3] + (255,)
    jx, jy = inner[r.randrange(len(inner))]           # a row of storage jars: lit top, dark foot
    for k in range(3):
        if (jx + 2 * k, jy + 1) in mask:
            p[jx + 2 * k, jy] = detail[0][:3] + (255,)
            p[jx + 2 * k, jy + 1] = detail[1][:3] + (255,)
    mx, my = inner[r.randrange(len(inner))]           # drying mat
    for dx in range(3):
        for dy in range(2):
            if (mx + dx, my + dy) in mask:
                p[mx + dx, my + dy] = (C.gold if (dx + dy) % 2 else C.ochre[3])[:3] + (255,)


def _timber(img, roof):
    """Half-timbering on stucco facades below a roof: posts every 5 px, sill and head beams,
    and a brace in every other panel."""
    stucco = {c[:3] for c in (C.marble[0], C.marble[1], C.travertine[2])}
    p = img.load()
    beam = C.earth[1][:3]
    roof_cols = {x for (x, _) in roof}
    for x in range(img.width):
        if x not in roof_cols:
            continue
        ys = [y for y in range(img.height) if p[x, y][3] and p[x, y][:3] in stucco and
              any((x, yy) in roof for yy in range(max(0, y - 30), y))]
        if not ys:
            continue
        runs, start = [], ys[0]
        for a, b in zip(ys, ys[1:] + [None]):
            if b != a + 1:
                runs.append((start, a))
                start = b
        for y in ys:                                   # daub infill, warmer than stucco
            p[x, y] = (C.ochre[3] if (x + y // 2) % 7 else C.travertine[2])[:3] + (p[x, y][3],)
            if x % 6 == 0:                             # posts run the full height, past the windows
                p[x, y] = beam + (p[x, y][3],)
        for (y0, y1) in runs:
            if y1 - y0 < 4:
                continue
            for y in (y0, y1):
                p[x, y] = beam + (p[x, y][3],)
            if x % 6 == 0:
                for y in range(y0, y1 + 1):
                    p[x, y] = beam + (p[x, y][3],)
            elif (x // 6) % 2 == 0 and y1 - y0 >= 6:  # brace: rises left to right across the panel
                k = x % 6
                y = y1 - round(k * (y1 - y0) / 6)
                if y0 < y < y1:
                    p[x, y] = beam + (p[x, y][3],)


def _shape(region, name, img, roof):
    if name not in HOUSING:
        return
    if region == "egypt":
        _flat_roof(img, roof, (C.ochre[1], C.earth[3]), C.ochre[3], C.earth[1], (C.roof_terracotta[2], C.earth[0]),
                   name)
    elif region == "east":
        _flat_roof(img, roof, (C.marble[1], C.marble[0]), C.white, C.travertine[1], (C.roof_terracotta[2], C.earth[1]),
                   name)
    elif region == "north":
        _timber(img, roof)


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
            roof = _roof_mask(img)
            v = img.copy()
            px = v.load()
            for y in range(v.height):
                for x in range(v.width):
                    c = px[x, y]
                    if c[3] and c[:3] in remap:
                        px[x, y] = remap[c[:3]] + (c[3],)  # keep the ground flag
            _shape(region, base, v, roof)
            out.append(("%s@%s" % (base, region), v, anchor))
    return out
