#!/usr/bin/env python3
"""UI chrome generator: carved travertine frames, bronze trim, parchment, Pompeian-red plaques.

Every piece is a small height field shaded with the world's sun (upper left), coloured with procedural
stone / bronze / parchment albedo, and packed into one atlas at 2 texels per UI point:

    assets/sprites_iso/ui/chrome/chrome.png   the atlas (RGBA, straight alpha)
    assets/sprites_iso/ui/chrome/chrome.json  per sprite: rect, 9-slice margins, outset, tiling

Margins and outsets are in texels. `o` (outset) is how far the picture reaches beyond the rectangle it skins (a drop
shadow, a rosette); `m` are the 9-slice margins measured from the picture's edges; `t` is the period of the centre
(0 = stretch); `i` is the width of a frame's border, inside the rectangle (content starts beyond it). Run:  python3 tools/uiart/gen_chrome.py [--preview out.png]
Never edit the PNG by hand; change this script and run it again (docs/07-art-pipeline.md, "UI chrome").
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_DIR = os.path.join(ROOT, "assets", "sprites_iso", "ui", "chrome")


# ---------------------------------------------------------------------------------------------------------------------
# Colour and noise helpers


def hexc(s):
    s = s.lstrip("#")
    return np.array([int(s[i : i + 2], 16) / 255.0 for i in (0, 2, 4)])


TRAV_LO, TRAV_HI = hexc("#cbb48c"), hexc("#eadfc4")
PARCH = hexc("#f2e7cd")
RED, RED_HI, RED_LO = hexc("#8e3324"), hexc("#a8432e"), hexc("#5e1d14")
IVORY = hexc("#f3e9cf")
GOLD = hexc("#d9b25a")
SHADOW = hexc("#1c1108")

# Light from the upper left and a little in front, as the world's sun.
LIGHT = np.array([-0.55, -0.62, 0.66])
LIGHT /= np.linalg.norm(LIGHT)
HALF = LIGHT + np.array([0.0, 0.0, 1.0])
HALF /= np.linalg.norm(HALF)


def gauss_fft(a, sx, sy):
    """Periodic gaussian blur (sigma in texels along x and y)."""
    h, w = a.shape
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    k = np.exp(-2.0 * np.pi**2 * ((fx * sx) ** 2 + (fy * sy) ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(a) * k))


def blur(a, s):
    """Non-periodic blur (edges extended)."""
    p = int(3 * s) + 2
    b = np.pad(a, p, mode="edge")
    return gauss_fft(b, s, s)[p:-p, p:-p]


_noise_cache = {}


def pnoise(period, sx, sy, seed):
    """Tileable noise with period `period` in both axes, zero mean, unit deviation."""
    key = (period, sx, sy, seed)
    if key not in _noise_cache:
        r = np.random.default_rng(seed)
        n = gauss_fft(r.standard_normal((period, period)), sx, sy)
        _noise_cache[key] = (n - n.mean()) / (n.std() + 1e-9)
    return _noise_cache[key]


def sample(n, X, Y):
    p = n.shape[0]
    return n[np.asarray(Y, dtype=np.int64) % p, np.asarray(X, dtype=np.int64) % p]


def grid(w, h):
    Y, X = np.mgrid[0:h, 0:w]
    return X, Y


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def ramp(x, stops):
    """Piecewise-linear colour ramp; stops = [(pos, '#rrggbb'), ...]."""
    pos = np.array([p for p, _ in stops])
    cols = np.array([hexc(c) for _, c in stops])
    out = np.empty(x.shape + (3,))
    for i in range(3):
        out[..., i] = np.interp(x, pos, cols[:, i])
    return out


def normals(h):
    gy, gx = np.gradient(h)
    n = np.stack([-gx, -gy, np.ones_like(h)], -1)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def lit(h):
    """Lambert term normalised so a flat surface is 1, and the Blinn half-vector cosine."""
    n = normals(h)
    return (n @ LIGHT) / LIGHT[2], np.clip(n @ HALF, 0, 1)


def sd_rrect(X, Y, x0, y0, x1, y1, r):
    """Signed distance to a rounded rectangle (negative inside), at texel centres."""
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    hx, hy = (x1 - x0) / 2.0 - r, (y1 - y0) / 2.0 - r
    qx, qy = np.abs(X + 0.5 - cx) - hx, np.abs(Y + 0.5 - cy) - hy
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r


def cover(sd):
    return np.clip(0.5 - sd, 0.0, 1.0)


# ---------------------------------------------------------------------------------------------------------------------
# Materials


def travertine(X, Y, seed=11, period=256, light=0.0):
    """Travertine albedo: creamy, with faint layers running along x (pass swapped coordinates for vertical members),
    soft mottling and small elongated pores."""
    layers = sample(pnoise(period, 70, 1.6, seed), X, Y)
    mottle = sample(pnoise(period, 22, 16, seed + 1), X, Y)
    fine = sample(pnoise(period, 1.0, 0.8, seed + 2), X, Y)
    pores = sample(pnoise(period, 2.2, 0.8, seed + 3), X, Y)
    t = 0.64 + light + 0.035 * layers + 0.045 * mottle + 0.018 * fine
    t -= 0.06 * np.exp(-((layers - 1.2) / 0.08) ** 2)  # the odd thin darker layer
    c = TRAV_LO[None, None, :] + (TRAV_HI - TRAV_LO)[None, None, :] * np.clip(t, 0, 1.1)[..., None]
    c *= 1.0 - 0.20 * smoothstep(2.6, 3.1, pores)[..., None]  # pores
    return c


def pores_height(X, Y, seed=11, period=256):
    """Pores as small dents in a height field (so the light catches their lower lip)."""
    return -0.6 * smoothstep(2.6, 3.1, sample(pnoise(period, 2.2, 0.8, seed + 3), X, Y))


def parchment(X, Y, seed=21, period=256, base=PARCH):
    blot = sample(pnoise(period, 34, 30, seed), X, Y)
    blot2 = sample(pnoise(period, 9, 9, seed + 1), X, Y)
    fib = sample(pnoise(period, 7, 0.55, seed + 2), X, Y) + 0.6 * sample(pnoise(period, 0.55, 6, seed + 3), X, Y)
    fine = sample(pnoise(period, 0.7, 0.7, seed + 4), X, Y)
    k = 1.0 + 0.008 * blot + 0.008 * blot2 + 0.010 * fib + 0.007 * fine
    c = base[None, None, :] * k[..., None]
    # Age stains: soft patches going warm ochre rather than grey.
    stain = smoothstep(0.2, 2.2, -blot)[..., None] * 0.22
    c = c * (1 - stain) + (base * hexc("#e3c98f") / hexc("#f2e7cd"))[None, None, :] * c * stain
    return c


def fresco_red(X, Y, seed=31, period=128, base=RED):
    a = sample(pnoise(period, 16, 12, seed), X, Y)
    b = sample(pnoise(period, 2.5, 2.5, seed + 1), X, Y)
    c = sample(pnoise(period, 0.7, 0.7, seed + 2), X, Y)
    k = 1.0 + 0.05 * a + 0.025 * b + 0.02 * c
    return base[None, None, :] * k[..., None]


BRONZE_STOPS = [
    (-0.2, "#24150a"),
    (0.35, "#3f2610"),
    (0.70, "#6a4420"),
    (0.95, "#86592a"),
    (1.15, "#a8743a"),
    (1.40, "#c79246"),
    (1.75, "#e9c47c"),
]
GOLD_STOPS = [
    (-0.2, "#3a2408"),
    (0.40, "#6b4614"),
    (0.75, "#a07228"),
    (1.00, "#c39a44"),
    (1.25, "#dfbb64"),
    (1.60, "#f6dc98"),
]


def metal(h, stops=BRONZE_STOPS, spec=0.55, power=28):
    lam, nh = lit(h)
    c = ramp(lam, stops)
    s = (nh**power)[..., None]
    return np.clip(c + spec * s * hexc("#fff1c8")[None, None, :], 0, 1)


def stone_shade(albedo, h, amb=0.42, k=0.58, spec=0.06):
    lam, nh = lit(h)
    sh = amb + k * np.clip(lam, 0, 1.6)
    return np.clip(albedo * sh[..., None] + spec * (nh**12)[..., None], 0, 1)


def rgba(rgb, a):
    return np.concatenate([np.clip(rgb, 0, 1), np.clip(a, 0, 1)[..., None]], -1)


def over(top, bottom):
    """Straight-alpha 'over'."""
    ta, ba = top[..., 3:4], bottom[..., 3:4]
    oa = ta + ba * (1 - ta)
    rgb = (top[..., :3] * ta + bottom[..., :3] * ba * (1 - ta)) / np.maximum(oa, 1e-6)
    return np.concatenate([rgb, oa], -1)


def drop_shadow(mask, dx, dy, s, strength):
    m = np.roll(np.roll(mask, dy, 0), dx, 1)
    a = blur(m, s) * strength
    out = np.zeros(mask.shape + (4,))
    out[..., :3] = SHADOW
    out[..., 3] = a
    return out


# ---------------------------------------------------------------------------------------------------------------------
# Sprites. Each returns (rgba float image, meta dict).

SPRITES = {}


def sprite(name):
    def deco(fn):
        SPRITES[name] = fn
        return fn

    return deco


def bead(d, a, b, height):
    """A half-round bead profile across d in [a, b)."""
    t = np.clip((d - a) / (b - a), 0, 1)
    return height * np.sqrt(np.clip(1 - (2 * t - 1) ** 2, 0, 1))


def rosette(img, cx, cy, R, seed=5):
    """A bronze rosette stud with a soft shadow, painted over `img`."""
    h, w = img.shape[:2]
    X, Y = grid(w, h)
    dx, dy = X + 0.5 - cx, Y + 0.5 - cy
    r = np.hypot(dx, dy)
    th = np.arctan2(dy, dx)
    rn = r / R
    petals = 0.5 + 0.5 * np.cos(8 * th)
    dome = np.sqrt(np.clip(1 - rn**2, 0, 1))
    hgt = R * 0.32 * dome * (0.78 + 0.22 * petals * smoothstep(0.35, 0.6, rn))
    hgt += R * 0.18 * np.sqrt(np.clip(1 - (r / (R * 0.32)) ** 2, 0, 1))  # the central boss
    hgt -= R * 0.06 * smoothstep(0.30, 0.34, rn) * (1 - smoothstep(0.34, 0.40, rn))  # groove round the boss
    col = metal(hgt, spec=0.7)
    a = np.clip(R - r + 0.5, 0, 1)
    sh = drop_shadow((r < R).astype(float), 2, 3, R * 0.18, 0.55)
    out = over(sh, img)
    return over(rgba(col, a), out)


@sprite("panel")
def panel():
    """The big frame: drop shadow, carved travertine, a bronze bead, parchment with an inner shadow."""
    S, B, T, V, P = 30, 22, 6, 40, 256
    M = S + B + T + V
    N = 2 * M + P
    X, Y = grid(N, N)
    sd = sd_rrect(X, Y, S, S, N - S, N - S, 3.0)
    d = -sd  # depth inside the window edge
    # Which side each texel belongs to (mitred): grain runs along that side.
    dl, dr, dt, db = X + 0.5 - S, N - S - (X + 0.5), Y + 0.5 - S, N - S - (Y + 0.5)
    vertical = np.minimum(dl, dr) < np.minimum(dt, db)
    GX, GY = np.where(vertical, Y, X), np.where(vertical, X, Y)
    # Stone profile, outside in: a torus, a cove, a flat fillet, and a small step down to the bronze bead.
    hs = np.where(
        d < 9,
        2.0 + bead(d, -1, 9, 4.5),
        np.where(d < 15, 2.6 + 2.2 * (1 - np.sin(np.clip((d - 9) / 6, 0, 1) * math.pi / 2)) ** 1.6,
                 np.where(d < 20, 3.2, 3.2 - 1.6 * smoothstep(20, 22, d))),
    )
    hs = hs + pores_height(GX, GY, 11, P)
    stone = stone_shade(travertine(GX, GY, seed=11, period=P, light=0.04), hs * 1.5)
    hb = bead(d, B, B + T, 3.2) + 2.0
    bronze = metal(hb)
    par = parchment(X, Y, seed=21, period=P)
    # The frame shades the parchment: a cast shadow down-right of the top and left edges, and a soft vignette.
    inner = d - (B + T)
    frame_mask = (d < B + T).astype(float)
    cast = blur(np.roll(np.roll(frame_mask, 4, 0), 3, 1), 3.0)
    vign = 1 - smoothstep(0, V, inner)
    shade = 1 - 0.30 * np.clip(cast, 0, 1) - 0.07 * vign - 0.05 * vign**3
    par = par * shade[..., None]
    rgb = np.where((d < B)[..., None], stone, np.where((d < B + T)[..., None], bronze, par))
    # A thin dark line where stone meets bronze, and a highlight on the outer arris.
    rgb *= (1 - 0.35 * (np.abs(d - B) < 0.8))[..., None]
    img = rgba(rgb, cover(sd))
    img = over(img, drop_shadow((sd < 0).astype(float), 0, 7, 10, 0.55))
    for cx, cy in ((S + B / 2, S + B / 2), (N - S - B / 2, S + B / 2), (S + B / 2, N - S - B / 2),
                   (N - S - B / 2, N - S - B / 2)):
        img = rosette(img, cx, cy, 13.5)
    return img, {"m": [M] * 4, "o": [S] * 4, "t": [P, P], "inner": B + T}


@sprite("card")
def card():
    """Tooltips, popups, toasts: parchment on a slim bronze edge, a small shadow."""
    S, T, V, P = 16, 5, 16, 128
    M = S + T + V
    N = 2 * M + P
    X, Y = grid(N, N)
    sd = sd_rrect(X, Y, S, S, N - S, N - S, 4.0)
    d = -sd
    hb = np.where(d < T, bead(d, 0, T, 2.6) + 1.0, 0.0)
    bronze = metal(hb)
    par = parchment(X, Y, seed=23, period=P)
    inner = d - T
    cast = blur(np.roll(np.roll((d < T).astype(float), 2, 0), 2, 1), 1.6)
    vign = 1 - smoothstep(0, V, inner)
    par *= (1 - 0.22 * np.clip(cast, 0, 1) - 0.05 * vign)[..., None]
    rgb = np.where((d < T)[..., None], bronze, par)
    img = rgba(rgb, cover(sd))
    img = over(img, drop_shadow((sd < 0).astype(float), 0, 5, 6, 0.5))
    return img, {"m": [M] * 4, "o": [S] * 4, "t": [P, P], "inner": T}


def recess(name, base, rim=True, P=64, S=0, M=14, light=0.0, seed=51, depth=1.0):
    """A sunken well (inputs, child areas, top-bar slots): an inner shadow at the top left, a lip of light at the
    bottom right, an optional bronze hairline."""
    N = 2 * M + P
    X, Y = grid(N, N)
    sd = sd_rrect(X, Y, S, S, N - S, N - S, 4.0)
    d = -sd
    surf = base(X, Y)
    inner = (d > 0).astype(float)
    # Light falls in from the top left, so the rim shades the well's top and left sides.
    sh = blur(np.roll(np.roll(1 - inner, 3, 0), 3, 1), 2.4) * inner
    lip = blur(np.roll(np.roll(1 - inner, -2, 0), -2, 1), 1.2) * inner
    rgb = surf * (1 - 0.30 * depth * np.clip(sh, 0, 1) + 0.10 * np.clip(lip, 0, 1) + light)[..., None]
    if rim:
        line = np.clip(1.4 - np.abs(d - 0.9), 0, 1)
        rgb = rgb * (1 - line[..., None]) + hexc("#8a6131")[None, None, :] * line[..., None]
    return rgba(rgb, cover(sd)), {"m": [M] * 4, "o": [S] * 4, "t": [P, P] if P >= 64 else [0, 0]}


@sprite("inset")
def inset():
    return recess("inset", lambda X, Y: parchment(X, Y, seed=61, period=128, base=hexc("#ece0c2")), P=128, M=16)


@sprite("frame")
def frame():
    return recess("frame", lambda X, Y: parchment(X, Y, seed=62, period=64, base=hexc("#f6eedb")), P=32, M=10)


@sprite("frame_hov")
def frame_hov():
    return recess("frame", lambda X, Y: parchment(X, Y, seed=62, period=64, base=hexc("#fbf4e4")), P=32, M=10)


@sprite("frame_act")
def frame_act():
    return recess("frame", lambda X, Y: parchment(X, Y, seed=62, period=64, base=hexc("#e7d8b6")), P=32, M=10,
                  depth=1.4)


@sprite("slot")
def slot():
    """A recessed panel cut into the top bar's marble."""
    return recess("slot", lambda X, Y: travertine(X, Y, seed=71, period=64, light=-0.05) * 0.97, rim=False, P=64,
                  M=12, depth=1.3)


def button_img(face, rim_stops, w=128, h=60, pressed=False, flat=False, S=4, edge=1.0):
    """A raised stone button with a bronze rim and a short shadow."""
    W, H = w + 2 * S, h + 2 * S
    X, Y = grid(W, H)
    sd = sd_rrect(X, Y, S, S, W - S, H - S, 6.0)
    d = -sd
    R = 3.0  # bronze rim width
    rim_h = bead(d, 0, R, 1.6) + 1.0
    if flat:
        face_h = np.full_like(d, 1.0)
    else:
        face_h = 1.0 + 2.2 * smoothstep(R, R + 5, d) * (-1 if pressed else 1)
    hgt = np.where(d < R, rim_h, face_h)
    fx = face(X, Y)
    # Vertical gradient on the face: lighter at the top (or bottom when pressed).
    g = (Y - S) / h
    fx = fx * (1.06 - 0.12 * (1 - g if pressed else g))[..., None]
    rgb_face = stone_shade(fx, face_h * 1.6, amb=0.55, k=0.45)
    rgb_rim = metal(rim_h, rim_stops)
    rgb = np.where((d < R)[..., None], rgb_rim, rgb_face)
    img = rgba(rgb, cover(sd))
    if not pressed:
        img = over(img, drop_shadow((sd < 0).astype(float), 0, 2, 1.6, 0.45 * edge))
    return img


def _btn_meta():
    return {"m": [14] * 4, "o": [4] * 4, "t": [0, 0]}


def smooth_stone(X, Y, seed, period, light):
    """Polished travertine for button faces: the stone's colour and soft mottling, without pores."""
    mottle = sample(pnoise(period, 18, 12, seed), X, Y)
    layers = sample(pnoise(period, 60, 2.0, seed + 1), X, Y)
    t = 0.72 + light + 0.04 * mottle + 0.02 * layers
    return TRAV_LO[None, None, :] + (TRAV_HI - TRAV_LO)[None, None, :] * np.clip(t, 0, 1.15)[..., None]


def stone_face(light=0.0):
    return lambda X, Y: smooth_stone(X, Y, 81, 128, 0.08 + light)


@sprite("button")
def button():
    return button_img(stone_face(), BRONZE_STOPS), _btn_meta()


# Hover warms a face towards honey, so a hovered control reads at a glance and not only by its gilt rim.
WARM = np.array([1.03, 0.96, 0.82])


def warmed(face):
    return lambda X, Y: np.clip(face(X, Y) * WARM[None, None, :], 0, 1)


@sprite("button_hov")
def button_hov():
    return button_img(warmed(stone_face(0.12)), GOLD_STOPS), _btn_meta()


@sprite("button_act")
def button_act():
    return button_img(stone_face(-0.08), BRONZE_STOPS, pressed=True), _btn_meta()


@sprite("button_sel")
def button_sel():
    return button_img(lambda X, Y: fresco_red(X, Y, base=hexc("#953726")), GOLD_STOPS), _btn_meta()


@sprite("button_off")
def button_off():
    face = lambda X, Y: smooth_stone(X, Y, 81, 128, 0.0) * 0.92 + 0.06
    img = button_img(face, [(-0.2, "#6f6556"), (0.7, "#958a76"), (1.4, "#b9ad95"), (1.8, "#d2c7b0")], flat=True,
                     edge=0.4)
    return img, _btn_meta()


def medallion_img(face_col, rim_stops, D=96, S=6, pressed=False, glow=0.0, dish=1.0, lift=(1.0, 1.0), RW=5.5):
    """A round medallion with a slim bronze rim and an enamel face (icon buttons). `lift` scales the face's
    colour at its centre and at its edge: a soft lighter disc behind the icon."""
    N = D + 2 * S
    X, Y = grid(N, N)
    c = N / 2.0
    r = np.hypot(X + 0.5 - c, Y + 0.5 - c)
    R = D / 2.0
    rim_h = bead(r, R - RW, R, 2.6) + 1.5
    # A fine fillet just inside the rim.
    rim_h += 0.6 * np.clip(1 - np.abs(r - (R - RW - 0.8)), 0, 1)
    face_r = R - RW
    k = -1.0 if pressed else 1.0
    face_h = 1.0 - dish * k * 3.0 * (1 - np.clip(r / face_r, 0, 1) ** 2)  # concave dish
    face = face_col(X, Y)
    t2 = np.clip(r / face_r, 0, 1) ** 2
    face = face * (lift[0] * (1 - t2) + lift[1] * t2)[..., None]
    rgb_face = stone_shade(face, face_h * 1.5, amb=0.6, k=0.4)
    # The rim shades the dish's upper-left part.
    inner = (r < face_r).astype(float)
    sh = blur(np.roll(np.roll(1 - inner, 3, 0), 3, 1), 2.5) * inner
    rgb_face *= (1 - (0.42 if pressed else 0.28) * np.clip(sh, 0, 1))[..., None]
    rgb_rim = metal(rim_h, rim_stops)
    if glow:
        rgb_rim = np.clip(rgb_rim * (1 + glow), 0, 1)
    rgb = np.where((r > face_r)[..., None], rgb_rim, rgb_face)
    a = np.clip(R - r + 0.5, 0, 1)
    img = rgba(rgb, a)
    if not pressed:
        img = over(img, drop_shadow((r < R).astype(float), 1, 3, 2.2, 0.5))
    return img, {"m": [0] * 4, "o": [S] * 4, "t": [0, 0]}


def ivory(light=0.0):
    return lambda X, Y: parchment(X, Y, seed=91, period=128, base=np.clip(IVORY * (1 + light), 0, 1))


# The medallions' face: Pompeian black enamel, warm and a little lighter in the middle, so the icons (most of them
# light or gilt) stand out from it and the buttons stand out from the pale marble of the top bar.
ATRAMENTUM = hexc("#3a302a")


def black_enamel(light=0.0):
    return lambda X, Y: parchment(X, Y, seed=91, period=128, base=np.clip(ATRAMENTUM * (1 + light), 0, 1))


@sprite("medal")
def medal():
    return medallion_img(black_enamel(), BRONZE_STOPS, lift=(1.3, 0.8))


@sprite("medal_hov")
def medal_hov():
    return medallion_img(black_enamel(0.35), GOLD_STOPS, glow=0.05, lift=(1.35, 0.85))


@sprite("medal_act")
def medal_act():
    return medallion_img(black_enamel(-0.2), BRONZE_STOPS, pressed=True, lift=(1.1, 0.8))


@sprite("medal_sel")
def medal_sel():
    return medallion_img(lambda X, Y: fresco_red(X, Y, base=hexc("#a63d29")), GOLD_STOPS, dish=0.6, lift=(1.12, 0.85))


def tile_img(face_col, rim_stops, W=112, S=6, pressed=False, dish=1.0):
    """A square tile with a slim bronze rim (the build rail, building pictures)."""
    N = W + 2 * S
    X, Y = grid(N, N)
    sd = sd_rrect(X, Y, S, S, N - S, N - S, 7.0)
    d = -sd
    RW = 4.0
    rim_h = bead(d, 0, RW, 2.2) + 1.5
    k = -1.0 if pressed else 1.0
    face_h = 1.0 - dish * k * 1.2 * smoothstep(RW, RW + 7, d)
    face = face_col(X, Y)
    rgb_face = stone_shade(face, face_h * 1.5, amb=0.6, k=0.4)
    inner = (d >= RW).astype(float)
    sh = blur(np.roll(np.roll(1 - inner, 2, 0), 2, 1), 2.0) * inner
    rgb_face *= (1 - (0.36 if pressed else 0.22) * np.clip(sh, 0, 1))[..., None]
    rgb = np.where((d < RW)[..., None], metal(rim_h, rim_stops), rgb_face)
    img = rgba(rgb, cover(sd))
    if not pressed:
        img = over(img, drop_shadow((sd < 0).astype(float), 1, 2, 2.0, 0.45))
    return img, {"m": [14] * 4, "o": [S] * 4, "t": [0, 0]}


@sprite("tile")
def tile():
    return tile_img(ivory(), BRONZE_STOPS)


@sprite("tile_hov")
def tile_hov():
    return tile_img(warmed(ivory(0.03)), GOLD_STOPS)


@sprite("tile_act")
def tile_act():
    return tile_img(ivory(-0.08), BRONZE_STOPS, pressed=True)


@sprite("tile_sel")
def tile_sel():
    return tile_img(lambda X, Y: fresco_red(X, Y, base=hexc("#9a3a28")), GOLD_STOPS, dish=0.5)


@sprite("tile_off")
def tile_off():
    img, meta = tile_img(lambda X, Y: parchment(X, Y, seed=91, period=128, base=hexc("#ddd3bd")),
                         [(-0.2, "#6f6556"), (0.7, "#958a76"), (1.4, "#b9ad95"), (1.8, "#d2c7b0")])
    return img, meta


def highlight_img(alpha, col, lines):
    W, H, M = 96, 48, 16
    X, Y = grid(W, H)
    fx = smoothstep(0, M, X + 0.5) * smoothstep(0, M, W - X - 0.5)
    a = alpha * fx * (0.75 + 0.25 * np.sin(np.clip((Y + 0.5) / H, 0, 1) * math.pi))
    rgb = np.broadcast_to(col, (H, W, 3)).copy()
    if lines:
        ln = ((Y < 1.5) | (Y >= H - 1.5)).astype(float) * fx
        rgb = rgb * (1 - ln[..., None]) + hexc("#8a5a26")[None, None, :] * ln[..., None]
        a = np.maximum(a, 0.85 * ln)
    return rgba(rgb, a), {"m": [M, 4, M, 4], "o": [0] * 4, "t": [0, 0]}


@sprite("hilite")
def hilite():
    return highlight_img(0.22, hexc("#d7a54e"), False)


@sprite("hilite_sel")
def hilite_sel():
    return highlight_img(0.40, hexc("#d49a3e"), True)


@sprite("cornice")
def cornice():
    """The top bar: a marble entablature, tiled across the screen. From the top: a fillet, the frieze (stretched),
    a small cyma stepping back, a slim bronze fillet with a gilt edge, then a soft shadow on the world."""
    P = 256
    FIL, FR, CYMA, BRZ, SH = 6, 72, 4, 5, 18
    H = FIL + FR + CYMA + BRZ + SH
    X, Y = grid(P, H)
    y = Y + 0.5
    hgt = np.where(y < FIL, 4.0 + 1.2 * np.sin(np.clip(y / FIL, 0, 1) * math.pi), 4.0)
    # cyma: the frieze's lower edge rolls back towards the wall
    y0 = FIL + FR
    t = np.clip((y - y0) / CYMA, 0, 1)
    hgt = np.where((y >= y0) & (y < y0 + CYMA), 4.0 - 2.2 * smoothstep(0, 1, t), hgt)
    stone = stone_shade(travertine(X, Y, seed=103, period=P, light=0.02), (hgt + pores_height(X, Y, 103, P)) * 1.5)
    # the bronze fillet: a flat band with a rounded lower edge
    y1 = y0 + CYMA
    fb = (y >= y1) & (y < y1 + BRZ)
    bh = 2.0 + bead(y, y1 - 1.0, y1 + BRZ, 1.6)
    brz = metal(bh, BRONZE_STOPS, spec=0.35)
    # a hairline of light where the stone meets the bronze
    brz = np.where((np.abs(y - y1 - 0.5) < 0.6)[..., None], np.clip(brz * 1.25, 0, 1), brz)
    rgb = np.where(fb[..., None], brz, stone)
    a = (y < y1 + BRZ).astype(float)
    img = rgba(rgb, a)
    shadow = np.zeros((H, P, 4))
    shadow[..., :3] = SHADOW
    ts = np.clip((y - (y1 + BRZ)) / SH, 0, 1)
    shadow[..., 3] = np.where(y >= y1 + BRZ, 0.42 * (1 - ts) ** 2.2, 0)
    img = over(img, shadow)
    return img, {"m": [0, FIL, 0, CYMA + BRZ + SH], "o": [0, 0, 0, SH], "t": [P, 0]}


@sprite("column")
def column():
    """The build rail: a slim travertine pilaster with a low capital, a fluted shaft and a low base. Tiles
    vertically."""
    W, P = 120, 256
    CAP, BASE, SH = 22, 20, 12
    SIDE = 12
    H = CAP + P + BASE + SH
    Wt = W + SH
    X, Y = grid(Wt, H)
    x, y = X + 0.5, Y + 0.5
    inside = (x < W) & (y < H - SH)
    # Shaft: side bevels, a sunk line near each side, shallow flutes across the middle.
    dx = np.minimum(x, W - x)
    shaft = 4.0 + 1.0 * smoothstep(0, 4, dx)
    shaft -= 1.1 * np.clip(1 - np.abs(dx - 7) / 1.3, 0, 1)
    fl = np.clip((x - 12) / (W - 24), 0, 1)
    shaft += np.where((x > 12) & (x < W - 12), 0.35 * np.cos(fl * math.pi * 2 * 5) - 0.25, 0)
    # Capital: abacus, echinus, astragal; base: plinth, torus, fillet.
    yc = y
    cap = np.where(yc < 7, 6.0 + 0.8 * np.sin(np.clip(yc / 7, 0, 1) * math.pi),
                   np.where(yc < 16, 5.0 + 1.8 * (1 - smoothstep(7, 16, yc)) ** 0.7,
                            np.where(yc < 20, 5.2 + bead(yc, 16, 20, 1.2), 4.5)))
    cap = cap + 0.8 * smoothstep(0, 3, dx) - 0.8
    yb = (H - SH) - y
    base = np.where(yb < 7, 6.2, np.where(yb < 15, 5.0 + bead(yb, 7, 15, 1.8), np.where(yb < 18, 5.4, 4.6)))
    base = base + 0.8 * smoothstep(0, 3, dx) - 0.8
    hgt = np.where(y < CAP, cap, np.where(y >= H - SH - BASE, base, shaft))
    # Vertical grain on the shaft, horizontal on capital and base.
    vert = (y >= CAP) & (y < H - SH - BASE)
    GX, GY = np.where(vert, Y, X), np.where(vert, X, Y)
    stone = stone_shade(travertine(GX, GY, seed=113, period=P, light=0.0),
                        (hgt + pores_height(GX, GY, 113, P)) * 1.4)
    # Dark seams between capital, shaft and base.
    seam = (np.abs(y - CAP) < 0.8) | (np.abs(y - (H - SH - BASE)) < 0.8)
    stone *= (1 - 0.3 * seam)[..., None]
    img = rgba(stone, inside.astype(float))
    img = over(img, drop_shadow(inside.astype(float), 4, 4, 4, 0.45))
    return img, {"m": [SIDE, CAP, SIDE + SH, BASE + SH], "o": [0, 0, SH, SH], "t": [0, P]}


@sprite("band")
def band():
    """A panel's title band: Pompeian red with gilt hairlines, set into the frame's top."""
    P, H, E, SH = 128, 64, 14, 6
    W = 2 * E + P
    X, Y = grid(W, H + SH)
    y = Y + 0.5
    inside = y < H
    d = np.minimum(y, H - y)
    hgt = 2.0 + 1.6 * smoothstep(0, 4, d)
    red = stone_shade(fresco_red(X, Y), hgt * 1.4, amb=0.62, k=0.38)
    # Gilt hairlines: one near the top, one near the bottom.
    for yy in (4.5, H - 5.5):
        ln = np.clip(1.2 - np.abs(y - yy), 0, 1)
        gold = metal(np.full(y.shape, 1.0) + 0.6 * np.sin((X + 0.5) * 0.15), GOLD_STOPS, spec=0.2)
        red = red * (1 - ln[..., None]) + gold * ln[..., None]
    img = rgba(red, inside.astype(float))
    sh = np.zeros((H + SH, W, 4))
    sh[..., :3] = SHADOW
    sh[..., 3] = np.where(y >= H, 0.45 * (1 - np.clip((y - H) / SH, 0, 1)) ** 2, 0)
    img = over(img, sh)
    return img, {"m": [E, 8, E, 8 + SH], "o": [0, 0, 0, SH], "t": [P, 0]}


@sprite("plaque")
def plaque():
    """A tabula ansata: the red plaque with dovetail handles that carries a menu's title."""
    P, H, EAR, IN, S = 128, 84, 28, 14, 10
    E = EAR + IN
    W = 2 * E + P
    N_W, N_H = W + 2 * S, H + 2 * S
    X, Y = grid(N_W, N_H)
    x, y = X + 0.5 - S, Y + 0.5 - S
    body = sd_rrect(X, Y, S + EAR, S, S + W - EAR, S + H, 3.0)
    # The ears: dovetails pointing outwards, narrower at the body.
    def ear(xe, sign):
        u = (xe if sign > 0 else W - xe)  # distance from the outer tip
        half = np.where(u < EAR + 2, H * 0.5 * (0.40 + 0.50 * np.clip(1 - u / EAR, 0, 1)), -1)
        return np.abs(y - H / 2) - half, u

    e1, u1 = ear(x, 1)
    e2, u2 = ear(x, -1)
    ears = np.minimum(np.where(u1 < EAR + 1, e1, 99), np.where(u2 < EAR + 1, e2, 99))
    ears = np.maximum(ears, -np.minimum(u1, u2))
    sd = np.minimum(body, ears)
    d = -sd
    hgt = 2.0 + 2.0 * smoothstep(0, 5, d)
    red = stone_shade(fresco_red(X, Y), hgt * 1.4, amb=0.62, k=0.38)
    # A gilt moulding round the body, inset.
    bd = -body
    ln = np.clip(1.3 - np.abs(bd - 6.0), 0, 1) * (bd > 0)
    gold = metal(np.full(bd.shape, 1.0) + 0.5 * np.sin((X + Y) * 0.12), GOLD_STOPS, spec=0.25)
    red = red * (1 - ln[..., None]) + gold * ln[..., None]
    # A gilt edge all round.
    edge = np.clip(1.6 - np.abs(d - 1.0), 0, 1)
    red = red * (1 - edge[..., None]) + metal(np.full(d.shape, 1.2), GOLD_STOPS, spec=0.1) * edge[..., None]
    img = rgba(red, cover(sd))
    img = over(img, drop_shadow((sd < 0).astype(float), 0, 4, 3.5, 0.5))
    return img, {"m": [E + S, 0, E + S, 0], "o": [S] * 4, "t": [P, 0]}


@sprite("rule")
def rule():
    """A section divider: a bronze line, embossed, fading at both ends."""
    P, E, H = 64, 56, 10
    W = 2 * E + P
    X, Y = grid(W, H)
    y = Y + 0.5
    x = X + 0.5
    hgt = bead(y, H / 2 - 1.6, H / 2 + 1.6, 1.2)
    col = metal(hgt + 1.0, spec=0.3)
    a = np.clip(2.0 - np.abs(y - H / 2) / 1.0, 0, 1)
    lip = np.clip(1 - np.abs(y - (H / 2 + 2.6)), 0, 1) * 0.5  # a lighter line under it (an incised groove)
    rgb = col * (a > lip)[..., None] + hexc("#fff8e6")[None, None, :] * (a <= lip)[..., None]
    a = np.maximum(a, lip)
    fade = smoothstep(0, E, x) * smoothstep(0, E, W - x)
    return rgba(rgb, a * fade), {"m": [E, 0, E, 0], "o": [0] * 4, "t": [P, 0]}


@sprite("rule_orn")
def rule_orn():
    """The divider's centre: a lozenge between two laurel sprigs, in bronze."""
    W, H = 88, 24
    X, Y = grid(W, H)
    x, y = X + 0.5 - W / 2, Y + 0.5 - H / 2
    hgt = np.zeros((H, W))
    # lozenge
    lz = np.abs(x) / 7.5 + np.abs(y) / 7.5
    hgt = np.maximum(hgt, np.where(lz < 1, 2.6 * (1 - lz) + 1.2, 0))
    # leaves: small ellipses along a stem on each side, angled.
    for side in (-1, 1):
        for k in range(4):
            cx = side * (13 + k * 7.0)
            for up in (-1, 1):
                cy = up * (2.6 + 0.3 * k)
                ang = side * up * 0.55
                lx = (x - cx) * math.cos(ang) + (y - cy) * math.sin(ang)
                ly = -(x - cx) * math.sin(ang) + (y - cy) * math.cos(ang)
                e = (lx / 4.2) ** 2 + (ly / 1.9) ** 2
                hgt = np.maximum(hgt, np.where(e < 1, 1.6 * np.sqrt(np.clip(1 - e, 0, 1)) + 0.8, 0))
        stem = (np.abs(y) < 0.9) & (np.abs(x) > 9) & (np.abs(x) < 38) & (np.sign(x) == side)
        hgt = np.maximum(hgt, stem * 1.0)
    a = np.clip(hgt * 3, 0, 1)
    col = metal(hgt * 1.6, spec=0.5)
    return rgba(col, a), {"m": [0] * 4, "o": [0] * 4, "t": [0, 0]}


@sprite("meander")
def meander():
    """A Greek key band in bronze between two lines; tiles horizontally."""
    U = 2  # texels per grid unit
    per = 9  # units per key
    keys = 8
    P = per * keys * U
    H = 10 * U
    X, Y = grid(P, H)
    m = np.zeros((10, per * keys), bool)
    m[0, :] = True
    m[9, :] = True
    for k in range(keys):
        x0 = k * per
        path = [(x0 + 1, 8), (x0 + 1, 2), (x0 + 7, 2), (x0 + 7, 6), (x0 + 3, 6), (x0 + 3, 4), (x0 + 5, 4)]
        for (ax, ay), (bx, by) in zip(path, path[1:]):
            for yy in range(min(ay, by), max(ay, by) + 1):
                for xx in range(min(ax, bx), max(ax, bx) + 1):
                    m[yy, xx] = True
    mask = m[(Y // U), (X // U)].astype(float)
    hgt = blur(mask, 0.7) * 2.0
    col = metal(hgt * 1.3, spec=0.35)
    a = np.clip(blur(mask, 0.45) * 1.6, 0, 1)
    return rgba(col, a), {"m": [0] * 4, "o": [0] * 4, "t": [P, 0]}


@sprite("bar_track")
def bar_track():
    W, H, M = 64, 16, 9
    X, Y = grid(W, H)
    sd = sd_rrect(X, Y, 0, 0, W, H, H / 2)
    d = -sd
    inner = (d > 1.2).astype(float)
    base = hexc("#c9b48a") * np.ones((H, W, 3))
    sh = blur(np.roll(1 - inner, 2, 0), 1.3) * inner
    rgb = base * (1 - 0.35 * np.clip(sh, 0, 1))[..., None]
    rim = (d <= 1.2)
    rgb = np.where(rim[..., None], hexc("#8f6a3c"), rgb)
    lip = np.clip(1 - np.abs(Y + 0.5 - (H - 1.0)), 0, 1) * (d > 0) * 0.0
    return rgba(rgb + lip[..., None], cover(sd)), {"m": [M, 0, M, 0], "o": [0] * 4, "t": [0, 0]}


@sprite("bar_fill")
def bar_fill():
    """A bar's fill in greys, tinted by the vertex colour: a highlight along the top, shade along the bottom."""
    W, H, M = 64, 12, 7
    X, Y = grid(W, H)
    sd = sd_rrect(X, Y, 0, 0, W, H, H / 2)
    t = (Y + 0.5) / H
    v = 1.0 - 0.38 * t
    v += 0.25 * np.exp(-(((t - 0.22) / 0.10) ** 2))
    v = np.clip(v, 0, 1.2) / 1.2
    rgb = np.repeat(v[..., None], 3, -1)
    return rgba(rgb, cover(sd)), {"m": [M, 0, M, 0], "o": [0] * 4, "t": [0, 0]}


@sprite("tick")
def tick():
    W, H = 8, 28
    X, Y = grid(W, H)
    sd = sd_rrect(X, Y, 1, 0, W - 1, H, 2.5)
    x = X + 0.5 - W / 2
    hgt = 2 * np.sqrt(np.clip(1 - (x / 3) ** 2, 0, 1))
    return rgba(metal(hgt, spec=0.4), cover(sd)), {"m": [0] * 4, "o": [0] * 4, "t": [0, 0]}


def grab_img(w, h, stops):
    S = 2
    X, Y = grid(w + 2 * S, h + 2 * S)
    sd = sd_rrect(X, Y, S, S, w + S, h + S, min(w, h) / 2 - 0.5)
    d = -sd
    hgt = 3.0 * np.sqrt(np.clip(d / (min(w, h) / 2), 0, 1))
    img = rgba(metal(hgt, stops, spec=0.6), cover(sd))
    img = over(img, drop_shadow((sd < 0).astype(float), 1, 1, 1.0, 0.45))
    m = int(min(w, h) / 2) + S + 1
    return img, {"m": [m] * 4, "o": [S] * 4, "t": [0, 0]}


@sprite("grab")
def grab():
    return grab_img(20, 40, BRONZE_STOPS)


@sprite("grab_act")
def grab_act():
    return grab_img(20, 40, GOLD_STOPS)


@sprite("scroll_track")
def scroll_track():
    W, H = 24, 64
    X, Y = grid(W, H)
    sd = sd_rrect(X, Y, 3, 0, W - 3, H, (W - 6) / 2)
    d = -sd
    inner = (d > 0).astype(float)
    base = hexc("#dccbaa") * np.ones((H, W, 3))
    sh = blur(np.roll(np.roll(1 - inner, 2, 0), 2, 1), 1.6) * inner
    rgb = base * (1 - 0.30 * np.clip(sh, 0, 1))[..., None]
    return rgba(rgb, cover(sd) * 0.9), {"m": [0, 13, 0, 13], "o": [0] * 4, "t": [0, 0]}


@sprite("scroll_grab")
def scroll_grab():
    return grab_img(16, 48, BRONZE_STOPS)


@sprite("scroll_grab_hov")
def scroll_grab_hov():
    return grab_img(16, 48, GOLD_STOPS)


@sprite("notch")
def notch():
    """The pointer on a report panel's top edge, aimed at its button: a little stone gable with a bronze edge."""
    W, H = 44, 22
    X, Y = grid(W, H)
    x, y = X + 0.5 - W / 2, Y + 0.5
    # Triangle: apex at the top, base on the bottom row.
    edge = (H - y) - (H / (W / 2)) * np.abs(x) * 0.0
    sd = (np.abs(x) * (H / (W / 2)) - y) / math.hypot(1, H / (W / 2))  # inside the wedge when < 0
    sd = np.maximum(sd, y - H)
    d = -sd
    hgt = np.where(d < 3.5, bead(d, 0, 3.5, 1.6) + 1.5, 2.5 + 0.08 * y)
    stone = stone_shade(travertine(X, Y, seed=121, period=64, light=0.06), hgt * 1.4)
    rgb = np.where((d < 3.5)[..., None], metal(hgt + 0.5), stone)
    return rgba(rgb, cover(sd)), {"m": [0] * 4, "o": [0] * 4, "t": [0, 0]}


def close_img(stops, face):
    D, S = 44, 3
    N = D + 2 * S
    X, Y = grid(N, N)
    c = N / 2
    x, y = X + 0.5 - c, Y + 0.5 - c
    r = np.hypot(x, y)
    R = D / 2
    hgt = np.where(r > R - 5, bead(r, R - 5, R, 2.2) + 1.0, 1.0)
    rgb = np.where((r > R - 5)[..., None], metal(hgt, stops), face * np.ones((N, N, 3)))
    # Shading of the face: a dish.
    inner = (r < R - 5).astype(float)
    sh = blur(np.roll(np.roll(1 - inner, 2, 0), 2, 1), 1.8) * inner
    rgb *= (1 - 0.3 * np.clip(sh, 0, 1))[..., None]
    # The cross, cream, raised.
    u, v = (x + y) / math.sqrt(2), (x - y) / math.sqrt(2)
    arm = np.minimum(np.maximum(np.abs(u) - 1.6, np.abs(v) - 8.0), np.maximum(np.abs(v) - 1.6, np.abs(u) - 8.0))
    ca = cover(arm)
    rgb = rgb * (1 - ca[..., None]) + hexc("#f6ead0")[None, None, :] * ca[..., None]
    img = rgba(rgb, np.clip(R - r + 0.5, 0, 1))
    img = over(img, drop_shadow((r < R).astype(float), 1, 2, 1.5, 0.45))
    return img, {"m": [0] * 4, "o": [S] * 4, "t": [0, 0]}


@sprite("close")
def close():
    return close_img(BRONZE_STOPS, hexc("#6e2418"))


@sprite("close_hov")
def close_hov():
    return close_img(GOLD_STOPS, hexc("#a33b27"))


def disc_img(face, ring_stops, mark):
    D, S = 40, 2
    N = D + 2 * S
    X, Y = grid(N, N)
    c = N / 2
    x, y = X + 0.5 - c, Y + 0.5 - c
    r = np.hypot(x, y)
    R = D / 2
    RW = 4.0
    hgt = np.where(r > R - RW, bead(r, R - RW, R, 1.8) + 1.0, 1.0)
    inner = (r < R - RW).astype(float)
    sh = blur(np.roll(np.roll(1 - inner, 2, 0), 2, 1), 1.6) * inner
    face_rgb = face * np.ones((N, N, 3)) * (1 - 0.28 * np.clip(sh, 0, 1))[..., None]
    rgb = np.where((r > R - RW)[..., None], metal(hgt, ring_stops), face_rgb)
    if mark:
        # A tick: two strokes.
        def seg(ax, ay, bx, by, w):
            px, py = x - ax, y - ay
            dx, dy = bx - ax, by - ay
            t = np.clip((px * dx + py * dy) / (dx * dx + dy * dy), 0, 1)
            return np.hypot(px - t * dx, py - t * dy) - w

        sd = np.minimum(seg(-7, 0.5, -2.2, 5.5, 2.0), seg(-2.2, 5.5, 7.5, -6, 2.0))
        ca = cover(sd)
        rgb = rgb * (1 - ca[..., None]) + hexc("#fbf3df")[None, None, :] * ca[..., None]
    return rgba(rgb, np.clip(R - r + 0.5, 0, 1)), {"m": [0] * 4, "o": [S] * 4, "t": [0, 0]}


@sprite("check_on")
def check_on():
    return disc_img(hexc("#4f7a32"), BRONZE_STOPS, True)


@sprite("check_off")
def check_off():
    return disc_img(hexc("#e6d9b9"), BRONZE_STOPS, False)


# ---------------------------------------------------------------------------------------------------------------------
# Packing


def pack(images, width=1024, gutter=4):
    """Shelf packer, tallest first. Each sprite is surrounded by `gutter` texels of its own edge, extended, so
    bilinear filtering and the first mip levels never reach a neighbour."""
    order = sorted(images, key=lambda k: -images[k].shape[0])
    x = y = shelf = 0
    pos = {}
    for k in order:
        h, w = images[k].shape[:2]
        W, H = w + 2 * gutter, h + 2 * gutter
        if x + W > width:
            x, y, shelf = 0, y + shelf, 0
        pos[k] = (x + gutter, y + gutter)
        x += W
        shelf = max(shelf, H)
    height = y + shelf
    height = (height + 15) // 16 * 16
    atlas = np.zeros((height, width, 4))
    for k, (px, py) in pos.items():
        im = images[k]
        h, w = im.shape[:2]
        padded = np.pad(im, ((gutter, gutter), (gutter, gutter), (0, 0)), mode="edge")
        atlas[py - gutter : py + h + gutter, px - gutter : px + w + gutter] = padded
    return atlas, pos


def main():
    preview = None
    if "--preview" in sys.argv:
        preview = sys.argv[sys.argv.index("--preview") + 1]
    images, meta = {}, {}
    for name, fn in SPRITES.items():
        img, m = fn()
        images[name] = np.clip(img, 0, 1)
        meta[name] = m
    atlas, pos = pack(images)
    os.makedirs(OUT_DIR, exist_ok=True)
    a8 = (atlas * 255 + 0.5).astype(np.uint8)
    Image.fromarray(a8, "RGBA").save(os.path.join(OUT_DIR, "chrome.png"), optimize=True)
    out = {"texels_per_point": 2, "sprites": {}}
    for name in SPRITES:
        h, w = images[name].shape[:2]
        x, y = pos[name]
        m = meta[name]
        out["sprites"][name] = {"x": int(x), "y": int(y), "w": int(w), "h": int(h), "m": [int(v) for v in m["m"]],
                                "o": [int(v) for v in m["o"]], "t": [int(v) for v in m["t"]],
                                "i": int(m.get("inner", 0))}
    with open(os.path.join(OUT_DIR, "chrome.json"), "w") as f:
        json.dump(out, f, indent=1, sort_keys=True)
        f.write("\n")
    print(f"chrome: {len(SPRITES)} sprites, atlas {atlas.shape[1]}x{atlas.shape[0]}")
    if preview:
        bg = np.zeros_like(atlas)
        bg[..., :3] = hexc("#5f7a45")
        bg[..., 3] = 1
        Image.fromarray((over(atlas, bg) * 255).astype(np.uint8), "RGBA").save(preview)


if __name__ == "__main__":
    main()
