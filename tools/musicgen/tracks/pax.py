"""Pax — prosperity under the Empire.

G Ionian, 3/4, quarter = 80. Warm and content: harp arpeggios over a soft
hydraulis pad, a tibia (soft pipe) melody, the lyre taking the middle tune.

Form: intro (4) · A tibia (16) · B lyre (16) · A' tibia with lyre in thirds
below (16) · coda (8) · final chord.
"""
import numpy as np

import arrange as A
import instruments as I
from core import SR, Mode, butter, bpm_clock, midi, phrase
from mix import Mix

MODE = Mode('G', 'ionian')
CLK = bpm_clock(80, 2, 3)   # eighths, 6 per bar

THEME_A = """
D5:4 B4:2       | C5:2 D5:2 E5:2 | D5:4 G4:2      | A4:6 |
B4:2 C5:2 D5:2  | E5:3 F#5 G5:2  | F#5:2 E5:2 D5:2 | D5:6 |
G5:4 F#5:2      | E5:2 D5:2 C5:2 | B4:2 C5:2 D5:2 | A4:6 |
B4:3 C5 D5:2    | E5:2 C5:2 A4:2 | D5:2 C5:2 A4:2 | G4:6 |
"""
ROOTS_A = "G C G D G C D G E C G D G A D G"

THEME_B = """
E5:3 F#5 G5:2   | B5:4 A5:2      | G5:2 F#5:2 E5:2 | D5:6 |
C5:3 D5 E5:2    | G5:4 E5:2      | D5:2 C5:2 B4:2 | A4:6 |
E5:3 F#5 G5:2   | A5:4 G5:2      | F#5:2 E5:2 D5:2 | B4:6 |
C5:2 E5:2 G5:2  | F#5:3 E5 D5:2  | E5:2 C5:2 A4:2 | D5:6 |
"""
ROOTS_B = "E E C D A C G D E A D E C D A D"

CODA = "D5:4 B4:2 | C5:2 D5:2 E5:2 | D5:6 | r:6 | B4:4 A4:2 | G4:6 | r:6 | r:6"
ROOTS_CODA = "G C G G E D G G"

HARP = [0, 1, 2, 3, 4, 3]


def crotala(v):
    """I.crotala's 2.5 s buffer cuts its 1.6 s ring off at about -14 dB; fade it out instead."""
    x = I.crotala(v)
    f = int(1.0 * SR)
    x[-f:] *= np.linspace(1, 0, f) ** 2
    return x


def tibia(notes):
    t0, sig = I.flute(notes, kind='tibia')
    return t0, butter(sig, 'high', 180)


def organ_pad(midis, dur, vel):
    return I.organ(midis, dur, vel, ranks='soft', attack=0.5)


def lyre_lead(m, notes, gain=1.0):
    for ts, td, p, v in notes:
        m.add('lyre', ts, I.pluck(p, v * gain, dark=0.35, length=3.5))
        if td > 3.5 * CLK.unit:
            m.add('lyre', ts + 3 * CLK.unit, I.pluck(p, 0.5 * v * gain, dark=0.45))


def build():
    m = Mix(61 * CLK.bar_s, tail=6.0)
    t = CLK.t
    harp = dict(clock=CLK, mode=MODE, low='C3', kind='harp', dark=0.45, length=4.0, accents=(0,), sd=0.006)
    pad = dict(clock=CLK, mode=MODE, fn=organ_pad, low='G3', vel=0.5)

    A.arpeggio(m, 'harp', bar0=0, roots="G C G D", pattern=HARP, vel=0.5, fade=[.6, .75, .9, 1], **harp)
    A.arpeggio(m, 'harp', bar0=4, roots=ROOTS_A, pattern=HARP, vel=0.52, **harp)
    A.arpeggio(m, 'harp', bar0=20, roots=ROOTS_B, pattern=[0, 2, 4, 3, 2, 1], vel=0.5, **harp)
    A.arpeggio(m, 'harp', bar0=36, roots=ROOTS_A, pattern=HARP, vel=0.56, **harp)
    A.arpeggio(m, 'harp', bar0=52, roots=ROOTS_CODA, pattern=HARP, vel=0.5,
               fade=[1, 1, .9, .85, .8, .7, .6, .5], **harp)
    m.add('harp', t(60), I.strum([midi(n) for n in ('G2', 'D3', 'G3', 'B3', 'D4', 'G4')], 0.45,
                                 spread=0.06, length=6.0, kind='harp', dark=0.45))

    A.held(m, 'organ', bar0=0, roots="G C G D", **pad)
    A.held(m, 'organ', bar0=4, roots=ROOTS_A, **pad)
    A.held(m, 'organ', bar0=20, roots=ROOTS_B, **pad)
    A.held(m, 'organ', bar0=36, roots=ROOTS_A, **pad)
    A.held(m, 'organ', bar0=52, roots=ROOTS_CODA, **pad)
    m.add('organ', t(60), organ_pad([midi('G3'), midi('B3'), midi('D4')], 2.5, 0.45))

    m.line('tibia', tibia(phrase(CLK, THEME_A, 4, vel=0.8)))
    lyre_lead(m, phrase(CLK, THEME_B, 20, sd=0.007))
    a2 = phrase(CLK, THEME_A, 36, vel=0.85)
    m.line('tibia', tibia(a2))
    lyre_lead(m, [(ts + 0.02, td, MODE.step(p, -2), v) for ts, td, p, v in a2], gain=0.55)
    m.line('tibia', tibia(phrase(CLK, CODA, 52, vel=0.65)), 0.85)

    A.drums(m, 'drums', CLK, 36, 16, [(0, 'b', .5), (4, 't', .18)], A.STD_KIT)
    for b in (20, 36, 52):
        m.add('crotala', t(b), crotala(0.5))

    return m, dict(space='hall', wet_db=-7, levels={
        'tibia': (-18, 0.1, 0.4), 'lyre': (-19, -0.1, 0.35), 'harp': (-23, -0.3, 0.4),
        'organ': (-28, 0.2, 0.5), 'drums': (-29, -0.1, 0.25), 'crotala': (-35, 0.4, 0.55),
    })
