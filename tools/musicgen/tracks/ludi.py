"""Ludi — the games, or a triumph.

C Ionian, 4/4 march, 112 bpm. Tuba and cornu fanfares stay on the natural
harmonic series of C (C3 G3 C4 E4 G4 Bb4 C5 D5 E5); the hydraulis carries the
tune, as organs did in the arena. War drums, frame drums, cymbals.
Fanfare (4) · march A, organ (16) · lighter middle, pan pipes + aulos (16) ·
march A', full (16) · fanfare and final chord (4).
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, ornament, parse, phrase
from mix import Mix

MODE = Mode('C', 'ionian')
CLK = bpm_clock(112, 4, 4)   # sixteenths, 16 per bar

FANFARE = """
G4:2 G4 G4 C5:6 r:2 G4:2 E4:2   | C5:2 C5 C5 E5:6 r:2 D5:2 C5:2 |
E4:2 G4:2 C5:2 E5:4 D5:2 C5:2 G4:2 | C5:14 r:2 |
"""
FANFARE_LOW = "C3:8 G3:8 | C4:8 G3:8 | C3:4 G3:4 C4:8 | C3:14 r:2"
MARCH = """
C5:4 C5:2 D5:2 E5:4 C5:4 | G5:6 F5:2 E5:4 D5:4 | E5:4 F5:2 G5:2 A5:4 G5:4 | F5:4 E5:4 D5:8 |
C5:4 C5:2 D5:2 E5:4 C5:4 | G5:6 F5:2 E5:4 G5:4 | C6:4 B5:4 A5:4 G5:2 F5:2 | E5:4 D5:4 C5:8 |
E5:4 E5:2 F5:2 G5:4 E5:4 | A5:6 G5:2 F5:4 E5:4 | D5:4 E5:2 F5:2 G5:4 F5:4 | E5:8 D5:8 |
C5:4 C5:2 D5:2 E5:4 C5:4 | G5:6 F5:2 E5:4 G5:4 | C6:4 B5:2 A5:2 G5:4 D5:4 | C5:12 r:4 |
"""
ROOTS_MARCH = "C G F G  C C F C  C F G C  C C G C"
# tuba answers in the bars where the tune holds still (bars 4, 8, 12, 16 of each march)
ANSWER = "r:8 G4:2 G4:2 C5:4"
ANSWER_HI = "r:8 C5:2 E5:2 G4:4"
CORNU_ROOT = {'C': 'C3', 'G': 'G3', 'F': 'C3'}   # F has no horn note: hold the C pedal

MIDDLE = """
A4:6 B4:2 C5:4 A4:4 | G4:4 A4:2 G4:2 F4:8 | C5:6 D5:2 E5:4 C5:4 | D5:8 C5:8 |
A4:6 B4:2 C5:4 E5:4 | D5:4 C5:2 B4:2 A4:8 | G4:4 A4:4 B4:4 G4:4 | A4:16 |
F5:6 E5:2 D5:4 C5:4 | D5:4 E5:4 C5:8     | B4:4 C5:2 D5:2 E5:4 D5:4 | C5:16 |
A4:6 B4:2 C5:4 A4:4 | G4:4 A4:2 G4:2 F4:4 E4:4 | F4:4 G4:4 A4:4 G4:4 | F4:8 G4:8 |
"""
ROOTS_MIDDLE = "F F C G  A D G F  D C G C  F C F G"

KIT = {
    'w': lambda v: I.war_drum(v, pitch=50, decay=0.45),
    't': lambda v: I.tek(v, pitch=420),
    'b': lambda v: I.boom(v),
    'y': lambda v: I.cymbal(v, decay=1.8),
    'r': lambda v: I.riq(v),
    'c': lambda v: I.clap(v),
}
MARCH_DRUMS = [(0, 'w', .9), (8, 'w', .7)]
SNARE = [(4, 't', .6), (10, 't', .3), (12, 't', .65), (14, 't', .3), (15, 't', .35)]
SNARE_FILL = [(4, 't', .6), (8, 't', .4), (9, 't', .45), (10, 't', .5), (11, 't', .55), (12, 't', .6),
              (13, 't', .65), (14, 't', .7), (15, 't', .8)]


def organ_line(m, stem, notes, ranks='soft'):
    for ts, td, p, v in notes:
        m.add(stem, ts, I.organ([p], td, v, ranks=ranks))


def march(m, bar0, full):
    """One 16-bar march statement."""
    vel = 0.85 if full else 0.75
    organ_line(m, 'organ', phrase(CLK, MARCH, bar0, octave=-1, vel=vel, legato=0.9))
    A.held(m, 'organ_ch', CLK, bar0, ROOTS_MARCH, MODE,
           lambda ms, d, v: I.organ(ms, d * 0.97, v, ranks='soft'), low='C3', vel=0.5)
    # cornu: oom on beats 1 and 3
    for i, r in enumerate(ROOTS_MARCH.split()):
        p = midi(CORNU_ROOT[r])
        m.line('cornu', I.brass([(CLK.t(bar0 + i), 0.45, p, 0.75), (CLK.t(bar0 + i, 8), 0.45, p, 0.6)], 'cornu'))
    for k, b in enumerate((3, 7, 11, 15)):
        sc = ANSWER_HI if (full and k % 2) else ANSWER
        m.line('tuba', I.brass(phrase(CLK, sc, bar0 + b, vel=0.8, legato=0.85), 'tuba'))
    if full:   # in the last statement the tubas also double the head of each phrase
        for b in (0, 4, 12):
            head = "C5:4 C5:2 D5:2 E5:4 C5:4"
            m.line('tuba', I.brass(phrase(CLK, head, bar0 + b, vel=0.7, legato=0.85), 'tuba'), 0.8)
    A.drums(m, 'war', CLK, bar0, 16, MARCH_DRUMS + ([(14, 'w', .5)] if full else []), KIT)
    A.drums(m, 'drums', CLK, bar0, 16, SNARE, KIT, fill=SNARE_FILL, fill_every=8, vel=1.1 if full else 1.0)
    for b in range(0, 16, 2 if full else 4):
        m.add('cymbal', CLK.t(bar0 + b), I.cymbal(0.8 if b % 4 == 0 else 0.5, decay=1.8))


def build():
    m = Mix(56 * CLK.bar_s, tail=5.0)
    t = CLK.t

    # opening fanfare
    m.line('tuba', I.brass(phrase(CLK, FANFARE, 0, vel=0.85, legato=0.85), 'tuba'))
    m.line('cornu', I.brass(phrase(CLK, FANFARE_LOW, 0, vel=0.75, legato=0.9), 'cornu'))
    m.add('cymbal', t(0), I.cymbal(0.9))
    A.drums(m, 'war', CLK, 0, 3, [(0, 'w', .9), (12, 'w', .5), (14, 'w', .6)], KIT)
    A.drums(m, 'drums', CLK, 3, 1, [(u, 't', .3 + .04 * u) for u in range(16)], KIT)

    march(m, 4, full=False)

    # lighter middle: pan pipes, lyre, frame drum and riq; aulos joins, the crowd claps
    for ts, td, p, v in phrase(CLK, MIDDLE, 20, vel=0.85):
        m.add('pipes', ts, I.pipes(p, td, v))
    second = "|".join(MIDDLE.split("|")[8:16])
    m.line('aulos', I.aulos(ornament(phrase(CLK, second, 28, octave=-1, vel=0.75), MODE, min_dur=0.5)))
    A.arpeggio(m, 'lyre', CLK, 20, ROOTS_MIDDLE, MODE, [0, 2, 1, 3, 0, 2, 1, 4], step=2, low='C3',
               accents=(0, 8), dark=0.5)
    A.drums(m, 'drums', CLK, 20, 16, [(0, 'b', .7), (4, 't', .4), (8, 'b', .5), (12, 't', .45)], KIT)
    A.drums(m, 'riq', CLK, 20, 16, [(2, 'r', .3), (6, 'r', .4), (10, 'r', .3), (14, 'r', .45)], KIT)
    A.drums(m, 'clap', CLK, 28, 8, [(4, 'c', .7), (12, 'c', .8)], KIT)
    m.add('organ_ch', t(35), I.organ([midi('G2'), midi('D3'), midi('G3'), midi('B3')], CLK.bar_s, 0.5))
    A.drums(m, 'drums', CLK, 35, 1, [(u, 't', .3 + .04 * u) for u in range(8, 16)], KIT)

    march(m, 36, full=True)

    # closing fanfare and final chord
    m.line('tuba', I.brass(phrase(CLK, "|".join(FANFARE.split("|")[:2]), 52, vel=0.9, legato=0.85), 'tuba'))
    m.line('cornu', I.brass(phrase(CLK, "C3:8 G3:8 | C4:8 G3:8", 52, vel=0.8, legato=0.9), 'cornu'))
    A.drums(m, 'war', CLK, 52, 2, [(0, 'w', .9), (8, 'w', .7)], KIT)
    A.drums(m, 'drums', CLK, 53, 1, [(u, 't', .35 + .04 * u) for u in range(16)], KIT)
    end = t(54)
    m.add('organ', end, I.organ([midi(n) for n in ('C3', 'G3', 'C4', 'E4', 'G4', 'C5')], 2 * CLK.bar_s, 0.6,
                                 ranks='full'))
    for n, kind, v in (('C5', 'tuba', .85), ('G4', 'tuba', .75), ('E4', 'tuba', .7), ('C3', 'cornu', .8),
                       ('G3', 'cornu', .7)):
        m.line(kind, I.brass([(end, 2 * CLK.bar_s - 0.1, midi(n), v)], kind), 0.5)
    m.add('war', end, I.war_drum(0.7, pitch=44, decay=0.8))
    m.add('cymbal', end, I.cymbal(0.7, decay=2.6))

    return m, dict(space='hall', levels={
        'organ': (-19, 0.0, 0.35), 'organ_ch': (-25, -0.1, 0.4), 'tuba': (-19, 0.25, 0.35),
        'cornu': (-23, -0.3, 0.35), 'pipes': (-19, 0.15, 0.4), 'aulos': (-23, 0.25, 0.35),
        'lyre': (-23, -0.3, 0.35), 'war': (-24, -0.05, 0.3), 'drums': (-26, 0.1, 0.25),
        'cymbal': (-29, 0.3, 0.4), 'riq': (-30, 0.35, 0.3), 'clap': (-30, -0.35, 0.35),
    })
