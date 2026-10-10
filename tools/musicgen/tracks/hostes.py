"""Hostes — a raid is approaching. Seamless loop.

D Aeolian, 4/4, quarter = 96, 32 bars. An insistent low lyre ostinato and
drum; distant cornua call to each other on the harmonic series of D (D, A
only). Builds in layers: ostinato (0-7), + horn calls (8-15), + high lyre
and aulos (16-23), peak with war drums (24-29), then a two-bar hush that
lets the loop start again.
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, phrase
from loopkit import LoopMix, loop_drone, pluck_score, riff_seq

MODE = Mode('D', 'aeolian')
CLK = bpm_clock(96, 2, 4)   # eighths
BARS = 32

RIFFS = {
    'D':  "D3 D3 A3 D3 Bb3 D3 A3 F3",
    'Bb': "Bb2 Bb2 F3 Bb2 G3 Bb2 F3 D3",
    'C':  "C3 C3 G3 C3 A3 C3 G3 E3",
    'A':  "A2 A2 E3 A2 F3 A2 E3 G3",
}
SEQ = "D D Bb C D D Bb A"
HIGH = "A4 r A4 Bb4 A4 r F4 r | A4 r A4 Bb4 A4 r G4 r | Bb4 r Bb4 C5 Bb4 r F4 r | A4 r A4 Bb4 A4 r E4 r"

CALL = "D4:1.5 D4:0.5 A4:4 r:2"           # near horn (harmonics 4, 6)
ANSWER = "A3:2 D4:6"                      # far horn, lower
URGENT = "A4 A4 A4 D5:3 r:2"
AULOS = """
D5:4 C5:2 Bb4:2 | A4:8 | Bb4:3 A4 G4:2 F4:2 | E4:4 A4:4 |
F5:4 E5:2 D5:2 | C5:4 D5:4 | Bb4:2 A4:2 G4:2 E4:2 | A4:8 |
"""


def build():
    L = BARS * CLK.bar_s
    m = LoopMix(L)

    seq = ' '.join([SEQ] * 4)
    pluck_score(m, 'lyre', CLK, 0, riff_seq(RIFFS, seq), vel=0.7, dark=0.6, length=1.2, accents=(0, 3, 6))
    high = ' | '.join([HIGH] * 3 + HIGH.split('|')[:2])   # bars 16-29
    pluck_score(m, 'lyre_hi', CLK, 16, high, vel=0.5, dark=0.45, length=1.0, accents=(0,))

    def horn(score, bar, vel, kind='cornu', stem='horn_near'):
        m.line(stem, I.brass(phrase(CLK, score, bar, vel=vel, sd=0.008), kind))
    for b in (9, 13):
        horn(CALL, b, 0.6)
        horn(ANSWER, b + 1, 0.45, stem='horn_far')
    for b in (17, 19, 21, 23):
        horn(CALL, b, 0.7)
        horn(ANSWER, b + 1, 0.55, stem='horn_far')
    for b in (24, 25, 26, 27, 28):
        horn(URGENT, b, 0.75 + 0.04 * (b - 24))
    horn(ANSWER, 29, 0.6, stem='horn_far')

    m.line('aulos', I.aulos(phrase(CLK, AULOS, 20, vel=0.8)))

    m.add('drone', 0, loop_drone([midi('D2'), midi('A2')], L, vel=0.6, formant=700))

    kit = A.STD_KIT
    s1 = [(0, 'B', .8), (2, 't', .3), (4, 'B', .65), (6, 't', .35)]
    s2 = s1 + [(7, 'b', .45)]
    s3 = s2 + [(1, 't', .2), (3, 't', .25), (5, 't', .25)]
    A.drums(m, 'drums', CLK, 0, 8, s1, kit)
    A.drums(m, 'drums', CLK, 8, 8, s2, kit)
    A.drums(m, 'drums', CLK, 16, 8, s3, kit, fill=s3 + [(6.5, 't', .4)], fill_every=4)
    A.drums(m, 'drums', CLK, 24, 6, [(u, 't', .3 + .05 * (u % 2)) for u in range(8)] +
            [(0, 'B', .9), (4, 'B', .7), (7, 'b', .5)], kit)
    A.drums(m, 'drums', CLK, 30, 2, s1, kit, fade=[.8, .9])
    A.drums(m, 'war', CLK, 24, 6, [(0, 'w', .8), (4, 'w', .6)], kit, fade=[.7, .8, .85, .9, .95, 1])
    A.drums(m, 'war', CLK, 16, 8, [(0, 'w', .55)], kit)

    return m, dict(space='courtyard', levels={
        'lyre': (-21, -0.2, 0.3), 'lyre_hi': (-26, 0.3, 0.4), 'aulos': (-21, 0.1, 0.4),
        'horn_near': (-24, 0.45, 0.6), 'horn_far': (-28, -0.55, 0.8), 'drone': (-30, 0.0, 0.4),
        'drums': (-23, -0.1, 0.25), 'war': (-25, 0.0, 0.3),
    })
