"""Saturnalia — a rustic festival.

G Mixolydian, 6/8, dotted quarter = 104, tarantella-like. Aulos and pan pipes
over a galloping lyre, frame drum, riq and hand claps. It builds to a full
last chorus and stops on a hit.

Form: intro drums (4) · A aulos (16) · A aulos + pipes (16) · B pipes (16) ·
A tutti (16) · break: drums and claps (8) · A tutti (16) · ending (4).
"""
import numpy as np

import arrange as A
import instruments as I
from core import SR, Mode, bpm_clock, midi, phrase
from mix import Mix

MODE = Mode('G', 'mixolydian')
CLK = bpm_clock(104, 3, 2)   # eighths, 6 per bar

THEME_A = """
D5 G5 D5 B4 D5 G5  | A5:2 G5 F5:2 E5   | D5 C5 B4 C5:2 A4  | G4:3 D5:3 |
D5 G5 D5 B4 D5 G5  | A5:2 B5 C6:2 A5   | G5 F5 E5 F5:2 D5  | G5:3 G4:3 |
B4 C5 D5 B4 C5 D5  | E5:2 F5 E5:2 D5   | C5 B4 A4 B4:2 G4  | A4:3 D5:3 |
D5 G5 D5 B4 D5 G5  | A5:2 G5 F5:2 E5   | D5 C5 A4 B4:2 A4  | G4:6 |
"""
ROOTS_A = "G F C G G F D G G C F D G F D G"

THEME_B = """
B5:3 A5:3          | G5:2 F5 E5:3      | F5:3 E5:2 D5      | C5:6 |
B4 C5 D5 E5:2 F5   | G5:3 E5:3         | F5:2 E5 D5:2 C5   | D5:6 |
B5:3 A5:3          | G5:2 F5 E5:3      | F5:3 A5:3         | G5:6 |
E5 F5 G5 F5 E5 D5  | C5:3 D5:3         | B4 C5 D5 C5:2 A4  | G4:6 |
"""
ROOTS_B = "E C D F G C F G E C F C C F D G"

ENDING = "D5 G5 D5 B4 D5 G5 | F5 E5 D5 C5 B4 A4 | G4:2 r:4 | r:6"

GALLOP = [0, 2, 1, 0, 2, 1]
DANCE = [(0, 'b', .85), (1, 'r', .25), (2, 'j', .45), (3, 'b', .6), (4, 'r', .25), (5, 'j', .5)]
LIGHT = [(0, 'b', .7), (2, 'j', .3), (3, 'b', .45), (5, 'j', .35)]
FILL = [(0, 'b', .9), (1, 'r', .4), (2, 'r', .5), (3, 'b', .7), (4, 'j', .6), (5, 'j', .7)]
CLAPS = [(0, 'c', .55), (3, 'c', .7)]


def crotala(v):
    """I.crotala's 2.5 s buffer cuts its 1.6 s ring off at about -14 dB; fade it out instead."""
    x = I.crotala(v)
    f = int(1.0 * SR)
    x[-f:] *= np.linspace(1, 0, f) ** 2
    return x


def pipes_line(m, stem, notes, gain=1.0, octave=0):
    for ts, td, p, v in notes:
        m.add(stem, ts + 0.01, I.pipes(p + 12 * octave, max(td, 0.12), v * gain))


def build():
    m = Mix(96 * CLK.bar_s, tail=4.0)
    t = CLK.t
    kit = A.STD_KIT
    arp = dict(clock=CLK, mode=MODE, low='C3', dark=0.5, length=1.6, accents=(0, 3), sd=0.004)

    # intro: drums and claps alone
    A.drums(m, 'drums', CLK, 0, 4, LIGHT, kit, fade=[.6, .75, .9, 1])
    A.drums(m, 'claps', CLK, 2, 2, CLAPS, kit, fade=[.7, 1])

    # A: aulos, lyre gallop
    m.line('aulos', I.aulos(phrase(CLK, THEME_A, 4, sd=0.003)))
    A.arpeggio(m, 'lyre', bar0=4, roots=ROOTS_A, pattern=GALLOP, vel=0.55, **arp)
    A.drums(m, 'drums', CLK, 4, 16, LIGHT, kit)

    # A: aulos doubled by pan pipes
    a2 = phrase(CLK, THEME_A, 20, vel=0.85, sd=0.003)
    m.line('aulos', I.aulos(a2))
    pipes_line(m, 'pipes_dbl', a2, gain=0.6)
    A.arpeggio(m, 'lyre', bar0=20, roots=ROOTS_A, pattern=GALLOP, vel=0.6, **arp)
    A.drums(m, 'drums', CLK, 20, 16, DANCE, kit, fill=FILL)

    # B: pipes lead, claps join
    pipes_line(m, 'pipes', phrase(CLK, THEME_B, 36, vel=0.85, sd=0.003))
    A.arpeggio(m, 'lyre', bar0=36, roots=ROOTS_B, pattern=[0, 1, 2, 3, 2, 1], vel=0.6, **arp)
    A.drums(m, 'drums', CLK, 36, 16, DANCE, kit, fill=FILL)
    A.drums(m, 'claps', CLK, 36, 16, CLAPS, kit)

    # A: tutti
    a3 = phrase(CLK, THEME_A, 52, vel=0.9, sd=0.003)
    m.line('aulos', I.aulos(a3))
    pipes_line(m, 'pipes_dbl', a3, gain=0.75)
    A.arpeggio(m, 'lyre', bar0=52, roots=ROOTS_A, pattern=GALLOP, vel=0.65, **arp)
    A.drums(m, 'drums', CLK, 52, 16, DANCE, kit, fill=FILL)
    A.drums(m, 'claps', CLK, 52, 16, CLAPS, kit)

    # break: drums, claps and shakers build
    A.drums(m, 'drums', CLK, 68, 8, FILL, kit, fade=[.7, .75, .8, .85, .9, .95, 1, 1.05])
    A.drums(m, 'claps', CLK, 68, 8, [(0, 'c', .6), (2, 'c', .4), (3, 'c', .7), (5, 'c', .4)], kit)
    A.drums(m, 'shake', CLK, 72, 4, [(0, 's', .5), (3, 's', .6)], kit, fade=[.6, .8, 1, 1.2])
    A.arpeggio(m, 'lyre', bar0=68, roots="G F G F G F D D", pattern=[0, None, None, 0, None, 1],
               vel=0.6, **arp)

    # A: tutti, last time
    a4 = phrase(CLK, THEME_A, 76, vel=0.95, sd=0.003)
    m.line('aulos', I.aulos(a4))
    pipes_line(m, 'pipes_dbl', a4, gain=0.8)
    A.arpeggio(m, 'lyre', bar0=76, roots=ROOTS_A, pattern=GALLOP, vel=0.7, **arp)
    A.drums(m, 'drums', CLK, 76, 16, DANCE, kit, fill=FILL, vel=1.05)
    A.drums(m, 'claps', CLK, 76, 16, CLAPS, kit)

    # ending: run down, one hit
    end = phrase(CLK, ENDING, 92, vel=0.95, sd=0.002)
    m.line('aulos', I.aulos(end))
    pipes_line(m, 'pipes_dbl', end, gain=0.8)
    A.drums(m, 'drums', CLK, 92, 2, DANCE, kit)
    A.drums(m, 'claps', CLK, 92, 2, CLAPS, kit)
    m.add('drums', t(94), I.boom(1.0, pitch=56, decay=0.4))
    m.add('drums', t(94), I.riq(0.8))
    m.add('lyre', t(94), I.strum([midi(n) for n in ('G3', 'D4', 'G4', 'B4')], 0.7, spread=0.015,
                                 length=3.0, dark=0.45))

    for b in (20, 36, 52, 60, 76, 84, 94):
        m.add('crotala', t(b), crotala(0.7))

    return m, dict(space='courtyard', wet_db=-9, levels={
        'aulos': (-18, 0.12, 0.25), 'pipes': (-19, 0.2, 0.35), 'pipes_dbl': (-23, 0.25, 0.35),
        'lyre': (-23, -0.3, 0.3), 'drums': (-24, -0.1, 0.15), 'claps': (-28, 0.3, 0.3),
        'shake': (-30, 0.35, 0.2), 'crotala': (-33, 0.4, 0.5),
    })
