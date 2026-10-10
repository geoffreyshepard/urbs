"""Incendium — fire or earthquake. Seamless loop.

E Phrygian with chromatic touches, 4/4, quarter = 100, 40 bars. An urgent
low kithara ostinato (with a D#-E lean back to the tonic), organ clusters
that swell and cut off, brass alarm calls, and a falling chromatic aulos wail.
Sections: ostinato and drums (0-7) · + organ swells (8-15) · + alarms
(16-23) · everything with the aulos (24-31) · thinner, organ low (32-39).
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, phrase
from loopkit import LoopMix, loop_drone, pluck_score, riff_seq, swell

MODE = Mode('E', 'phrygian')
CLK = bpm_clock(100, 2, 4)   # eighths
BARS = 40

RIFFS = {
    'E1': "E3 E3 F3 E3 G3 F3 E3 D#3",
    'E2': "E3 E3 F3 E3 A3 G3 F3 E3",
    'F':  "F3 F3 Gb3 F3 A3 Gb3 F3 E3",
    'D':  "D3 D3 Eb3 D3 F3 Eb3 D3 Eb3",
}
SEQ = "E1 E2 E1 F E1 E2 D E1"
CLUSTERS = [['E3', 'F3', 'B3'], ['E3', 'F3', 'Bb3'], ['D3', 'Eb3', 'A3'], ['F3', 'Gb3', 'C4']]
ALARM = "B4 B4 B4 C5:3 r:2"
FALL = "E4:2 Eb4:2 D4:2 C#4:2"
WAIL = """
B5:4 Bb5:2 A5:2 | G5:3 F5 E5:4 | F5:2 E5 Eb5 D5:2 C5:2 | B4:8 |
C5:3 B4 C5:2 D5:2 | Eb5:4 D5:2 C#5:2 | C5:2 B4:2 Bb4:2 A4:2 | B4:6 r:2 |
"""


def build():
    L = BARS * CLK.bar_s
    m = LoopMix(L)
    t = CLK.t

    pluck_score(m, 'ostinato', CLK, 0, riff_seq(RIFFS, ' '.join([SEQ] * 5)), vel=0.75,
                kind='kithara', dark=0.5, length=1.1, accents=(0, 4))

    # organ clusters: swell over two bars, then cut off
    for k, b in enumerate(range(8, 32, 2)):
        midis = [midi(n) for n in CLUSTERS[k % 4]]
        sig = I.organ(midis, CLK.bar_s * 1.8, vel=0.8, ranks='full')
        m.add('organ', t(b), swell(sig, CLK.bar_s * 1.8, rise=0.9, floor=0.05))
    for k, b in enumerate(range(32, 40, 4)):             # low and sustained in the last section
        sig = I.organ([midi('E2'), midi('F2'), midi('B2')], CLK.bar_s * 3.9, vel=0.7, ranks='soft')
        m.add('organ', t(b), swell(sig, CLK.bar_s * 3.9, rise=0.4, floor=0.3))

    for b in range(16, 32, 2):
        m.line('alarm', I.brass(phrase(CLK, ALARM, b, vel=0.8, sd=0.006), 'tuba'))
        m.line('fall', I.brass(phrase(CLK, FALL, b + 1, vel=0.7, sd=0.008), 'cornu'))

    m.line('aulos', I.aulos(phrase(CLK, WAIL, 24, vel=0.85, sd=0.006), vib=0.012, bright=0.1))

    m.add('drone', 0, loop_drone([midi('E2')], L, vel=0.5, formant=600))

    kit = A.STD_KIT
    drive = [(0, 'B', .9), (3, 'b', .6), (4, 'B', .85), (6, 't', .5), (7, 't', .45)]
    busy = drive + [(1, 't', .3), (2, 't', .35), (5, 't', .3)]
    A.drums(m, 'drums', CLK, 0, 24, drive, kit, fill=busy, fill_every=4)
    A.drums(m, 'drums', CLK, 24, 8, busy, kit)
    A.drums(m, 'drums', CLK, 32, 8, drive, kit, fill=busy, fill_every=4)
    A.drums(m, 'war', CLK, 0, BARS, [(0, 'w', .8)], kit, fade=[1.0 if i % 4 == 0 else 0.0 for i in range(BARS)])
    A.drums(m, 'war', CLK, 24, 8, [(4, 'w', .7)], kit)
    for b in (16, 24):
        m.add('cymbal', t(b), I.cymbal(0.6, decay=2.5))

    return m, dict(space='hall', levels={
        'ostinato': (-21, -0.2, 0.3), 'organ': (-26, 0.0, 0.5), 'alarm': (-22, 0.4, 0.45),
        'fall': (-24, -0.35, 0.45), 'aulos': (-19, 0.1, 0.4), 'drone': (-30, 0.0, 0.3),
        'drums': (-23, -0.1, 0.25), 'war': (-24, 0.0, 0.3), 'cymbal': (-31, 0.3, 0.5),
    })
