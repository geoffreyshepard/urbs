"""Shared constants, music theory, score parsing and DSP helpers for musicgen.

Every random choice comes from `core.rng`, which `seed(track_id)` resets with a
CRC of the track id, so a render is reproducible. Instruments read `core.rng`
at call time; never bind it with `from core import rng`.
"""
import zlib

import numpy as np
from scipy import signal

SR = 44100
rng = np.random.default_rng(0)


def seed(name):
    global rng
    rng = np.random.default_rng(zlib.crc32(name.encode()))


# ---------------------------------------------------------------- pitch
NOTE = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}
ACC = {'b': -1.0, '#': 1.0, '~': -0.5, '^': 0.5}   # ~ and ^ are quarter tones


def midi(name):
    """'D4' -> 62, 'Bb3' -> 58, 'E~4' -> 63.5 (quarter-flat E)."""
    pc, i = float(NOTE[name[0]]), 1
    while name[i] in ACC:
        pc += ACC[name[i]]
        i += 1
    m = pc + 12 * (int(name[i:]) + 1)
    return int(m) if m == int(m) else m


def hz(m):
    return 440.0 * 2 ** ((m - 69) / 12)


MODES = {
    'ionian':     [0, 2, 4, 5, 7, 9, 11],
    'dorian':     [0, 2, 3, 5, 7, 9, 10],
    'phrygian':   [0, 1, 3, 5, 7, 8, 10],
    'lydian':     [0, 2, 4, 6, 7, 9, 11],
    'mixolydian': [0, 2, 4, 5, 7, 9, 10],
    'aeolian':    [0, 2, 3, 5, 7, 8, 10],
    'hijaz':      [0, 1, 4, 5, 7, 8, 10],     # phrygian dominant
    'bayati':     [0, 1.5, 3, 5, 7, 8, 10],   # neutral second
}


class Mode:
    """A seven-note mode on a tonic, e.g. Mode('D', 'dorian')."""

    def __init__(self, tonic, name):
        self.tonic = NOTE[tonic[0]] + sum(ACC[c] for c in tonic[1:])
        self.steps = MODES[name]

    def _deg(self, m):
        rel = m - self.tonic
        octave = int(np.floor(rel / 12 + 1e-9))
        pc = rel - 12 * octave
        dists = [abs(pc - s) for s in self.steps]
        return octave * 7 + int(np.argmin(dists))

    def step(self, m, k):
        """Move pitch m by k diatonic steps (m snaps to the nearest scale note)."""
        o, i = divmod(self._deg(m) + k, 7)
        return self.tonic + 12 * o + self.steps[i]

    def snap(self, m):
        return self.step(m, 0)

    def triad(self, root, extra=()):
        """Diatonic chord on root: [root, third, fifth] plus extra degrees (e.g. 6 for a 7th)."""
        return [root, self.step(root, 2), self.step(root, 4)] + [self.step(root, k) for k in extra]


# ---------------------------------------------------------------- time and scores
class Clock:
    """unit: seconds per score unit; bar: units per bar."""

    def __init__(self, unit, bar):
        self.unit, self.bar = unit, bar

    def t(self, bar, u=0.0):
        return (bar * self.bar + u) * self.unit

    @property
    def bar_s(self):
        return self.bar * self.unit


def bpm_clock(bpm, units_per_beat, beats_per_bar):
    return Clock(60.0 / (bpm * units_per_beat), units_per_beat * beats_per_bar)


def parse(score, bar):
    """'A4:3 G4 F4 G4 | r:2 D5 ...' -> [(start_units, dur_units, midi)].

    Bars are separated by '|' and must each add up to `bar` units. A token is
    NOTE[:dur] (dur defaults to 1); 'r' is a rest.
    """
    out, t = [], 0.0
    for b in score.split('|'):
        if not b.strip():
            continue
        bt = 0.0
        for tok in b.split():
            n, _, d = tok.partition(':')
            d = float(d) if d else 1.0
            if n != 'r':
                out.append((t + bt, d, midi(n)))
            bt += d
        assert abs(bt - bar) < 1e-6, f"bar sums to {bt}, expected {bar}: {b!r}"
        t += bar
    return out


def bars_in(score):
    return sum(1 for b in score.split('|') if b.strip())


def humanize(t, sd=0.005):
    return t + rng.normal(0, sd)


def phrase(clock, score, bar0, vel=0.8, octave=0, sd=0.004, legato=0.95, transpose=0, shape=True):
    """Score -> [(t_seconds, dur_seconds, midi, vel)], humanized.

    With shape=True, longer and higher notes are played a little louder.
    """
    out = []
    for st, du, m in parse(score, clock.bar):
        m = m + 12 * octave + transpose
        v = vel
        if shape:
            v *= 0.88 + 0.12 * min(du * clock.unit, 0.9) / 0.9 + 0.008 * (m - 66)
        out.append((humanize(clock.t(bar0, st), sd), du * clock.unit * legato, m, v))
    return out


def ornament(notes, mode, min_dur=0.45, grace=0.075, kind='upper', every=1):
    """Heterophony: lead long notes in with a grace note from a neighbour."""
    out = []
    for i, (t, d, m, v) in enumerate(notes):
        if d >= min_dur and i % every == 0:
            k = 1 if kind == 'upper' else -1
            out.append((t, grace, mode.step(m, k), v * 0.85))
            out.append((t + grace, d - grace, m, v))
        else:
            out.append((t, d, m, v))
    return out


# ---------------------------------------------------------------- dsp
def butter(x, kind, f, order=2):
    sos = signal.butter(order, f, btype=kind, fs=SR, output='sos')
    return signal.sosfilt(sos, x)


def peaking(x, f0, gain_db, q):
    a_ = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * f0 / SR
    alpha = np.sin(w0) / (2 * q)
    b = [1 + alpha * a_, -2 * np.cos(w0), 1 - alpha * a_]
    a = [1 + alpha / a_, -2 * np.cos(w0), 1 - alpha / a_]
    return signal.lfilter(b, a, x)


def noise(n):
    return rng.standard_normal(n)


def unit_std(x, n=None):
    s = np.std(x[:n] if n else x)
    return x / (s + 1e-12)


def place(buf, start_s, sig, gain=1.0):
    s = int(round(start_s * SR))
    if s < 0:
        sig, s = sig[-s:], 0
    e = min(len(buf), s + len(sig))
    if e > s:
        buf[s:e] += gain * sig[:e - s]


def active_rms(x):
    env = butter(np.abs(x), 'low', 5.0, 1)
    mask = env > 0.02 * env.max()
    return np.sqrt(np.mean(x[mask] ** 2)) if mask.any() else 1e-9
