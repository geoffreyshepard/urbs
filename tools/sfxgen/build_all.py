#!/usr/bin/env python3
"""Render every sound file named in data/defs/sfx.json to assets/sfx/<file>.ogg.

    tools/sfxgen/.venv/bin/python tools/sfxgen/build_all.py [--only file,file] [--check] [--review]

A file id is <sound>_<variant> (click_2); sounds.SOUNDS[<sound>](variant) synthesizes it.
After encoding, every file is decoded again and checked: 44.1 kHz, peak below 0.95, no DC,
silent edges (no clicks), and for the ambient loop a seamless seam. --check only runs the
checks on the files already in assets/sfx. --review writes a spectrogram and level curve per
file to build/sfx_review/<file>.png.
"""
import argparse
import json
import os
import sys
import time
from multiprocessing import Pool

for _v in ('OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'VECLIB_MAXIMUM_THREADS', 'MKL_NUM_THREADS'):
    os.environ.setdefault(_v, '1')

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'tools', 'musicgen'))

import numpy as np  # noqa: E402

import core  # noqa: E402
import fx  # noqa: E402
import sounds  # noqa: E402

OUT = os.path.join(ROOT, 'assets', 'sfx')


def manifest():
    with open(os.path.join(ROOT, 'data', 'defs', 'sfx.json')) as f:
        d = json.load(f)
    files = []
    for e in d['events']:
        files += e['files']
    loops = [d['ambient']['file']] if d.get('ambient') else []
    return sorted(set(files) | set(loops)), set(loops)


def split(fid):
    name, _, v = fid.rpartition('_')
    return name, int(v)


def render_one(fid):
    t0 = time.time()
    core.seed('sfx/' + fid)
    name, v = split(fid)
    x, settings = sounds.SOUNDS[name](v)
    x, stats = fx.finish(x, **settings)
    fx.write_ogg(os.path.join(OUT, fid + '.ogg'), x, fid, quality=2 if settings.get('loop') else 3)
    stats['render_s'] = round(time.time() - t0, 1)
    return fid, stats


def check(fid, loop):
    """Decode the encoded file and look for what a listener would hear as a fault."""
    x, rate = fx.read_ogg(os.path.join(OUT, fid + '.ogg'))
    problems = []
    if rate != core.SR:
        problems.append(f'{rate} Hz')
    peak = float(np.abs(x).max())
    if peak > 0.95:
        problems.append(f'peak {peak:.3f}')
    dc = float(np.abs(x.mean(axis=0)).max())
    if dc > 0.002:
        problems.append(f'DC {dc:.4f}')
    rms = float(np.sqrt(np.mean(x ** 2)))
    if loop:
        # The jump from the last frame to the first should look like any other step in the file.
        seam = float(np.abs(x[0] - x[-1]).max())
        typical = float(np.percentile(np.abs(np.diff(x, axis=0)), 99.9))
        if seam > 2 * typical:
            problems.append(f'loop seam {seam:.4f} (typical step {typical:.4f})')
    else:
        edge = max(float(np.abs(x[:4]).max()), float(np.abs(x[-16:]).max()))
        if edge > 0.01:
            problems.append(f'edge {edge:.4f}')
    # Brightness: the spectral centroid and the share of energy above 6 kHz (harsh when large).
    mono = x.mean(axis=1)
    spec = np.abs(np.fft.rfft(mono)) ** 2
    freqs = np.fft.rfftfreq(len(mono), 1 / rate)
    centroid = float(np.sum(freqs * spec) / (np.sum(spec) + 1e-20))
    high = float(np.sum(spec[freqs > 6000]) / (np.sum(spec) + 1e-20))
    if high > 0.15:
        problems.append(f'harsh: {high * 100:.0f}% of the energy above 6 kHz')
    return dict(channels=x.shape[1], seconds=round(len(x) / rate, 2), peak=round(peak, 3),
                rms_db=round(20 * np.log10(rms + 1e-12), 1), dc=round(dc, 5), centroid=round(centroid),
                high_pct=round(high * 100, 1),
                kb=round(os.path.getsize(os.path.join(OUT, fid + '.ogg')) / 1024, 1)), problems, x


def review_png(path, x, title):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    mono = x.mean(axis=1)
    fig, ax = plt.subplots(2, 1, figsize=(10, 5), gridspec_kw={'height_ratios': [3, 1]})
    ax[0].specgram(mono, NFFT=1024, Fs=core.SR, noverlap=768, cmap='magma', vmin=-130)
    ax[0].set_ylim(30, 16000)
    ax[0].set_yscale('log')
    ax[0].set_title(title)
    hop = core.SR // 100
    rms = [20 * np.log10(np.sqrt(np.mean(mono[i:i + hop] ** 2)) + 1e-9) for i in range(0, len(mono) - hop, hop)]
    ax[1].plot(np.arange(len(rms)) / 100, rms)
    ax[1].set_ylim(-80, 0)
    ax[1].set_xlim(0, len(mono) / core.SR)
    plt.tight_layout()
    plt.savefig(path, dpi=60)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', default='')
    ap.add_argument('--check', action='store_true', help='only check the existing files')
    ap.add_argument('--review', action='store_true', help='spectrograms in build/sfx_review/')
    ap.add_argument('--jobs', type=int, default=os.cpu_count())
    a = ap.parse_args()
    todo, loops = manifest()
    if a.only:
        want = set(a.only.split(','))
        todo = [f for f in todo if f in want]
    missing = [f for f in todo if split(f)[0] not in sounds.SOUNDS]
    if missing:
        sys.exit(f"no generator for: {', '.join(missing)}")
    os.makedirs(OUT, exist_ok=True)
    if not a.check:
        with Pool(min(a.jobs, len(todo))) as pool:
            for fid, s in pool.imap_unordered(render_one, todo):
                print(f"{fid:12s} {s['seconds']:5.2f}s  {s['loudness']:6.1f} dB  peak {s['peak']:.3f}  "
                      f"({s['render_s']}s)", flush=True)
    bad, total = 0, 0.0
    for fid in todo:
        s, problems, x = check(fid, fid in loops)
        if a.review:
            d = os.path.join(ROOT, 'build', 'sfx_review')
            os.makedirs(d, exist_ok=True)
            review_png(os.path.join(d, fid + '.png'), x, fid)
        total += s['kb']
        flag = '  ' + '; '.join(problems) if problems else ''
        bad += bool(problems)
        print(f"check {fid:12s} {s['channels']}ch {s['seconds']:5.2f}s  peak {s['peak']:.3f}  "
              f"rms {s['rms_db']:6.1f} dB  dc {s['dc']:.5f}  centroid {s['centroid']:5d} Hz  "
              f">6k {s['high_pct']:4.1f}%  {s['kb']:6.1f} KB{flag}")
    print(f"{len(todo)} files, {total / 1024:.2f} MB, {bad} with problems")
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
