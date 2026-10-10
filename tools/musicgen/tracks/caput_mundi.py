"""Caput Mundi — Rome, the final mission. The Urbs theme in its grandest form.

D Dorian, lifting to E Dorian; 6/8, dotted quarter = 62. Built to build and
release rather than stay loud:
intro (6) · A1 lyre alone over a soft organ (16) · A2 aulos, drone, drums (16) ·
B cornu with organ and choir (16) · release: pan pipes (8) · turn G–D (2) ·
A3 in E Dorian, tutti (16) · outro, the aulos alone with the head (8).
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, ornament, phrase
from mix import Mix
from urbs import ROOTS_A, ROOTS_B, THEME_A, THEME_B

D = Mode('D', 'dorian')
E = Mode('E', 'dorian')
CLK = bpm_clock(62, 3, 2)
ROOTS_A_E = "E E D E  E D A B  D G G B  E E D E"


def bars(score, i, j):
    parts = [b for b in score.split('|') if b.strip()]
    return '|'.join(parts[i:j]) + '|'


def organ_line(m, stem, notes, ranks='soft'):
    for ts, td, p, v in notes:
        m.add(stem, ts, I.organ([p], td, v, ranks=ranks, attack=0.04))


def ms(*names):
    return [midi(n) for n in names]


def build():
    m = Mix(88 * CLK.bar_s, tail=6.0)
    t, bs = CLK.t, CLK.bar_s
    kit = A.STD_KIT

    def vo(mode):
        return lambda r: [r, mode.step(r, 4), r + 12, mode.step(r, 2) + 12]

    def cvo(mode):
        return lambda r: [r + 12, mode.step(r, 2) + 12, mode.step(r, 4) + 12]

    def organ_ch(ranks='soft', attack=0.15):
        return lambda x, d, v: I.organ(x, d, v, ranks=ranks, attack=attack)

    choir_ch = lambda x, d, v: A.choir_chord(x, d, v, 'o')   # noqa: E731
    arpD = dict(clock=CLK, mode=D, low='C3', dark=0.55, accents=(0, 3))
    arpE = dict(clock=CLK, mode=E, low='D3', dark=0.55, accents=(0, 3))
    fill = [(0, 'w', .7), (3, 'j', .45), (4, 'j', .5), (5, 'j', .6)]

    # intro: organ and choir from far away, a distant cornu
    m.add('organ', 0, I.organ(ms('D3', 'A3', 'D4'), 4 * bs, 0.55, ranks='flute', attack=4.0))
    m.add('organ', t(4), I.organ(ms('C3', 'G3', 'C4', 'E4'), bs, 0.45, ranks='flute', attack=0.5))
    m.add('organ', t(5), I.organ(ms('D3', 'A3', 'D4'), bs, 0.45, ranks='flute', attack=0.3))
    m.add('choir', t(1), A.choir_chord(ms('D4', 'A4'), 3 * bs, 0.35, 'u'))
    m.line('cornu', I.brass(phrase(CLK, "A3:3 D4:3 | A4:6 |", 2, vel=0.4, shape=False), 'cornu'))
    m.add('lyre_lead', t(4), I.strum(ms('D3', 'A3', 'D4', 'F4', 'A4'), 0.45, length=4.0))

    # A1: the lyre alone with the theme, organ breathing underneath
    for ts, td, p, v in phrase(CLK, THEME_A, 6, sd=0.007, vel=0.75):
        m.add('lyre_lead', ts, I.pluck(p, v, dark=0.35))
        if td > 5 * CLK.unit:
            m.add('lyre_lead', ts + 3 * CLK.unit, I.pluck(p, 0.5 * v, dark=0.45))
            m.add('lyre_lead', ts + 0.02, I.pluck(p - 12, 0.45 * v, dark=0.5))
    A.held(m, 'organ', CLK, 6, ROOTS_A, D, organ_ch('flute', 0.5), low='C3', voicing=vo(D), vel=0.35)
    m.add('crotala', t(6), I.crotala(0.5))

    # A2: aulos over lyre ostinato and drone; the drums arrive
    m.line('aulos', I.aulos(phrase(CLK, THEME_A, 22)))
    A.arpeggio(m, 'lyre_ost', bar0=22, roots=ROOTS_A, pattern=[0, 1, 2, 3, 2, 1], **arpD)
    m.add('drone', t(21.5), I.drone(ms('D3', 'A2'), 17 * bs, 1.5 * bs, 1.0 * bs))
    A.drums(m, 'drums', CLK, 22, 16, [(0, 'b', .75), (3, 't', .38), (5, 't', .22)], kit)

    # B: the cornu takes the second theme, organ and choir open out
    m.line('cornu', I.brass(phrase(CLK, THEME_B, 38, octave=-1, vel=0.8), 'cornu'))
    A.held(m, 'organ', CLK, 38, ROOTS_B, D, organ_ch(), low='C3', voicing=vo(D), vel=0.5)
    A.held(m, 'choir', CLK, 38, ROOTS_B, D, choir_ch, low='C3', voicing=cvo(D), vel=0.5)
    A.arpeggio(m, 'lyre_ost', bar0=38, roots=ROOTS_B, pattern=[0, 2, 4, 3, 2, 1], **arpD)
    A.drums(m, 'drums', CLK, 38, 16, [(0, 'w', .6), (2, 'j', .3), (3, 'b', .55), (5, 'j', .4)], kit, fill=fill)
    m.add('metal', t(38), I.cymbal(0.5))

    # release: pan pipes, lyre only
    for ts, td, p, v in phrase(CLK, bars(THEME_B, 0, 8), 54, vel=0.7):
        m.add('pipes', ts, I.pipes(p, td, v))
    A.arpeggio(m, 'lyre_ost', bar0=54, roots=' '.join(ROOTS_B.split()[:8]), pattern=[0, 1, 2, 3, 2, 1],
               fade=[.8, .75, .7, .7, .7, .7, .75, .8], **arpD)

    # turn: G then D major, swelling into E Dorian
    m.add('organ', t(62), I.organ(ms('G2', 'D3', 'G3', 'B3'), bs, 0.5, attack=0.6))
    m.add('organ', t(63), I.organ(ms('D3', 'A3', 'D4', 'F#4'), bs, 0.6, attack=0.4))
    m.add('choir', t(62), A.choir_chord(ms('G3', 'B3', 'D4'), bs, 0.4, 'a'))
    m.add('choir', t(63), A.choir_chord(ms('F#4', 'A4', 'D4'), bs, 0.5, 'a'))
    A.arpeggio(m, 'lyre_ost', bar0=62, roots="G D", pattern=[0, 1, 2, 3, 4, 5 - 1], **arpE)
    A.drums(m, 'drums', CLK, 62, 1, [(0, 'b', .5), (3, 'b', .55)], kit)
    A.drums(m, 'drums', CLK, 63, 1, [(0, 'b', .6), (2, 'j', .4), (3, 'j', .5), (4, 'j', .6), (5, 'j', .75)], kit)

    # A3: E Dorian, everyone
    organ_line(m, 'organ_lead', phrase(CLK, THEME_A, 64, transpose=2, vel=0.8), ranks='full')
    m.line('cornu_dbl', I.brass(phrase(CLK, THEME_A, 64, transpose=2 - 12, vel=0.65), 'cornu'))
    A.held(m, 'organ', CLK, 64, ROOTS_A_E, E, organ_ch(), low='D3', voicing=vo(E), vel=0.5)
    A.held(m, 'choir', CLK, 64, ROOTS_A_E, E, choir_ch, low='D3', voicing=cvo(E), vel=0.55)
    A.arpeggio(m, 'lyre_ost', bar0=64, roots=ROOTS_A_E, pattern=[0, 1, 2, 3, 4, 2], **arpE)
    A.drums(m, 'drums', CLK, 64, 16, [(0, 'w', .75), (2, 'j', .3), (3, 'b', .6), (5, 'j', .45)], kit, fill=fill)
    m.add('metal', t(64), I.cymbal(0.65, decay=2.6))
    m.add('metal', t(72), I.cymbal(0.45))

    # outro: the aulos alone with the head, the city quiet again
    head = ornament(phrase(CLK, bars(THEME_A, 0, 4), 80, transpose=2, vel=0.6), E, min_dur=2 * CLK.unit * 0.95)
    m.line('aulos', I.aulos(head, vib=0.012))
    A.arpeggio(m, 'lyre_ost', bar0=80, roots="E E D E", pattern=[0, 1, 2, 3, 2, 1], fade=[.8, .7, .6, .55], **arpE)
    A.arpeggio(m, 'lyre_ost', bar0=84, roots="E E", pattern=[0, None, 2, None, 1, None], fade=[.5, .4], **arpE)
    m.add('organ', t(80), I.organ(ms('E3', 'B3', 'E4'), 6 * bs, 0.35, ranks='flute', attack=1.0))
    m.add('lyre_lead', t(86), I.strum(ms('E3', 'B3', 'E4', 'B4', 'E5'), 0.38, spread=0.045, length=5.5, dark=0.5))
    m.add('choir', t(86), A.choir_chord(ms('E4', 'B4'), 1.6 * bs, 0.3, 'u'))
    m.add('crotala', t(86), I.crotala(0.3))
    for b in (22, 38, 54, 64):
        m.add('crotala', t(b), I.crotala(0.6))

    return m, dict(space='temple', wet_db=-9, levels={
        'organ': (-24, 0.0, 0.45), 'organ_lead': (-19, 0.05, 0.4),
        'cornu': (-20, 0.2, 0.45), 'cornu_dbl': (-24, 0.2, 0.4),
        'aulos': (-18, 0.12, 0.3), 'pipes': (-19, 0.18, 0.45),
        'lyre_ost': (-23, -0.3, 0.35), 'lyre_lead': (-19, -0.05, 0.35),
        'drone': (-29, 0.0, 0.4), 'choir': (-27, 0.0, 0.55),
        'drums': (-25, -0.15, 0.22), 'metal': (-32, 0.3, 0.5), 'crotala': (-33, 0.35, 0.55),
    })
