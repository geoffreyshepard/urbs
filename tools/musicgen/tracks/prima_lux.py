"""Prima Lux — the founding (mission 1).

D Dorian, 6/8, dotted quarter = 52. A solo lyre with lots of air over a soft
drone; pan pipes answer now and then; a quiet frame-drum heartbeat only in the
middle. The pipe phrase opens with the head of the Urbs theme (A G F G A) and
then turns away from it.

Form: intro (4) · lyre song (16) · pipes (8) · lyre song with pipe echoes (16)
· pipes (8) · lyre fragments and close (8).
"""
import numpy as np

import arrange as A
import instruments as I
from core import SR, Mode, bpm_clock, midi, phrase
from mix import Mix

MODE = Mode('D', 'dorian')
CLK = bpm_clock(52, 3, 2)   # eighths, 6 per bar

SONG = """
r:3 A4:2 G4  | F4:3 G4:2 A4 | D5:4 C5:2    | A4:6 |
r:3 G4:2 F4  | E4:3 F4:2 G4 | A4:3 E4:3    | D4:6 |
r:3 F4:2 G4  | A4:3 C5:2 D5 | E5:4 D5:2    | C5:3 A4:3 |
r:3 G4:2 A4  | B4:3 A4:2 G4 | F4:3 E4:3    | D4:6 |
"""
ROOTS_SONG = "D D G D C C A D F F C F G G A D"

PIPES = """
A4:3 G4 F4 G4 | A4:4 r:2   | C5:3 B4 A4 G4 | E4:4 r:2 |
F4:3 G4 A4 C5 | D5:4 C5:2  | A4:3 G4:2 E4  | D4:6 |
"""
PIPES_2 = """
A4:3 G4 F4 G4 | A4:4 r:2   | C5:3 D5 E5 C5 | A4:4 r:2 |
G4:3 F4 E4 F4 | G4:4 A4:2  | F4:3 E4:2 C4  | D4:6 |
"""
ROOTS_PIPES = "D D G A F G C D"
ROOTS_PIPES_2 = "D D C A C G A D"

ECHOES = {4: "D5:2 C5 r:3", 8: "A4:2 C5 r:3", 12: "E5:2 D5 r:3"}

CODA = """
r:3 A4:2 G4 | F4:3 G4:2 A4 | D5:6 | r:6 |
r:3 A3:2 G3 | F3:3 E3:3    | r:6  | r:6 |
"""


def crotala(v):
    """I.crotala's 2.5 s buffer cuts its 1.6 s ring off at about -14 dB; fade it out instead."""
    x = I.crotala(v)
    f = int(1.0 * SR)
    x[-f:] *= np.linspace(1, 0, f) ** 2
    return x


def bass(m, bar0, roots, vel=0.5, pattern=(0, None, None, 1, None, None)):
    A.arpeggio(m, 'lyre_bass', CLK, bar0, roots, MODE, list(pattern), low='C3', vel=vel,
               dark=0.62, length=3.6, sd=0.008)


def song(m, bar0, vel, octave_double=False):
    for ts, td, p, v in phrase(CLK, SONG, bar0, vel=vel, sd=0.009):
        m.add('lyre', ts, I.pluck(p, v, dark=0.4, length=4.0))
        if td > 3.5 * CLK.unit:   # let long notes bloom: a soft re-pluck
            m.add('lyre', ts + 3 * CLK.unit, I.pluck(p, 0.45 * v, dark=0.5, length=3.5))
            if octave_double:
                m.add('lyre', ts + 0.03, I.pluck(p - 12, 0.4 * v, dark=0.5, length=3.5))


def pipes(m, score, bar0, vel=0.7):
    for ts, td, p, v in phrase(CLK, score, bar0, vel=vel, sd=0.006):
        m.add('pipes', ts, I.pipes(p, td, v))


def build():
    m = Mix(60 * CLK.bar_s, tail=6.0)
    t = CLK.t

    # intro: drone, then single low notes
    bass(m, 1, "D D D", vel=0.45, pattern=(0, None, None, None, None, None))
    m.add('lyre', t(2, 3), I.pluck(midi('A4'), 0.35, dark=0.45, length=4))
    m.add('lyre', t(3, 3), I.pluck(midi('D5'), 0.3, dark=0.45, length=4))

    # lyre song
    song(m, 4, vel=0.7)
    bass(m, 4, ROOTS_SONG)

    # pipes, heartbeat begins
    pipes(m, PIPES, 20)
    bass(m, 20, ROOTS_PIPES, pattern=(0, None, 2, 1, None, None))

    # song again, the pipes echo in the gaps
    song(m, 28, vel=0.78, octave_double=True)
    bass(m, 28, ROOTS_SONG, vel=0.55, pattern=(0, None, 2, 1, None, None))
    for i, frag in ECHOES.items():
        pipes(m, frag, 28 + i, vel=0.45)

    # pipes once more, heartbeat fades
    pipes(m, PIPES_2, 44, vel=0.72)
    bass(m, 44, ROOTS_PIPES_2, pattern=(0, None, 2, 1, None, None))

    # coda: fragments, then the final chord
    for ts, td, p, v in phrase(CLK, CODA, 52, vel=0.6, sd=0.01):
        m.add('lyre', ts, I.pluck(p, v, dark=0.45, length=4.5))
    bass(m, 52, "D D D D C - - -", vel=0.45, pattern=(0, None, None, None, None, None))
    m.add('lyre', t(57.5), I.strum([midi(n) for n in ('D3', 'A3', 'D4', 'F4', 'A4')], 0.32,
                                   spread=0.07, length=6.0, dark=0.5))

    # drone through everything
    m.add('drone', 0, I.drone([midi('D3'), midi('A2')], t(59.5), 3 * CLK.bar_s, 4 * CLK.bar_s, vel=0.5))

    # heartbeat: only in the middle
    n = 24
    fade = [min(1, (i + 1) / 3, (n - i) / 5) for i in range(n)]
    A.drums(m, 'drums', CLK, 20, n, [(0, 'b', .5), (3, 'b', .28)], A.STD_KIT, fade=fade, sd=0.007)

    m.add('crotala', t(20), crotala(0.5))
    m.add('crotala', t(44), crotala(0.4))

    return m, dict(space='courtyard', wet_db=-7, levels={
        'lyre': (-19, -0.1, 0.4), 'lyre_bass': (-25, -0.3, 0.4), 'pipes': (-20, 0.25, 0.5),
        'drone': (-30, 0.0, 0.45), 'drums': (-28, -0.15, 0.25), 'crotala': (-35, 0.35, 0.6),
    })
