"""Mare Nostrum — the province map. Harp waves under a ney, a soft organ pad.

G Mixolydian, 3/4, quarter = 84. Intro (4) · A ney (16) · B tibia quotes the
Urbs head in G Mixolydian (D C B C D | G F) (16) · A' ney with grace notes,
harp doubling the long notes (16) · outro (4).
"""
import numpy as np

import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, ornament, phrase
from mix import Mix

MODE = Mode('G', 'mixolydian')
CLK = bpm_clock(84, 2, 3)   # eighths, 6 per bar

THEME = """
D5:4 B4 C5     | D5:2 E5:2 D5:2  | C5:3 B4 A4:2    | G4:6 |
F4:2 G4:2 A4:2 | C5:4 B4:2       | A4:2 G4:2 F4:2  | G4:6 |
E5:4 D5 C5     | D5:2 F5:2 E5:2  | D5:3 C5 B4:2    | A4:6 |
C5:2 B4:2 A4:2 | G4:2 A4:2 F4:2  | A4:4 F4:2       | G4:6 |
"""
ROOTS = "G G A G  F C F G  C D G A  A F F G"

QUOTE = """
D5:3 C5 B4 C5  | D5:2 G5 F5:3    | D5:2 C5 B4:2 A4 | G4:6 |
D5:3 E5 F5 G5  | A5:2 G5 F5:2 D5 | E5:2 D5 C5:2 A4 | D5:6 |
F5:3 A5:2 G5   | F5:2 D5 C5:3    | B4:2 C5 D5:2 F5 | D5:6 |
G5:3 F5 D5 C5  | D5:2 G5 F5:3    | D5:2 C5 B4:2 A4 | G4:6 |
"""
ROOTS_B = "G G F G  G D C D  F D G D  C G F G"


def waves(m, bar0, roots, vel=0.6, fade=None):
    """Harp arpeggios rising one bar and falling the next, swelling every four bars."""
    rs = roots.split()
    up = ' '.join(r if i % 2 == 0 else '-' for i, r in enumerate(rs))
    down = ' '.join(r if i % 2 == 1 else '-' for i, r in enumerate(rs))
    swell = [0.82 + 0.18 * np.sin(2 * np.pi * (i - 1) / 4) for i in range(len(rs))]
    if fade:
        swell = [s * f for s, f in zip(swell, fade)]
    vo = lambda r: [r, MODE.step(r, 4), r + 12, MODE.step(r, 2) + 12, MODE.step(r, 4) + 12, r + 24]  # noqa: E731
    kw = dict(clock=CLK, mode=MODE, low='F2', kind='harp', dark=0.45, vel=vel, voicing=vo,
              accents=(0,), fade=swell, length=3.5)
    A.arpeggio(m, 'harp', bar0=bar0, roots=up, pattern=[0, 1, 2, 3, 4, 5], **kw)
    A.arpeggio(m, 'harp', bar0=bar0, roots=down, pattern=[5, 4, 3, 2, 1, 0], **kw)


def build():
    m = Mix(56 * CLK.bar_s, tail=5.0)
    t, bs = CLK.t, CLK.bar_s
    pad = lambda x, d, v: I.organ(x, d, v, ranks='flute', attack=0.6)  # noqa: E731
    kit = A.STD_KIT

    # intro
    waves(m, 0, "G G F G", fade=[0.6, 0.75, 0.9, 1.0])
    A.held(m, 'pad', CLK, 2, "F G", MODE, pad, low='F3', vel=0.4)

    # A: ney
    waves(m, 4, ROOTS)
    A.held(m, 'pad', CLK, 4, ROOTS, MODE, pad, low='F3', vel=0.4)
    m.line('ney', I.flute(phrase(CLK, THEME, 4, vel=0.8), 'ney'))
    A.drums(m, 'drums', CLK, 4, 16, [(0, 'b', .35)], kit)

    # B: the tibia remembers the Urbs theme
    waves(m, 20, ROOTS_B)
    A.held(m, 'pad', CLK, 20, ROOTS_B, MODE, pad, low='F3', vel=0.45)
    m.line('tibia', I.flute(phrase(CLK, QUOTE, 20, vel=0.8), 'tibia'))
    A.drums(m, 'drums', CLK, 20, 16, [(0, 'b', .45), (4, 't', .2)], kit,
            fill=[(0, 'b', .45), (3, 'j', .2), (4, 'j', .3)], fill_every=4)

    # A': ney with grace notes; harp doubles the long notes an octave down
    notes = phrase(CLK, THEME, 36, vel=0.82)
    m.line('ney', I.flute(ornament(notes, MODE, min_dur=2 * CLK.unit * 0.95), 'ney'))
    for ts, td, p, v in notes:
        if td >= 3 * CLK.unit * 0.95:
            m.add('harp_lead', ts, I.pluck(p - 12, 0.6 * v, kind='harp', dark=0.35, length=3.5))
    waves(m, 36, ROOTS, vel=0.65)
    A.held(m, 'pad', CLK, 36, ROOTS, MODE, pad, low='F3', vel=0.45)
    A.drums(m, 'drums', CLK, 36, 16, [(0, 'b', .45), (4, 't', .22)], kit,
            fill=[(0, 'b', .45), (3, 'j', .2), (4, 'j', .3)], fill_every=4)

    # outro: the waves settle
    waves(m, 52, "G F G -", fade=[0.85, 0.7, 0.55, 0.0])
    A.held(m, 'pad', CLK, 52, "G F", MODE, pad, low='F3', vel=0.35)
    m.add('pad', t(54), I.organ([midi(n) for n in ('G3', 'D4', 'G4')], 2 * bs, 0.35, ranks='flute', attack=0.6))
    m.add('harp_lead', t(55), I.strum([midi(n) for n in ('G2', 'D3', 'G3', 'B3', 'D4', 'G4')], 0.28,
                                      spread=0.06, kind='harp', length=5.0, dark=0.4))

    for b in (4, 20, 36, 55):
        m.add('crotala', t(b), I.crotala(0.6 if b != 55 else 0.35))

    return m, dict(space='courtyard', levels={
        'harp': (-22, -0.25, 0.4), 'harp_lead': (-23, -0.1, 0.4), 'pad': (-28, 0.1, 0.5),
        'ney': (-18, 0.12, 0.4), 'tibia': (-19, 0.15, 0.4),
        'drums': (-28, -0.15, 0.25), 'crotala': (-34, 0.35, 0.55),
    })
