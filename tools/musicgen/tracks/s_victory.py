"""Victory stinger: the Urbs head on brass and organ, opening into D major."""
import arrange as A
import instruments as I
from core import bpm_clock, midi, phrase
from mix import Mix

CLK = bpm_clock(72, 3, 2)
HEAD = "A4:3 G4 F4 G4 | A4:2 D5 C5:3 | B4:2 C#5 D5:3 |"
THIRDS = "F4:3 E4 D4 E4 | F4:2 F4 E4:3 | D4:2 E4 F#4:3 |"



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
    m = Mix(9.0, tail=3.5)
    t, bs = CLK.t, CLK.bar_s
    hold = 4.2
    hi = phrase(CLK, HEAD, 0, vel=0.85, sd=0.002)
    lo = phrase(CLK, THIRDS, 0, vel=0.7, sd=0.002)
    hi[-1] = (hi[-1][0], hold, hi[-1][2], hi[-1][3])
    lo[-1] = (lo[-1][0], hold, lo[-1][2], lo[-1][3])
    m.line('tuba', I.brass(hi, 'tuba'))
    m.line('cornu', I.brass(lo, 'cornu'))
    m.add('organ', t(0), I.organ(ms('D3', 'A3', 'D4', 'F4'), bs, 0.55, attack=0.08))
    m.add('organ', t(1), I.organ(ms('C3', 'G3', 'C4', 'E4'), bs, 0.55, attack=0.08))
    m.add('organ', t(2), I.organ(ms('G2', 'D3', 'G3', 'B3'), 0.5 * bs, 0.55, attack=0.08))
    m.add('organ', t(2, 3), I.organ(ms('D3', 'A3', 'D4', 'F#4', 'A4'), hold, 0.75, ranks='full', attack=0.05))
    m.add('choir', t(2, 3), A.choir_chord(ms('D4', 'F#4', 'A4'), hold, 0.6, 'a'))
    A.drums(m, 'drums', CLK, 0, 2, [(0, 'b', .6), (3, 't', .35)], A.STD_KIT)
    A.drums(m, 'drums', CLK, 2, 1, [(0, 'b', .6), (1, 'j', .4), (2, 'j', .55)], A.STD_KIT)
    m.add('drums', t(2, 3), faded(I.war_drum(1.0, pitch=40, decay=0.8)))
    m.add('metal', t(2, 3), I.cymbal(0.7, decay=2.6))
    m.add('crotala', t(2, 3), I.crotala(0.5))
    return m, dict(space='hall', levels={
        'tuba': (-18, 0.1, 0.45), 'cornu': (-21, -0.2, 0.45), 'organ': (-21, 0.0, 0.45),
        'choir': (-27, 0.0, 0.55), 'drums': (-24, -0.1, 0.25), 'metal': (-30, 0.3, 0.5),
        'crotala': (-32, 0.35, 0.55),
    })
