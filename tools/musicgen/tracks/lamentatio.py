"""Lamentatio — lament and rebuilding, after a fire, a revolt or a funeral.

4/4, 52 bpm. A solo aulos (the funeral instrument) laments in A Aeolian over
harp and a soft choir. The harmony turns (F, G), and the last third moves into
D Dorian: the harp quotes the Urbs theme, the aulos answers with its close,
and the piece ends on a D major chord.
Intro (4) · lament (8) · lament, higher (8) · turn (2) · Urbs (8) · coda (2).
"""
import arrange as A
import instruments as I
from core import Clock, Mode, bpm_clock, midi, ornament, phrase
from mix import Mix
from urbs import THEME_A as URBS

AEOL = Mode('A', 'aeolian')
DOR = Mode('D', 'dorian')
CLK = bpm_clock(52, 2, 4)                # eighths, 8 per bar
HYMN = Clock(CLK.bar_s / 6, 6)           # the 6/8 Urbs theme, one bar per 4/4 bar

LAMENT = """
A4:6 G4 F4        | E4:4 F4:2 E4 D4   | E4:6 r:2          | C5:3 B4 A4:2 G4 F4 |
G4:2 F4:2 E4:4    | D4:2 E4 F4 E4:2 D4 C4 | B3:2 C4 D4 E4:4 | A3:6 r:2 |
E5:6 D5 C5        | B4:4 C5:2 B4 A4   | A4:6 r:2          | F5:3 E5 D5:2 C5 B4 |
C5:2 D5 E5 D5:4   | C5:2 B4 A4 G4:2 F4 E4 | F4:2 E4 D4 E4:4 | E4:8 |
"""
ROOTS_LAMENT = "A C E F C D E A  A E A D C F A E"
URBS_BARS = URBS.split("|")
HEAD = "|".join(URBS_BARS[0:4])     # A G F G | A D C | A G F E | D
CLOSE = "|".join(URBS_BARS[12:16])  # D C A G | A D C | A G F E | D

KIT = {'b': lambda v: I.boom(v, pitch=50, decay=0.5)}


def choir(vowel):
    return lambda midis, dur, vel: A.choir_chord(midis, dur, vel, vowel)


def build():
    m = Mix(32 * CLK.bar_s, tail=6.0)
    t = CLK.t

    # choir pad throughout
    A.held(m, 'choir', CLK, 0, "A A F E", AEOL, choir('u'), low='F3', vel=0.45)
    A.held(m, 'choir', CLK, 4, ROOTS_LAMENT, AEOL, choir('u'), low='F3', vel=0.5)
    A.held(m, 'choir', CLK, 20, "F G", AEOL, choir('o'), low='F3', vel=0.55)
    A.held(m, 'choir', CLK, 22, "D D C D D D C D", DOR, choir('a'), low='F3', vel=0.5)
    m.add('choir', t(30), A.choir_chord([midi(n) for n in ('D3', 'A3', 'D4', 'F#4')], 2 * CLK.bar_s, 0.6, 'a'))

    # harp
    arp = dict(clock=CLK, low='E2', kind='harp', dark=0.5)
    A.arpeggio(m, 'harp', bar0=0, roots="A A F E", mode=AEOL, pattern=[0, None, None, None, 2, None, None, None],
               vel=0.5, **arp)
    A.arpeggio(m, 'harp', bar0=4, roots=" ".join(ROOTS_LAMENT.split()[:8]), mode=AEOL,
               pattern=[0, None, 1, None, 2, None, 3, None], vel=0.55, **arp)
    A.arpeggio(m, 'harp', bar0=12, roots=" ".join(ROOTS_LAMENT.split()[8:]), mode=AEOL, pattern=[0, 1, 2, 3, 4, 3, 2, 1],
               vel=0.6, **arp)
    A.arpeggio(m, 'harp', bar0=20, roots="F G", mode=AEOL, pattern=[0, 1, 2, 3, 4, 3, 4, None],
               vel=0.65, **arp)
    hymn_arp = dict(arp, clock=HYMN, low='C3')
    A.arpeggio(m, 'harp', bar0=22, roots="D D C D D D C D", mode=DOR, pattern=[0, 1, 2, 3, 2, 1],
               accents=(0, 3), vel=0.6, **hymn_arp)
    m.add('harp', t(30), I.strum([midi(n) for n in ('D2', 'A2', 'D3', 'F#3', 'A3', 'D4')], 0.5,
                                  spread=0.07, kind='harp', dark=0.5, length=7.0))

    # the lament
    lament = ornament(phrase(CLK, LAMENT, 4, sd=0.015), AEOL, min_dur=2.5, kind='lower', grace=0.11, every=2)
    m.line('aulos', I.aulos(lament, vib=0.014))

    # the turn to hope: harp quotes Urbs, the aulos answers with its close
    for ts, td, p, v in phrase(HYMN, HEAD, 22, sd=0.008, vel=0.75):
        m.add('harp_lead', ts, I.pluck(p, v, kind='harp', dark=0.35, length=4.0))
        if td > 4 * HYMN.unit:
            m.add('harp_lead', ts + 3 * HYMN.unit, I.pluck(p, 0.5 * v, kind='harp', dark=0.4))
    close = ornament(phrase(HYMN, CLOSE, 26, vel=0.75, sd=0.01), DOR, min_dur=2 * HYMN.unit)
    m.line('aulos', I.aulos(close, vib=0.010), 0.9)

    # a distant funeral drum in the second lament
    A.drums(m, 'drum', CLK, 12, 8, [(0, 'b', .6)], KIT, fade=[.7, .8, .9, 1, 1, .9, .8, .6])

    return m, dict(space='temple', levels={
        'aulos': (-18, 0.08, 0.4), 'harp': (-23, -0.25, 0.4), 'harp_lead': (-20, -0.1, 0.4),
        'choir': (-27, 0.1, 0.55), 'drum': (-30, -0.1, 0.35),
    })
