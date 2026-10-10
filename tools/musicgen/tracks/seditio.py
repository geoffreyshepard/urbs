"""Seditio — unrest and low favour. Seamless loop.

C Phrygian, 4/4, quarter = 72, 24 bars. A muffled heartbeat drum on every
beat, a low muted-lyre ostinato, a drone on C with a Db pipe that swells in
and out against it, and a wailing aulos. A: first lament (bars 2-7);
B: the lament rises (10-15); C: low murmurs, the drum grows restless (16-23),
and the last bar's Db falls back to C at the loop.
"""
import numpy as np

import arrange as A
import instruments as I
from core import Mode, SR, bpm_clock, midi, phrase
from loopkit import LoopMix, loop_drone, pluck_score, riff_seq

MODE = Mode('C', 'phrygian')
CLK = bpm_clock(72, 2, 4)   # eighths, 8 per bar
BARS = 24

RIFFS = {
    'C':  "C3 G3 Ab3 G3 C3 G3 Db4 G3",
    'C2': "C3 G3 Ab3 G3 Bb2 F3 Db3:2",
    'Db': "Db3 Ab3 Bb3 Ab3 Db3 Ab3 C4 Ab3",
    'Ab': "Ab2 Eb3 F3 Eb3 Ab2 Eb3 G3 Eb3",
    'Bb': "Bb2 F3 G3 F3 Bb2 F3 Ab3 F3",
}
SEQ = "C C Db C2  C C Bb C2  Ab Ab Db C2  Bb Bb Db C2  C Db C Db  Ab Bb Db C2"

LAMENT_1 = """
r:4 G4:3 Ab4 | G4:6 F4 Eb4 | F4:3 G4 Db4:4 | C4:8 |
r:2 Eb4 F4 G4:2 Ab4 Bb4 | Ab4:4 G4:4 |
"""
LAMENT_2 = """
r:4 C5:2 Db5:2 | C5:4 Bb4:2 Ab4:2 | Db5:3 C5 Bb4:2 Ab4 G4 | Ab4:6 G4:2 |
F4:2 G4:2 Ab4 G4 F4 Eb4 | Db4:4 C4:4 |
"""
MURMUR = "r:4 C4:2 Db4:2 | C4:8 | r:4 Eb4:2 Db4:2 | C4:8 |"


def wail(notes):
    """Aulos with slow, sliding glides between notes."""
    return I._line(notes, lambda f, e: I.reed(f, e, formant=1200, breath=0.04, bright=0.1),
                   vib_depth=0.014, glide=0.09, vib_rate=4.6)


def build():
    L = BARS * CLK.bar_s
    m = LoopMix(L)

    pluck_score(m, 'lyre', CLK, 0, riff_seq(RIFFS, SEQ), vel=0.65, dark=0.65, length=1.4)

    m.line('aulos', wail(phrase(CLK, LAMENT_1, 2, vel=0.75, sd=0.01)))
    m.line('aulos', wail(phrase(CLK, LAMENT_2, 10, vel=0.85, sd=0.01)))
    m.line('aulos', wail(phrase(CLK, MURMUR, 18, vel=0.55, octave=0, sd=0.01)), 0.8)

    m.add('drone', 0, loop_drone([midi('C3'), midi('G2')], L, vel=0.6))
    # the Db pipe breathes in and out three times per loop (periodic, so loop-safe)
    period = 8 * CLK.bar_s
    m.add('rub', 0, loop_drone([midi('Db3')], L, vel=0.5,
                               swell=lambda t: 0.5 - 0.5 * np.cos(2 * np.pi * t / period)))

    kit = {'h': lambda v: I.boom(v, pitch=55, decay=0.35), 't': lambda v: I.tek(v, pitch=300)}
    beat = [(0, 'h', .62), (2, 'h', .42), (4, 'h', .55), (6, 'h', .42)]
    A.drums(m, 'drums', CLK, 0, 16, beat, kit, sd=0.004)
    restless = beat + [(3, 't', .22), (7, 't', .28)]
    A.drums(m, 'drums', CLK, 16, 8, restless, kit, sd=0.004,
            fill=beat + [(5, 't', .2), (6.5, 't', .25), (7, 't', .3)], fill_every=4)

    return m, dict(space='hall', levels={
        'lyre': (-23, -0.25, 0.35), 'aulos': (-19, 0.15, 0.4), 'drone': (-28, 0.0, 0.4),
        'rub': (-31, 0.1, 0.5), 'drums': (-25, -0.1, 0.25),
    })
