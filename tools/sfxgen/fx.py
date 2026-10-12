"""Sound-effect building blocks and the finishing chain (room, loudness, fades, OGG).

Shares the sample rate, RNG, filters and room impulse responses with tools/musicgen
(core.py, mix.py), so effects and music sit in the same acoustic world.
"""
import os
import subprocess
import tempfile
import wave

import numpy as np
from scipy import signal

import core as C
import mix as M
from core import SR, butter


def secs(n_s):
    return int(round(n_s * SR))


def tvec(n):
    return np.arange(n) / SR


# ---------------------------------------------------------------- sources
def modal(n, modes, phase=True):
    """Damped sines: modes = [(freq Hz, amplitude, decay s)]. Wood, stone, metal, bronze."""
    t = tvec(n)
    out = np.zeros(n)
    for f, a, d in modes:
        if f >= 0.45 * SR:
            continue
        ph = C.rng.uniform(0, 2 * np.pi) if phase else 0.0
        out += a * np.sin(2 * np.pi * f * t + ph) * np.exp(-t / d)
    return out


def burst(n, lo, hi, tau, order=2):
    """A noise transient: band-limited noise with an exponential decay."""
    t = tvec(n)
    x = C.noise(n)
    if lo and hi:
        x = butter(x, 'band', [lo, hi], order)
    elif hi:
        x = butter(x, 'low', hi, order)
    elif lo:
        x = butter(x, 'high', lo, order)
    x = x / (np.std(x[:max(64, secs(tau * 3))]) + 1e-12)
    return x * np.exp(-t / tau)


def thud(n, pitch, decay, drop=0.8):
    """A soft low impact: a sine whose pitch falls quickly (earth, wood, a heavy block)."""
    t = tvec(n)
    f = pitch * (1 + drop * np.exp(-t / 0.02))
    ph = 2 * np.pi * np.cumsum(f) / SR
    return np.sin(ph) * np.exp(-t / decay) * np.clip(t / 0.0015, 0, 1)


def add(buf, start_s, sig, gain=1.0, wrap=False):
    """Mix sig into buf at start_s. wrap=True places it circularly (for loops)."""
    s = secs(start_s)
    if wrap:
        idx = (s + np.arange(len(sig))) % len(buf)
        np.add.at(buf, idx, gain * sig)
    else:
        C.place(buf, start_s, sig, gain)


def grains(n, rate, dur, lo, hi, env=None, amp_sd=0.6, tau=(0.0008, 0.004)):
    """Granular crunch: `rate` tiny noise bursts per second (gravel, debris dust, papyrus, crackle)."""
    out = np.zeros(n)
    total = int(rate * dur)
    starts = np.sort(C.rng.uniform(0, dur, total))
    for st in starts:
        g = int(C.rng.uniform(lo, hi))
        ln = secs(0.02)
        b = burst(ln, max(80, g * 0.6), min(g * 1.6, 0.45 * SR), C.rng.uniform(*tau), 1)
        a = np.exp(np.clip(C.rng.normal(0, amp_sd), -2.5, 0.9))  # no lone grain towers over the rest
        if env is not None:
            a *= env[min(n - 1, secs(st))]
        add(out, st, b, a)
    return out


def periodic_noise(n, shape):
    """Noise whose spectrum follows shape(freqs) and that repeats exactly every n samples."""
    f = np.fft.rfftfreq(n, 1 / SR)
    mag = shape(f)
    ph = C.rng.uniform(0, 2 * np.pi, len(f))
    x = np.fft.irfft(mag * np.exp(1j * ph), n)
    return x / (np.std(x) + 1e-12)


# ---------------------------------------------------------------- finishing
def room(x, space, wet_db, circular=False):
    """Mono or stereo signal through one of musicgen's rooms (mix.SPACES); returns the same shape.

    circular=True convolves around the end of the buffer, so a loop stays seamless.
    """
    ir = M.room_ir(space)
    stereo = x.ndim == 2
    src = x if stereo else x[:, None]
    n = len(src)
    wet = np.zeros_like(src)
    for ch in range(src.shape[1]):
        h = ir[:, ch % 2]
        if circular:
            ln = max(n, len(h))
            y = np.fft.irfft(np.fft.rfft(src[:, ch], 2 * ln) * np.fft.rfft(h, 2 * ln), 2 * ln)
            y = y[:n + len(h)]
            folded = y[:n].copy()
            tail = y[n:]
            while len(tail):
                k = min(n, len(tail))
                folded[:k] += tail[:k]
                tail = tail[k:]
            wet[:, ch] = folded
        else:
            wet[:, ch] = signal.oaconvolve(src[:, ch], h)[:n]
    wet *= 10 ** (wet_db / 20) * np.sqrt(np.mean(src ** 2)) / (np.sqrt(np.mean(wet ** 2)) + 1e-12)
    out = src + wet
    return out if stereo else out[:, 0]


def momentary_db(x, win=0.1):
    """Loudest 100 ms RMS (dBFS): how loud a short effect feels at its peak."""
    mono = x.mean(axis=1) if x.ndim == 2 else x
    w = max(1, secs(win))
    if len(mono) <= w:
        return 10 * np.log10(np.mean(mono ** 2) + 1e-20)
    c = np.cumsum(np.concatenate([[0.0], mono ** 2]))
    ms = (c[w:] - c[:-w]) / w
    return 10 * np.log10(ms.max() + 1e-20)


PEAK = 0.85  # sample peak ceiling before encoding (Vorbis overshoots a little)


def finish(x, loudness=-16.0, space=None, wet_db=-14.0, loop=False, fade_out=0.08, lowpass=11000, max_s=None):
    """Room, loudness, peak ceiling, trimmed tail and click-free edges. Returns (signal, stats).

    A one-shot is cut where its tail falls 60 dB below the peak, or at max_s, then faded out over
    fade_out seconds.
    """
    x = np.asarray(x, dtype=np.float64)
    hp = lambda y: np.stack([butter(y[:, c], 'high', 35) for c in range(y.shape[1])], 1) if y.ndim == 2 \
        else butter(y, 'high', 35)
    if not loop:
        x = np.concatenate([x, np.zeros((secs(1.5),) + x.shape[1:])])  # room for the reverb tail
    if space:
        x = room(x, space, wet_db, circular=loop)
    lp = lambda y: np.stack([butter(y[:, c], 'low', lowpass) for c in range(y.shape[1])], 1) if y.ndim == 2 \
        else butter(y, 'low', lowpass)
    if loop:
        # Filters run over two periods and keep the second, so the seam stays continuous.
        x = hp(lp(np.concatenate([x, x])) if lowpass else np.concatenate([x, x]))[len(x):]
    else:
        x = hp(lp(x) if lowpass else x)
    # One-shots by their loudest moment; a loop by its average (it has no attack to speak of).
    level = 10 * np.log10(np.mean(x ** 2) + 1e-20) if loop else momentary_db(x)
    x *= 10 ** ((loudness - level) / 20)
    peak = float(np.abs(x).max())
    if peak > PEAK:
        x *= PEAK / peak
    if not loop:
        mono = np.abs(x).max(axis=1) if x.ndim == 2 else np.abs(x)
        last = int(np.nonzero(mono > mono.max() * 10 ** (-60 / 20))[0][-1]) + secs(0.02)
        x = x[:min(last, len(x), secs(max_s) if max_s else len(x))]
        f = min(secs(fade_out), len(x) // 3)
        ramp = 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, f))
        x[-f:] *= ramp[:, None] if x.ndim == 2 else ramp
        k = secs(0.002)
        ramp = 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, k))
        x[:k] *= ramp[:, None] if x.ndim == 2 else ramp
        # 6 ms of silence first: Vorbis smears a sharp attack backwards (pre-echo) by up to a short
        # block, and this keeps that smear off the first samples.
        x = np.concatenate([np.zeros((secs(0.006),) + x.shape[1:]), x])
    if loop:
        x -= x.mean(axis=0)  # any DC left after the high-pass (a one-shot keeps its silent edges)
    level = 10 * np.log10(np.mean(x ** 2) + 1e-20) if loop else momentary_db(x)
    stats = dict(seconds=round(len(x) / SR, 2), loudness=round(level, 1),
                 peak=round(float(np.abs(x).max()), 3), dc=round(float(np.abs(x.mean(axis=0)).max()), 6))
    return x, stats


def write_ogg(path, x, title, quality=3):
    """Mono or stereo Vorbis via ffmpeg, with musicgen's bitexact flags (byte-identical re-renders)."""
    ch = 2 if x.ndim == 2 else 1
    pcm = (np.clip(x, -1, 1) * 32767).astype('<i2')
    with tempfile.TemporaryDirectory() as td:
        wav = os.path.join(td, 'x.wav')
        with wave.open(wav, 'wb') as w:
            w.setnchannels(ch)
            w.setsampwidth(2)
            w.setframerate(SR)
            w.writeframes(pcm.tobytes())
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', wav,
                        '-fflags', '+bitexact', '-flags:a', '+bitexact',
                        '-c:a', 'libvorbis', '-q:a', str(quality),
                        '-metadata', f'title={title}', '-metadata', 'artist=URBS (sfxgen)',
                        '-metadata', 'copyright=CC BY-NC-SA 4.0', path], check=True)


def read_ogg(path):
    """Decode an .ogg back to float samples (frames x channels) for checking."""
    out = subprocess.run(['ffmpeg', '-loglevel', 'error', '-i', path, '-f', 'f32le', '-'],
                         check=True, capture_output=True).stdout
    probe = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'a:0', '-show_entries',
                            'stream=channels,sample_rate', '-of', 'default=nw=1', path],
                           check=True, capture_output=True, text=True).stdout
    kv = dict(line.split('=', 1) for line in probe.split())
    rate, ch = int(kv['sample_rate']), int(kv['channels'])
    return np.frombuffer(out, dtype='<f4').reshape(-1, ch), rate
