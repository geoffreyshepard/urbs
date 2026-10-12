#!/usr/bin/env python3
"""Flat ground art for the isometric view: terrain, transitions, cliff faces and roads.

    python3 tools/artgen3d/terrain_tex.py [terrain] [cliffs] [shores] [roads]   (default: all)

Ground is flat, so it is painted in top-down tile space with periodic noise and projected onto
the 2:1 iso diamond (128 x 64 px per tile).  3D objects that stand on the ground (trees, bushes,
boulders, resource markers) are Blender renders: see terrain.py.

Terrain textures repeat every PERIOD tiles; a dual cell's sprite is picked by its position class
p = (x & 1) + 2 * (y & 1), so neighbouring cells continue the same texture and the alpha masks of
transitions continue across cell borders (docs/13-isometric.md).  Every ground sprite is drawn a
couple of pixels past its diamond so neighbours overlap instead of leaving hairline seams.

Outputs (assets/sprites_iso, anchors in meta/terrain.json; ground anchor = bottom vertex):
  terrain/<t>_<p>            base of a dual cell (the cell's lowest-priority terrain)
  terrain/dg_<t>_<mask>_<p>  higher terrain over a dual cell; mask bits NW=1 NE=2 SW=4 SE=8
  terrain/q_<t>_<k>_<p>      quarter k (0 NW, 1 NE, 2 SW, 3 SE) of a dual cell, for cells whose
                             tiles sit on different height levels
  terrain/cliff_s_<v>, cliff_e_<v>  one height level of cliff under a tile's south / east edge
  terrain/shore_<mask>_<p>, shore_x<open>_<p>  shore pieces for water tiles next to land (data
                             for shaders/ground.frag, not colour; see shores())
  road/<dirt|paved>_<c|n|ne|...|fill_ne|...|q_ne|...>  road surfaces
  road/<dirt|paved>_k_<c|n|...|fill_ne|...>         their edge bands (dirt: worn edge, paved: kerb)
"""
import json
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "assets", "sprites_iso")
META = os.path.join(OUT, "meta", "terrain.json")

PERIOD = 2          # terrain texture period, tiles
RES = 128           # texture px per tile (tile space)
SS = 3              # supersampling for the iso projection
EXPAND = 2.0        # px every ground sprite extends past its diamond
LEVEL_PX = 16       # iso px per height level (iso::kLevelPx)

TERRAINS = ["grass", "fertile", "sand", "rock", "forest", "marsh", "shallow", "deep"]
# Toward the sun in tile space (x east, y south, z up): from the screen's left, high.
LIGHT = np.array([-0.62, 0.62, 0.92]) / np.linalg.norm([-0.62, 0.62, 0.92])


def hexc(h):
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.float64) / 255.0


# ---------------------------------------------------------------- periodic noise

def vnoise(n, cells, seed):
    """n x n periodic value noise with a cells x cells lattice, smooth (quintic) interpolation."""
    g = np.random.default_rng(seed).random((cells, cells))
    c = np.arange(n) * cells / n
    i0 = np.floor(c).astype(int) % cells
    i1 = (i0 + 1) % cells
    f = c - np.floor(c)
    f = f * f * f * (f * (f * 6 - 15) + 10)
    a = g[np.ix_(i0, i0)]
    b = g[np.ix_(i0, i1)]
    cc = g[np.ix_(i1, i0)]
    d = g[np.ix_(i1, i1)]
    fy = f[:, None]
    fx = f[None, :]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (cc * (1 - fx) + d * fx) * fy


def fbm(n, cells, octaves, seed, gain=0.5):
    out = np.zeros((n, n))
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        out += amp * vnoise(n, cells * (2 ** o), seed + 17 * o)
        tot += amp
        amp *= gain
    return out / tot


def voronoi(n, count, seed, period_cells=1, jitter_grid=0):
    """Periodic Voronoi on an n x n texture: (F1 distance, F2 - F1, cell id), distances in texture px.
    jitter_grid=g: one point per cell of a g x g grid (even, paving-like polygons) instead of count
    random points."""
    rng = np.random.default_rng(seed)
    if jitter_grid:
        g = jitter_grid
        gy, gx = np.mgrid[0:g, 0:g]
        pts = (np.stack([gx.ravel(), gy.ravel()], 1) + 0.15 + 0.7 * rng.random((g * g, 2))) * (n / g)
    else:
        pts = rng.random((count, 2)) * n
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float64) + 0.5
    best = np.full((n, n), 1e9)
    second = np.full((n, n), 1e9)
    ident = np.zeros((n, n), dtype=int)
    for k, (px, py) in enumerate(pts):
        for ox in (-n, 0, n):
            for oy in (-n, 0, n):
                d = np.hypot(xx - (px + ox), yy - (py + oy))
                closer = d < best
                second = np.where(closer, best, np.minimum(second, d))
                ident = np.where(closer, k, ident)
                best = np.where(closer, d, best)
    return best, second - best, ident


def shade(height, strength):
    """Lambert shading of a periodic height field (tile-space px), 1.0 on flat ground."""
    gx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * strength
    gy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * strength
    nz = 1.0 / np.sqrt(gx * gx + gy * gy + 1.0)
    ndotl = (-gx * LIGHT[0] - gy * LIGHT[1] + LIGHT[2]) * nz
    return 0.62 + 0.38 * ndotl / LIGHT[2]


def mix(a, b, t):
    t = np.clip(t, 0.0, 1.0)[..., None]
    return a * (1 - t) + b * t


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


# ---------------------------------------------------------------- terrain textures (tile space)

def tex_terrain(name):
    """RGB texture (N x N x 3, linear 0..1 in sRGB terms) for one period of a terrain."""
    n = PERIOD * RES
    seed = 1000 + TERRAINS.index(name) * 101
    big = fbm(n, 2 * PERIOD, 3, seed)
    mid = fbm(n, 8 * PERIOD, 3, seed + 1)
    fine = fbm(n, 32 * PERIOD, 2, seed + 2)
    speck = vnoise(n, 64 * PERIOD, seed + 3)
    if name in ("grass", "fertile", "forest"):
        pal = {"grass": ("#6f8f3c", "#5d7c31", "#93a25f", "#55702c"),
               "fertile": ("#62893a", "#527830", "#7f9d4c", "#4a6a2a"),
               "forest": ("#4f6a2c", "#415a25", "#6b6a3a", "#33491f")}[name]
        col = mix(hexc(pal[0]), hexc(pal[1]), smoothstep(0.35, 0.65, big * 0.6 + mid * 0.4))
        col = mix(col, hexc(pal[2]), smoothstep(0.72, 0.9, speck) * 0.55)       # dry blade tips
        col = mix(col, hexc(pal[3]), smoothstep(0.75, 0.95, 1 - speck) * 0.5)   # dark clumps
        if name == "fertile":
            soil = smoothstep(0.62, 0.75, fbm(n, 6 * PERIOD, 3, seed + 9))
            col = mix(col, hexc("#7a6040"), soil * 0.35)
        if name == "forest":
            litter = smoothstep(0.66, 0.82, fbm(n, 16 * PERIOD, 2, seed + 7))
            col = mix(col, hexc("#6b5532"), litter * 0.55)
        h = mid * 3 + fine * 2 + speck * 1.5
        return col * shade(h, 2.0)[..., None]
    if name == "sand":
        yy, xx = np.mgrid[0:n, 0:n] / RES
        warp = fbm(n, 4 * PERIOD, 2, seed + 5) * 1.6
        ripple = 0.5 + 0.5 * np.sin(2 * np.pi * (5 * (xx + yy) / PERIOD * 2 + warp))
        col = mix(hexc("#d8c18e"), hexc("#c7ab76"), smoothstep(0.3, 0.7, big * 0.6 + mid * 0.4))
        col = mix(col, hexc("#e6d4a4"), smoothstep(0.8, 0.95, speck) * 0.4)
        h = ripple * 1.2 + mid * 2 + fine
        return col * shade(h, 1.6)[..., None]
    if name == "rock":
        f1, edge, ident = voronoi(n, 40 * PERIOD * PERIOD, seed + 4)
        per = np.random.default_rng(seed + 6).random(ident.max() + 1)[ident]
        col = mix(hexc("#9a9180"), hexc("#7f7767"), smoothstep(0.3, 0.7, big * 0.5 + mid * 0.5))
        stones = smoothstep(0.55, 0.75, per) * smoothstep(1.0, 3.0, edge)
        col = mix(col, hexc("#8a8678"), stones * 0.45)
        crack = (1 - smoothstep(0.0, 1.2, edge)) * smoothstep(0.45, 0.6, mid)
        col = mix(col, hexc("#5f594e"), crack * 0.5)
        col = mix(col, hexc("#b0a794"), smoothstep(0.84, 0.97, speck) * 0.35)
        col = mix(col, hexc("#6f7a45"), smoothstep(0.68, 0.8, fbm(n, 6 * PERIOD, 2, seed + 8)) * 0.35)
        h = stones * 3 - crack * 2 + mid * 2 + fine * 2
        return col * shade(h, 2.0)[..., None]
    if name == "marsh":
        col = mix(hexc("#5f6c3a"), hexc("#4c5a31"), smoothstep(0.35, 0.65, big * 0.5 + mid * 0.5))
        pud = smoothstep(0.6, 0.66, fbm(n, 6 * PERIOD, 3, seed + 8))
        col = mix(col, hexc("#3f5b56"), pud * 0.85)
        col = mix(col, hexc("#7d8a4c"), smoothstep(0.8, 0.95, speck) * (1 - pud) * 0.5)
        h = mid * 2 + fine * 2 - pud * 2
        return col * shade(h, 1.8)[..., None]
    if name == "shallow":
        col = mix(hexc("#5aa3b6"), hexc("#4a91ab"), smoothstep(0.3, 0.7, big))
        col = mix(col, hexc("#9cb79c"), smoothstep(0.55, 0.8, mid) * 0.22)        # sandy bottom
        col = mix(col, hexc("#a9d6dc"), smoothstep(0.78, 0.9, fine * 0.5 + speck * 0.5) * 0.25)
        return col
    if name == "deep":
        col = mix(hexc("#3a7ba3"), hexc("#2f6b93"), smoothstep(0.3, 0.7, big))
        col = mix(col, hexc("#5e9cbf"), smoothstep(0.78, 0.9, fine * 0.5 + mid * 0.5) * 0.3)
        return col
    raise ValueError(name)


def sample(tex, tx, ty, period):
    """Bilinear sample of a periodic tile-space texture at tile coords (arrays)."""
    n = tex.shape[0]
    u = (tx % period) * n / period - 0.5
    v = (ty % period) * n / period - 0.5
    x0 = np.floor(u).astype(int)
    y0 = np.floor(v).astype(int)
    fx = (u - x0)[..., None] if tex.ndim == 3 else u - x0
    fy = (v - y0)[..., None] if tex.ndim == 3 else v - y0
    x0 %= n
    y0 %= n
    x1 = (x0 + 1) % n
    y1 = (y0 + 1) % n
    a, b, c, d = tex[y0, x0], tex[y0, x1], tex[y1, x0], tex[y1, x1]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


# ---------------------------------------------------------------- iso projection

def iso_grid(fw, fh, bx, by, extra_h=0):
    """Supersampled pixel centres of a ground canvas -> tile coords.

    Canvas: width (fw + fh) * 64 + 2 * pad, height (fw + fh) * 32 + extra_h + 2 * pad; the
    footprint's bottom vertex (tile point (bx, by)) sits at (fw * 64 + pad, H - pad).
    Returns (tx, ty, W, H, ax, ay).
    """
    pad = int(np.ceil(EXPAND))
    w = (fw + fh) * 64 + 2 * pad
    h = (fw + fh) * 32 + extra_h + 2 * pad
    ax, ay = fw * 64 + pad, h - pad
    ys, xs = np.mgrid[0:h * SS, 0:w * SS].astype(np.float64)
    X = (xs + 0.5) / SS - ax
    Y = (ys + 0.5) / SS - ay
    tx = bx + (X / 64 + Y / 32) / 2
    ty = by + (Y / 32 - X / 64) / 2
    return tx, ty, w, h, ax, ay


def finish(rgb, alpha, w, h):
    """Downsample supersampled premultiplied colour + coverage to an RGBA image."""
    rgb = np.clip(rgb, 0, 1) * alpha[..., None]
    rgb = rgb.reshape(h, SS, w, SS, 3).mean(axis=(1, 3))
    a = alpha.reshape(h, SS, w, SS).mean(axis=(1, 3))
    out = np.zeros((h, w, 4))
    nz = a > 1e-4
    out[..., :3][nz] = rgb[nz] / a[nz][..., None]
    out[..., 3] = a
    return Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA")


def inside(s, t, s0, s1, t0, t1):
    """Coverage of the tile-space rect [s0,s1] x [t0,t1], grown by EXPAND iso px."""
    e = EXPAND / 64.0
    return ((s >= s0 - e) & (s <= s1 + e) & (t >= t0 - e) & (t <= t1 + e)).astype(np.float64)


# ---------------------------------------------------------------- outputs

_meta = {}


def save(name, img, ax, ay):
    path = os.path.join(OUT, name + ".png")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    _meta[name] = {"ax": int(ax), "ay": int(ay)}


def dual_cells():
    """Base, transition and quarter sprites for every terrain and position class."""
    texs = {t: tex_terrain(t) for t in TERRAINS}
    # Transition shapes: continuous noise in tile space (period PERIOD) per terrain.
    n = PERIOD * RES
    edge_noise = {t: fbm(n, 5 * PERIOD, 3, 500 + i, gain=0.6) - 0.5 for i, t in enumerate(TERRAINS)}
    soft = {"shallow": 0.16, "deep": 0.16, "marsh": 0.09, "sand": 0.07}
    for p in range(4):
        px, py = p & 1, p >> 1
        # dual cell with bottom vertex at tile point (px + 0.5, py + 0.5): local (s, t) in [0, 1]
        tx, ty, w, h, ax, ay = iso_grid(1, 1, px + 0.5, py + 0.5)
        s = tx - (px - 0.5)
        t = ty - (py - 0.5)
        sc, tc = np.clip(s, 0, 1), np.clip(t, 0, 1)
        full = inside(s, t, 0, 1, 0, 1)
        quarters = [inside(s, t, 0, 0.5, 0, 0.5), inside(s, t, 0.5, 1, 0, 0.5),
                    inside(s, t, 0, 0.5, 0.5, 1), inside(s, t, 0.5, 1, 0.5, 1)]
        for tn in TERRAINS:
            col = sample(texs[tn], tx, ty, PERIOD)
            save("terrain/%s_%d" % (tn, p), finish(col, full, w, h), ax, ay)
            for k in range(4):
                save("terrain/q_%s_%d_%d" % (tn, k, p), finish(col, quarters[k], w, h), ax, ay)
            if tn == "deep":
                continue
            noise = sample(edge_noise[tn], tx, ty, PERIOD)
            e = soft.get(tn, 0.05)
            # bilinear corner field, shaped so blobs are round, plus the edge noise
            ss, ts = smoothstep(0, 1, sc), smoothstep(0, 1, tc)
            for mask in range(1, 15):
                c = [(mask >> b) & 1 for b in range(4)]  # NW, NE, SW, SE
                f = (c[0] * (1 - ss) * (1 - ts) + c[1] * ss * (1 - ts) + c[2] * (1 - ss) * ts
                     + c[3] * ss * ts)
                amp = {"sand": 0.45, "shallow": 0.5, "marsh": 0.6}.get(tn, 0.7)
                v = f + noise * amp * (1 - np.abs(2 * f - 1) ** 2)
                a = smoothstep(0.5 - e, 0.5 + e, v) * full
                cc = col
                if tn in ("grass", "fertile", "forest"):
                    rim = smoothstep(0.5 + e, 0.5 + 3 * e, v)
                    cc = col * (0.86 + 0.14 * rim)[..., None]   # tufted edge, a touch darker
                save("terrain/dg_%s_%d_%d" % (tn, mask, p), finish(cc, a, w, h), ax, ay)


def cliffs():
    """One height level of earth-and-rock face under a tile's south (lit) or east (shaded) edge."""
    n = 128
    for v in range(2):
        strata_n = fbm(n, 8, 3, 700 + v)
        grain = fbm(n, 32, 2, 720 + v)
        for side in ("s", "e"):
            pad = int(np.ceil(EXPAND))
            w = 64 + 2 * pad
            h = 32 + LEVEL_PX + 2 * pad
            # south face: canvas spans the tile's left half; edge from (0, 32) to (64, 64)
            ax = 64 + pad if side == "s" else pad
            ay = h - pad
            ys, xs = np.mgrid[0:h * SS, 0:w * SS].astype(np.float64)
            X = (xs + 0.5) / SS - ax
            Y = (ys + 0.5) / SS - ay
            if side == "s":
                u = (X + 64) / 64          # 0 at the left vertex, 1 at the bottom vertex
                edge = (u - 1) * 32         # edge y relative to the anchor
            else:
                u = X / 64                  # 0 at the bottom vertex, 1 at the right vertex
                edge = -u * 32
            z = (edge - Y) / LEVEL_PX       # 0 at the bottom of the face, 1 at the top
            e = EXPAND / LEVEL_PX
            cov = ((u >= -EXPAND / 64) & (u <= 1 + EXPAND / 64) & (z >= -e) & (z <= 1 + e))
            cov = cov.astype(np.float64)
            uu = np.clip(u, 0, 1)
            zz = np.clip(z, 0, 1)
            band = sample(strata_n, uu * 0.25 + (0.5 if side == "e" else 0), zz * 0.5, 1)
            gr = sample(grain, uu, zz, 1)
            col = mix(hexc("#8b7457"), hexc("#a08a68"), smoothstep(0.4, 0.6, band))
            col = mix(col, hexc("#6a5641"), smoothstep(0.6, 0.8, gr) * 0.5)
            col = mix(col, hexc("#5d6a35"), smoothstep(0.82, 0.95, zz) * 0.6)  # turf lip
            col = col * (0.8 + 0.2 * zz)[..., None]                            # darker at the foot
            col = col * (1.0 if side == "s" else 0.7)
            save("terrain/cliff_%s_%d" % (side, v), finish(col, cov, w, h), ax, ay)


# ---------------------------------------------------------------- shores (beaches)
#
# A water tile next to land gets a shore piece that slopes from the land's edge down under the water:
# a beach below low land, scree at the foot of rock cliffs (the game picks the paint, see
# docs/13-isometric.md "Coasts").  Which of the tile's four corners touch land (bit NW=1,
# NE=2, SW=4, SE=8, as the dual grid) picks the piece: a marching-squares field v over the tile is 1
# at those corners and 0 at the others (bilinear; the saddles 6 and 9 keep their water connected),
# the waterline is v = 0.5, and the sand rises to one level at v = 1.  Every piece agrees with its
# neighbours along shared edges, so a diagonal coast becomes a straight line instead of a staircase.
# A tile whose four corners all touch land (a channel or an inlet one tile wide) uses a lagoon piece
# instead, shore_x<o>: water around the tile's centre that reaches the middle of each edge whose
# neighbour is another such tile (bit N=1, E=2, S=4, W=8).
#
# The pieces are data, not colour; ground.frag paints sand, wet sand, foam and shallows from them
# (render/terrain.hpp, kKindShore):
#   R  slope shading (Lambert, 1.0 = flat ground) / 1.5
#   G  surface height in levels above the water, (h + 0.6) / 1.6; below 0 the sand is under water
#   B  the field v (0.5 on the waterline)
#   A  coverage
SHORE_TOP = 1.0      # levels: the beach rises one level, to the land's edge (or a cliff's foot)
SHORE_FLOOR = -0.6   # deepest sand depth encoded
SHORE_POW = 1.3      # beach profile above the water: 1 - (1 - u) ** POW, flat where it meets the land
SHORE_WOBBLE = 0.18  # noise on the field: the waterline meanders instead of running ruler-straight
LAGOON_R = 0.2       # lagoon pieces: half width of the water channel, tiles
LEVEL_TILES = 0.204  # one height level in tile lengths (16 iso px under a 30 degree camera)


def shore_field(mask, s, t, noise):
    """Marching-squares field over a tile (s, t in [0, 1]) for corner mask bits NW=1 NE=2 SW=4 SE=8."""
    c = [(mask >> b) & 1 for b in range(4)]
    basis = [(1 - s) * (1 - t), s * (1 - t), (1 - s) * t, s * t]
    if mask in (6, 9):  # saddle: two land corners across the tile, the water between them stays open
        # A 4-norm instead of max(): the same on the edges, with no crease where the two meet.
        a = c[0] * basis[0] + c[3] * basis[3]
        b = c[1] * basis[1] + c[2] * basis[2]
        v = (a ** 4 + b ** 4) ** 0.25
    else:
        v = sum(ci * b for ci, b in zip(c, basis))
    return v + noise * 4 * v * (1 - v)


def lagoon_field(open_bits, s, t, noise):
    """Field of a tile with land at all four corners: water along a skeleton from the centre to the
    middle of each open edge (N=1 E=2 S=4 W=8), sand everywhere else."""
    d = np.hypot(s - 0.5, t - 0.5)
    for b, (ex, ey) in enumerate(((0.5, 0.0), (1.0, 0.5), (0.5, 1.0), (0.0, 0.5))):
        if open_bits >> b & 1:
            vx, vy = ex - 0.5, ey - 0.5
            k = np.clip(((s - 0.5) * vx + (t - 0.5) * vy) / (vx * vx + vy * vy), 0, 1)
            d = np.minimum(d, np.hypot(s - 0.5 - k * vx, t - 0.5 - k * vy))
    v = np.where(d < LAGOON_R, 0.5 * d / LAGOON_R, 0.5 + 0.5 * np.clip((d - LAGOON_R) / (0.5 - LAGOON_R), 0, 1))
    return v + noise * 4 * v * (1 - v)


def shore_height(v):
    """Beach profile: SHORE_TOP at v = 1, the waterline at v = 0.5, a shallow shelf below it."""
    u = np.clip((v - 0.5) / 0.5, 0, 1)
    above = SHORE_TOP * (1 - (1 - u) ** SHORE_POW)
    below = np.maximum(SHORE_FLOOR, -(0.5 - v) * 1.2)
    return np.where(v >= 0.5, above, below)


def shores():
    n = PERIOD * RES
    wobble = fbm(n, 3 * PERIOD, 3, 760, gain=0.55) - 0.5
    pad = int(np.ceil(EXPAND))
    w, h = 128 + 2 * pad, 64 + LEVEL_PX + 2 * pad
    ax, ay = 64 + pad, h - pad  # the tile's bottom vertex on the water's level
    ys, xs = np.mgrid[0:h * SS, 0:w * SS].astype(np.float64)
    X = (xs + 0.5) / SS - ax
    Y = (ys + 0.5) / SS - ay
    e = EXPAND / 64.0
    steps = 96

    g = 320  # height field grid over the tile, cells per tile
    gt, gs = np.mgrid[0:g + 1, 0:g + 1].astype(np.float64) / g

    def lut(grid, s, t):
        """Bilinear lookup of a (g+1)^2 grid over the tile at (s, t), clamped to the tile."""
        u = np.clip(s, 0, 1) * g
        w_ = np.clip(t, 0, 1) * g
        i = np.minimum(np.floor(u).astype(int), g - 1)
        j = np.minimum(np.floor(w_).astype(int), g - 1)
        fu, fw = u - i, w_ - j
        return ((grid[j, i] * (1 - fu) + grid[j, i + 1] * fu) * (1 - fw)
                + (grid[j + 1, i] * (1 - fu) + grid[j + 1, i + 1] * fu) * fw)

    def render(name, field, p):
        px, py = p & 1, p >> 1
        nz = sample(wobble, gs + px, gt + py, PERIOD) * SHORE_WOBBLE
        vg = np.clip(field(gs, gt, nz), 0, 1)
        hg = shore_height(vg)
        dgy, dgx = np.gradient(hg, 1.0 / g)

        def hfield(s, t):
            return lut(hg, s, t), lut(vg, s, t)

        # March each pixel's view ray down from the top of the canvas to the water surface: on level z
        # the pixel sees tile point s = (X/64 + (Y + 16 z)/32)/2 + 1, t = ((Y + 16 z)/32 - X/64)/2 + 1.
        hit = np.zeros(X.shape, bool)
        hz = np.full(X.shape, np.nan)
        for k in range(steps + 1):
            z = SHORE_TOP * (1 - k / steps)
            b = (Y + LEVEL_PX * z) / 32
            s = (X / 64 + b) / 2 + 1
            t = (b - X / 64) / 2 + 1
            ins = (s >= -e) & (s <= 1 + e) & (t >= -e) & (t <= 1 + e)
            hh, _ = hfield(s, t)
            new = ins & ~hit & (z <= hh + 1e-6)
            hz[new] = z
            hit |= new
        # Where the ray reaches the water first, take the sand under the surface point.
        b = Y / 32
        s0 = (X / 64 + b) / 2 + 1
        t0 = (b - X / 64) / 2 + 1
        water = ~hit & (s0 >= -e) & (s0 <= 1 + e) & (t0 >= -e) & (t0 <= 1 + e)
        z = np.where(hit, hz, 0.0)
        bz = (Y + LEVEL_PX * z) / 32
        s = (X / 64 + bz) / 2 + 1
        t = (bz - X / 64) / 2 + 1
        hh, v = hfield(s, t)
        gx = np.where(hit, lut(dgx, s, t) * LEVEL_TILES, 0.0)
        gy = np.where(hit, lut(dgy, s, t) * LEVEL_TILES, 0.0)
        nzz = 1.0 / np.sqrt(gx * gx + gy * gy + 1.0)
        ndotl = (-gx * LIGHT[0] - gy * LIGHT[1] + LIGHT[2]) * nzz
        shade_v = 0.62 + 0.38 * ndotl / LIGHT[2]
        cov = (hit | water).astype(np.float64)
        data = np.stack([shade_v / 1.5, (hh - SHORE_FLOOR) / (SHORE_TOP - SHORE_FLOOR), v], -1)
        save(name, finish(data, cov, w, h), ax, ay)

    for p in range(4):
        for mask in range(1, 15):
            render("terrain/shore_%d_%d" % (mask, p), lambda s, t, nz, m=mask: shore_field(m, s, t, nz), p)
        for o in range(16):
            render("terrain/shore_x%d_%d" % (o, p), lambda s, t, nz, o=o: lagoon_field(o, s, t, nz), p)


ROAD_DIRS = {"n": (0.5, 0.0), "ne": (1.0, 0.0), "e": (1.0, 0.5), "se": (1.0, 1.0),
             "s": (0.5, 1.0), "sw": (0.0, 1.0), "w": (0.0, 0.5), "nw": (0.0, 0.0)}
FILL_CORNERS = {"ne": (1.0, 0.0), "se": (1.0, 1.0), "sw": (0.0, 1.0), "nw": (0.0, 0.0)}


def road_textures():
    """Tile-space textures (period 1 tile): surfaces and the edge band under them."""
    n = RES
    big = fbm(n, 2, 3, 900)
    mid = fbm(n, 8, 3, 901)
    fine = fbm(n, 32, 2, 902)
    speck = vnoise(n, 64, 903)
    # Dirt: light packed earth, pale dusty patches, a few pebbles.
    dirt = mix(hexc("#c8ad80"), hexc("#b4976a"), smoothstep(0.3, 0.7, big * 0.6 + mid * 0.4))
    dirt = mix(dirt, hexc("#d8c398"), smoothstep(0.6, 0.8, mid) * 0.4)
    dirt = mix(dirt, hexc("#dccba6"), smoothstep(0.88, 0.98, speck) * 0.25)
    dirt = mix(dirt, hexc("#9a8060"), smoothstep(0.9, 0.99, 1 - speck) * 0.2)
    dirt = dirt * shade(mid * 2 + fine * 0.6, 1.0)[..., None]
    # Dirt edge: darker trodden earth breaking into grass tufts.
    tuft = smoothstep(0.45, 0.75, fbm(n, 24, 2, 907))
    dirt_edge = mix(hexc("#a88d64"), hexc("#927a54"), smoothstep(0.3, 0.7, mid))
    dirt_edge = mix(dirt_edge, hexc("#7a8a40"), tuft * 0.6)
    dirt_edge = dirt_edge * shade(mid * 2 + tuft * 3 + fine, 1.6)[..., None]
    # Paved: polygonal grey stone (basalt-ish, worn light), dark joints, each stone domed.
    f1, edge, ident = voronoi(n, 0, 905, jitter_grid=6)
    per = np.random.default_rng(906).random(ident.max() + 1)[ident]
    stone = mix(hexc("#c2bcaf"), hexc("#a6a197"), per * 0.85 + fine * 0.15)
    stone = mix(stone, hexc("#b0afa8"), smoothstep(0.5, 0.8, big) * 0.35)
    stone = mix(stone, hexc("#c9c2b2"), smoothstep(0.8, 0.97, speck) * 0.25)
    gap = 1 - smoothstep(0.3, 1.3, edge)
    dome = np.clip(f1 / 9.0, 0, 1)
    paved = mix(stone, hexc("#736d62"), gap * 0.7)
    paved = paved * shade(-dome * 3 - gap * 2 + fine * 0.5, 1.2)[..., None]
    # Kerb: cream travertine blocks with fine joints.
    kf1, kedge, kid = voronoi(n, 70, 908)
    kper = np.random.default_rng(909).random(kid.max() + 1)[kid]
    kerb = mix(hexc("#e2d6bb"), hexc("#cdbf9f"), kper * 0.8 + fine * 0.2)
    kgap = 1 - smoothstep(0.4, 1.3, kedge)
    kerb = mix(kerb, hexc("#8a7f68"), kgap * 0.7)
    kerb = kerb * shade(-np.clip(kf1 / 6.0, 0, 1) * 2 - kgap * 2, 1.2)[..., None]
    return ({"dirt": dirt, "paved": paved}, {"dirt": dirt_edge, "paved": kerb}, fbm(n, 4, 3, 910) - 0.5,
            fbm(n, 12, 2, 911) - 0.5)


# Road widths (tile units).  Streets fill most of the tile diamond: the surface's half width, then
# the edge band around it (dirt: worn earth and tufts, soft; paved: a kerb of cream blocks).
ROAD_HALF = {"dirt": 0.39, "paved": 0.35}
ROAD_EDGE = {"dirt": 0.08, "paved": 0.105}


def roads():
    """Road pieces.  Every piece is a surface sprite road/<k>_<piece> plus its edge band
    road/<k>_k_<piece> (the band only, from the surface outward).  The game draws all edge bands of
    a tile under all its surfaces, so the union of the surfaces hides the bands' inner overlaps and
    a kerb shows only along the street's true outline, in every autotile shape."""
    texs, edges, wobble, wob2 = road_textures()
    tx, ty, w, h, ax, ay = iso_grid(1, 1, 1.0, 1.0)
    s, t = tx, ty  # tile (0, 0): local coords are tile coords
    wob = sample(wobble, s, t, 1)
    wb2 = sample(wob2, s, t, 1)
    tile = inside(s, t, 0, 1, 0, 1)
    for kind in ("dirt", "paved"):
        half, band = ROAD_HALF[kind], ROAD_EDGE[kind]
        col = sample(texs[kind], s, t, 1)
        ecol = sample(edges[kind], s, t, 1)
        if kind == "dirt":
            e, amp, eamp = 0.03, 0.05, 0.10
        else:
            e, amp, eamp = 0.008, 0.0, 0.006

        def emit(name, dist, edge=True):
            d = dist + wob * amp
            a = smoothstep(half + e, half - e, d) * tile
            # (no edge shading here: surfaces overlap each other, so any rim would show inside the street)
            save("road/%s_%s" % (kind, name), finish(col, a, w, h), ax, ay)
            if not edge:
                return
            de = dist + wob * amp + wb2 * eamp
            if kind == "dirt":  # worn edge fading into the grass
                outer = smoothstep(half + band, half + band * 0.3, de)
                shade_in = 1.0 - 0.12 * smoothstep(half + band, half, de)
                ea = outer * smoothstep(half - 2 * e, half, d) * tile
                save("road/%s_k_%s" % (kind, name), finish(ecol * shade_in[..., None], ea, w, h), ax, ay)
            else:  # kerb: crisp outer edge with a thin dark lip where it meets the ground
                outer = smoothstep(half + band + 0.008, half + band - 0.004, de)
                lip = smoothstep(half + band - 0.02, half + band, de)
                cc = ecol * (1.0 - 0.35 * lip)[..., None]
                ea = outer * smoothstep(half - 2 * e, half, d) * tile
                save("road/%s_k_%s" % (kind, name), finish(cc, ea, w, h), ax, ay)

        emit("c", np.hypot(s - 0.5, t - 0.5))
        for d, (ex, ey) in ROAD_DIRS.items():
            # distance to the segment from the centre to the edge midpoint / corner
            vx, vy = ex - 0.5, ey - 0.5
            L2 = vx * vx + vy * vy
            k = np.clip(((s - 0.5) * vx + (t - 0.5) * vy) / L2, 0, 1)
            emit(d, np.hypot(s - 0.5 - k * vx, t - 0.5 - k * vy))
        for c, (cx, cy) in FILL_CORNERS.items():
            # the diagonal road passing through this corner, between the two side neighbours
            dx, dy = (1.0, 1.0) if c in ("ne", "sw") else (1.0, -1.0)
            dist = np.abs((s - cx) * dy - (t - cy) * dx) / np.sqrt(2)
            emit("fill_" + c, dist)
            # the tile quarter at this corner, paved solid: where two orthogonal neighbours and the
            # diagonal one between them are all road (a wide street or a square), no kerb island
            qs = (s >= 0.5) if cx > 0.5 else (s <= 0.5)
            qt = (t >= 0.5) if cy > 0.5 else (t <= 0.5)
            emit("q_" + c, np.where(qs & qt, 0.0, 1.0), edge=False)


def main():
    os.makedirs(os.path.join(OUT, "meta"), exist_ok=True)
    try:
        with open(META) as f:
            _meta.update(json.load(f))
    except (OSError, ValueError):
        pass
    parts = sys.argv[1:] or ["terrain", "cliffs", "shores", "roads"]  # e.g. terrain_tex.py roads
    if "terrain" in parts:
        dual_cells()
    if "cliffs" in parts:
        cliffs()
    if "shores" in parts:
        shores()
    if "roads" in parts:
        roads()
    with open(META, "w") as f:
        json.dump(_meta, f, indent=1, sort_keys=True)
        f.write("\n")
    print("terrain_tex: %d sprites in meta" % len(_meta))


if __name__ == "__main__":
    main()
