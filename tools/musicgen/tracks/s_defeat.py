"""s_defeat — mission failed (~6 s).

A Phrygian: the aulos falls stepwise from A4 to the Bb-A Phrygian cadence and
holds a low A over a dark organ chord; two low drum strokes.
"""
import instruments as I
from core import Clock, midi, phrase
from loopkit import faded
from mix import Mix

CLK = Clock(0.36, 8)
FALL = "A4 G4 F4 E4 D4 C4 Bb3:2 | A3:8 |"


def build():
    m = Mix(5.6, tail=3.0)
    notes = phrase(CLK, FALL, 0, vel=0.8, sd=0.006)
    notes = [(t, d, p, v * (1 - 0.03 * i)) for i, (t, d, p, v) in enumerate(notes)]
    m.line('aulos', I.aulos(notes, vib=0.012, bright=0.1))
    m.add('organ', CLK.t(1) - 0.1, I.organ([midi('A2'), midi('E3'), midi('C4')], 2.6, vel=0.6, ranks='soft'))
    m.add('drone', CLK.t(0.5), I.drone([midi('A2')], 3.9, 1.0, 1.2, vel=0.5, formant=600))
    m.add('drum', 0.0, faded(I.boom(0.8, pitch=52, decay=0.45)))
    m.add('drum', CLK.t(1), faded(I.war_drum(0.9, pitch=40, decay=0.9)))
    return m, dict(space='hall', levels={
        'aulos': (-18, 0.1, 0.4), 'organ': (-26, 0.0, 0.5), 'drone': (-28, 0.0, 0.4), 'drum': (-22, -0.1, 0.3),
    })
