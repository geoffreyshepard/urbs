"""Imperium — Empire city music. Stately and relaxed: the hydraulis leads.

D Mixolydian, 4/4, quarter = 76. Intro (4) · A: organ melody, cornu answers (16) ·
B: tibia with the Urbs head in D Mixolydian (12) · A': cornu leads the first
half, organ the second (16) · outro (4).
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, phrase
from mix import Mix

MODE = Mode('D', 'mixolydian')
CLK = bpm_clock(76, 2, 4)   # eighths, 8 per bar

THEME = """
D4:3 E4 F#4:2 A4:2 | G4:3 F#4 E4:4  | A4:3 B4 C5:2 B4:2 | A4:8 |
D5:3 C5 B4:2 A4:2  | G4:3 A4 B4:2 G4:2 | E4:2 F#4:2 G4:2 E4:2 | D4:8 |
F#4:3 G4 A4:2 D5:2 | C5:3 B4 A4:4   | B4:3 C5 D5:2 E5:2 | D5:8 |
C5:3 B4 A4:2 G4:2  | F#4:3 G4 A4:4  | B4:2 A4:2 G4:2 E4:2 | D4:8 |
"""
ROOTS_A = "D E A D  G G C D  D A G D  C D E D"
CALLS = "A3:2 D4:2", "A3 A3 D4:2", "F#3:2 A3:2", "D4:2 A3:2"   # second half of bars 4, 8, 12, 16

QUOTE = """
A4:3 G4 F#4 G4 A4:2 | D5:2 C5:2 A4:4 | A4:2 G4:2 F#4:2 E4:2 | D4:8 |
B4:3 A4 G4:2 A4:2   | B4:2 C5:2 D5:4 | E5:3 D5 C5:2 B4:2    | A4:8 |
A4:3 G4 F#4 G4 A4:2 | D5:2 C5:2 A4:4 | B4:2 A4:2 G4:2 E4:2  | D4:8 |
"""
ROOTS_B = "D A E D  E G C D  D A E D"


def bars(score, i, j):
    parts = [b for b in score.split('|') if b.strip()]
    return '|'.join(parts[i:j]) + '|'


def organ_line(m, stem, notes, ranks='soft'):
    for ts, td, p, v in notes:
        m.add(stem, ts, I.organ([p], td, v, ranks=ranks, attack=0.04))


def calls(m, bar0, which):
    notes = []
    for k in which:
        notes += phrase(CLK, 'r:4 ' + CALLS[k] + ' |', bar0 + 4 * k + 3, vel=0.6, shape=False)
    m.line('cornu_call', I.brass(notes, 'cornu'))


def build():
    m = Mix(52 * CLK.bar_s, tail=5.0)
    t, bs = CLK.t, CLK.bar_s
    vo = lambda r: [r, MODE.step(r, 4), r + 12, MODE.step(r, 2) + 12]   # noqa: E731
    chords = lambda x, d, v: I.organ(x, d, v, ranks='flute', attack=0.2)  # noqa: E731
    arp = dict(clock=CLK, mode=MODE, low='C3', dark=0.55, accents=(0, 4))
    ost = [0, 1, 2, 1, 3, 1, 2, 1]
    kit = A.STD_KIT
    groove = [(0, 'b', .8), (3, 't', .35), (4, 'b', .55), (6, 't', .4)]
    groove2 = groove + [(7, 'j', .25)]
    fill = [(0, 'b', .8), (4, 'b', .6), (5, 'j', .35), (6, 'j', .45), (7, 'j', .55)]

    # intro
    A.held(m, 'organ', CLK, 0, "D C G D", MODE, lambda x, d, v: I.organ(x, d, v, ranks='flute', attack=1.2),
           low='C3', voicing=vo, vel=0.5)
    A.arpeggio(m, 'lyre_ost', bar0=0, roots="D C G D", pattern=ost, fade=[0.6, 0.7, 0.8, 0.9], **arp)

    # A: the hydraulis sings, the cornu answers at each phrase end
    organ_line(m, 'organ_lead', phrase(CLK, THEME, 4, vel=0.75))
    calls(m, 4, range(4))
    A.held(m, 'organ', CLK, 4, ROOTS_A, MODE, chords, low='C3', voicing=vo, vel=0.45)
    A.arpeggio(m, 'lyre_ost', bar0=4, roots=ROOTS_A, pattern=ost, **arp)
    A.drums(m, 'drums', CLK, 4, 16, groove, kit)

    # B: the tibia quotes the Urbs head; everything else steps back
    m.line('tibia', I.flute(phrase(CLK, QUOTE, 20, vel=0.8), 'tibia'))
    A.held(m, 'organ', CLK, 20, ROOTS_B, MODE, chords, low='C3', voicing=vo, vel=0.35)
    A.arpeggio(m, 'lyre_ost', bar0=20, roots=ROOTS_B, pattern=[0, None, 2, None, 3, None, 2, None], vel=0.65, **arp)
    A.drums(m, 'drums', CLK, 20, 12, [(0, 'b', .55), (4, 't', .3)], kit)

    # A': cornu leads the first half, the organ takes the second with cornu answers
    m.line('cornu', I.brass(phrase(CLK, bars(THEME, 0, 8), 32, vel=0.75), 'cornu'))
    organ_line(m, 'organ_lead', phrase(CLK, bars(THEME, 8, 16), 40, vel=0.78))
    calls(m, 32, [2, 3])
    A.held(m, 'organ', CLK, 32, ROOTS_A, MODE, chords, low='C3', voicing=vo, vel=0.5)
    A.arpeggio(m, 'lyre_ost', bar0=32, roots=ROOTS_A, pattern=ost, vel=0.75, **arp)
    A.drums(m, 'drums', CLK, 32, 16, groove2, kit, fill=fill)

    # outro
    A.held(m, 'organ', CLK, 48, "D C D", MODE, chords, low='C3', voicing=vo, vel=0.4)
    A.arpeggio(m, 'lyre_ost', bar0=48, roots="D C D", pattern=[0, 1, 2, 1, 3, 1, 2, None],
               fade=[0.8, 0.65, 0.5], **arp)
    A.drums(m, 'drums', CLK, 48, 3, [(0, 'b', .6)], kit, fade=[1, .75, .5])
    final = [midi(n) for n in ('D3', 'A3', 'D4', 'F#4')]
    m.add('organ', t(51), I.organ(final, 1.5 * bs, 0.45, ranks='flute', attack=0.3))
    m.add('lyre_lead', t(51), I.strum(final + [midi('A4')], 0.45, spread=0.05, length=5.0, dark=0.5))

    for b in (4, 20, 32, 51):
        m.add('crotala', t(b), I.crotala(0.6 if b != 51 else 0.35))

    return m, dict(space='hall', levels={
        'organ_lead': (-19, 0.05, 0.4), 'organ': (-24, 0.0, 0.45),
        'cornu': (-20, 0.2, 0.45), 'cornu_call': (-23, 0.3, 0.5), 'tibia': (-19, 0.15, 0.4),
        'lyre_ost': (-23, -0.3, 0.35), 'lyre_lead': (-24, -0.1, 0.35),
        'drums': (-26, -0.15, 0.25), 'crotala': (-34, 0.35, 0.55),
    })
