"""Urbs — the Republic city theme, and the source of the campaign leitmotif.

D Dorian, 6/8, dotted quarter = 66. Intro · A (aulos) · B (pan pipes) ·
A' (lyre lead, aulos heterophony an octave below) · outro.
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, phrase, ornament
from mix import Mix

MODE = Mode('D', 'dorian')
CLK = bpm_clock(66, 3, 2)   # eighths, 6 per bar

# The leitmotif. Other tracks quote THEME_A (transposed or re-moded).
THEME_A = """
A4:3 G4 F4 G4   | A4:2 D5 C5:3     | A4:2 G4 F4:2 E4 | D4:6 |
A4:3 B4 C5 D5   | E5:2 D5 C5:2 A4  | B4:2 A4 G4:2 E4 | A4:6 |
C5:3 E5:2 D5    | C5:2 A4 G4:3     | F4:2 G4 A4:2 C5 | A4:6 |
D5:3 C5 A4 G4   | A4:2 D5 C5:3     | A4:2 G4 F4:2 E4 | D4:6 |
"""
THEME_B = """
F5:3 E5 D5 C5   | D5:2 E5 F5:3     | G5:2 F5 E5:2 D5 | C5:6 |
C5:3 D5 E5 C5   | A4:2 C5 D5:3     | E5:2 D5 C5:2 B4 | A4:6 |
F5:3 G5 A5:2    | G5:2 F5 E5:3     | D5:2 C5 D5:2 E5 | F5:3 E5:3 |
D5:3 C5 A4 C5   | D5:2 C5 A4:3     | G4:2 A4 C5:2 B4 | A4:6 |
"""
ROOTS_A = "D D C D  D C G A  C F F A  D D C D"
ROOTS_B = "F D C C  A D G A  F C G C  D D G A"


def build():
    m = Mix(63 * CLK.bar_s)
    t = CLK.t
    arp = dict(clock=CLK, mode=MODE, low='C3', dark=0.55)

    # lyre ostinato
    A.arpeggio(m, 'lyre_ost', bar0=0, roots="D D", pattern=[0], **arp, vel=0.7)
    A.arpeggio(m, 'lyre_ost', bar0=2, roots="C D D D C A", pattern=[0, 1, 2, 3, 2, 1], **arp)
    A.arpeggio(m, 'lyre_ost', bar0=8, roots=ROOTS_A, pattern=[0, 1, 2, 3, 2, 1], accents=(0, 3), **arp)
    A.arpeggio(m, 'lyre_ost', bar0=24, roots=ROOTS_B, pattern=[0, 2, 4, 3, 2, 1], accents=(0, 3), **arp)
    A.arpeggio(m, 'lyre_ost', bar0=40, roots=ROOTS_A, pattern=[0, 1, 2, 3, 4, 2], accents=(0, 3), **arp)
    A.arpeggio(m, 'lyre_ost', bar0=56, roots="D D C D", pattern=[0, 1, 2, 3, 2, 1], accents=(0, 3),
               fade=[1, .93, .86, .79], **arp)
    A.arpeggio(m, 'lyre_ost', bar0=60, roots="D D", pattern=[0, None, 2, None, 1, None], fade=[.7, .6], **arp)
    m.add('lyre_lead', t(62), I.strum([midi(n) for n in ('D3', 'A3', 'D4', 'A4', 'D5')], 0.38,
                                       spread=0.045, length=5.5, dark=0.5))

    # A: aulos lead
    m.line('aulos', I.aulos(phrase(CLK, THEME_A, 8)))
    # B: pan pipes lead
    for ts, td, p, v in phrase(CLK, THEME_B, 24, vel=0.85):
        m.add('pipes', ts, I.pipes(p, td, v))
    # A': lyre lead; aulos heterophony an octave below
    for ts, td, p, v in phrase(CLK, THEME_A, 40, sd=0.007):
        m.add('lyre_lead', ts, I.pluck(p, v, dark=0.3))
        if td > 5 * CLK.unit:   # re-pluck long notes, with the octave below
            m.add('lyre_lead', ts + 3 * CLK.unit, I.pluck(p, 0.55 * v, dark=0.4))
            m.add('lyre_lead', ts + 0.02, I.pluck(p - 12, 0.5 * v, dark=0.45))
    het = ornament(phrase(CLK, THEME_A, 40, octave=-1, vel=0.75), MODE, min_dur=2 * CLK.unit * 0.95)
    m.line('aulos_het', I.aulos(het, vib=0.008))
    # outro: the aulos remembers the first phrase
    m.line('aulos', I.aulos([n for n in phrase(CLK, THEME_A, 56, vel=0.6) if n[0] < t(60)], vib=0.012), 0.75)

    # drone: second aulos pipe on D3 + A2, out during B
    m.add('drone', 0, I.drone([midi('D3'), midi('A2')], t(24) + 0.6, 2 * CLK.bar_s, 1.2 * CLK.bar_s))
    m.add('drone', t(39.5), I.drone([midi('D3'), midi('A2')], t(23.5), 1.5 * CLK.bar_s, 2.5 * CLK.bar_s))

    # tympanum
    kit = A.STD_KIT
    A.drums(m, 'drums', CLK, 4, 4, [(0, 'b', .55)], kit)
    A.drums(m, 'drums', CLK, 8, 16, [(0, 'b', .8), (3, 't', .4), (5, 't', .22)], kit)
    fill = [(0, 'b', .9), (3, 'j', .45), (4, 'j', .5), (5, 'j', .6)]
    A.drums(m, 'drums', CLK, 24, 16, [(0, 'b', .9), (2, 'j', .35), (3, 'b', .55), (5, 'j', .4)], kit, fill=fill)
    A.drums(m, 'drums', CLK, 40, 16, [(0, 'b', .9), (2, 'j', .3), (3, 'b', .55), (4, 'j', .2), (5, 'j', .4)],
            kit, fill=fill)
    A.drums(m, 'drums', CLK, 56, 4, [(0, 'b', .6)], kit, fade=[1, .82, .64, .46])

    for b in (8, 24, 28, 32, 36, 40, 48, 62):
        m.add('crotala', t(b), I.crotala(0.8 if b != 62 else 0.3))

    return m, dict(space='courtyard', levels={
        'lyre_ost': (-23, -0.3, 0.35), 'lyre_lead': (-19, -0.05, 0.35),
        'aulos': (-18, 0.12, 0.3), 'aulos_het': (-23, 0.2, 0.35), 'pipes': (-19, 0.18, 0.45),
        'drone': (-29, 0.0, 0.4), 'drums': (-26, -0.15, 0.2), 'crotala': (-34, 0.35, 0.55),
    })
