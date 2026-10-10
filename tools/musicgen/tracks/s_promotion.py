"""Promotion stinger: a cornu fanfare on the natural harmonics of D, crotala, organ chord."""
import arrange as A
import instruments as I
from core import bpm_clock, midi, phrase
from mix import Mix

CLK = bpm_clock(80, 3, 2)
HI = "A3 D4:2 F#4 A4:2 | A4:3 F#4 D4 A4 |"
LO = "D3 A3:2 D4 F#4:2 | F#4:3 D4 A3 F#4 |"



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
    m = Mix(6.0, tail=3.5)
    t, bs = CLK.t, CLK.bar_s
    hold = 2.6
    m.line('cornu', I.brass(phrase(CLK, HI, 0, vel=0.85, sd=0.002, shape=False) +
                            [(t(2), hold, midi('D5'), 0.9)], 'cornu'))
    m.line('buccina', I.brass(phrase(CLK, LO, 0, vel=0.7, sd=0.002, shape=False) +
                              [(t(2), hold, midi('A4'), 0.75)], 'buccina'))
    m.line('tuba', I.brass([(t(2), hold, midi('D3'), 0.7)], 'tuba'))
    m.add('organ', 0, I.organ(ms('D3', 'A3', 'D4'), 2 * bs, 0.45, ranks='flute', attack=0.3))
    m.add('organ', t(2), I.organ(ms('D3', 'A3', 'D4', 'F#4', 'A4'), hold, 0.7, ranks='full', attack=0.05))
    A.drums(m, 'drums', CLK, 0, 1, [(0, 'b', .6)], A.STD_KIT)
    A.drums(m, 'drums', CLK, 1, 1, [(0, 'b', .5), (3, 'j', .35), (4, 'j', .45), (5, 'j', .6)], A.STD_KIT)
    m.add('drums', t(2), faded(I.war_drum(0.9, pitch=42, decay=0.7)))
    m.add('crotala', 0, I.crotala(0.5))
    m.add('crotala', t(2), I.crotala(0.8))
    return m, dict(space='hall', levels={
        'cornu': (-18, 0.15, 0.45), 'buccina': (-21, -0.2, 0.45), 'tuba': (-24, 0.0, 0.4),
        'organ': (-22, 0.0, 0.45), 'drums': (-24, -0.1, 0.25), 'crotala': (-30, 0.35, 0.55),
    })
