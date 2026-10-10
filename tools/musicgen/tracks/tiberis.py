"""Tiberis — river and trade (port cities).

C Ionian, 4/4, quarter = 92. A lyre runs in steady eighths like moving water;
the aulos sings over it; light frame drum and a shaker keep the docks busy.

Form: intro (4) · A aulos (16) · B aulos (8) · break, lyre alone (4) ·
A' aulos with pan pipes a third below (16) · outro (4).
"""
import numpy as np

import arrange as A
import instruments as I
from core import SR, Mode, bpm_clock, midi, phrase
from mix import Mix

MODE = Mode('C', 'ionian')
CLK = bpm_clock(92, 2, 4)   # eighths, 8 per bar

THEME_A = """
E5:3 D5 C5:2 D5:2 | E5:2 G5:2 E5:4    | D5:3 C5 A4:2 C5:2 | D5:8 |
E5:3 D5 C5:2 E5:2 | G5:3 A5 G5:2 E5:2 | D5:2 C5:2 D5:2 E5:2 | C5:8 |
A4:3 C5 D5:2 E5:2 | F5:3 E5 D5:4      | G4:3 A4 C5:2 D5:2 | E5:8 |
F5:3 E5 D5:2 C5:2 | A4:2 C5:2 D5:4    | E5:3 D5 C5:2 B4:2 | C5:8 |
"""
ROOTS_A = "C C F G C C G C A D G C F D G C"

THEME_B = """
A5:3 G5 E5:2 C5:2 | D5:3 E5 F5:4      | G5:3 F5 E5:2 D5:2 | E5:6 r:2 |
A5:3 G5 E5:2 C5:2 | D5:2 E5:2 F5:2 A5:2 | G5:3 F5 E5:2 D5:2 | D5:8 |
"""
ROOTS_B = "A D G C A D G G"

RUN_A = [0, 1, 2, 3, 4, 3, 2, 1]
RUN_B = [0, 2, 1, 3, 2, 4, 3, 1]


def crotala(v):
    """I.crotala's 2.5 s buffer cuts its 1.6 s ring off at about -14 dB; fade it out instead."""
    x = I.crotala(v)
    f = int(1.0 * SR)
    x[-f:] *= np.linspace(1, 0, f) ** 2
    return x


def build():
    m = Mix(52 * CLK.bar_s)
    t = CLK.t
    run = dict(clock=CLK, mode=MODE, low='C3', dark=0.55, length=2.2, accents=(0, 4), sd=0.005)

    # the river: running lyre eighths throughout
    A.arpeggio(m, 'lyre_run', bar0=0, roots="C F C G", pattern=RUN_A, vel=0.55, fade=[.7, .8, .9, 1], **run)
    A.arpeggio(m, 'lyre_run', bar0=4, roots=ROOTS_A, pattern=RUN_A, vel=0.6, **run)
    A.arpeggio(m, 'lyre_run', bar0=20, roots=ROOTS_B, pattern=RUN_B, vel=0.62, **run)
    A.arpeggio(m, 'lyre_run', bar0=28, roots="C A F G", pattern=RUN_A, vel=0.6, **run)
    A.arpeggio(m, 'lyre_run', bar0=32, roots=ROOTS_A, pattern=RUN_B, vel=0.64, **run)
    A.arpeggio(m, 'lyre_run', bar0=48, roots="C F G -", pattern=RUN_A, vel=0.55, fade=[1, .85, .7, 0], **run)
    m.add('lyre_run', t(51), I.strum([midi(n) for n in ('C3', 'G3', 'C4', 'E4', 'G4')], 0.45,
                                     spread=0.05, length=5.0, dark=0.5))

    # aulos
    m.line('aulos', I.aulos(phrase(CLK, THEME_A, 4)))
    m.line('aulos', I.aulos(phrase(CLK, THEME_B, 20, vel=0.85)))
    a2 = phrase(CLK, THEME_A, 32, vel=0.85)
    m.line('aulos', I.aulos(a2))
    for ts, td, p, v in a2:   # pan pipes a diatonic third below
        m.add('pipes', ts + 0.012, I.pipes(MODE.step(p, -2), td, 0.6 * v))

    # frame drum and shaker
    kit = A.STD_KIT
    A.drums(m, 'drums', CLK, 4, 16, [(0, 'b', .7), (4, 't', .35), (6, 't', .2)], kit)
    fill = [(0, 'b', .8), (3, 'j', .3), (4, 'b', .5), (5, 'j', .3), (6, 'j', .45), (7, 'j', .55)]
    busy = [(0, 'b', .8), (3, 'j', .3), (4, 'b', .5), (6, 'j', .4), (7, 'j', .25)]
    A.drums(m, 'drums', CLK, 20, 8, busy, kit, fill=fill)
    A.drums(m, 'drums', CLK, 32, 16, busy, kit, fill=fill)
    A.drums(m, 'drums', CLK, 48, 2, [(0, 'b', .6)], kit, fade=[1, .7])
    shk = [(u, 's', .3) for u in (1, 3, 5, 7)]
    A.drums(m, 'shaker', CLK, 2, 26, shk, {'s': lambda v: I.shake(v, 0.12)},
            fade=[.5, .75] + [1] * 24)
    A.drums(m, 'shaker', CLK, 30, 18, shk, {'s': lambda v: I.shake(v, 0.12)},
            fade=[.5, .75] + [1] * 16)

    for b in (4, 20, 32, 40):
        m.add('crotala', t(b), crotala(0.6))

    return m, dict(space='courtyard', levels={
        'aulos': (-18, 0.12, 0.3), 'pipes': (-23, 0.3, 0.45), 'lyre_run': (-23, -0.3, 0.35),
        'drums': (-26, -0.1, 0.2), 'shaker': (-31, 0.25, 0.25), 'crotala': (-34, 0.35, 0.55),
    })
