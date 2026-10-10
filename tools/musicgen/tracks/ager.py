"""Ager — farms and vineyards.

F Lydian, 3/4, quarter = 96. Pastoral: pan pipes alone, then as a duet in
thirds; a ney takes a wistful middle tune; the lyre sways underneath; finger
cymbals mark the last return.

Form: intro (4) · A pipes (16) · A pipes duet (16) · B ney (16) ·
A duet with ney sustains (16) · outro (4).
"""
import numpy as np

import arrange as A
import instruments as I
from core import SR, Mode, butter, bpm_clock, midi, phrase
from mix import Mix

MODE = Mode('F', 'lydian')
CLK = bpm_clock(96, 2, 3)   # eighths, 6 per bar

THEME_A = """
C5:2 F5:2 E5 D5 | C5:4 A4:2      | B4:2 C5:2 D5:2 | C5:6 |
A4:2 C5:2 F5:2  | G5:3 F5 E5:2   | D5:2 E5 D5 C5:2 | G4:6 |
C5:2 F5:2 E5 D5 | C5:4 A4:2      | B4:2 D5:2 G5:2 | E5:6 |
F5:2 E5 D5 C5 A4 | G4:2 B4:2 D5:2 | A4:2 B4 C5 E5:2 | F5:6 |
"""
ROOTS_A = "F F G C F C G C F F G C D G C F"

THEME_B = """
D5:4 E5 F5     | E5:3 D5 C5:2   | B4:2 C5:2 D5:2 | A4:6 |
F4:2 A4:2 C5:2 | D5:3 C5 B4:2   | C5:2 A4:2 G4:2 | A4:6 |
D5:4 E5 F5     | G5:3 F5 E5:2   | D5:2 E5:2 C5:2 | B4:6 |
C5:2 D5 E5 F5:2 | G5:3 E5 C5:2  | D5:2 B4:2 G4:2 | C5:6 |
"""
ROOTS_B = "D A G D F G C A D C A G F C G C"

WALTZ = [0, None, 1, None, 2, None]
FLOW = [0, 1, 2, 3, 2, 1]


def crotala(v):
    """I.crotala's 2.5 s buffer cuts its 1.6 s ring off at about -14 dB; fade it out instead."""
    x = I.crotala(v)
    f = int(1.0 * SR)
    x[-f:] *= np.linspace(1, 0, f) ** 2
    return x


def ney(notes):
    t0, sig = I.flute(notes, kind='ney')
    return t0, butter(sig, 'high', 220)


def pipes_line(m, stem, notes, step=0, gain=1.0):
    for ts, td, p, v in notes:
        p = MODE.step(p, step) if step else p
        m.add(stem, ts + (0.015 if step else 0), I.pipes(p, td, v * gain))


def build():
    m = Mix(72 * CLK.bar_s)
    t = CLK.t
    arp = dict(clock=CLK, mode=MODE, low='C3', dark=0.58, length=3.0, accents=(0,))

    A.arpeggio(m, 'lyre', bar0=0, roots="F C F C", pattern=WALTZ, vel=0.55, fade=[.6, .75, .9, 1], **arp)
    A.arpeggio(m, 'lyre', bar0=4, roots=ROOTS_A, pattern=WALTZ, vel=0.55, **arp)
    A.arpeggio(m, 'lyre', bar0=20, roots=ROOTS_A, pattern=WALTZ, vel=0.6, **arp)
    A.arpeggio(m, 'lyre', bar0=36, roots=ROOTS_B, pattern=FLOW, vel=0.55, **arp)
    A.arpeggio(m, 'lyre', bar0=52, roots=ROOTS_A, pattern=[0, None, 1, 3, 2, None], vel=0.6, **arp)
    A.arpeggio(m, 'lyre', bar0=68, roots="F C F", pattern=WALTZ, vel=0.5, fade=[1, .8, .6], **arp)
    m.add('lyre', t(71), I.strum([midi(n) for n in ('F3', 'C4', 'F4', 'A4', 'C5')], 0.4,
                                 spread=0.05, length=5.0, dark=0.5))

    # A: pipes alone
    pipes_line(m, 'pipes', phrase(CLK, THEME_A, 4, vel=0.75))
    # A: duet in thirds
    a2 = phrase(CLK, THEME_A, 20, vel=0.8)
    pipes_line(m, 'pipes', a2)
    pipes_line(m, 'pipes_low', a2, step=-2, gain=0.75)
    # B: ney (its breath noise rides the pitch, so keep it in its middle register)
    m.line('ney', ney(phrase(CLK, THEME_B, 36, vel=0.8)))
    # A: duet again; the ney holds the root under each phrase end
    a3 = phrase(CLK, THEME_A, 52, vel=0.85)
    pipes_line(m, 'pipes', a3)
    pipes_line(m, 'pipes_low', a3, step=-2, gain=0.75)
    holds = [(t(52 + b), 2.6 * CLK.bar_s, midi(n), 0.55)   # a third under each phrase end
             for b, n in ((3, 'A4'), (7, 'E4'), (11, 'C5'), (15, 'A4'))]
    m.line('ney', ney(holds), 0.8)
    # outro: a last echo of the opening
    pipes_line(m, 'pipes', phrase(CLK, "C5:2 F5:2 E5 D5 | C5:6 | r:6", 68, vel=0.55))

    # light drum only in the last A, crotala marks phrases
    A.drums(m, 'drums', CLK, 52, 16, [(0, 'b', .55), (4, 't', .2)], A.STD_KIT)
    for b in (20, 36, 52, 56, 60, 64, 71):
        m.add('crotala', t(b), crotala(0.55 if b != 71 else 0.35))

    return m, dict(space='field', wet_db=-7, levels={
        'pipes': (-19, 0.2, 0.45), 'pipes_low': (-23, -0.2, 0.45), 'ney': (-19, 0.05, 0.45),
        'lyre': (-23, -0.25, 0.35), 'drums': (-28, -0.1, 0.25), 'crotala': (-34, 0.4, 0.55),
    })
