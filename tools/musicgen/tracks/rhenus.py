"""Rhenus — a great battle on the frontier. Seamless loop; the most intense track.

E Phrygian, 4/4, quarter = 120, 48 bars. The Germanic carnyx (left) growls
on the Phrygian F-E semitone; the Roman cornu (right) answers on the pure
harmonics of E (E, B). Big war drums, cymbals, a kithara ostinato, and the
aulos battle tune with tuba doubling. Sections: the enemy calls (0-7) ·
dialogue (8-15) · aulos tune (16-23) · chaos breakdown (24-31) · overlapping
dialogue (32-39) · tune tutti (40-47), whose drum fill throws back to the start.
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, ornament, phrase
from loopkit import LoopMix, loop_drone, pluck_score, riff_seq

MODE = Mode('E', 'phrygian')
CLK = bpm_clock(120, 2, 4)   # eighths
BARS = 48

RIFFS = {
    'E': "E3 E3 B3 E3 F3 E3 B3 E3",
    'F': "F3 F3 C4 F3 G3 F3 C4 F3",
    'C': "C3 C3 G3 C3 D3 C3 G3 C3",
    'D': "D3 D3 A3 D3 E3 D3 A3 D3",
}
SEQ = "E E F E C D F E"

CARNYX = ["E3:3 F3 E3:2 r:2", "B2:2 C3 B2 E3:4", "E3:2 F3:2 G3:2 F3 E3"]
CORNU = ["B3 B3 E4:2 B3:2 E4:2", "E4:1.5 E4:0.5 B4:4 r:2", "B3:2 E4:2 B4:4"]
TUNE = """
E5:2 F5 E5 D5:2 C5 D5 | E5:3 F5 G5:4 | F5:2 E5 D5 C5:2 B4 C5 | D5:2 C5 B4 A4:4 |
B4:2 C5 D5 E5:2 F5 G5 | A5:3 G5 F5:2 E5:2 | F5:2 E5:2 D5 C5 D5 F5 | E5:8 |
"""


def build():
    L = BARS * CLK.bar_s
    m = LoopMix(L)
    t = CLK.t

    pluck_score(m, 'kithara', CLK, 0, riff_seq(RIFFS, ' '.join([SEQ] * 6)), vel=0.75, kind='kithara',
                dark=0.45, length=1.0, accents=(0, 3, 6))

    def carnyx(i, bar, vel=0.85):
        m.line('carnyx', I.brass(phrase(CLK, CARNYX[i], bar, vel=vel, sd=0.012), 'carnyx'))

    def cornu(i, bar, vel=0.8):
        m.line('cornu', I.brass(phrase(CLK, CORNU[i], bar, vel=vel, sd=0.006), 'cornu'))

    for k, b in enumerate((1, 3, 5, 7)):
        carnyx(k % 3, b, 0.75 + 0.04 * k)
    for k, b in enumerate(range(8, 16, 2)):          # dialogue: enemy, then Rome
        carnyx(k % 3, b)
        cornu(k % 3, b + 1)
    for b in (25, 27, 29, 31):                         # breakdown: carnyx swells
        m.line('carnyx', I.brass([(t(b) - 0.1, CLK.bar_s * 0.9, midi('E3'), 0.9),
                                  (t(b, 7.2), 0.2, midi('F3'), 0.7)], 'carnyx'))
    for k, b in enumerate(range(32, 40)):              # overlapping, a bar each
        if k % 2 == 0:
            carnyx(k % 3, b, 0.9)
        else:
            cornu(k % 3, b, 0.9)
        if k >= 4:
            cornu((k + 1) % 3, b, 0.7)

    m.line('aulos', I.aulos(phrase(CLK, TUNE, 16, vel=0.85), vib=0.006, bright=0.1))
    m.line('aulos', I.aulos(phrase(CLK, TUNE, 40, vel=0.95), vib=0.006, bright=0.1))
    m.line('aulos_low', I.aulos(ornament(phrase(CLK, TUNE, 40, vel=0.7, octave=-1), MODE, min_dur=0.4)))
    m.line('tuba', I.brass(phrase(CLK, TUNE, 40, vel=0.8, octave=-1, sd=0.006), 'tuba'))
    # Roman horn chords under the tune: E-B fifths
    for b in range(16, 24, 2):
        for n in ('E3', 'B3'):
            m.line('cornu', I.brass([(t(b), CLK.bar_s * 1.9, midi(n), 0.6)], 'cornu'), 0.7)

    m.add('drone', 0, loop_drone([midi('E2'), midi('B2')], L, vel=0.5, formant=550))

    kit = A.STD_KIT
    big = [(0, 'W', 1.0), (3, 'w', .7), (4, 'W', .9), (6, 'w', .7), (7, 'w', .6)]
    fill = [(0, 'W', 1.0), (2, 'w', .6), (3, 'w', .7), (4, 'w', .75), (5, 'w', .8), (6, 'w', .9), (7, 'W', 1.0)]
    A.drums(m, 'war', CLK, 0, 24, big, kit, fill=fill, fill_every=8)
    A.drums(m, 'war', CLK, 24, 8, [(0, 'W', 1.0), (1.5, 'w', .6), (4, 'W', 1.0), (5.5, 'w', .6)], kit,
            fill=fill, fill_every=8)
    A.drums(m, 'war', CLK, 32, 16, big, kit, fill=fill, fill_every=8)
    A.drums(m, 'frame', CLK, 8, 40, [(u, 't', .3 + .2 * (u % 2 == 0)) for u in range(8)], kit)
    for b in (0, 8, 16, 24, 28, 32, 40, 44):
        m.add('cymbal', t(b), I.cymbal(0.8 if b % 16 == 0 else 0.6))

    return m, dict(space='field', levels={
        'kithara': (-22, -0.1, 0.25), 'carnyx': (-19, -0.45, 0.35), 'cornu': (-20, 0.45, 0.4),
        'aulos': (-18, 0.05, 0.3), 'aulos_low': (-24, -0.15, 0.3), 'tuba': (-22, 0.2, 0.35),
        'drone': (-30, 0.0, 0.3), 'war': (-20, 0.0, 0.25), 'frame': (-26, 0.15, 0.2),
        'cymbal': (-28, 0.25, 0.4),
    })
