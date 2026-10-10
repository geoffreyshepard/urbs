"""Mission start stinger: a lyre strum, then the Urbs head on pan pipes."""
import instruments as I
from core import bpm_clock, midi, phrase
from mix import Mix

CLK = bpm_clock(72, 3, 2)
HEAD = "A4:3 G4 F4 G4 | A4:2 D5 C5:3 | D5:6 |"


def ms(*names):
    return [midi(n) for n in names]


def build():
    m = Mix(6.0, tail=3.5)
    t = CLK.t
    off = 0.35
    m.add('lyre', 0, I.strum(ms('D3', 'A3', 'D4', 'F4', 'A4'), 0.6, spread=0.05, length=4.0))
    for ts, td, p, v in phrase(CLK, HEAD, 0, vel=0.8, sd=0.003):
        m.add('pipes', ts + off, I.pipes(p, td if p != midi('D5') else 2.2, v))
    m.add('lyre', t(2) + off, I.strum(ms('D3', 'A3', 'D4', 'A4', 'D5'), 0.5, spread=0.05, length=4.5, dark=0.45))
    m.add('drone', 0, I.drone(ms('D3', 'A2'), 6.0, 0.8, 1.5, vel=0.5))
    m.add('crotala', t(2) + off, I.crotala(0.5))
    return m, dict(space='courtyard', levels={
        'lyre': (-20, -0.2, 0.4), 'pipes': (-18, 0.15, 0.45), 'drone': (-28, 0.0, 0.4),
        'crotala': (-32, 0.35, 0.55),
    })
