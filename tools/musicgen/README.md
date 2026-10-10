# URBS music generators

Every track is synthesized by code. No samples are used. See `docs/11-music.md` for the direction and playback rules.

## Run (from the repo root)

```bash
python3 -m venv tools/musicgen/.venv
tools/musicgen/.venv/bin/pip install -r tools/musicgen/requirements.txt
tools/musicgen/.venv/bin/python tools/musicgen/build_all.py [--only id,id] [--review] [--jobs N]
```

Output goes to `assets/music/<id>.ogg` (Vorbis q3, 44.1 kHz stereo, encoded with ffmpeg's bitexact flags). Every random choice comes from `core.rng`, seeded with a CRC of the track id, so renders are reproducible. `--review` writes spectrograms to `build/music_review/`.

## Files

| File | Contents |
|---|---|
| `core.py` | `SR`, `seed`, pitch names (`midi('Bb3')`, quarter tones `E~4`), `Mode` (diatonic steps, triads), `Clock`/`bpm_clock`, score `parse`, `phrase`, `ornament`, DSP helpers |
| `instruments.py` | `pluck` (lyre, kithara, harp, oud, qanun), `strum`, `aulos`, `flute` (ney, tibia), `pipes`, `drone`, `brass` (cornu, tuba, buccina, carnyx), `organ` (hydraulis), `choir`, percussion (`boom`, `tek`, `war_drum`, `riq`, `shake`, `sistrum`, `crotala`, `cymbal`, `clap`, `bell`) |
| `arrange.py` | `arpeggio` ostinati over chord roots, `drums` patterns with fills, `held` chords, `STD_KIT` |
| `mix.py` | `Mix` (stems → levels, pan, room reverb, loudness target, seamless loops), `write_ogg` |
| `build_all.py` | Renders every id in `data/defs/music.json` in parallel |
| `tracks/<id>.py` | One module per track or stinger: the score and arrangement; `build()` returns `(Mix, settings)` |

## Score format

```
"A4:3 G4 F4 G4 | A4:2 D5 C5:3 | r:6 |"
```

`NOTE[:duration]` in clock units (default 1). `r` is a rest. Bars are separated by `|`, and each bar must add up to the clock's bar length, which `parse` asserts.

## Adding a track

1. Add an entry to `data/defs/music.json`.
2. Write `tracks/<id>.py` (copy `tracks/urbs.py` as a template).
3. Render with `--only <id> --review`, check the spectrogram, then listen.
