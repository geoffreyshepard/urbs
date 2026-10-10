"""Arrangement helpers: chord roots, arpeggio ostinati, drum patterns, held chords."""
import core as C
import instruments as I
from core import humanize, midi


def root_midi(name, low):
    """Pitch with letter `name` (e.g. 'Bb') in [low, low + 12)."""
    m = midi(name + '4')
    while m >= low + 12:
        m -= 12
    while m < low:
        m += 12
    return m


def default_voicing(mode):
    return lambda r: [r, mode.step(r, 4), r + 12, mode.step(r, 2) + 12, mode.step(r, 4) + 12]


def arpeggio(mix, stem, clock, bar0, roots, mode, pattern, step=1, low='C3', vel=0.7,
             accents=(0,), kind='lyre', dark=0.55, length=3.2, voicing=None, sd=0.006,
             fade=None):
    """Plucked ostinato. roots: 'D D C -' (one per bar, '-' = silent bar).

    pattern: chord-tone index per step (None = rest); step: score units per entry.
    fade: optional per-bar velocity multipliers.
    """
    vfn = voicing or default_voicing(mode)
    for i, root in enumerate(roots.split()):
        if root == '-':
            continue
        vox = vfn(root_midi(root, midi(low)))
        f = fade[i] if fade else 1.0
        for j, idx in enumerate(pattern):
            if idx is None:
                continue
            u = j * step
            acc = 1.0 if u % clock.bar in accents else 0.72
            v = vel * f * acc * (1 - 0.05 * idx) * (1 + C.rng.normal(0, 0.07))
            mix.add(stem, humanize(clock.t(bar0 + i, u), sd),
                    I.pluck(vox[idx], v, length=length, dark=dark, kind=kind))


def drums(mix, stem, clock, bar0, nbars, hits, kit, vel=1.0, sd=0.005, fill=None,
          fill_every=4, fade=None):
    """hits: [(unit, kit_key, vel)] repeated each bar; fill replaces every Nth bar."""
    for i in range(nbars):
        pat = fill if (fill and i % fill_every == fill_every - 1) else hits
        f = fade[i] if fade else 1.0
        for u, key, v in pat:
            vv = vel * f * v * (1 + C.rng.normal(0, 0.08))
            mix.add(stem, humanize(clock.t(bar0 + i, u), sd), kit[key](vv))


def held(mix, stem, clock, bar0, roots, mode, fn, low='C3', bars_per=1, voicing=None, vel=0.6):
    """Sustained chords, one per `bars_per` bars. fn(midis, dur, vel) -> signal."""
    vfn = voicing or (lambda r: mode.triad(r))
    dur = clock.bar_s * bars_per
    for i, root in enumerate(roots.split()):
        if root == '-':
            continue
        mix.add(stem, clock.t(bar0 + i * bars_per), fn(vfn(root_midi(root, midi(low))), dur, vel))


def choir_chord(midis, dur, vel=0.5, vowel='a'):
    out = None
    for m in midis:
        s = I.choir(m, dur, vel / len(midis) ** 0.5, vowel)
        out = s if out is None else out + s
    return out


STD_KIT = {
    'b': lambda v: I.boom(v),
    'B': lambda v: I.boom(v, pitch=52, decay=0.35),
    't': lambda v: I.tek(v),
    'j': lambda v: I.tek(v, jingle=0.6),
    'w': lambda v: I.war_drum(v),
    'W': lambda v: I.war_drum(v, pitch=38, decay=0.8),
    'r': lambda v: I.riq(v),
    's': lambda v: I.shake(v),
    'x': lambda v: I.sistrum(v),
    'c': lambda v: I.clap(v),
    'y': lambda v: I.cymbal(v),
}
