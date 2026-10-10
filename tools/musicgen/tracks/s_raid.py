"""s_raid — raiders sighted (~4 s).

Two carnyces blast a fifth apart, sliding up into E and falling to the
Phrygian F-E, over three war-drum strokes and a final heavy one.
"""
import instruments as I
from core import midi
from loopkit import faded
from mix import Mix


def build():
    m = Mix(3.6, tail=2.5)
    m.line('carnyx', I.brass([(0.05, 1.3, midi('E3'), 0.95), (1.45, 0.3, midi('F3'), 0.8),
                              (1.8, 1.1, midi('E3'), 0.85)], 'carnyx'))
    m.line('carnyx_low', I.brass([(0.08, 1.3, midi('A2'), 0.85), (1.8, 1.1, midi('A2'), 0.8)], 'carnyx'))
    for t, v in ((0.0, 0.9), (0.4, 0.75), (0.8, 0.85), (1.8, 1.0)):
        m.add('war', t, faded(I.war_drum(v, pitch=42, decay=0.6)))
    return m, dict(space='field', levels={
        'carnyx': (-18, -0.2, 0.4), 'carnyx_low': (-22, 0.2, 0.4), 'war': (-20, 0.0, 0.3),
    })
