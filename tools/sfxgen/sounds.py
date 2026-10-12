"""The sound effects. One function per sound; `v` is the variant (1, 2, 3...).

Each returns (signal, settings): a mono (or, for the ambient bed, stereo) float array at
core.SR, and the keyword arguments for fx.finish (loudness, room, loop). All randomness
comes from core.rng, seeded per file by build_all.py, so renders are reproducible.

Direction: wood, stone, bronze, cloth and voices, in a room or out of doors. Soft attacks,
nothing bright above ~10 kHz, no synth tones.
"""
import numpy as np

import core as C
import instruments as I
from core import SR, butter, midi
from fx import add, burst, grains, modal, periodic_noise, secs, thud, tvec


def _wood_tap(n, f0, tau=0.03):
    """A small wooden block struck lightly."""
    return modal(n, [(f0, 1.0, tau), (f0 * 2.03, 0.45, tau * 0.55), (f0 * 3.2, 0.25, tau * 0.35),
                     (f0 * 4.6, 0.1, tau * 0.25)])


def _stone_knock(n, f0, tau=0.035, grit=0.5):
    """A stone or brick knocked: inharmonic, quickly damped, with grit."""
    m = modal(n, [(f0, 1.0, tau), (f0 * 2.31, 0.6, tau * 0.7), (f0 * 3.73, 0.4, tau * 0.45),
                  (f0 * 5.9, 0.2, tau * 0.3)])
    return m + grit * burst(n, 400, 4000, 0.008)


# ---------------------------------------------------------------- interface
def click(v):
    """UI click: a fingertip on a wax tablet's wooden frame."""
    n = secs(0.16)
    f0 = [820, 905, 760][v - 1] * C.rng.uniform(0.98, 1.02)
    x = _wood_tap(n, f0, 0.022) + 0.35 * burst(n, None, 3500, 0.002)
    x += 0.25 * thud(n, 180, 0.02)
    return x, dict(loudness=-18, space='hall', wet_db=-20, fade_out=0.04, max_s=0.3)


def scroll(v):
    """New letter: a papyrus scroll unrolled, then laid flat."""
    dur = 0.55 + 0.05 * v
    n = secs(dur + 0.3)
    t = tvec(n)
    env = np.clip(t / 0.08, 0, 1) * np.clip((dur - t) / 0.15, 0, 1) ** 1.2
    env *= 0.7 + 0.3 * np.sin(2 * np.pi * (3.5 + v) * t)
    crackle = grains(n, 260, dur, 1100, 4200, env=env, amp_sd=0.7, tau=(0.0006, 0.0025))
    rustle = butter(C.noise(n), 'band', [900, 4500]) * env
    rustle /= np.std(rustle) + 1e-12
    flap = burst(secs(0.12), 150, 900, 0.03)
    x = 0.8 * crackle + 0.18 * rustle
    add(x, dur - 0.04, flap, 0.9)
    return x, dict(loudness=-19, space='hall', wet_db=-18, fade_out=0.1, lowpass=7000, max_s=1.3)


def chime(v):
    """Alert: a small bronze tintinnabulum, struck twice, softly."""
    notes = [('E5', 'A5'), ('D5', 'G5')][v - 1]
    n = secs(2.6)
    x = np.zeros(n)
    for k, (nm, t0, vel) in enumerate(((notes[0], 0.0, 0.8), (notes[1], 0.16, 1.0))):
        b = I.bell(midi(nm), vel, decay=1.1)
        b *= np.clip(tvec(len(b)) / 0.003, 0, 1)  # a padded striker, not a hammer
        add(x, t0, b)
    return x, dict(loudness=-19, space='courtyard', wet_db=-11, fade_out=0.4, lowpass=7000, max_s=2.4)


# ---------------------------------------------------------------- building
def place(v):
    """Building placed: a stone block set down, then a mallet tap on it."""
    n = secs(0.9)
    x = np.zeros(n)
    f0 = [430, 380, 470][v - 1]
    add(x, 0.0, 0.9 * thud(secs(0.4), 72, 0.11))
    add(x, 0.0, 0.55 * _stone_knock(secs(0.3), f0, 0.04, grit=0.6))
    add(x, 0.0, 0.25 * burst(secs(0.1), 200, 2500, 0.02))
    t2 = 0.16 + 0.02 * v
    add(x, t2, 0.35 * _wood_tap(secs(0.2), 520 + 40 * v, 0.03))
    add(x, t2, 0.25 * _stone_knock(secs(0.2), f0 * 1.4, 0.025, grit=0.3))
    return x, dict(loudness=-15, space='courtyard', wet_db=-17, fade_out=0.12, lowpass=9000, max_s=0.9)


def road(v):
    """Road laid: a spade of gravel scraped and dropped."""
    dur = 0.42 + 0.06 * v
    n = secs(dur + 0.25)
    t = tvec(n)
    env = np.sin(np.pi * np.clip(t / dur, 0, 1)) ** 0.8
    crunch = grains(n, 420, dur, 700, 3200, env=env, amp_sd=0.5)
    scrape = butter(C.noise(n), 'band', [250, 1400]) * env
    scrape /= np.std(scrape) + 1e-12
    x = 0.7 * crunch + 0.15 * scrape
    add(x, dur * 0.85, 0.5 * thud(secs(0.2), 95, 0.05))
    return x, dict(loudness=-18, space='courtyard', wet_db=-20, fade_out=0.12, lowpass=6500, max_s=0.9)


def crumble(v):
    """Demolition: a wall pulled down, rubble settling."""
    dur = 1.1 + 0.15 * v
    n = secs(dur + 0.5)
    x = np.zeros(n)
    add(x, 0.0, thud(secs(0.8), 55, 0.22))
    for _ in range(36):
        st = dur * C.rng.uniform(0, 1) ** 1.8
        a = np.exp(-st / (dur * 0.45)) * np.exp(C.rng.normal(0, 0.4))
        add(x, st, 0.35 * a * _stone_knock(secs(0.12), C.rng.uniform(280, 1500), C.rng.uniform(0.01, 0.03)))
    t = tvec(n)
    dust = butter(C.noise(n), 'band', [150, 1800]) * np.exp(-t / (dur * 0.35)) * np.clip(t / 0.02, 0, 1)
    x += 0.25 * dust / (np.std(dust) + 1e-12)
    return x, dict(loudness=-15, space='courtyard', wet_db=-15, fade_out=0.3, lowpass=7500, max_s=1.9)


# ---------------------------------------------------------------- money and letters
def coins(v):
    """Coins: a few bronze and silver pieces dropped onto a table."""
    n = secs(0.9)
    x = np.zeros(n)
    hits = [(0.0, 1.0), (0.055, 0.7), (0.11, 0.5), (0.2, 0.3)][:2 + v]
    for t0, a in hits:
        f0 = C.rng.uniform(2300, 3300)
        c = modal(secs(0.6), [(f0, 1.0, 0.16), (f0 * 1.61, 0.45, 0.1), (f0 * 2.33, 0.25, 0.06),
                              (f0 * 0.52, 0.2, 0.05)])
        c += 0.3 * burst(secs(0.6), 2000, 8000, 0.0015)
        add(x, t0 + C.rng.uniform(0, 0.01), a * c)
    add(x, 0.0, 0.25 * thud(secs(0.15), 160, 0.03))
    return x, dict(loudness=-19, space='hall', wet_db=-18, fade_out=0.15, lowpass=9500, max_s=1.1)


# ---------------------------------------------------------------- disasters
def fire(v):
    """Fire: a whoosh as the roof catches, then crackling timber."""
    dur = 2.2
    n = secs(dur + 0.4)
    t = tvec(n)
    swell = np.clip(t / 0.35, 0, 1) ** 2 * np.exp(-np.clip(t - 0.35, 0, None) / 1.1)
    cut = 250 + 1300 * np.clip(t / 0.35, 0, 1) * np.exp(-np.clip(t - 0.35, 0, None) / 0.8)
    roar = np.zeros(n)
    nz = C.noise(n)
    blk = secs(0.02)
    zi = None
    from scipy import signal
    for s in range(0, n, blk):  # a low-pass that opens and closes: the whoosh
        sos = signal.butter(2, cut[s], btype='low', fs=SR, output='sos')
        if zi is None:
            zi = np.zeros((sos.shape[0], 2))
        roar[s:s + blk], zi = signal.sosfilt(sos, nz[s:s + blk], zi=zi)
    roar *= swell / (np.std(roar) + 1e-12)
    env = np.clip(t / 0.2, 0, 1) * np.clip((dur - t) / 0.5, 0, 1)
    crackle = grains(n, 38, dur, 1500, 7000, env=env, amp_sd=0.9, tau=(0.0005, 0.003))
    pops = grains(n, 6, dur, 400, 1500, env=env, amp_sd=0.5, tau=(0.004, 0.012))
    x = 0.55 * roar + 0.5 * crackle + 0.6 * pops
    return x, dict(loudness=-17, space='field', wet_db=-14, fade_out=0.4, lowpass=9000, max_s=2.8)


def collapse(v):
    """Collapse: a building comes down, a long rumble and falling masonry."""
    dur = 2.6
    n = secs(dur + 0.6)
    t = tvec(n)
    x = np.zeros(n)
    rumble = butter(C.noise(n), 'low', 140, 4)
    rumble *= np.clip(t / 0.08, 0, 1) * np.exp(-t / 0.9) / (np.std(rumble) + 1e-12)
    x += 0.9 * rumble
    add(x, 0.02, 1.0 * thud(secs(1.2), 45, 0.35, drop=0.5))
    add(x, 0.45, 0.6 * thud(secs(0.8), 60, 0.2))
    for _ in range(55):
        st = dur * C.rng.uniform(0, 1) ** 1.5
        a = np.exp(-st / (dur * 0.5)) * np.exp(C.rng.normal(0, 0.45))
        add(x, st, 0.3 * a * _stone_knock(secs(0.15), C.rng.uniform(200, 1200), C.rng.uniform(0.015, 0.04)))
    return x, dict(loudness=-14, space='field', wet_db=-10, fade_out=0.5, lowpass=6000, max_s=3.4)


# ---------------------------------------------------------------- war
def _clang(n, f0):
    """Iron on iron: a sword blade meeting another, or a shield boss."""
    m = modal(n, [(f0, 1.0, 0.22), (f0 * 2.71, 0.55, 0.12), (f0 * 4.53, 0.35, 0.07),
                  (f0 * 6.9, 0.2, 0.04), (f0 * 1.38, 0.3, 0.09)])
    return m + 0.6 * burst(n, 1800, 7000, 0.006)


def _shield(n):
    """Wood and hide: a blow on a scutum."""
    return thud(n, 120, 0.07, 1.0) + 0.6 * _wood_tap(n, C.rng.uniform(240, 300), 0.04) + \
        0.4 * burst(n, 150, 1600, 0.012)


def clash(v):
    """Battle: swords and shields, a short exchange."""
    n = secs(1.8)
    x = np.zeros(n)
    t0 = 0.0
    for k in range(5 + (v % 2)):
        if C.rng.uniform() < 0.55:
            add(x, t0, (0.9 - 0.08 * k) * _clang(secs(0.6), C.rng.uniform(650, 1050)))
        else:
            add(x, t0, (1.0 - 0.08 * k) * _shield(secs(0.4)))
        t0 += C.rng.uniform(0.14, 0.32)
    t = tvec(n)
    scuffle = butter(C.noise(n), 'band', [200, 1200]) * np.clip(t / 0.1, 0, 1) * np.clip((t0 - t) / 0.3, 0, 1)
    x += 0.08 * scuffle / (np.std(scuffle) + 1e-12)
    return x, dict(loudness=-16, space='field', wet_db=-12, fade_out=0.25, lowpass=7500, max_s=2.1)


def horn(v):
    """Military horn: a low cornu call across the walls (raids, the legion marching)."""
    calls = [[(0.05, 1.0, 'D3', 0.8), (1.15, 1.5, 'A3', 0.9)],
             [(0.05, 0.5, 'A2', 0.75), (0.62, 0.5, 'A2', 0.75), (1.2, 1.6, 'E3', 0.9)]][v - 1]
    t0, sig = I.brass([(a, d, midi(m), vel) for a, d, m, vel in calls], 'cornu')
    n = secs(4.0)
    x = np.zeros(n)
    add(x, t0, sig)
    return x, dict(loudness=-17, space='field', wet_db=-7, fade_out=0.5, lowpass=5000, max_s=3.8)


# ---------------------------------------------------------------- crowds
VOWEL_FORMANTS = {'a': [(750, 1.0), (1250, 0.5), (2600, 0.2)],
                  'o': [(500, 1.0), (850, 0.6), (2700, 0.12)],
                  'e': [(450, 1.0), (1900, 0.55), (2600, 0.25)]}


def _voice(n, f0, start, length, vowel, rise):
    """One cheering voice: a glottal tone whose pitch jumps up and falls, through a vowel."""
    t = tvec(n)
    tt = np.clip(t - start, 0, None)
    env = np.clip(tt / 0.06, 0, 1) * np.clip((length - tt) / 0.35, 0, 1) * (t >= start)
    contour = 2 ** ((rise * np.clip(tt / 0.12, 0, 1) - 0.5 * rise * np.clip((tt - 0.3) / length, 0, 1)) / 12)
    jitter = butter(C.noise(n), 'low', 8, 1)
    jitter *= 0.012 / (np.std(jitter) + 1e-12)
    ph = 2 * np.pi * np.cumsum(f0 * contour * (1 + jitter)) / SR
    out = np.zeros(n)
    for k in range(1, int(4000 / f0) + 1):
        fk = k * f0
        w = sum(g * np.exp(-((fk - F) / 140) ** 2) for F, g in VOWEL_FORMANTS[vowel])
        out += (0.05 + w) * k ** -0.8 * np.sin(k * ph + C.rng.uniform(0, 6.28))
    return out * env


def cheer(v):
    """A crowd cheers: dozens of voices and hands (festivals, a mission won)."""
    dur = 2.8
    n = secs(dur + 0.4)
    x = np.zeros(n)
    for _ in range(34):
        f0 = C.rng.uniform(105, 175) if C.rng.uniform() < 0.6 else C.rng.uniform(190, 320)
        st = C.rng.uniform(0, 0.45)
        x += C.rng.uniform(0.4, 1.0) * _voice(n, f0, st, C.rng.uniform(1.0, dur - st - 0.2),
                                                str(C.rng.choice(list(VOWEL_FORMANTS))), C.rng.uniform(2, 6))
    t = tvec(n)
    breath = butter(C.noise(n), 'band', [500, 2800]) * np.clip(t / 0.15, 0, 1) * np.clip((dur - t) / 0.8, 0, 1)
    x = x / (np.std(x) + 1e-12) + 0.35 * breath / (np.std(breath) + 1e-12)
    claps = np.zeros(n)
    for _ in range(70):
        st = C.rng.uniform(0.25, dur - 0.3)
        add(claps, st, C.rng.uniform(0.4, 1.0) * I.clap(1.0))
    x += 0.6 * claps / (np.std(claps) + 1e-12)
    return x, dict(loudness=-15, space='field', wet_db=-9, fade_out=0.7, lowpass=8000, max_s=3.3)


# ---------------------------------------------------------------- ambient
def city(v):
    """Ambient city bed (seamless stereo loop): a distant murmur, a far workshop, sparrows, swifts."""
    length = 24.0
    n = secs(length)
    t = tvec(n)
    # Far-off talk is dark: speech-shaped, rolled off above ~2 kHz.
    shape = lambda f: (np.exp(-((np.log2(np.maximum(f, 1) / 500)) ** 2) / 1.6) +
                       0.3 * np.exp(-((np.log2(np.maximum(f, 1) / 1500)) ** 2) / 0.6)) / (1 + (f / 2200) ** 4)
    out = np.zeros((n, 2))
    common = periodic_noise(n, shape)  # shared by both sides: the bed sits in the middle, a little wide
    for ch in range(2):
        murmur = 0.7 * periodic_noise(n, shape)
        # Loudness drifts: sums of sines with whole cycles per loop, so the seam is invisible.
        mod = 1 + 0.22 * np.sin(2 * np.pi * 3 * t / length + C.rng.uniform(0, 6)) + \
            0.12 * np.sin(2 * np.pi * 7 * t / length + C.rng.uniform(0, 6)) + \
            0.06 * np.sin(2 * np.pi * 61 * t / length + C.rng.uniform(0, 6))
        out[:, ch] = (murmur + 0.6 * common) * mod
        wind = periodic_noise(n, lambda f: 1 / (1 + (np.maximum(f, 1) / 180) ** 2))
        out[:, ch] += 0.35 * wind
    out *= 0.06 / (np.std(out) + 1e-12)
    # Babble: hundreds of short far-off syllables, so the bed reads as people rather than wind.
    babble = np.zeros((n, 2))
    for _ in range(320):
        d = C.rng.uniform(0.1, 0.3)
        syl = _voice(secs(d + 0.05), C.rng.uniform(95, 240), 0.0, d, str(C.rng.choice(list(VOWEL_FORMANTS))),
                     C.rng.uniform(-2, 3))
        syl = butter(syl, 'low', 1800)
        th = (C.rng.uniform(-0.8, 0.8) + 1) * np.pi / 4
        st, g = C.rng.uniform(0, length), C.rng.uniform(0.3, 1.0)
        add(babble[:, 0], st, np.cos(th) * g * syl, wrap=True)
        add(babble[:, 1], st, np.sin(th) * g * syl, wrap=True)
    out += babble * (0.05 / (np.std(babble) + 1e-12))
    events = np.zeros((n, 2))

    def put(sig, start, pan, gain):
        th = (pan + 1) * np.pi / 4
        add(events[:, 0], start, np.cos(th) * gain * sig, wrap=True)
        add(events[:, 1], start, np.sin(th) * gain * sig, wrap=True)

    for k in range(5):  # a mallet in a far workshop: a few taps, then quiet
        st, pan = C.rng.uniform(0, length), C.rng.uniform(-0.8, 0.8)
        for j in range(int(C.rng.integers(2, 5))):
            tap = _wood_tap(secs(0.2), C.rng.uniform(380, 520), 0.03) + 0.4 * thud(secs(0.2), 110, 0.04)
            put(butter(tap, 'low', 2500), st + j * C.rng.uniform(0.32, 0.45), pan, 0.018)
    for k in range(7):  # sparrows: quick chirps in short bursts
        st, pan = C.rng.uniform(0, length), C.rng.uniform(-0.9, 0.9)
        f0 = C.rng.uniform(3600, 4800)
        for j in range(int(C.rng.integers(2, 6))):
            d = C.rng.uniform(0.04, 0.08)
            m = secs(d)
            tt = tvec(m)
            f = f0 * (1 + 0.25 * np.sin(np.pi * tt / d)) * C.rng.uniform(0.95, 1.05)
            ph = 2 * np.pi * np.cumsum(f) / SR
            chirp = (np.sin(ph) + 0.15 * np.sin(2 * ph)) * np.sin(np.pi * tt / d) ** 2
            put(chirp, st + j * C.rng.uniform(0.09, 0.16), pan, 0.012)
    for k in range(2):  # swifts high overhead: a thin descending trill
        st, pan = C.rng.uniform(0, length), C.rng.uniform(-0.6, 0.6)
        d = 0.7
        m = secs(d)
        tt = tvec(m)
        f = 6200 - 900 * tt / d + 300 * np.sin(2 * np.pi * 28 * tt)
        ph = 2 * np.pi * np.cumsum(f) / SR
        put(np.sin(ph) * np.sin(np.pi * tt / d) ** 1.5, st, pan, 0.004)
    out += events
    return out, dict(loudness=-26, space='courtyard', wet_db=-12, loop=True, lowpass=9000)


SOUNDS = {
    'click': click, 'scroll': scroll, 'chime': chime, 'place': place, 'road': road, 'crumble': crumble,
    'coins': coins, 'fire': fire, 'collapse': collapse, 'clash': clash, 'horn': horn, 'cheer': cheer,
    'city': city,
}
