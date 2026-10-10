#!/usr/bin/env python3
"""Render every track and stinger listed in data/defs/music.json to assets/music/<id>.ogg.

    tools/musicgen/.venv/bin/python tools/musicgen/build_all.py [--only id,id] [--review] [--jobs N]

Each id needs a generator module tools/musicgen/tracks/<id>.py whose build()
returns (Mix, render settings). --review also writes spectrograms to
build/music_review/<id>.png.
"""
import argparse
import importlib
import json
import os
import sys
import time
from multiprocessing import Pool

# One render per process: keep numpy/scipy from spawning a thread pool in each.
for _v in ('OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'VECLIB_MAXIMUM_THREADS', 'MKL_NUM_THREADS'):
    os.environ.setdefault(_v, '1')

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'tracks'))

import core  # noqa: E402
import mix as M  # noqa: E402


def entries():
    with open(os.path.join(ROOT, 'data', 'defs', 'music.json')) as f:
        d = json.load(f)
    out = [dict(e) for e in d['tracks']]
    for s in d['stingers']:
        out.append(dict(id=s['id'], title=s['id'], kind='stinger', loop=False))
    return out


def review_png(path, x, title):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
    mono = x.mean(axis=1)
    fig, ax = plt.subplots(2, 1, figsize=(14, 6), gridspec_kw={'height_ratios': [3, 1]})
    ax[0].specgram(mono, NFFT=4096, Fs=core.SR, noverlap=3072, cmap='magma', vmin=-120)
    ax[0].set_ylim(30, 6000)
    ax[0].set_yscale('log')
    ax[0].set_title(title)
    hop = core.SR // 10
    rms = [20 * np.log10(np.sqrt(np.mean(mono[i:i + hop] ** 2)) + 1e-9) for i in range(0, len(mono) - hop, hop)]
    ax[1].plot(np.arange(len(rms)) / 10, rms)
    ax[1].set_ylim(-60, 0)
    ax[1].set_xlim(0, len(mono) / core.SR)
    plt.tight_layout()
    plt.savefig(path, dpi=55)
    plt.close(fig)


def render_one(args):
    e, review = args
    t0 = time.time()
    core.seed(e['id'])
    mod = importlib.import_module(e['id'])
    mix, settings = mod.build()
    settings.setdefault('loudness', M.LOUDNESS[e['kind']])
    x, stats = mix.render(loop=e.get('loop', False), **settings)
    out = os.path.join(ROOT, 'assets', 'music', e['id'] + '.ogg')
    M.write_ogg(out, x, e['title'])
    if review:
        d = os.path.join(ROOT, 'build', 'music_review')
        os.makedirs(d, exist_ok=True)
        review_png(os.path.join(d, e['id'] + '.png'), x, e['title'])
    stats['render_s'] = round(time.time() - t0, 1)
    return e['id'], stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', default='')
    ap.add_argument('--review', action='store_true')
    ap.add_argument('--jobs', type=int, default=os.cpu_count())
    a = ap.parse_args()
    todo = entries()
    if a.only:
        want = set(a.only.split(','))
        todo = [e for e in todo if e['id'] in want]
    missing = [e['id'] for e in todo if not os.path.exists(os.path.join(HERE, 'tracks', e['id'] + '.py'))]
    if missing:
        sys.exit(f"no generator for: {', '.join(missing)}")
    os.makedirs(os.path.join(ROOT, 'assets', 'music'), exist_ok=True)
    total = 0.0
    with Pool(min(a.jobs, len(todo))) as pool:
        for tid, s in pool.imap_unordered(render_one, [(e, a.review) for e in todo]):
            total += s['seconds']
            flag = '  LIMITING' if s['limited_pct'] > 0.3 else ''
            print(f"{tid:16s} {s['seconds']:6.1f}s  {s['loudness']:6.1f} dB  peak {s['peak']:.3f}  "
                  f"limited {s['limited_pct']:.2f}%  ({s['render_s']}s){flag}", flush=True)
    print(f"total {total / 60:.1f} min")


if __name__ == '__main__':
    main()
