"""Urbs Aeterna — main menu theme. The Urbs leitmotif, grand but warm.

D Dorian (fanfares brighten to D major), 6/8, dotted quarter = 60.
Intro: hydraulis swell + cornu fanfare (6) · A1 aulos over lyre and drone (16) ·
A2 organ + cornu with choir pad and war drum (16) · B pan pipes breath (16) ·
final statement, second half of the theme, tutti (8) · fanfare cadence (4).
"""
import arrange as A
import instruments as I
from core import Mode, bpm_clock, midi, phrase
from mix import Mix
from urbs import ROOTS_A, ROOTS_B, THEME_A, THEME_B

MODE = Mode('D', 'dorian')
CLK = bpm_clock(60, 3, 2)

FANFARE = "A3 D4:2 F#4 A4:2 | A4:3 F#4 D4 F#4 | D4:6 |"
CADENCE_HI = "A3 D4:2 F#4 A4:2 | A4:3 F#4 D4 A4 |"
CADENCE_LO = "D3 A3:2 D4 F#4:2 | F#4:3 D4 A3 F#4 |"


def bars(score, i, j):
    parts = [b for b in score.split('|') if b.strip()]
    return '|'.join(parts[i:j]) + '|'


def organ_line(m, stem, notes, ranks='soft'):
    for ts, td, p, v in notes:
        m.add(stem, ts, I.organ([p], td, v, ranks=ranks, attack=0.04))


def ms(*names):
    return [midi(n) for n in names]


def build():
    m = Mix(66 * CLK.bar_s, tail=5.5)
    t, bs = CLK.t, CLK.bar_s
    vo = lambda r: [r, MODE.step(r, 4), r + 12, MODE.step(r, 2) + 12]           # noqa: E731
    cvo = lambda r: [r + 12, MODE.step(r, 2) + 12, MODE.step(r, 4) + 12]        # noqa: E731
    organ_ch = lambda x, d, v: I.organ(x, d, v, attack=0.15)                    # noqa: E731
    choir_ch = lambda x, d, v: A.choir_chord(x, d, v, 'o')                      # noqa: E731
    arp = dict(clock=CLK, mode=MODE, low='C3', dark=0.55)
    kit = A.STD_KIT
    fill = [(0, 'w', .7), (3, 'j', .45), (4, 'j', .5), (5, 'j', .6)]

    # intro: the hydraulis swells in and the cornu calls
    m.add('organ', 0, I.organ(ms('D3', 'A3', 'D4'), 5 * bs, 0.7, attack=3.0))
    m.add('organ', t(5), I.organ(ms('C3', 'G3', 'C4', 'E4'), bs, 0.6, attack=0.3))
    m.line('cornu', I.brass(phrase(CLK, FANFARE, 2, vel=0.75, shape=False), 'cornu'))
    A.arpeggio(m, 'lyre_ost', bar0=4, roots="D C", pattern=[0, 1, 2, 3, 2, 1], accents=(0, 3),
               fade=[0.8, 0.9], **arp)
    m.add('crotala', t(2), I.crotala(0.5))

    # A1: aulos over lyre and drone
    m.add('drone', t(5.5), I.drone(ms('D3', 'A2'), 17 * bs, bs, 1.5 * bs))
    A.arpeggio(m, 'lyre_ost', bar0=6, roots=ROOTS_A, pattern=[0, 1, 2, 3, 2, 1], accents=(0, 3), **arp)
    m.line('aulos', I.aulos(phrase(CLK, THEME_A, 6)))
    A.drums(m, 'drums', CLK, 6, 16, [(0, 'b', .8), (3, 't', .4), (5, 't', .22)], kit)

    # A2: organ and cornu carry the theme; choir pad; war drum
    organ_line(m, 'organ_lead', phrase(CLK, THEME_A, 22, vel=0.75))
    m.line('cornu_dbl', I.brass(phrase(CLK, THEME_A, 22, octave=-1, vel=0.6), 'cornu'))
    A.held(m, 'organ', CLK, 22, ROOTS_A, MODE, organ_ch, low='C3', voicing=vo, vel=0.5)
    A.held(m, 'choir', CLK, 22, ROOTS_A, MODE, choir_ch, low='C3', voicing=cvo, vel=0.5)
    A.arpeggio(m, 'lyre_ost', bar0=22, roots=ROOTS_A, pattern=[0, 1, 2, 3, 4, 2], accents=(0, 3), **arp)
    A.drums(m, 'drums', CLK, 22, 16, [(0, 'w', .6), (2, 'j', .3), (3, 'b', .6), (5, 'j', .4)], kit, fill=fill)

    # B: a breath — pan pipes and lyre, the drone creeping back in
    for ts, td, p, v in phrase(CLK, THEME_B, 38, vel=0.8):
        m.add('pipes', ts, I.pipes(p, td, v))
    A.arpeggio(m, 'lyre_ost', bar0=38, roots=ROOTS_B, pattern=[0, 2, 4, 3, 2, 1], accents=(0, 3), **arp)
    A.drums(m, 'drums', CLK, 38, 16, [(0, 'b', .5), (3, 't', .25)], kit)
    m.add('drone', t(48), I.drone(ms('D3', 'A2'), 14 * bs, 4 * bs, 1.5 * bs, vel=0.5))

    # final statement: the second half of the theme, tutti
    fin = bars(THEME_A, 8, 16)
    rf = "C F F A D D C D"
    organ_line(m, 'organ_lead', phrase(CLK, fin, 54, vel=0.82))
    m.line('cornu_dbl', I.brass(phrase(CLK, fin, 54, octave=-1, vel=0.68), 'cornu'))
    A.held(m, 'organ', CLK, 54, rf, MODE, organ_ch, low='C3', voicing=vo, vel=0.55)
    A.held(m, 'choir', CLK, 54, rf, MODE, choir_ch, low='C3', voicing=cvo, vel=0.55)
    A.arpeggio(m, 'lyre_ost', bar0=54, roots=rf, pattern=[0, 1, 2, 3, 4, 2], accents=(0, 3), **arp)
    A.drums(m, 'drums', CLK, 54, 8, [(0, 'w', .75), (2, 'j', .3), (3, 'b', .6), (5, 'j', .45)], kit, fill=fill)
    m.add('metal', t(54), I.cymbal(0.55))

    # fanfare cadence onto D major
    hi = phrase(CLK, CADENCE_HI, 62, vel=0.85, shape=False) + [(t(64), 2.1 * bs, midi('D5'), 0.85)]
    lo = phrase(CLK, CADENCE_LO, 62, vel=0.7, shape=False) + [(t(64), 2.1 * bs, midi('A4'), 0.7)]
    m.line('cornu', I.brass(hi, 'cornu'))
    m.line('buccina', I.brass(lo, 'buccina'))
    m.add('organ', t(62), I.organ(ms('D3', 'A3', 'D4'), 2 * bs, 0.6, attack=0.1))
    m.add('organ', t(64), I.organ(ms('D3', 'A3', 'D4', 'F#4', 'A4'), 2.1 * bs, 0.75, ranks='full', attack=0.06))
    m.add('choir', t(64), A.choir_chord(ms('D4', 'F#4', 'A4'), 2.1 * bs, 0.6, 'a'))
    m.add('lyre_lead', t(64), I.strum(ms('D3', 'A3', 'D4', 'F#4', 'A4', 'D5'), 0.5, length=5.0, dark=0.45))
    A.drums(m, 'drums', CLK, 62, 1, [(0, 'w', .7), (3, 'b', .5)], kit)
    A.drums(m, 'drums', CLK, 63, 1, [(0, 'b', .6), (3, 'j', .45), (4, 'j', .55), (5, 'j', .7)], kit)
    m.add('drums', t(64), I.war_drum(1.0, pitch=38, decay=0.8))
    m.add('metal', t(64), I.cymbal(0.7, decay=2.8))

    for b in (6, 22, 38, 46, 64):
        m.add('crotala', t(b), I.crotala(0.7 if b != 64 else 0.5))

    return m, dict(space='hall', levels={
        'organ': (-22, 0.0, 0.45), 'organ_lead': (-19, 0.05, 0.4),
        'cornu': (-20, 0.25, 0.45), 'cornu_dbl': (-24, 0.2, 0.4), 'buccina': (-23, -0.25, 0.45),
        'aulos': (-18, 0.12, 0.3), 'pipes': (-19, 0.18, 0.45),
        'lyre_ost': (-23, -0.3, 0.35), 'lyre_lead': (-23, -0.1, 0.35),
        'drone': (-29, 0.0, 0.4), 'choir': (-28, 0.0, 0.55),
        'drums': (-25, -0.15, 0.25), 'metal': (-32, 0.3, 0.5), 'crotala': (-33, 0.35, 0.55),
    })
