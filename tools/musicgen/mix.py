"""Stems -> stereo master: levels, panning, room reverb, loudness, looping, OGG."""
import os
import subprocess
import tempfile
import wave

import numpy as np
from scipy import signal

import core as C
from core import SR, active_rms, butter, place

SPACES = {   # RT60 s, damping lowpass Hz, pre-delay s
    'courtyard': (2.0, 2500, 0.018),
    'temple':    (3.4, 1800, 0.030),
    'hall':      (2.6, 2200, 0.022),
    'field':     (1.1, 4000, 0.008),
}

# Integrated loudness targets (gated RMS, dBFS) per music.json "kind".
LOUDNESS = {
    'meta': -18, 'city': -19, 'tension': -18, 'battle': -16, 'festival': -17,
    'disaster': -17, 'lament': -20, 'stinger': -17,
}


def room_ir(space):
    t60, damp, pre_s = SPACES[space]
    length = t60 * 1.3
    n = int(length * SR)
    t = np.arange(n) / SR
    pre = int(pre_s * SR)
    ir = np.zeros((n, 2))
    for ch in range(2):
        nz = C.noise(n)
        lo = butter(nz, 'low', damp)
        hi = nz - lo
        tail = lo * 10 ** (-3 * t / t60) + 0.5 * hi * 10 ** (-3 * t / (t60 * 0.45))
        tail *= np.clip(t / 0.03, 0, 1)
        ir[pre:, ch] = tail[:n - pre]
        for d_ms, g in ((7, .5), (13, .35), (21, .3), (34, .2)):
            ir[int((d_ms * t60 / 2 + 3 * ch) * SR / 1000), ch] += g * 3
    return ir / np.sqrt(np.sum(ir ** 2) / 2)


def gated_rms_db(x):
    mono = x.mean(axis=1) if x.ndim == 2 else x
    blk = int(0.4 * SR)
    blocks = np.array([np.mean(mono[i:i + blk] ** 2) for i in range(0, len(mono) - blk, blk // 2)])
    blocks = blocks[blocks > 10 ** (-60 / 10)]
    if not len(blocks):
        return -120.0
    rel = blocks[blocks > np.mean(blocks) * 10 ** (-20 / 10)]
    return 10 * np.log10(np.mean(rel))


class Mix:
    """Collects stems for one piece. `length` is the musical body in seconds."""

    def __init__(self, length, tail=4.5):
        self.length = length
        self.n = int((length + tail) * SR)
        self.stems = {}

    def add(self, stem, start, sig, gain=1.0):
        buf = self.stems.setdefault(stem, np.zeros(self.n))
        place(buf, start, sig, gain)

    def line(self, stem, t0_sig, gain=1.0):
        t0, sig = t0_sig
        self.add(stem, t0, sig, gain)

    def render(self, levels, space='courtyard', wet_db=-8.0, loudness=-19, loop=False):
        """levels: {stem: (active RMS dB, pan -1..1, reverb send 0..1)}."""
        missing = set(self.stems) - set(levels)
        assert not missing, f"no level for stems {missing}"
        dry = np.zeros((self.n, 2))
        send = np.zeros(self.n)
        for k, x in self.stems.items():
            db, pan, snd = levels[k]
            x = x * (10 ** (db / 20) / active_rms(x))
            th = (pan + 1) * np.pi / 4
            dry[:, 0] += np.cos(th) * x
            dry[:, 1] += np.sin(th) * x
            send += snd * x
        ir = room_ir(space)
        wet = np.stack([signal.oaconvolve(send, ir[:, ch])[:self.n] for ch in range(2)], axis=1)
        wet *= 10 ** (wet_db / 20) * np.sqrt(np.mean(dry ** 2)) / (np.sqrt(np.mean(wet ** 2)) + 1e-12)
        out = dry + wet
        out = np.stack([butter(out[:, ch], 'high', 32) for ch in range(2)], axis=1)
        if loop:   # wrap the tail onto the head so the file loops seamlessly
            body = int(round(self.length * SR))
            tail = out[body:]
            out = out[:body].copy()
            out[:len(tail)] += tail[:body]
        out *= 10 ** ((loudness - gated_rms_db(out)) / 20)
        a = np.abs(out)
        limited = float(np.mean(a > 0.8) * 100)
        out = np.where(a < 0.8, out, np.sign(out) * (0.8 + 0.19 * np.tanh((a - 0.8) / 0.19)))
        if not loop:
            mono = np.abs(out).max(axis=1)
            last = int(np.nonzero(mono > 10 ** (-66 / 20))[0][-1]) + int(0.05 * SR)
            out = out[:min(last, len(out))]
            f = min(int(0.6 * SR), len(out) // 4)
            out[-f:] *= np.linspace(1, 0, f)[:, None] ** 2
            out[:64] *= np.linspace(0, 1, 64)[:, None]
        stats = dict(seconds=round(len(out) / SR, 1), loudness=round(gated_rms_db(out), 1),
                     peak=round(float(np.abs(out).max()), 3), limited_pct=round(limited, 3))
        return out, stats


def write_wav(path, x):
    pcm = (np.clip(x, -1, 1) * 32767).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def write_ogg(path, x, title, quality=3):
    """Vorbis via ffmpeg; bitexact flags keep the output byte-identical between runs."""
    with tempfile.TemporaryDirectory() as td:
        wav = os.path.join(td, 'x.wav')
        write_wav(wav, x)
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', wav,
                        '-fflags', '+bitexact', '-flags:a', '+bitexact',
                        '-c:a', 'libvorbis', '-q:a', str(quality),
                        '-metadata', f'title={title}', '-metadata', 'artist=URBS (musicgen)',
                        '-metadata', 'copyright=CC BY-NC-SA 4.0', path], check=True)
