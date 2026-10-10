"""Corinthus — Greek Corinth, refounded (mission 10).

E Phrygian, 7/8 grouped 2+2+3, eighth = 0.21 s. Kithara ostinato in open
fifths, aulos melody, frame drum and riq. The B tune borrows the Greek
chromatic genus (E F G# A), the sound later heard as Hijaz.

Form: intro (4) · A aulos (16) · A' kithara lead, aulos heterophony below (16)
· B aulos (16) · interlude kithara (8) · B' aulos + kithara unison (16) ·
A'' tutti (16) · outro (6).
"""
import numpy as np

import arrange as A
import instruments as I
from core import SR, Clock, Mode, midi, ornament, phrase
from mix import Mix

MODE = Mode('E', 'phrygian')
CLK = Clock(0.21, 7)   # eighths, 7 per bar (2+2+3)

THEME_A = """
E5:2 F5:2 G5:3   | A5:2 G5:2 F5:3   | E5:2 F5 E5 D5:3  | E5:7 |
G5:2 F5:2 E5:3   | D5:2 E5 F5 G5:3  | A5:2 G5:2 F5:3   | D5:7 |
E5:2 F5:2 G5:3   | B5:2 A5:2 G5:3   | A5:2 G5 F5 E5:3  | F5:7 |
E5:2 D5:2 C5:3   | D5:2 C5 B4 A4:3  | D5:2 C5:2 F5:3   | E5:7 |
"""
ROOTS_A = "E F D E C G F G E E F D A D F E"

THEME_B = """
A4:2 B4:2 C5:3   | B4:2 A4:2 G#4:3  | A4:2 F4:2 E4:3   | E4:7 |
C5:2 D5:2 E5:3   | F5:2 E5:2 D5:3   | C5:2 B4 A4 G#4:3 | A4:7 |
E5:2 F5:2 G#5:3  | A5:2 G#5:2 F5:3  | E5:2 D5:2 C5:3   | B4:7 |
C5:2 D5 C5 B4:3  | A4:2 G#4:2 A4:3  | C5:2 A4:2 F4:3   | E4:7 |
"""
ROOTS_B = "A E F E C D E A E F A E A A F E"

OPEN5 = lambda r: [r, r + 7, r + 12, r + 19, r + 24]   # noqa: E731  open fifths, no third
OST = [0, None, 1, None, 2, 1, None]
OST2 = [0, None, 1, 2, 0, 1, 2]


def crotala(v):
    """I.crotala's 2.5 s buffer cuts its 1.6 s ring off at about -14 dB; fade it out instead."""
    x = I.crotala(v)
    f = int(1.0 * SR)
    x[-f:] *= np.linspace(1, 0, f) ** 2
    return x


def kithara_lead(m, notes, gain=1.0):
    for ts, td, p, v in notes:
        m.add('kithara', ts, I.pluck(p, v * gain, kind='kithara', dark=0.3, length=3.0))
        if td > 4 * CLK.unit:
            m.add('kithara', ts + 4 * CLK.unit, I.pluck(p, 0.5 * v * gain, kind='kithara', dark=0.4))


def build():
    m = Mix(98 * CLK.bar_s)
    t = CLK.t
    arp = dict(clock=CLK, mode=MODE, low='C3', kind='kithara', dark=0.55, length=2.6,
               voicing=OPEN5, accents=(0, 2, 4), sd=0.004)

    A.arpeggio(m, 'kith_ost', bar0=0, roots="E E F E", pattern=OST, vel=0.6, fade=[.7, .8, .9, 1], **arp)
    A.arpeggio(m, 'kith_ost', bar0=4, roots=ROOTS_A, pattern=OST, vel=0.6, **arp)
    A.arpeggio(m, 'kith_ost', bar0=20, roots=ROOTS_A, pattern=OST, vel=0.6, **arp)
    A.arpeggio(m, 'kith_ost', bar0=36, roots=ROOTS_B, pattern=OST2, vel=0.6, **arp)
    A.arpeggio(m, 'kith_ost', bar0=52, roots="E F E F D E F E", pattern=OST2, vel=0.65, **arp)
    A.arpeggio(m, 'kith_ost', bar0=60, roots=ROOTS_B, pattern=OST2, vel=0.62, **arp)
    A.arpeggio(m, 'kith_ost', bar0=76, roots=ROOTS_A, pattern=OST2, vel=0.65, **arp)
    A.arpeggio(m, 'kith_ost', bar0=92, roots="E F E F E", pattern=OST, vel=0.55,
               fade=[1, .85, .7, .6, .5], **arp)
    m.add('kithara', t(97), I.strum([midi(n) for n in ('E3', 'B3', 'E4', 'B4', 'E5')], 0.45,
                                    spread=0.04, length=5.0, kind='kithara', dark=0.45))

    # A: aulos
    m.line('aulos', I.aulos(phrase(CLK, THEME_A, 4)))
    # A': kithara lead, aulos heterophony an octave below
    kithara_lead(m, phrase(CLK, THEME_A, 20, sd=0.006))
    het = ornament(phrase(CLK, THEME_A, 20, octave=-1, vel=0.7), MODE, min_dur=3 * CLK.unit * 0.95)
    m.line('aulos_het', I.aulos(het, vib=0.008))
    # B: aulos (Greek chromatic)
    m.line('aulos', I.aulos(phrase(CLK, THEME_B, 36, vel=0.82)))
    # interlude: kithara improvises over the ostinato
    inter = """
    E4:2 F4:2 G4:3 | F4:2 E4:2 r:3 | B4:2 A4:2 G4:3 | A4:7 |
    G4:2 A4:2 B4:3 | C5:2 B4:2 A4:3 | G4:2 F4:2 G4:3 | E4:7 |
    """
    kithara_lead(m, phrase(CLK, inter, 52, vel=0.7), gain=0.9)
    # B': aulos + kithara unison
    b2 = phrase(CLK, THEME_B, 60, vel=0.85)
    m.line('aulos', I.aulos(b2))
    kithara_lead(m, b2, gain=0.8)
    # A'': tutti
    a3 = phrase(CLK, THEME_A, 76, vel=0.9)
    m.line('aulos', I.aulos(a3))
    kithara_lead(m, a3, gain=0.85)
    m.line('aulos_het', I.aulos(ornament(phrase(CLK, THEME_A, 76, octave=-1, vel=0.7), MODE,
                                         min_dur=3 * CLK.unit * 0.95), vib=0.008))

    # percussion: dum on 1 and 3 of 2+2+3, tek and riq between
    kit = A.STD_KIT
    light = [(0, 'b', .55), (4, 'r', .3)]
    dance = [(0, 'b', .75), (2, 'r', .4), (4, 'b', .5), (5, 't', .3), (6, 'r', .45)]
    fill = [(0, 'b', .8), (2, 'r', .5), (3, 'r', .3), (4, 'b', .6), (5, 'r', .5), (6, 'r', .6)]
    A.drums(m, 'perc', CLK, 2, 2, light, kit, fade=[.6, .8])
    A.drums(m, 'perc', CLK, 4, 16, light + [(2, 't', .3)], kit)
    A.drums(m, 'perc', CLK, 20, 32, dance, kit, fill=fill)
    A.drums(m, 'perc', CLK, 52, 8, light, kit)
    A.drums(m, 'perc', CLK, 60, 32, dance, kit, fill=fill)
    A.drums(m, 'perc', CLK, 92, 4, light, kit, fade=[1, .8, .6, .4])

    for b in (20, 36, 60, 76, 84, 97):
        m.add('crotala', t(b), crotala(0.6 if b != 97 else 0.35))

    return m, dict(space='courtyard', levels={
        'aulos': (-18, 0.15, 0.3), 'aulos_het': (-23, 0.25, 0.35), 'kithara': (-19, -0.1, 0.35),
        'kith_ost': (-23, -0.3, 0.35), 'perc': (-26, -0.05, 0.2), 'crotala': (-34, 0.4, 0.55),
    })
