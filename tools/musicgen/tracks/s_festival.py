"""Stinger: a festival begins. Tambourine roll, an aulos run up to G, crotala."""
import numpy as np

import instruments as I
from core import SR, midi
from mix import Mix

LAND = 1.05   # seconds: where the run lands


def crotala(v):
    """I.crotala's 2.5 s buffer cuts its 1.6 s ring off at about -14 dB; fade it out instead."""
    x = I.crotala(v)
    f = int(1.0 * SR)
    x[-f:] *= np.linspace(1, 0, f) ** 2
    return x


def build():
    m = Mix(2.4, tail=3.0)

    # accelerating riq roll under a shaker swell
    gaps = np.linspace(0.13, 0.04, 14)
    t = 0.0
    for i, g in enumerate(gaps):
        m.add('perc', t, I.riq(0.35 + 0.035 * i))
        t += g
    m.add('perc', 0.0, I.shake(0.6, dur=LAND))
    hit = I.boom(0.9, pitch=58, decay=0.35)
    hit[-int(0.4 * SR):] *= np.linspace(1, 0, int(0.4 * SR))   # boom's buffer ends mid-decay
    m.add('perc', LAND, hit)
    m.add('perc', LAND, I.riq(0.9))

    # aulos run G4 .. G5, landing on the tonic
    run = ['G4', 'A4', 'B4', 'C5', 'D5', 'E5', 'F5']
    step = (LAND - 0.45) / len(run)
    notes = [(0.45 + i * step, step * 0.95, midi(n), 0.7 + 0.03 * i) for i, n in enumerate(run)]
    notes.append((LAND, 1.2, midi('G5'), 0.95))
    m.line('aulos', I.aulos(notes, vib=0.012))

    m.add('lyre', LAND, I.strum([midi(n) for n in ('G3', 'D4', 'G4', 'B4')], 0.6, spread=0.02,
                                length=3.0, dark=0.45))
    m.add('crotala', LAND, crotala(0.8))

    return m, dict(space='courtyard', wet_db=-8, levels={
        'aulos': (-18, 0.1, 0.3), 'perc': (-23, -0.1, 0.2), 'lyre': (-23, -0.25, 0.3),
        'crotala': (-28, 0.35, 0.5),
    })
