"""Night window lights: bld/<name>_lit masks.

Windows and doorways are small closed clusters of C.outline pixels inside a building sprite
(see kit.window, placeholder._window), with dark earth below.  For each building sprite this
finds those clusters (at most 4x4 px, fully surrounded by other opaque pixels), keeps a stable
subset of them, and paints them warm gold in an otherwise transparent sprite of the same size
and anchor.  The renderer adds the mask over the graded frame at night (render/renderer.cpp).
"""
from palette import C
from primitives import new, px, hash2

OUT = C.outline[:3]
DARK = {C.outline[:3], C.earth[0][:3]}
MAX_SIDE = 4
WALL = {c[:3] for c in (C.marble + C.travertine + C.gray[2:] + C.ochre[1:] + C.earth[2:] +
                        C.roof_terracotta[1:] + C.roman_red[1:] + (C.white,))}
LIT_SHARE = 7  # of 10 windows
# open-air structures: no lamps behind their dark openings
UNLIT = ("statue", "well", "fountain", "triumphal_arch", "reservoir", "clay_pit", "sand_pit", "pozzolana_pit",
         "quarry", "limestone_quarry", "marble_quarry", "iron_mine", "silver_mine", "lumber_camp", "training_ground",
         "circus", "circus_maximus", "salt_works", "house_plot", "domus_plot")


def _clusters(img):
    w, h = img.size
    p = img.load()
    seen = set()
    out = []
    for y in range(h):
        for x in range(w):
            if (x, y) in seen or p[x, y][3] == 0 or p[x, y][:3] != OUT:
                continue
            stack, comp, closed = [(x, y)], [], True
            seen.add((x, y))
            while stack:
                cx, cy = stack.pop()
                comp.append((cx, cy))
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = cx + dx, cy + dy
                    if not (0 <= nx < w and 0 <= ny < h) or p[nx, ny][3] == 0:
                        closed = False
                        continue
                    if (nx, ny) not in seen and p[nx, ny][:3] in DARK:
                        seen.add((nx, ny))
                        stack.append((nx, ny))
                if len(comp) > MAX_SIDE * MAX_SIDE:
                    closed = False
            xs = [c[0] for c in comp]
            ys = [c[1] for c in comp]
            bw, bh = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
            if not (closed and 2 <= bw <= MAX_SIDE and 2 <= bh <= MAX_SIDE):
                continue
            if len(comp) < 0.8 * bw * bh:          # windows are filled rectangles, props are not
                continue
            ring = [(x, y) for x in range(min(xs) - 1, max(xs) + 2) for y in (min(ys) - 1, max(ys) + 1)]
            ring += [(x, y) for y in range(min(ys), max(ys) + 1) for x in (min(xs) - 1, max(xs) + 1)]
            wall = sum(1 for (x, y) in ring if 0 <= x < w and 0 <= y < h and p[x, y][:3] in WALL)
            if wall >= 0.6 * len(ring):            # set into a wall face
                out.append(comp)
    return out


def lit_mask(name, img):
    m = new(img.width, img.height)
    n = 0
    for i, comp in enumerate(_clusters(img)):
        x0, y0 = min(comp)
        if hash2(x0, y0, len(name)) % 10 >= LIT_SHARE:
            continue
        for (x, y) in comp:
            px(m, x, y, C.gold if (x + y) % 3 else C.ochre[3])
        n += 1
    return m if n else None


def generate(sprites):
    """sprites: list of (name, image, anchor) -> [(name_lit, mask, anchor)] for buildings."""
    out = []
    for name, img, anchor in sprites:
        if not name.startswith("bld/") or "@" in name or "_tile_" in name or name.endswith("_hut"):
            continue
        if any(name.startswith("bld/" + p) for p in ("wall_", "aqueduct_", "pipe_", "sewer_", "bridge_", "placeholder")):
            continue
        if name[4:] in UNLIT:
            continue
        mask = lit_mask(name, img)
        if mask is not None:
            out.append((name + "_lit", mask, anchor))
    return out
