"""Limes — city music for the northern frontier (Aquileia, Gaul, the Rhine, Britannia).

A Aeolian, 4/4, 66 bpm. Cold and wide: low drone and wind, a breathy low
ney, kithara, a slow heavy frame drum and a distant carnyx calling over the hills.
Intro (6) · A ney theme (16) · B kithara lead, ney counter (12) · A' (8) · outro (4).
"""
import numpy as np

import arrange as A
import core as C
import instruments as I
from core import SR, Mode, bpm_clock, butter, midi, ornament, phrase, unit_std
from mix import Mix

MODE = Mode('A', 'aeolian')
CLK = bpm_clock(66, 2, 4)   # eighths, 8 per bar

THEME_A = """
E4:4 A4:2 G4:2   | A4:6 B4 C5      | B4:3 A4 G4:2 E4:2 | E4:8 |
D4:2 E4:2 G4:3 A4 | G4:2 E4:2 D4:4 | C4:2 D4:2 E4:3 D4 | A3:8 |
A4:4 C5:2 B4:2   | C5:3 D5 E5:4    | D5:2 C5:2 B4:2 G4:2 | A4:8 |
E5:4 D5:2 C5:2   | B4:3 C5 A4:4    | G4:3 F4 E4:2 D4:2 | E4:8 |
"""
ROOTS_A = "A A E E  G C C A  A C G A  C E D E"
THEME_B = """
F4:2 A4:2 C5:4   | B4:2 A4:2 G4:4  | E4:2 G4:2 C5:3 B4 | A4:8 |
F4:2 A4:2 D5:4   | C5:2 B4:2 A4:2 G4:2 | A4:3 B4 C5:2 D5:2 | E5:8 |
F5:4 E5:2 D5:2   | C5:3 B4 A4:4    | G4:2 B4:2 E4:2 G4:2 | A4:8 |
"""
ROOTS_B = "F G C A  D C F E  D A E A"
COUNTER_B = "A4:8 | B4:8 | G4:8 | C5:8 | A4:8 | G4:8 | C5:8 | B4:8 | A4:8 | E4:8 | B4:8 | A4:8"

KIT = {
    'B': lambda v: I.boom(v, pitch=48, decay=0.45),
    't': lambda v: I.tek(v, pitch=300),
}


def wind(length, vel=1.0):
    """Cold wind: band-passed noise swelling slowly."""
    n = int(length * SR)
    t = np.arange(n) / SR
    nz = unit_std(butter(C.noise(n), 'band', [250, 1400]))
    lfo = 0.55 + 0.45 * np.sin(2 * np.pi * 0.07 * t + C.rng.uniform(0, 6)) * np.sin(2 * np.pi * 0.023 * t + 1)
    env = np.clip(t / 6, 0, 1) * np.clip((length - t) / 6, 0, 1)
    return vel * nz * lfo * env


def build():
    m = Mix(46 * CLK.bar_s, tail=5.5)
    t = CLK.t
    arp = dict(clock=CLK, mode=MODE, low='E2', kind='kithara', dark=0.6, step=2)

    # kithara: slow quarter-note ostinato
    A.arpeggio(m, 'kithara', bar0=2, roots="A A F E", pattern=[0, None, 1, None], vel=0.6, **arp)
    A.arpeggio(m, 'kithara', bar0=6, roots=ROOTS_A, pattern=[0, 1, 2, 1], **arp)
    A.arpeggio(m, 'kithara', bar0=22, roots=ROOTS_B, pattern=[0, 2, 1, 3], vel=0.6, **arp)
    A.arpeggio(m, 'kithara', bar0=34, roots="A A E E G C C A", pattern=[0, 1, 2, 3], **arp)
    A.arpeggio(m, 'kithara', bar0=42, roots="A F A -", pattern=[0, None, 1, None], fade=[.8, .65, .5, 0], **arp)
    m.add('kithara', t(44), I.strum([midi(n) for n in ('A2', 'E3', 'A3', 'C4', 'E4')], 0.5,
                                     spread=0.06, length=6.0, dark=0.6, kind='kithara'))

    # ney theme, B counter, A'
    m.line('ney', I.flute(phrase(CLK, THEME_A, 6, sd=0.008)))
    m.line('ney_counter', I.flute(phrase(CLK, COUNTER_B, 22, octave=-1, vel=0.55, shape=False), kind='tibia'))
    first_half = "|".join(THEME_A.split("|")[:8])
    m.line('ney', I.flute(ornament(phrase(CLK, first_half, 34, vel=0.85, sd=0.008), MODE, min_dur=1.5,
                                   kind='lower')))
    # B: kithara takes the tune
    for ts, td, p, v in phrase(CLK, THEME_B, 22, sd=0.008):
        m.add('kithara_lead', ts, I.pluck(p, v, dark=0.4, kind='kithara', length=3.6))
        if td > 5 * CLK.unit:
            m.add('kithara_lead', ts + 4 * CLK.unit, I.pluck(p, 0.5 * v, dark=0.5, kind='kithara'))
    # A': kithara doubles the tune an octave down
    for ts, td, p, v in phrase(CLK, first_half, 34, sd=0.008, vel=0.6):
        m.add('kithara_lead', ts + 0.015, I.pluck(p - 12, v, dark=0.5, kind='kithara'))

    # drone and wind
    m.add('drone', 0, I.drone([midi('A2'), midi('E3')], t(46), 2 * CLK.bar_s, 3 * CLK.bar_s, formant=700))
    m.add('wind', 0, wind(t(46) + 3))

    # distant carnyx calls on the frontier
    m.line('carnyx', I.brass([(t(3), 1.0, midi('A2'), .6), (t(3) + 1.1, 2.6, midi('E3'), .7)], 'carnyx'))
    m.line('carnyx', I.brass([(t(28), 0.9, midi('E3'), .6), (t(28) + 1.0, 3.0, midi('A3'), .65)], 'carnyx'))
    m.line('carnyx', I.brass([(t(42), 3.2, midi('A2'), .45)], 'carnyx'), 0.6)

    # slow, heavy frame drum
    A.drums(m, 'drums', CLK, 2, 4, [(0, 'B', .6)], KIT)
    A.drums(m, 'drums', CLK, 6, 16, [(0, 'B', .8), (4, 'B', .45), (6, 't', .2)], KIT)
    heavy = [(0, 'B', .85), (3, 'B', .35), (4, 'B', .55), (6, 't', .25)]
    A.drums(m, 'drums', CLK, 22, 12, heavy, KIT)
    A.drums(m, 'drums', CLK, 34, 8, heavy, KIT, fill=[(0, 'B', .85), (4, 'B', .6), (5, 'B', .5), (6, 'B', .65)])
    A.drums(m, 'drums', CLK, 42, 3, [(0, 'B', .6)], KIT, fade=[1, .75, .5])

    return m, dict(space='hall', levels={
        'ney': (-18, 0.1, 0.45), 'ney_counter': (-25, 0.3, 0.5), 'kithara': (-23, -0.3, 0.4),
        'kithara_lead': (-20, -0.1, 0.4), 'drone': (-28, 0.0, 0.45), 'wind': (-34, 0.0, 0.3),
        'carnyx': (-29, 0.55, 0.85), 'drums': (-26, -0.1, 0.3),
    })
