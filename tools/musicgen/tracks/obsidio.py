"""Obsidio — a siege. Seamless loop.

C Aeolian, 4/4, quarter = 80, 32 bars, chords i-VI-iv-v (C Ab F G) every
two bars. Slow massive war drums like siege engines; a grinding low kithara
ostinato that leans on the G-Ab and D-Eb semitones; a low hydraulis;
cornua and tubae in held chords; a slow tuba melody. Sections: engines
(0-7) · horns (8-15) · melody (16-23) · everything (24-31). The closing G
bar falls back to C at the loop.
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, phrase
from loopkit import LoopMix, brass_chord, loop_drone, pluck_score, riff_seq, swell

MODE = Mode('C', 'aeolian')
CLK = bpm_clock(80, 2, 4)   # eighths
BARS = 32

RIFFS = {
    'C':  "C3:2 Eb3 D3 C3:2 G2 Ab2",
    'Ab': "Ab2:2 C3 Bb2 Ab2:2 Eb2 G2",
    'F':  "F2:2 Ab2 G2 F2:2 C3 Eb3",
    'G':  "G2:2 Bb2 Ab2 G2:2 D3 Eb3",
}
CHORDS = "C C Ab Ab F F G G"
VOICES = {   # cornu / tuba chord voicings
    'C': ['C3', 'G3', 'Eb4'], 'Ab': ['Ab2', 'Eb3', 'C4'], 'F': ['F2', 'C3', 'Ab3'], 'G': ['G2', 'D3', 'Bb3'],
}
MELODY = """
C4:4 D4:2 Eb4:2 | D4:4 C4:2 Bb3:2 | Ab3:6 G3:2 | C4:8 |
Eb4:4 F4:2 G4:2 | Ab4:4 G4:2 F4:2 | Eb4:3 D4 C4:2 Bb3:2 | G3:8 |
"""


def build():
    L = BARS * CLK.bar_s
    m = LoopMix(L)
    t = CLK.t

    seq = ' '.join([CHORDS] * 4)
    pluck_score(m, 'grind', CLK, 0, riff_seq(RIFFS, seq), vel=0.75, kind='kithara', dark=0.55,
                length=1.6, accents=(0, 4))

    # organ: low held chords, one per two bars, swelling in
    for i, c in enumerate(CHORDS.split()[::2] * 4):
        midis = [midi(n) - 12 for n in VOICES[c][:2]]
        sig = I.organ(midis, CLK.bar_s * 2 * 0.98, vel=0.7, ranks='full')
        m.add('organ', t(2 * i), swell(sig, CLK.bar_s * 2, rise=0.5, floor=0.35))

    # horn chords from bar 8, two bars each
    for i in range(4, 16):
        c = CHORDS.split()[(2 * i) % 8]
        for t0sig in brass_chord([midi(n) for n in VOICES[c]], t(2 * i), CLK.bar_s * 1.9, 0.7,
                                 kinds=('cornu', 'cornu', 'tuba')):
            m.line('horns', t0sig)

    m.line('tuba', I.brass(phrase(CLK, MELODY, 16, vel=0.85, sd=0.008), 'tuba'))
    m.line('tuba', I.brass(phrase(CLK, MELODY, 24, vel=0.9, sd=0.008), 'tuba'))
    m.line('tuba_low', I.brass(phrase(CLK, MELODY, 24, vel=0.7, octave=-1, sd=0.01), 'cornu'))

    m.add('drone', 0, loop_drone([midi('C2')], L, vel=0.5, formant=500))

    kit = A.STD_KIT
    engine = [(0, 'W', 1.0), (4, 'w', .75), (6.5, 'w', .45), (7, 'w', .6)]
    A.drums(m, 'war', CLK, 0, BARS, engine, kit, sd=0.008,
            fill=[(0, 'W', 1.0), (4, 'w', .8), (5, 'w', .6), (6, 'w', .7), (7, 'w', .85)], fill_every=8)
    A.drums(m, 'frame', CLK, 8, 24, [(2, 't', .4), (6, 't', .45), (7.5, 't', .25)], kit)
    for b in (16, 24):
        m.add('cymbal', t(b), I.cymbal(0.7, decay=3.0))

    return m, dict(space='hall', levels={
        'grind': (-22, -0.25, 0.3), 'organ': (-27, 0.0, 0.45), 'horns': (-23, 0.2, 0.45),
        'tuba': (-20, 0.1, 0.4), 'tuba_low': (-25, -0.1, 0.4), 'drone': (-30, 0.0, 0.3),
        'war': (-21, 0.0, 0.3), 'frame': (-27, 0.25, 0.25), 'cymbal': (-31, -0.3, 0.5),
    })
