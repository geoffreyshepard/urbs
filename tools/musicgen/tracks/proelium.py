"""Proelium — a skirmish. Seamless loop.

D Phrygian, 4/4, quarter = 132, 48 bars. War drums and frame drums march;
a fast lyre ostinato; the military aulos (Spartans marched to it) carries the
tune; a straight trumpet (tuba) calls on the harmonics of D and cornua hold
the bass. Six 8-bar sections: drums and calls · aulos A · aulos B with
trumpet answers · A doubled an octave down · breakdown · A again, whose
Eb-D Phrygian cadence hands back to the start.
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, ornament, phrase
from loopkit import LoopMix, loop_drone, pluck_score, riff_seq

MODE = Mode('D', 'phrygian')
CLK = bpm_clock(132, 2, 4)   # eighths
BARS = 48

RIFFS = {
    'D':  "D3 A3 D4 A3 Eb4 A3 D4 A3",
    'Eb': "Eb3 Bb3 Eb4 Bb3 F4 Bb3 Eb4 Bb3",
    'C':  "C3 G3 C4 G3 D4 G3 C4 G3",
    'Bb': "Bb2 F3 Bb3 F3 C4 F3 Bb3 F3",
}
SEQ = "D D Eb D C Bb C Eb"

TUNE_A = """
D5:2 D5 Eb5 F5:2 Eb5 D5 | C5:2 D5:2 A4:4 | Bb4:2 C5 D5 Eb5:2 D5 C5 | D5:6 r:2 |
F5:2 F5 G5 A5:2 G5 F5 | Eb5:2 F5:2 D5:4 | C5 Bb4 C5 D5 Eb5:4 | D5:6 r:2 |
"""
TUNE_B = """
A4:3 Bb4 A4:2 G4 F4 | G4:2 A4:2 D4:4 | A4:3 Bb4 C5:2 Bb4 A4 | Bb4:2 A4:2 G4:4 |
F4:2 G4 A4 Bb4:2 C5 D5 | Eb5:4 D5:4 | C5:2 Bb4:2 A4:2 Bb4 C5 | D5:8 |
"""
CALL = "D4 D4 A4:2 D4 A4:2 r"
CALL_HI = "A4:1.5 A4:0.5 D5:4 r:2"


def build():
    L = BARS * CLK.bar_s
    m = LoopMix(L)
    t = CLK.t

    seq = ' '.join([SEQ] * 6)
    pluck_score(m, 'lyre', CLK, 0, riff_seq(RIFFS, seq), vel=0.7, dark=0.5, length=0.9, accents=(0, 4))
    # breakdown: the lyre doubles up into sixteenths
    for i, r in enumerate(SEQ.split()):
        notes = RIFFS[r].split()
        sixteenths = ' '.join(f"{n}:0.5 {n}:0.5" for n in notes)
        pluck_score(m, 'lyre_hi', CLK, 32 + i, sixteenths, vel=0.45, dark=0.4, length=0.6,
                    accents=(0, 2, 4, 6), octave=1)

    def aulos(score, bar, vel=0.85, octave=0, stem='aulos'):
        m.line(stem, I.aulos(phrase(CLK, score, bar, vel=vel, octave=octave, sd=0.004), vib=0.006, bright=0.1))
    aulos(TUNE_A, 8)
    aulos(TUNE_B, 16)
    aulos(TUNE_A, 24)
    m.line('aulos_low', I.aulos(ornament(phrase(CLK, TUNE_A, 24, vel=0.7, octave=-1), MODE, min_dur=0.4), vib=0.006))
    aulos(TUNE_A, 40, vel=0.9)

    def trumpet(score, bar, vel):
        m.line('tuba', I.brass(phrase(CLK, score, bar, vel=vel, sd=0.006), 'tuba'))
    for b in (3, 7):
        trumpet(CALL, b, 0.7)
    for b in (17, 19, 21):
        trumpet(CALL_HI, b, 0.75)
    for b in (32, 34, 36, 38):
        trumpet(CALL, b, 0.8)
        trumpet(CALL_HI, b + 1, 0.7)
    # cornua hold the bass under the doubled tune and the last section
    for b0 in (24, 40):
        for k, r in enumerate("D D Eb D C Bb C Eb".split()):
            note = midi(r + '2') if r in ('C', 'D', 'Eb') else midi(r + '1')
            m.line('cornu', I.brass([(t(b0 + k), CLK.bar_s * 0.96, note + 12, 0.7)], 'cornu'))

    m.add('drone', 0, loop_drone([midi('D2')], L, vel=0.5, formant=600))

    kit = A.STD_KIT
    march = [(0, 'w', .9), (2, 't', .5), (3, 't', .35), (4, 'w', .75), (6, 't', .55), (7, 't', .4)]
    roll = [(u, 't', .35 + .06 * u) for u in range(8)] + [(0, 'w', .9), (4, 'w', .8), (6, 'w', .85)]
    A.drums(m, 'war', CLK, 0, BARS, [(0, 'w', .95), (4, 'w', .75)], kit, fill=[(0, 'w', .95), (4, 'w', .8), (6, 'w', .85), (7, 'w', .9)], fill_every=8)
    A.drums(m, 'frame', CLK, 0, BARS, march, A.STD_KIT, fill=roll, fill_every=8)
    for b in (8, 24, 40):
        m.add('cymbal', t(b), I.cymbal(0.8))
    m.add('cymbal', t(32), I.cymbal(0.6, decay=1.5))

    return m, dict(space='field', levels={
        'lyre': (-22, -0.3, 0.25), 'lyre_hi': (-26, 0.35, 0.3), 'aulos': (-18, 0.1, 0.3),
        'aulos_low': (-23, -0.15, 0.3), 'tuba': (-21, 0.45, 0.45), 'cornu': (-25, -0.2, 0.35),
        'drone': (-30, 0.0, 0.3), 'war': (-22, 0.0, 0.25), 'frame': (-25, -0.1, 0.2),
        'cymbal': (-30, 0.3, 0.4),
    })
