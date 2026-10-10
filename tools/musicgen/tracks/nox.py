"""Nox — night and winter.

E Aeolian, 4/4, quarter = 54. Very sparse: a low ney, slow harp, and a quiet
flue-pipe pad in a stone temple, with long rests between phrases.

Form: pad (2) · ney I (4) · rest (2) · ney II (4) · harp alone (3) · harp
song (4) · pad only (3) · ney III (4) · harp/ney dialogue (4) · close (4).
"""
import arrange as A
import instruments as I
from core import Mode, butter, bpm_clock, midi, phrase
from mix import Mix

MODE = Mode('E', 'aeolian')
CLK = bpm_clock(54, 2, 4)   # eighths, 8 per bar

NEY_1 = "B4:4 A4:2 G4:2 | E4:6 r:2 | G4:3 A4 B4:2 D5:2 | B4:8"
NEY_2 = "C5:4 B4:2 A4:2 | F#4:4 G4:2 A4:2 | B4:3 A4 G4:2 F#4:2 | E4:8"
HARP_SONG = "E5:3 B4 G4:4 | F#4:2 G4:2 B4:4 | C5:3 B4 A4:4 | B4:8"
NEY_3 = "E4:4 G4:2 B4:2 | A4:6 r:2 | G4:3 F#4 E4:2 D4:2 | E4:8"
NEY_4 = "B4:6 r:2 | r:8 | A4:4 G4:2 F#4:2 | E4:8"
HARP_4 = "r:4 B4:2 G4:2 | E4:4 r:4 | r:8 | r:8"

BROKEN = [0, 1, 3, None]   # one pluck every quarter, step 2


def ney(notes):
    """Ney line; highpassed because its breath noise rides the pitch down to DC."""
    t0, sig = I.flute(notes, kind='ney')
    return t0, butter(sig, 'high', 200)


def pad(midis, dur, vel):
    return I.organ(midis, dur, vel, ranks='flute', attack=0.9)


def harp_line(m, score, bar0, vel=0.6):
    for ts, td, p, v in phrase(CLK, score, bar0, vel=vel, sd=0.012):
        m.add('harp', ts, I.pluck(p, v, kind='harp', dark=0.45, length=5.0))


def build():
    m = Mix(34 * CLK.bar_s, tail=7.0)
    t = CLK.t
    arp = dict(clock=CLK, mode=MODE, low='C3', kind='harp', dark=0.55, length=5.0, step=2, sd=0.012)
    pd = dict(clock=CLK, mode=MODE, fn=pad, low='E3', vel=0.45)

    A.held(m, 'organ', bar0=0, roots="E", bars_per=2, **pd)
    m.add('harp_low', t(1, 4), I.pluck(midi('B3'), 0.35, kind='harp', dark=0.5, length=5))

    m.line('ney', ney(phrase(CLK, NEY_1, 2, vel=0.75)))
    A.arpeggio(m, 'harp_low', bar0=2, roots="E C G B", pattern=BROKEN, vel=0.42, **arp)
    A.held(m, 'organ', bar0=2, roots="E C G B", **pd)

    m.add('harp_low', t(6, 2), I.pluck(midi('E3'), 0.3, kind='harp', dark=0.5, length=5))
    m.add('harp_low', t(7, 0), I.pluck(midi('B3'), 0.25, kind='harp', dark=0.5, length=5))

    m.line('ney', ney(phrase(CLK, NEY_2, 8, vel=0.75)))
    A.arpeggio(m, 'harp_low', bar0=8, roots="A D G E", pattern=BROKEN, vel=0.42, **arp)
    A.held(m, 'organ', bar0=8, roots="A D G E", **pd)

    harp_line(m, "r:4 E4:4 | r:8 | B3:4 r:4", 12, vel=0.4)
    m.add('organ', t(12), pad([midi('E3'), midi('B3')], 2.6 * CLK.bar_s, 0.3))   # keep the air alive
    harp_line(m, HARP_SONG, 15, vel=0.6)
    A.arpeggio(m, 'harp_low', bar0=15, roots="E G A B", pattern=[0, None, 1, None], vel=0.35, **arp)

    A.held(m, 'organ', bar0=19, roots="E C E", **pd)

    m.line('ney', ney(phrase(CLK, NEY_3, 22, vel=0.72)))
    A.arpeggio(m, 'harp_low', bar0=22, roots="E A C E", pattern=BROKEN, vel=0.4, **arp)
    A.held(m, 'organ', bar0=22, roots="E A C E", **pd)

    m.line('ney', ney(phrase(CLK, NEY_4, 26, vel=0.6)))
    harp_line(m, HARP_4, 26, vel=0.5)
    A.held(m, 'organ', bar0=26, roots="E - A E", **pd)

    m.add('harp', t(30), I.strum([midi(n) for n in ('E3', 'B3', 'E4', 'G4', 'B4')], 0.4,
                                 spread=0.12, length=7.0, kind='harp', dark=0.5))
    m.add('harp', t(32), I.pluck(midi('E4'), 0.25, kind='harp', dark=0.5, length=6))
    m.add('organ', t(30), pad([midi('E3'), midi('B3'), midi('G4')], 3.5 * CLK.bar_s, 0.4))

    return m, dict(space='temple', wet_db=-6, levels={
        'ney': (-19, 0.1, 0.5), 'harp': (-20, -0.15, 0.5), 'harp_low': (-24, -0.3, 0.5),
        'organ': (-29, 0.15, 0.55),
    })
