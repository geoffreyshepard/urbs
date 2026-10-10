"""Nilus — Alexandria city music.

D Hijaz, 4/4, 88 bpm, maqsum rhythm (dum tek . tek dum . tek .). Egyptian harp
ostinato, ney melody with grace notes and glides, sistrum, frame drum, low drone.
Intro (8) · A ney theme (16) · B up a fourth, on G (16) · A' with harp
heterophony (8) · outro (7).
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, ornament, phrase
from mix import Mix

MODE = Mode('D', 'hijaz')
CLK = bpm_clock(88, 2, 4)   # eighths, 8 per bar

THEME_A = """
A4:4 Bb4 A4 G4 F#4      | G4:2 F#4 Eb4 D4:4       | D4 Eb4 F#4 G4 A4:4     | Bb4:2 A4 G4 A4:4 |
C5:3 Bb4 A4:2 G4 F#4    | G4:2 A4 Bb4 A4:4        | G4 F#4 Eb4 F#4 G4:2 F#4 Eb4 | D4:8 |
D5:4 C5 Bb4 A4 G4       | A4:2 Bb4 C5 Bb4:2 A4:2  | G4:3 A4 Bb4:2 A4 G4    | F#4:6 r:2 |
A4:3 Bb4 C5:2 Bb4 A4    | G4:2 F#4 G4 A4:4        | Bb4 A4 G4 F#4 G4:2 Eb4 F#4 | D4:8 |
"""
THEME_B = """
G4:2 A4 Bb4 C5:4        | D5:3 C5 Bb4:2 A4:2      | Bb4 C5 D5 Eb5 D5:4     | C5:2 Bb4 A4 G4:4 |
G4:2 A4 Bb4 C5:2 D5:2   | Eb5:3 D5 C5:2 Bb4:2     | A4 Bb4 C5 Bb4 A4:2 G4:2 | A4:6 r:2 |
D5:4 Eb5 D5 C5 D5       | Bb4:2 C5 D5 C5:4        | A4:3 Bb4 C5 Bb4 A4 G4  | F#4:4 G4:4 |
A4:3 G4 F#4 Eb4 F#4:2   | G4:2 F#4 Eb4 D4:4       | D4 Eb4 F#4 G4 A4 Bb4 A4 G4 | A4:8 |
"""
ROOTS_A = "D D D G  C G D D  Bb G G D  D D C D"
ROOTS_B = "G G Bb G  G C D D  G C G D  D D D D"
OUTRO = """
C5:3 Bb4 A4:2 G4 F#4    | G4:2 A4 Bb4 A4:4        | G4 F#4 Eb4 F#4 G4:2 F#4 Eb4 | D4:8 |
"""

KIT = {
    'b': lambda v: I.boom(v, pitch=72, decay=0.2),
    't': lambda v: I.tek(v, pitch=520),
    'x': lambda v: I.sistrum(v),
}
MAQSUM = [(0, 'b', .8), (1, 't', .4), (3, 't', .45), (4, 'b', .65), (6, 't', .45)]
FILL = [(0, 'b', .8), (1, 't', .35), (2, 't', .3), (3, 't', .45), (4, 'b', .65), (5, 't', .3),
        (6, 't', .5), (7, 't', .55)]


def build():
    m = Mix(55 * CLK.bar_s)
    t = CLK.t
    arp = dict(clock=CLK, mode=MODE, low='C3', kind='harp', dark=0.45, accents=(0, 4))
    pat = [0, None, 2, 3, 1, None, 2, 4]

    # harp ostinato
    A.arpeggio(m, 'harp', bar0=0, roots="D D D D", pattern=[0, None, None, None, 1, None, 2, None],
               fade=[.6, .7, .8, .9], **arp)
    A.arpeggio(m, 'harp', bar0=4, roots="D D C D", pattern=pat, vel=0.6, **arp)
    A.arpeggio(m, 'harp', bar0=8, roots=ROOTS_A, pattern=pat, **arp)
    A.arpeggio(m, 'harp', bar0=24, roots=ROOTS_B, pattern=[0, 2, 1, 3, 0, 4, 2, 3], **arp)
    A.arpeggio(m, 'harp', bar0=40, roots="D D D G C G D D", pattern=pat, **arp)
    A.arpeggio(m, 'harp', bar0=48, roots="C G D D", pattern=pat, fade=[.9, .8, .7, .6], **arp)
    A.arpeggio(m, 'harp', bar0=52, roots="D D", pattern=[0, None, 2, None, 1, None, None, None],
               fade=[.5, .4], **arp)
    m.add('harp', t(54), I.strum([midi(n) for n in ('D3', 'A3', 'D4', 'F#4', 'A4')], 0.45,
                                  spread=0.05, length=5.0, dark=0.45, kind='harp'))

    # ney: A, B, A' (ornamented), outro
    m.line('ney', I.flute(ornament(phrase(CLK, THEME_A, 8), MODE, min_dur=1.0, every=2)))
    m.line('ney', I.flute(ornament(phrase(CLK, THEME_B, 24, vel=0.85), MODE, min_dur=1.0, every=2)))
    first_half = "|".join(THEME_A.split("|")[:8])
    m.line('ney', I.flute(ornament(phrase(CLK, first_half, 40, vel=0.85), MODE, min_dur=0.6)))
    m.line('ney', I.flute(phrase(CLK, OUTRO, 48, vel=0.65)), 0.8)
    # A': the harp shadows the melody (heterophony)
    for ts, td, p, v in phrase(CLK, first_half, 40, sd=0.008, vel=0.6):
        m.add('harp_lead', ts + 0.012, I.pluck(p, v, dark=0.35, kind='harp'))

    # drone on D
    m.add('drone', 0, I.drone([midi('D3'), midi('A2')], t(55), 2 * CLK.bar_s, 3 * CLK.bar_s, formant=800))

    # maqsum on frame drum
    A.drums(m, 'drums', CLK, 4, 4, [(0, 'b', .6), (4, 'b', .45)], KIT)
    A.drums(m, 'drums', CLK, 8, 16, MAQSUM, KIT, fill=FILL, fill_every=8)
    A.drums(m, 'drums', CLK, 24, 16, MAQSUM + [(7, 't', .25)], KIT, fill=FILL, fill_every=4, vel=1.05)
    A.drums(m, 'drums', CLK, 40, 8, MAQSUM, KIT, fill=FILL, fill_every=8)
    A.drums(m, 'drums', CLK, 48, 4, MAQSUM, KIT, fade=[.9, .75, .6, .45])
    A.drums(m, 'drums', CLK, 52, 2, [(0, 'b', .4)], KIT)

    # sistrum: sacred shimmer at phrase starts
    A.drums(m, 'sistrum', CLK, 4, 4, [(0, 'x', .6)], KIT)
    A.drums(m, 'sistrum', CLK, 8, 16, [(0, 'x', .7), (4, 'x', .35)], KIT)
    A.drums(m, 'sistrum', CLK, 24, 16, [(0, 'x', .8), (4, 'x', .5), (6, 'x', .3)], KIT)
    A.drums(m, 'sistrum', CLK, 40, 12, [(0, 'x', .7)], KIT)
    m.add('sistrum', t(54), I.sistrum(0.5))

    return m, dict(space='courtyard', levels={
        'ney': (-18, 0.12, 0.4), 'harp': (-23, -0.25, 0.35), 'harp_lead': (-23, -0.1, 0.35),
        'drone': (-30, 0.0, 0.4), 'drums': (-26, -0.1, 0.2), 'sistrum': (-32, 0.35, 0.5),
    })
