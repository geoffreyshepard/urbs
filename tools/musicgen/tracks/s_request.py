"""s_request — a request from Rome: a dignified cornu call on the harmonic series of C."""
import numpy as np

import instruments as I
from core import midi
from mix import Mix


def build():
    m = Mix(3.0, tail=2.0)
    m.line('cornu', I.brass([(0.05, 0.3, midi('G3'), .7), (0.4, 0.3, midi('C4'), .75),
                             (0.75, 1.7, midi('G4'), .85)], 'cornu'))
    m.line('cornu_lo', I.brass([(0.75, 1.7, midi('C3'), .7)], 'cornu'))
    boom = I.boom(0.8, pitch=56, decay=0.4)
    boom[-int(0.6 * 44100):] *= np.linspace(1, 0, int(0.6 * 44100)) ** 2   # boom() ends abruptly
    m.add('drum', 0.75, boom)
    return m, dict(space='courtyard', levels={
        'cornu': (-18, 0.1, 0.4), 'cornu_lo': (-24, -0.2, 0.4), 'drum': (-26, -0.1, 0.3),
    })
