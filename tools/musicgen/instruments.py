"""Synthesized ancient instruments. No samples: every sound is built here.

Plucked:   pluck() with presets lyre, kithara, harp, oud, qanun (Karplus-Strong).
Winds:     aulos(), flute() (ney / tibia), pipes() (syrinx), drone().
Brass:     brass() with presets cornu, tuba, buccina, carnyx.
Keyboard:  organ() (hydraulis, the Roman water organ).
Voices:    choir() (a vowel pad, used sparingly).
Percussion: boom/tek (tympanum), war_drum, riq, shake, sistrum, crotala,
           cymbal, clap, bell.

Line instruments (aulos, flute, brass) take a note list
[(t_seconds, dur_seconds, midi, vel)] and return (start_time, signal).
Everything else returns a signal that starts at time 0.
"""
import numpy as np
from scipy import signal

import core as C
from core import SR, butter, hz, peaking, unit_std


# ---------------------------------------------------------------- plucked strings
BODIES = {   # (peaks [(f, dB, Q)], lowpass Hz, t60 scale, pluck position)
    'lyre':    ([(230, 4, 1.2), (1100, 2, 1.5)], 6500, 1.0, 0.13),
    'kithara': ([(180, 5, 1.1), (900, 3, 1.4)], 7000, 1.2, 0.12),
    'harp':    ([(300, 2, 1.0)], 8000, 1.5, 0.18),
    'oud':     ([(250, 5, 1.0), (2500, 3, 2.0)], 9000, 0.8, 0.10),
    'qanun':   ([(2000, 3, 1.5)], 10000, 0.9, 0.08),
}


def _ks(f, vel, n, dark, t60, pos):
    """Karplus-Strong string with an allpass in the loop for exact tuning."""
    p = SR / f
    n_del = int(p - 0.6)
    d = p - 0.5 - n_del
    c = (1 - d) / (1 + d)
    g = 10 ** (-3 / (f * t60))
    a = np.zeros(n_del + 3)
    a[0], a[1] = 1.0, c
    a[n_del] -= 0.5 * g * c
    a[n_del + 1] -= 0.5 * g * (1 + c)
    a[n_del + 2] -= 0.5 * g
    burst = C.rng.uniform(-1, 1, n_del)
    burst = signal.lfilter([1 - dark], [1, -dark], burst)
    burst -= np.roll(burst, max(1, int(pos * n_del)))
    burst /= np.max(np.abs(burst)) + 1e-9
    x = np.zeros(n)
    x[:n_del] = burst * vel
    return signal.lfilter([1, c], a, x)


def pluck(m, vel, length=3.2, dark=0.5, kind='lyre'):
    peaks, lp, t60s, pos = BODIES[kind]
    f = hz(m)
    t60 = float(np.clip(2.6 * t60s * (196 / f) ** 0.5, 0.6, 5.0))
    n = int(length * SR)
    y = _ks(f, vel, n, dark, t60, pos)
    if kind == 'oud':   # double courses, slightly apart
        y = 0.6 * y + 0.6 * _ks(f * 1.0023, vel, n, dark, t60, pos * 1.1)
    click = butter(C.noise(int(0.004 * SR)), 'high', 3000) * 0.04 * vel
    y[:len(click)] += click
    for f0, gdb, q in peaks:
        y = peaking(y, f0, gdb, q)
    y = butter(y, 'low', lp)
    fade = int(0.12 * SR)
    y[-fade:] *= np.linspace(1, 0, fade)
    return y


def strum(midis, vel, spread=0.04, **kw):
    """Arpeggiated chord, low to high; returns one signal."""
    sigs = [pluck(m, vel * (1 - 0.05 * i), **kw) for i, m in enumerate(midis)]
    off = int(spread * SR)
    out = np.zeros(max(len(s) for s in sigs) + off * len(sigs))
    for i, s in enumerate(sigs):
        out[i * off:i * off + len(s)] += s
    return out


# ---------------------------------------------------------------- continuous lines
def legato_tracks(notes, n, vib_depth, glide=0.028, vib_rate=5.1, attack=0.035,
                  release=0.09, scoop=0.0, drift=0.0015):
    """notes: [(t_start, t_dur, midi, vel)] relative to the buffer -> (freq, env)."""
    target = np.zeros(n)
    env = np.zeros(n)
    since = np.zeros(n)
    bend = np.ones(n)
    atk, rel = max(1, int(attack * SR)), max(1, int(release * SR))
    for i, (ts, td, m, v) in enumerate(notes):
        s, e = max(0, int(ts * SR)), int((ts + td) * SR)
        nxt = int(notes[i + 1][0] * SR) if i + 1 < len(notes) else n
        nxt = max(nxt, s + 1)
        if i == 0:
            target[:s] = hz(m)
        target[s:nxt] = hz(m)
        since[s:nxt] = np.arange(nxt - s) / SR
        if scoop:
            k = np.arange(min(nxt - s, int(0.2 * SR))) / SR
            bend[s:s + len(k)] = 2 ** (-scoop / 1200 * np.exp(-k / 0.03))
        ln = max(e - s, 2)
        tt = np.arange(ln) / ln
        body = v * (0.9 + 0.1 * np.sin(np.pi * tt))
        a = min(atk, ln)
        body[:a] *= np.linspace(0, 1, a)
        ne = np.concatenate([body, body[-1] * np.linspace(1, 0, rel)])
        hi = min(n, s + len(ne))
        env[s:hi] = np.maximum(env[s:hi], ne[:hi - s])
    k = np.exp(-1 / (glide * SR))
    freq, _ = signal.lfilter([1 - k], [1, -k], target, zi=[k * target[0]])
    t = np.arange(n) / SR
    vib = vib_depth * np.clip((since - 0.22) / 0.35, 0, 1) * \
        np.sin(2 * np.pi * vib_rate * t + C.rng.uniform(0, 6.28))
    dr = butter(C.noise(n), 'low', 3.0, 1)
    dr *= drift / (np.std(dr) + 1e-12)
    return freq * bend * (1 + vib + dr), env


def _line(notes, synth, tail=0.4, **kw):
    notes = sorted(notes)
    t0 = notes[0][0] - 0.05
    n = int((notes[-1][0] + notes[-1][1] - t0 + tail) * SR)
    rel = [(ts - t0, td, m, v) for ts, td, m, v in notes]
    f, env = legato_tracks(rel, n, **kw)
    return t0, synth(f, env)


def reed(freq, env, formant=1300, k_max=24, even=0.7, breath=0.03, bright=0.08, slope=0.9):
    """Additive reed/lip source: 1/k spectrum through two formants, brighter when loud."""
    n = len(freq)
    ph = 2 * np.pi * np.cumsum(freq) / SR
    envn = env / (env.max() + 1e-12)
    out = np.zeros(n)
    for k in range(1, k_max + 1):
        fk = k * freq
        if fk.min() > 0.45 * SR:
            break
        lf = np.log2(fk / formant)
        w = k ** -slope * (0.35 + np.exp(-lf ** 2 / 0.405) + 0.5 * np.exp(-(lf - 1.2) ** 2 / 0.245))
        w = w / (1 + (fk / 6000) ** 4) * (fk < 0.45 * SR)
        if k % 2 == 0:
            w *= even
        out += w * envn ** (bright * (k - 1)) * np.sin(k * ph)
    out *= env
    if breath:
        nz = butter(C.noise(n), 'band', [1500, 5000])
        out += breath * env * unit_std(nz)
    return out


def aulos(notes, vib=0.010, formant=1300, breath=0.03, bright=0.08):
    """The aulos: a buzzy, nasal double reed."""
    return _line(notes, lambda f, e: reed(f, e, formant=formant, breath=breath, bright=bright),
                 vib_depth=vib)


BRASS = {   # formant, brightness, scoop cents, vibrato, attack
    'cornu':   dict(formant=700, bright=0.22, scoop=35, vib=0.004, attack=0.045),
    'tuba':    dict(formant=1300, bright=0.28, scoop=25, vib=0.003, attack=0.03),
    'buccina': dict(formant=900, bright=0.25, scoop=30, vib=0.004, attack=0.04),
    'carnyx':  dict(formant=550, bright=0.25, scoop=60, vib=0.012, attack=0.07),
}


def brass(notes, kind='cornu'):
    """Roman horns. Fanfares should stay on the natural harmonic series."""
    p = BRASS[kind]

    def synth(f, env):
        y = reed(f, env, formant=p['formant'], k_max=30, even=1.0, breath=0.008,
                 bright=p['bright'], slope=0.8)
        if kind == 'carnyx':   # the boar-headed war horn growls
            t = np.arange(len(y)) / SR
            y *= 1 + 0.3 * np.sin(2 * np.pi * 27 * t) * env / (env.max() + 1e-9)
            y = np.tanh(1.8 * y / (np.abs(y).max() + 1e-9)) * np.abs(y).max()
        return y
    return _line(notes, synth, vib_depth=p['vib'], scoop=p['scoop'], attack=p['attack'],
                 vib_rate=4.2 if kind == 'carnyx' else 5.0, release=0.12)


FLUTES = {   # breath, 2nd and 3rd harmonics, vibrato
    'ney':   dict(breath=0.35, h2=0.15, h3=0.05, vib=0.007, rate=4.6),
    'tibia': dict(breath=0.18, h2=0.25, h3=0.10, vib=0.005, rate=5.0),
}


def flute(notes, kind='ney'):
    """End-blown flute (ney) or soft pipe (tibia): breath noise rides the pitch."""
    p = FLUTES[kind]

    def synth(freq, env):
        n = len(freq)
        ph = 2 * np.pi * np.cumsum(freq) / SR
        tone = np.sin(ph) + p['h2'] * np.sin(2 * ph) + p['h3'] * np.sin(3 * ph)
        # noise band f +- bw; keep bw under f/2 so low notes don't rumble down to DC
        bw = float(np.clip(0.4 * freq.min(), 60, 300))
        pitched = unit_std(butter(C.noise(n), 'low', bw)) * np.sin(ph)
        hiss = unit_std(butter(C.noise(n), 'band', [2000, 7000]))
        return env * (tone + p['breath'] * (0.6 * pitched + 0.25 * hiss))
    return _line(notes, synth, vib_depth=p['vib'], vib_rate=p['rate'], attack=0.07,
                 release=0.15, glide=0.04)


def pipes(m, dur, vel):
    """Syrinx (pan pipes): near-sine tone, breathy, with a chiff on the attack."""
    f = hz(m)
    n = int((dur + 0.25) * SR)
    t = np.arange(n) / SR
    env = np.clip(t / 0.05, 0, 1) * np.clip((dur + 0.15 - t) / 0.15, 0, 1)
    vib = 0.005 * np.clip((t - 0.3) / 0.3, 0, 1) * np.sin(2 * np.pi * 4.6 * t)
    ph = 2 * np.pi * np.cumsum(f * (1 + vib)) / SR
    tone = np.sin(ph) + 0.12 * np.sin(2 * ph) + 0.06 * np.sin(3 * ph)
    breath = unit_std(butter(C.noise(n), 'band', [f * 0.85, f * 1.18], 2))
    hiss = unit_std(butter(C.noise(n), 'band', [2000, 7000]))
    chiff = butter(C.noise(n), 'band', [f * 2, min(f * 8, 15000)]) * np.exp(-t / 0.03)
    chiff = unit_std(chiff, int(0.05 * SR))
    return vel * (env * (tone + 0.22 * breath + 0.05 * hiss) + 0.25 * chiff * np.clip(t / 0.005, 0, 1))


def drone(midis, length, fade_in, fade_out, vel=0.6, formant=900):
    """Held drone pipe(s), as on the second pipe of an aulos."""
    n = int(length * SR)
    t = np.arange(n) / SR
    env = (np.clip(t / max(fade_in, 1e-3), 0, 1) *
           np.clip((length - t) / max(fade_out, 1e-3), 0, 1)) ** 1.5
    out = np.zeros(n)
    for i, m in enumerate(midis):
        f = np.full(n, hz(m) * (1 + 0.0007 * i))
        f *= 1 + 0.0015 * np.sin(2 * np.pi * (0.11 + 0.03 * i) * t + C.rng.uniform(0, 6))
        out += reed(f, env * vel, formant=formant, k_max=16, even=0.85, breath=0.015, bright=0.0)
    return out


# ---------------------------------------------------------------- organ and voices
RANKS = {
    'full':  [(0, 1.0), (12, 0.55), (19, 0.3), (24, 0.25)],
    'soft':  [(0, 1.0), (12, 0.3)],
    'flute': [(0, 1.0)],
}
PIPE = [1, .45, .25, .14, .08, .05]


def organ(midis, dur, vel=0.7, ranks='soft', attack=0.05):
    """Hydraulis: flue pipes on water-regulated wind (hence the slight waver)."""
    n = int((dur + 0.35) * SR)
    t = np.arange(n) / SR
    env = np.clip(t / attack, 0, 1) * np.clip((dur + 0.2 - t) / 0.2, 0, 1)
    wob = 1 + 0.003 * np.sin(2 * np.pi * 0.7 * t + C.rng.uniform(0, 6))
    out = np.zeros(n)
    for m in midis:
        for off, ra in RANKS[ranks]:
            f = hz(m + off) * (1 + C.rng.normal(0, 0.0008))
            if f * 1 > 0.4 * SR:
                continue
            ph = 2 * np.pi * np.cumsum(np.full(n, f) * wob) / SR
            for k, a in enumerate(PIPE, 1):
                if k * f < 0.4 * SR:
                    out += ra * a * np.sin(k * ph)
            chiff = butter(C.noise(n), 'band', [min(f * 2, 9000), min(f * 4, 18000)])
            out += 0.15 * ra * unit_std(chiff) * np.exp(-t / 0.03)
    wind = butter(C.noise(n), 'band', [300, 3000]) * 0.01 * len(midis)
    return vel * env * (out + unit_std(wind) * 0.02 * len(midis))


VOWELS = {   # (formant Hz, bandwidth Hz, gain)
    'a': [(700, 130, 1.0), (1220, 90, 0.5), (2600, 160, 0.25)],
    'o': [(450, 100, 1.0), (800, 90, 0.6), (2830, 160, 0.15)],
    'u': [(325, 80, 1.0), (700, 90, 0.35), (2530, 160, 0.1)],
}


def choir(m, dur, vel=0.6, vowel='a', voices=3, attack=0.3):
    """A small vowel choir on one pitch: detuned voices through vowel formants."""
    n = int((dur + 0.6) * SR)
    t = np.arange(n) / SR
    env = np.clip(t / attack, 0, 1) * np.clip((dur + 0.5 - t) / 0.5, 0, 1)
    f0 = hz(m)
    out = np.zeros(n)
    for v in range(voices):
        det = 2 ** (C.rng.normal(0, 7) / 1200)
        rate = 5.3 + C.rng.normal(0, 0.4)
        vib = 0.005 * np.clip((t - 0.2) / 0.5, 0, 1) * np.sin(2 * np.pi * rate * t + C.rng.uniform(0, 6))
        dr = butter(C.noise(n), 'low', 4.0, 1)
        dr *= 0.002 / (np.std(dr) + 1e-12)
        ph = 2 * np.pi * np.cumsum(f0 * det * (1 + vib + dr)) / SR
        for k in range(1, int(5000 / f0) + 1):
            fk = k * f0
            w = sum(g * np.exp(-((fk - F) / bw) ** 2) for F, bw, g in VOWELS[vowel])
            w = (0.04 + w) * k ** -0.7
            out += w * np.sin(k * ph + C.rng.uniform(0, 6))
    breath = butter(C.noise(n), 'band', [1000, 4000]) * 0.03
    return vel * env * (out / voices + unit_std(breath) * 0.02)


# ---------------------------------------------------------------- percussion
def _tail_faded(fn):
    """Percussion buffers end before the decay is inaudible: fade the last quarter to zero."""
    def wrapped(*a, **kw):
        y = fn(*a, **kw)
        f = len(y) // 4
        y[-f:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, f))
        return y
    wrapped.__name__, wrapped.__doc__ = fn.__name__, fn.__doc__
    return wrapped


@_tail_faded
def boom(vel, pitch=64, decay=0.26):
    """Frame drum (tympanum), struck in the centre."""
    n = int((decay * 3 + 0.2) * SR)
    t = np.arange(n) / SR
    f = pitch + 0.86 * pitch * np.exp(-t / 0.04)
    ph = 2 * np.pi * np.cumsum(f) / SR
    tone = np.sin(ph) * np.exp(-t / decay) + 0.35 * np.sin(1.59 * ph) * np.exp(-t / (decay * 0.42))
    skin = butter(C.noise(n), 'low', 900) * np.exp(-t / 0.025)
    return vel * (tone + 0.2 * unit_std(skin, 1000))


@_tail_faded
def tek(vel, jingle=0.0, pitch=380):
    """Frame drum struck at the rim, optionally with jingles."""
    n = int(0.3 * SR)
    t = np.arange(n) / SR
    slap = unit_std(butter(C.noise(n), 'band', [1800, 6000]) * np.exp(-t / 0.016), 500)
    tone = np.sin(2 * np.pi * pitch * t) * np.exp(-t / 0.03)
    jing = sum(np.sin(2 * np.pi * fr * t + C.rng.uniform(0, 6)) for fr in (6100, 7350, 8900, 10400))
    return vel * (0.5 * slap + 0.4 * tone + jingle * 0.25 * jing * np.exp(-t / 0.11))


@_tail_faded
def war_drum(vel, pitch=46, decay=0.6):
    """Big low drum for battles: long boom with a heavy skin hit."""
    n = int((decay * 3 + 0.3) * SR)
    t = np.arange(n) / SR
    f = pitch + 1.2 * pitch * np.exp(-t / 0.05)
    ph = 2 * np.pi * np.cumsum(f) / SR
    tone = np.sin(ph) * np.exp(-t / decay) + 0.3 * np.sin(1.5 * ph) * np.exp(-t / (decay * 0.3))
    skin = unit_std(butter(C.noise(n), 'band', [80, 1500]) * np.exp(-t / 0.04), 2000)
    y = tone + 0.35 * skin
    return vel * np.tanh(1.4 * y) / np.tanh(1.4)


@_tail_faded
def riq(vel):
    """Tambourine hit: skin slap plus a bright jingle cluster."""
    n = int(0.4 * SR)
    t = np.arange(n) / SR
    jing = sum(np.sin(2 * np.pi * fr * t + C.rng.uniform(0, 6))
               for fr in C.rng.uniform(5000, 11000, 8)) / 8
    jing *= np.exp(-t / 0.13) * (1 + 0.5 * np.sin(2 * np.pi * 31 * t))
    slap = unit_std(butter(C.noise(n), 'band', [1500, 5000]) * np.exp(-t / 0.012), 400)
    return vel * (0.8 * jing + 0.25 * slap)


def shake(vel, dur=0.25):
    """Tambourine or rattle shake: a swell of jingle noise."""
    n = int((dur + 0.15) * SR)
    t = np.arange(n) / SR
    env = np.sin(np.pi * np.clip(t / dur, 0, 1)) ** 0.7 + np.exp(-np.clip(t - dur, 0, None) / 0.05) * (t > dur)
    nz = unit_std(butter(C.noise(n), 'band', [5000, 12000]))
    return vel * 0.4 * env * nz


@_tail_faded
def sistrum(vel):
    """Egyptian sistrum: a few metal discs sliding on rods, struck in a quick burst."""
    n = int(0.45 * SR)
    out = np.zeros(n)
    t = np.arange(n) / SR
    freqs = C.rng.uniform(3500, 9500, 6)
    for k, delay in enumerate((0.0, 0.011, 0.024)):
        s = int(delay * SR)
        tt = t[:n - s]
        hit = sum(np.sin(2 * np.pi * f * tt + C.rng.uniform(0, 6)) for f in freqs) / 6
        out[s:] += (1 - 0.25 * k) * hit * np.exp(-tt / 0.12)
    return vel * out


@_tail_faded
def crotala(vel):
    """Finger cymbals: two small bronze discs struck together."""
    n = int(2.5 * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)
    for det in (1.0, 1.0045):
        for r, a, d in ((1, 1, 1.6), (1.53, .6, 1.1), (2.47, .4, .7), (3.6, .25, .4)):
            out += a * np.sin(2 * np.pi * 2100 * r * det * t + C.rng.uniform(0, 6)) * np.exp(-t / d)
    click = butter(C.noise(n), 'high', 4000) * np.exp(-t / 0.004)
    return vel * (out + 0.5 * click)


@_tail_faded
def cymbal(vel, decay=2.2):
    """Large clashed cymbals (cymbala) for festivals and battles."""
    n = int((decay * 2 + 0.2) * SR)
    t = np.arange(n) / SR
    parts = C.rng.uniform(1500, 12000, 40)
    out = sum(np.sin(2 * np.pi * f * t + C.rng.uniform(0, 6)) * np.exp(-t / (decay * (0.3 + 3000 / f)))
              for f in parts) / 40
    wash = unit_std(butter(C.noise(n), 'high', 3500)) * np.exp(-t / (decay * 0.4)) * 0.3
    return vel * (out + wash) * np.clip(t / 0.002, 0, 1)


@_tail_faded
def clap(vel):
    """Hand clap (a few people at once)."""
    n = int(0.25 * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)
    for k in range(3):
        s = int((k * 0.009 + C.rng.uniform(0, 0.004)) * SR)
        tt = t[:n - s]
        out[s:] += unit_std(butter(C.noise(n - s), 'band', [900, 3200]), 300) * np.exp(-tt / (0.01 if k < 2 else 0.05))
    return vel * 0.5 * out


@_tail_faded
def bell(m, vel, decay=2.5):
    """Small bronze bell (tintinnabulum): inharmonic partials."""
    f = hz(m)
    n = int((decay * 2) * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)
    for r, a, d in ((1, 1, 1), (2.76, .5, .5), (5.4, .25, .3), (8.93, .12, .15)):
        if f * r < 0.45 * SR:
            out += a * np.sin(2 * np.pi * f * r * t) * np.exp(-t / (decay * d))
    return vel * out * np.clip(t / 0.001, 0, 1)
