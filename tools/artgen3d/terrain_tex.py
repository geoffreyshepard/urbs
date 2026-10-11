#!/usr/bin/env python3
"""Flat ground art for the isometric view: terrain, transitions, cliff faces and roads.

    python3 tools/artgen3d/terrain_tex.py

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
  road/<dirt|paved>_<c|n|ne|...|fill_ne|...>
"""
import json
import os

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


def voronoi(n, count, seed, period_cells=1):
    """Periodic Voronoi on an n x n texture: (F1 distance, F2 - F1, cell id), distances in texture px."""
    rng = np.random.default_rng(seed)
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


ROAD_DIRS = {"n": (0.5, 0.0), "ne": (1.0, 0.0), "e": (1.0, 0.5), "se": (1.0, 1.0),
             "s": (0.5, 1.0), "sw": (0.0, 1.0), "w": (0.0, 0.5), "nw": (0.0, 0.0)}
FILL_CORNERS = {"ne": (1.0, 0.0), "se": (1.0, 1.0), "sw": (0.0, 1.0), "nw": (0.0, 0.0)}


def road_textures():
    n = RES
    big = fbm(n, 2, 3, 900)
    mid = fbm(n, 8, 3, 901)
    fine = fbm(n, 32, 2, 902)
    speck = vnoise(n, 64, 903)
    dirt = mix(hexc("#a3845c"), hexc("#8e6f4a"), smoothstep(0.3, 0.7, big * 0.5 + mid * 0.5))
    dirt = mix(dirt, hexc("#b89d78"), smoothstep(0.8, 0.95, speck) * 0.5)
    dirt = mix(dirt, hexc("#776046"), smoothstep(0.75, 0.9, 1 - speck) * 0.4)
    dirt = dirt * shade(mid * 2 + fine * 2 + speck, 2.0)[..., None]
    f1, edge, ident = voronoi(n, 13, 905)
    per = np.random.default_rng(906).random(ident.max() + 1)[ident]
    stone = mix(hexc("#6c6a66"), hexc("#55534f"), per * 0.8 + fine * 0.2)
    stone = mix(stone, hexc("#7d7a74"), smoothstep(0.75, 0.95, speck) * 0.3)
    gap = 1 - smoothstep(0.6, 2.2, edge)
    dome = np.clip(f1 / 9.0, 0, 1)
    paved = mix(stone, hexc("#3a3733"), gap * 0.85)
    paved = paved * shade(-dome * 5 - gap * 3 + fine, 1.4)[..., None]
    return {"dirt": dirt, "paved": paved}, fbm(n, 4, 3, 910) - 0.5


def roads():
    texs, wobble = road_textures()
    tx, ty, w, h, ax, ay = iso_grid(1, 1, 1.0, 1.0)
    s, t = tx, ty  # tile (0, 0): local coords are tile coords
    wob = sample(wobble, s, t, 1)
    tile = inside(s, t, 0, 1, 0, 1)
    for kind, half in (("dirt", 0.25), ("paved", 0.28)):
        col = sample(texs[kind], s, t, 1)
        e = 0.035 if kind == "dirt" else 0.012
        amp = 0.07 if kind == "dirt" else 0.015

        def emit(name, dist):
            a = smoothstep(half + e, half - e, dist + wob * amp) * tile
            save("road/%s_%s" % (kind, name), finish(col, a, w, h), ax, ay)

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


def main():
    os.makedirs(os.path.join(OUT, "meta"), exist_ok=True)
    try:
        with open(META) as f:
            _meta.update(json.load(f))
    except (OSError, ValueError):
        pass
    dual_cells()
    cliffs()
    roads()
    with open(META, "w") as f:
        json.dump(_meta, f, indent=1, sort_keys=True)
        f.write("\n")
    print("terrain_tex: %d sprites in meta" % len(_meta))


if __name__ == "__main__":
    main()
