"""Helpers for seamless-loop tracks (tension, battle, disaster) and their stingers.

Not a track: build_all only renders modules named in data/defs/music.json.
"""
import numpy as np

import core as C
import instruments as I
from core import SR, humanize, parse
from mix import Mix


class LoopMix(Mix):
    """A Mix whose events that start before t=0 wrap to the end of the loop."""

    def add(self, stem, start, sig, gain=1.0):
        if start < 0:
            start += self.length
        super().add(stem, start, sig, gain)


def seamless(sig, length, xfade):
    """sig spans [0, length + xfade]. Shape the head and tail as an equal-power
    crossfade so Mix.render's loop wrap joins them without a bump or dip."""
    n, x = int(round(length * SR)), int(xfade * SR)
    out = np.zeros(n + x)
    out[:min(len(sig), n + x)] = sig[:n + x]
    ramp = np.sin(0.5 * np.pi * np.arange(x) / x)
    out[:x] *= ramp
    out[n:n + x] *= ramp[::-1]
    return out


def loop_drone(midis, length, vel=0.6, formant=900, xfade=2.0, swell=None):
    """A drone that runs through the whole loop. swell(t) -> gain multiplier (periodic)."""
    sig = I.drone(midis, length + xfade, 1e-3, 1e-3, vel, formant)
    if swell is not None:
        t = np.arange(len(sig)) / SR
        sig *= swell(t)
    return seamless(sig, length, xfade)


def pluck_score(mix, stem, clock, bar0, score, vel=0.7, kind='lyre', dark=0.55, length=1.6,
                accents=(0, 4), weak=0.72, sd=0.005, octave=0):
    """Plucked riff written as a score string; accents are units within the bar."""
    for st, du, m in parse(score, clock.bar):
        u = st % clock.bar
        v = vel * (1.0 if u in accents else weak) * (1 + C.rng.normal(0, 0.07))
        mix.add(stem, humanize(clock.t(bar0, st), sd),
                I.pluck(m + 12 * octave, v, length=length, dark=dark, kind=kind))


def riff_seq(riffs, seq):
    return ' | '.join(riffs[k] for k in seq.split())


def swell(sig, dur, rise=0.7, floor=0.08):
    """Give a held sound a crescendo over `rise` of its duration, then hold."""
    t = np.arange(len(sig)) / SR
    g = floor + (1 - floor) * np.clip(t / (dur * rise), 0, 1) ** 2
    return sig * g


def brass_chord(midis, t, dur, vel, kinds=('cornu',)):
    """Several sustained horns as one chord: [(t0, sig)] per voice."""
    out = []
    for i, m in enumerate(midis):
        k = kinds[i % len(kinds)]
        out.append(I.brass([(t + C.rng.normal(0, 0.01), dur, m, vel)], k))
    return out


def faded(sig, fade=0.4):
    """Fade out the end of a percussion hit. instruments.boom/war_drum stop at
    about exp(-3) of their level, which is audible when the drum is exposed."""
    n = min(len(sig), int(fade * SR))
    out = sig.copy()
    out[-n:] *= np.linspace(1, 0, n) ** 2
    return out
