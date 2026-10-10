"""s_wrath — a god's wrath (~5 s).

A low drum roll that accelerates and swells, then a dissonant brass cluster
(C-Db-Gb, a tritone over a semitone) and a low organ cluster that hang and decay.
"""
import core as C

import instruments as I
from core import midi
from loopkit import faded, swell
from mix import Mix


def build():
    m = Mix(4.6, tail=3.0)
    t, gap = 0.0, 0.2
    while t < 1.45:                      # accelerating roll
        v = 0.3 + 0.6 * t / 1.45
        m.add('roll', t, faded(I.war_drum(v, pitch=44, decay=0.35)))
        t += gap
        gap = max(0.045, gap * 0.86)
    hit = 1.5
    m.add('roll', hit, faded(I.war_drum(1.0, pitch=36, decay=1.0)))
    m.add('roll', hit, I.cymbal(0.5, decay=2.0))
    for n, k in (('C3', 'cornu'), ('Db3', 'tuba'), ('Gb3', 'buccina'), ('C4', 'tuba')):
        m.line('brass', I.brass([(hit + C.rng.normal(0, 0.005), 2.4, midi(n), 0.85)], k))
    sig = I.organ([midi('C2'), midi('Db2'), midi('G2'), midi('Gb3')], 2.8, vel=0.8, ranks='full')
    m.add('organ', hit - 0.6, swell(sig, 1.0, rise=0.6, floor=0.05))
    return m, dict(space='temple', levels={
        'roll': (-20, 0.0, 0.3), 'brass': (-19, 0.15, 0.45), 'organ': (-24, -0.1, 0.5),
    })
