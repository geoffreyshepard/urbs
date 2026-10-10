"""Monument stinger: organ swells from C to D major; choir, brass chord and cymbal on arrival."""
import numpy as np

import arrange as A
import instruments as I
from core import SR, midi
from mix import Mix



def faded(sig, sec=0.5):
    """War drums end abruptly at about -30 dB; fade the tail so it can't click."""
    import numpy as _np
    n = min(len(sig), int(sec * 44100))
    out = sig.copy()
    out[-n:] *= _np.linspace(1, 0, n)
    return out

def ms(*names):
    return [midi(n) for n in names]


def build():
    m = Mix(6.5, tail=3.5)
    arrive, hold = 2.2, 3.6
    m.add('organ', 0, I.organ(ms('C3', 'G3', 'C4', 'E4', 'G4'), arrive, 0.6, attack=1.8))
    m.add('organ', arrive, I.organ(ms('D3', 'A3', 'D4', 'F#4', 'A4'), hold, 0.8, ranks='full', attack=0.05))
    m.add('choir', arrive - 0.1, A.choir_chord(ms('D4', 'F#4', 'A4'), hold, 0.6, 'a'))
    m.add('choir', 0.6, A.choir_chord(ms('C4', 'E4', 'G4'), arrive - 0.6, 0.35, 'o'))
    swell = None
    for name, kind, v in (('D3', 'cornu', 0.8), ('A3', 'cornu', 0.75), ('D4', 'tuba', 0.75), ('F#4', 'tuba', 0.7)):
        t0, sig = I.brass([(arrive, hold, midi(name), v)], kind)
        n = len(sig)
        swell = np.clip(np.arange(n) / (1.2 * SR), 0.55, 1.0)   # crescendo through the chord
        m.add('brass', t0, sig * swell)
    for k, at in enumerate(np.linspace(1.3, arrive - 0.12, 7)):
        m.add('drums', at, I.tek(0.3 + 0.07 * k, jingle=0.4))
    m.add('drums', arrive, faded(I.war_drum(1.0, pitch=38, decay=0.9)))
    m.add('metal', arrive, I.cymbal(0.8, decay=3.0))
    m.add('bells', arrive + 0.05, I.bell(midi('D6'), 0.5) + I.bell(midi('A6'), 0.3))
    return m, dict(space='temple', levels={
        'organ': (-20, 0.0, 0.5), 'choir': (-25, 0.0, 0.6), 'brass': (-21, 0.15, 0.45),
        'drums': (-24, -0.1, 0.25), 'metal': (-28, 0.3, 0.5), 'bells': (-31, 0.35, 0.55),
    })
