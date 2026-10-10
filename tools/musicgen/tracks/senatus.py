"""Senatus — mission briefing and epilogue. A seamless loop.

A Aeolian, 4/4, quarter = 56. Kithara chords over a quiet A–E organ drone.
Intro (4) · A: low aulos melody (8) · B: kithara melody, aulos answering (8).
The last bar (E minor) leads back to the first (A minor).
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, phrase
from mix import Mix

MODE = Mode('A', 'aeolian')
CLK = bpm_clock(56, 2, 4)   # eighths, 8 per bar
BARS = 20

ROOTS = "A A F G  A F C G  A D E A  F C D A  F G E E"

AULOS_A = """
A3:3 B3 C4:2 B3:2 | A3:2 C4:2 F4:3 E4 | E4:3 D4 C4:2 E4:2 | D4:6 B3:2 |
C4:3 D4 E4:2 A4:2 | G4:2 F4:2 D4:3 E4 | E4:3 D4 B3:2 G3:2 | A3:8 |
"""
KITH_B = """
C5:3 A4 F4:2 A4:2 | G4:3 E4 C5:4 | D5:3 C5 A4:2 F4:2 | E4:4 A4:4 |
C5:3 D5 E5:2 C5:2 | D5:4 B4:4    | C5:2 B4:2 G4:2 E4:2 | B4:4 G4:2 E4:2 |
"""
AULOS_B = "r:8 | r:4 E4:4 | r:8 | r:4 C4:4 | r:8 | r:4 D4:4 | r:8 | r:4 B3:4 |"


def build():
    m = Mix(BARS * CLK.bar_s, tail=5.0)
    t, bs = CLK.t, CLK.bar_s
    rs = ROOTS.split()
    acc = dict(clock=CLK, mode=MODE, low='E2', kind='kithara', dark=0.55, length=3.5, accents=(0, 4))

    # kithara accompaniment: flowing in the intro and A, sparse under its own melody in B
    A.arpeggio(m, 'kith_acc', bar0=0, roots=' '.join(rs[:4]), pattern=[0, None, 1, 2, 3, None, 2, None],
               vel=0.65, **acc)
    A.arpeggio(m, 'kith_acc', bar0=4, roots=' '.join(rs[4:12]), pattern=[0, None, 1, None, 2, None, 3, None],
               vel=0.6, **acc)
    A.arpeggio(m, 'kith_acc', bar0=12, roots=' '.join(rs[12:]), pattern=[0, None, None, None, 1, None, 2, None],
               vel=0.55, **acc)

    # A: low aulos
    m.line('aulos', I.aulos(phrase(CLK, AULOS_A, 4, vel=0.75), vib=0.009))
    # B: kithara melody, the aulos answers softly
    for ts, td, p, v in phrase(CLK, KITH_B, 12, vel=0.8, sd=0.006):
        m.add('kith_lead', ts, I.pluck(p, v, kind='kithara', dark=0.35, length=3.0))
    m.line('aulos_ans', I.aulos(phrase(CLK, AULOS_B, 12, vel=0.6), vib=0.009))

    # organ drone on A2 + E3, re-voiced every two bars; the last one wraps onto bar 0
    for b in range(0, BARS, 2):
        m.add('organ', t(b), I.organ([midi('A2'), midi('E3')], 2 * bs, 0.5, ranks='flute', attack=0.8))

    m.add('crotala', t(4), I.crotala(0.4))
    m.add('crotala', t(12), I.crotala(0.3))

    return m, dict(space='hall', levels={
        'kith_acc': (-23, -0.25, 0.4), 'kith_lead': (-19, -0.05, 0.4),
        'aulos': (-19, 0.15, 0.35), 'aulos_ans': (-23, 0.2, 0.4),
        'organ': (-28, 0.0, 0.5), 'crotala': (-35, 0.35, 0.55),
    })
