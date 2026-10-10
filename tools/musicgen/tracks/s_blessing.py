"""s_blessing — a god answers: harp glissando up, small bells, a soft major choir chord (G)."""
import numpy as np

import arrange as A
import core as C
import instruments as I
from core import Mode, midi
from mix import Mix

MODE = Mode('G', 'ionian')


def faded(sig, seconds):
    """Fade a percussion tail to zero (bell() ends abruptly)."""
    n = min(len(sig), int(seconds * 44100))
    sig = sig.copy()
    sig[-n:] *= np.linspace(1, 0, n) ** 2
    return sig


def build():
    m = Mix(5.0, tail=2.5)
    # harp glissando G3 -> G5 over 0.8 s, getting softer as it rises
    p, k = midi('G3'), 0
    while p <= midi('G5'):
        m.add('harp', 0.05 + k * 0.055 + C.rng.normal(0, 0.004), I.pluck(p, 0.75 - 0.012 * k, kind='harp',
                                                                           dark=0.3, length=4.0))
        p, k = MODE.step(p, 1), k + 1
    # bells sprinkle at the top
    for t, n, v in ((0.85, 'G5', .7), (1.05, 'B5', .5), (1.3, 'D6', .55), (1.7, 'G6', .4), (2.2, 'D6', .3)):
        m.add('bells', t, faded(I.bell(midi(n), v, decay=2.0), 2.0))
    m.add('choir', 0.6, A.choir_chord([midi(n) for n in ('G3', 'B3', 'D4', 'G4')], 3.2, 0.6, 'a'))
    return m, dict(space='temple', levels={
        'harp': (-20, -0.2, 0.45), 'bells': (-25, 0.3, 0.55), 'choir': (-24, 0.05, 0.6),
    })
