"""Oriens — Antioch and the eastern markets.

D Bayati (E is a quarter-tone flat), 4/4, 80 bpm, baladi rhythm on riq and
frame drum. Oud, qanun (bright plucks with tremolo on long notes), ney.
Oud taqsim intro (8) · A ney theme, qanun answers (16) · B qanun lead, ney
counter (16) · A' ney + qanun heterophony (8) · oud outro (4).
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, humanize, midi, ornament, parse, phrase
from mix import Mix

MODE = Mode('D', 'bayati')
CLK = bpm_clock(80, 2, 4)   # eighths, 8 per bar

TAQSIM = """
D3:3 r E~3 F3 G3 F3   | E~3:4 D3:4          | A3:2 G3 F3 G3:2 A3:2 | Bb3 A3 G3 F3 E~3:2 D3:2 |
D4:3 C4 Bb3 A3 G3:2   | A3:4 r:4            | G3 F3 E~3 F3 G3:2 F3 E~3 | D3:8 |
"""
THEME_A = """
A4:3 G4 F4:2 E~4 F4   | G4:2 F4 E~4 D4:4    | D4 E~4 F4 G4 A4:2 Bb4 A4 | G4:6 r:2 |
A4:2 Bb4 C5 D5:3 C5   | Bb4:2 A4 G4 A4:4    | G4 A4 Bb4 A4 G4:2 F4 E~4 | F4:6 r:2 |
D5:3 C5 Bb4:2 A4:2    | C5:2 Bb4 A4 G4:4    | A4 Bb4 A4 G4 F4:2 G4:2   | A4:6 r:2 |
G4:3 F4 E~4:2 F4:2    | G4:2 A4 G4 F4:2 E~4:2 | F4:2 G4 F4 E~4:4       | D4:8 |
"""
ANSWERS_A = """
r:8 | r:8 | r:8 | r:5 D5:0.5 C5:0.5 Bb4:0.5 A4:0.5 G4 |
r:8 | r:8 | r:8 | r:5 A4:0.5 G4:0.5 F4:0.5 E~4:0.5 F4 |
r:8 | r:8 | r:8 | r:5 C5:0.5 D5:0.5 C5:0.5 Bb4:0.5 A4 |
r:8 | r:8 | r:8 | r:4 A4:0.5 G4:0.5 F4:0.5 E~4:0.5 D4:2 |
"""
THEME_B = """
D5:2 C5 Bb4 A4:2 Bb4 C5 | D5:3 C5 Bb4:2 A4:2 | G4:0.5 A4:0.5 Bb4 C5 D5:2 F5 E~5 D5 | C5:6 r:2 |
Bb4:2 C5 D5 F5:2 E~5 D5 | C5:2 D5 C5 Bb4:2 A4:2 | G4 A4 Bb4 C5 A4:2 G4 F4 | G4:6 r:2 |
G5:3 F5 E~5:2 D5:2      | F5:2 E~5 D5 C5:2 D5:2 | Bb4:0.5 C5:0.5 D5 C5 Bb4 A4:2 Bb4 C5 | D5:6 r:2 |
A4:2 Bb4 C5 Bb4:2 A4 G4 | F4:2 G4 A4 G4:2 F4 E~4 | F4:2 G4 F4 E~4:3 F4 | D4:8 |
"""
COUNTER_B = """
D4:8 | F4:8 | G4:8 | A4:8 | F4:8 | A4:8 | G4:8 | G4:8 |
Bb4:8 | A4:8 | F4:8 | G4:8 | A4:8 | F4:8 | E~4:8 | D4:8 |
"""
OUTRO = "A3:2 G3 F3 E~3:2 F3 G3 | F3:2 E~3 D3 E~3:4 | D3:4 r:4"

BASS = {   # one bar of oud bass per root
    'D': "D3:2 A2 D3 F3 E~3 D3 A2",
    'G': "G2:2 D3 G3 Bb3 A3 G3 D3",
    'C': "C3:2 G2 C3 D3 E~3 D3 C3",
    'F': "F2:2 C3 F3 A3 G3 F3 C3",
    'Bb': "Bb2:2 F3 Bb3 A3 G3 F3 D3",
}
ROOTS_A = "D D D G  G F C F  Bb C F D  G C D D"
ROOTS_B = "Bb Bb G F  Bb F C G  G D Bb G  F D C D"

KIT = {
    'D': lambda v: I.boom(v, pitch=66, decay=0.22),
    'r': lambda v: I.riq(v),
}
BALADI = [(0, 'D', .8), (1, 'D', .6), (4, 'D', .7)]
RIQ = [(2, 'r', .25), (3, 'r', .6), (5, 'r', .25), (6, 'r', .6), (7, 'r', .3)]


def oud_bass(m, stem, bar0, roots, vel=0.6, fade=None):
    for i, r in enumerate(roots.split()):
        f = fade[i] if fade else 1.0
        for st, du, p in parse(BASS[r], CLK.bar):
            acc = 1.0 if st in (0, 4) else 0.75
            m.add(stem, humanize(CLK.t(bar0 + i, st), 0.007),
                  I.pluck(p, vel * f * acc, kind='oud', dark=0.4, length=2.0))


def qanun(m, stem, notes, tremolo=True):
    """Bright plucks; long notes are sustained by tremolo, as a qanun player would."""
    for ts, td, p, v in notes:
        m.add(stem, ts, I.pluck(p, v, kind='qanun', dark=0.25, length=2.2))
        if tremolo and td >= 1.5 * CLK.unit:
            k, tt = 1, 0.085
            while tt < td - 0.05:
                m.add(stem, humanize(ts + tt, 0.004),
                      I.pluck(p, v * (0.42 + 0.08 * (k % 2)), kind='qanun', dark=0.3, length=0.8))
                tt += 0.085
                k += 1


def build():
    m = Mix(52 * CLK.bar_s, tail=5.0)
    t = CLK.t

    # taqsim: free oud solo over the drone
    for ts, td, p, v in phrase(CLK, TAQSIM, 0, sd=0.03, vel=0.8):
        m.add('oud_solo', ts, I.pluck(p, v, kind='oud', dark=0.35, length=3.5))
    m.add('drone', 0, I.drone([midi('D3'), midi('A2')], t(52), 2 * CLK.bar_s, 3 * CLK.bar_s, formant=800))

    # oud bass ostinato
    oud_bass(m, 'oud', 8, ROOTS_A)
    oud_bass(m, 'oud', 24, ROOTS_B)
    oud_bass(m, 'oud', 40, "D D D G G F C F")
    oud_bass(m, 'oud', 48, "D", fade=[0.7])
    for ts, td, p, v in phrase(CLK, OUTRO, 48, sd=0.02, vel=0.8):
        m.add('oud_solo', ts, I.pluck(p, v, kind='oud', dark=0.35, length=3.5))
    m.add('oud_solo', t(51), I.strum([midi(n) for n in ('D3', 'A3', 'D4')], 0.6, spread=0.05,
                                      kind='oud', dark=0.4, length=4.0))

    # ney: theme A, counter in B, A' with ornaments
    m.line('ney', I.flute(ornament(phrase(CLK, THEME_A, 8), MODE, min_dur=1.0, every=3)))
    m.line('ney_counter', I.flute(phrase(CLK, COUNTER_B, 24, vel=0.55, shape=False)))
    first_half = "|".join(THEME_A.split("|")[:8])
    m.line('ney', I.flute(ornament(phrase(CLK, first_half, 40, vel=0.85), MODE, min_dur=0.8)))

    # qanun: answers in A, lead in B, heterophony in A'
    qanun(m, 'qanun', phrase(CLK, ANSWERS_A, 8, vel=0.7), tremolo=False)
    qanun(m, 'qanun', phrase(CLK, THEME_B, 24, vel=0.85, sd=0.006))
    qanun(m, 'qanun', phrase(CLK, first_half, 40, vel=0.55, sd=0.01))

    # baladi: deep dums on frame drum, teks on riq
    A.drums(m, 'drum', CLK, 8, 16, BALADI, KIT)
    A.drums(m, 'drum', CLK, 24, 16, BALADI, KIT, vel=1.05)
    A.drums(m, 'drum', CLK, 40, 8, BALADI, KIT)
    A.drums(m, 'drum', CLK, 48, 1, [(0, 'D', .5)], KIT)
    m.add('riq', t(7, 5), I.shake(0.5, dur=0.7))
    A.drums(m, 'riq', CLK, 8, 16, RIQ, KIT, fill=RIQ + [(4, 'r', .4), (7.5, 'r', .4)], fill_every=4)
    A.drums(m, 'riq', CLK, 24, 16, RIQ + [(1, 'r', .2)], KIT, vel=1.1,
            fill=RIQ + [(4, 'r', .4), (6.5, 'r', .4), (7.5, 'r', .5)], fill_every=4)
    A.drums(m, 'riq', CLK, 40, 8, RIQ, KIT, fade=[1, 1, 1, 1, .9, .8, .7, .6])

    return m, dict(space='courtyard', levels={
        'ney': (-18, 0.1, 0.4), 'ney_counter': (-25, 0.25, 0.45), 'qanun': (-20, 0.3, 0.3),
        'oud_solo': (-19, -0.15, 0.3), 'oud': (-23, -0.25, 0.3), 'drone': (-30, 0.0, 0.4),
        'drum': (-27, -0.05, 0.2), 'riq': (-29, 0.2, 0.25),
    })
