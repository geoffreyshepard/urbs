# 11 — Music and sound effects

## Approach: music is code

Like the sprites (docs/07), every piece of music is produced by a program. `tools/musicgen/` synthesizes its own instruments with numpy and scipy. No samples or sound fonts are used, so the soundtrack is reproducible, reviewable as diffs, and licensed like the rest of the assets (CC BY-NC-SA 4.0). Scores are hand-written melodies stored as text inside each track module. Instruments, rooms and the mix are shared code.

Never edit a rendered `.ogg`. Change the generator and re-render.

## Direction

Almost no Roman music survives, so the soundtrack is stylized the same way the art is: historically flavoured, built to be enjoyable for hours.

- **Language:** Greek modes (Dorian, Phrygian, Lydian, Mixolydian, Aeolian), drones, and heterophony (two instruments playing the same melody with different ornaments). Some tracks use odd meters (7/8). Regional tracks use Hijaz (Egypt) and Bayati with a quarter-flat second (the East).
- **Republic palette:** lyre and kithara, aulos (double reed, with a drone pipe), syrinx (pan pipes), tympanum (frame drum), crotala (finger cymbals).
- **Empire palette:** adds the hydraulis (water organ), cornu, tuba and buccina horns (fanfares stay on the natural harmonic series), a small vowel choir, and war drums.
- **Regional colour:** harp, ney and sistrum (Egypt); carnyx and cold drones (the north); oud, qanun-like plucks, ney and riq (the East).
- **Low fatigue:** city tracks are sparse, have dynamic arcs, and are separated by 45–150 s of silence.
- **Leitmotif:** "Urbs" (`tracks/urbs.py`, `THEME_A`) runs through the campaign. It is a lyre-and-aulos tune in the Republic, it appears in the menu, map, lament and victory music, and it returns in its grandest form in mission 16 (`caput_mundi`).

## Track list

`data/defs/music.json` is the source of truth: ids, titles, kinds, playlist rules and stinger events.

| Kind | Tracks | Plays when |
|---|---|---|
| meta | urbs_aeterna (menu), mare_nostrum (province map), senatus (briefing/epilogue, loop) | Out of mission |
| city | prima_lux (M1), tiberis, ager, urbs, corinthus (M10), imperium, pax, nox, caput_mundi (M16), nilus (Egypt), limes (north), oriens (East) | Normal play; shuffled by era/region/mission, weighted, with silence between tracks |
| tension | seditio (unrest), hostes (raid approaching) | Loops while the condition holds |
| battle | proelium (skirmish), obsidio (siege), rhenus (large battle) | Loops during combat |
| festival | saturnalia (festivals), ludi (games, triumphs) | Once per event |
| disaster | incendium (loop), lamentatio (aftermath) | Major fire/quake; then once afterwards |
| stinger | s_victory, s_defeat, s_promotion, s_blessing, s_wrath, s_request, s_raid, s_monument, s_festival, s_mission_start | One-shot over the music, which ducks |

Loops are rendered seamlessly: the reverb tail is wrapped onto the head of the file.

## Loudness

`mix.LOUDNESS` sets a gated-RMS target per kind (city −19 dBFS, battle −16, lament −20, and so on), so a city track never jumps out at the player, while battles feel bigger. A soft limiter above 0.8 keeps peaks below 0.99. The build reports the share of samples limited and flags any track above 0.3%.

## Playback (engine)

Music runs entirely in the presentation layers. The sim never knows about it (CLAUDE.md rule 1).

- **`platform::Audio`** (`src/platform/audio.*`): SDL3 audio stream, stb_vorbis decoding, two crossfading music voices and a stinger voice that ducks the music by `stinger_duck_db`. Decoding and mixing happen on the main thread in `pump()`, which keeps ~250 ms queued, so no state is shared with the audio thread. No audio device means silence and nothing else.
- **`game::MusicDirector`** (`src/game/music.*`) decides what plays. The rules it uses (manifest loading, which tracks fit, signals, mood priority) are in `src/game/music_rules.*`, which has no SDL dependency and is unit-tested (`tests/test_music.cpp`).
- **Out of a mission:** main menu → `urbs_aeterna`, province map → `mare_nostrum` (each replays after a 8–20 s silence), setup screens (sandbox, custom, mods, editor) → `senatus`.
- **In a mission:** a `mission_start` stinger, then the city playlist. A city track is eligible when its `missions` list names the mission, or when both `eras` and `regions` match (`default` = no region). The era comes from the world: `empire`, else Late Republic from mission 9, else Republic. Picks are weighted by `weight`, doubled in `prefer_seasons`, never the same track twice in a row, with 45–150 s of silence between tracks.
- **Moods**, read from the world every 0.5 s, highest priority first:

| Mood | Cause | Track |
|---|---|---|
| Mission over | Won or lost, while the end panel shows | `urbs_aeterna` (won), `senatus` (lost) |
| Large battle | Fighting with ≥150 enemy soldiers | `rhenus` |
| Siege | Fighting, no city soldiers in the field (walls and towers alone) | `obsidio` |
| Skirmish | Any other fighting | `proelium` |
| Disaster | ≥5 buildings burning, or an earthquake in the last 6 game hours | `incendium` |
| Raid near | Enemy soldiers on the map, no fighting yet | `hostes` |
| Unrest | A riot or slave revolt in the last game day, or favor below 20 | `seditio` |
| Aftermath | After a disaster that lasted ≥30 s (plays once) | `lamentatio` |
| Festival | A festival is running (plays once per festival) | `saturnalia` (Republic), `ludi` (Empire) |
| City | Otherwise | playlist |

  Battle music holds 8 s and disaster music 10 s after their cause ends, so the music doesn't flicker. Crossfades are 2.5 s (1 s into battle). When the city returns, the playlist resumes with a new track after a short pause.
- **Stingers**, from the world's alerts (at most one per 0.5 s poll, each event at most every 20 s, never stacked except the mission outcome): raid sighted, god's blessing or wrath, request from Rome, monument dedicated, festival begins, a won campaign (`s_promotion`), mission won or failed, and a career promotion (on the province map).
- **Settings:** "Music volume" (`music_volume` in settings.json, default 0.7) and "Sound effects volume" (`sfx_volume`, default 0.7; settings files from before v1.1 get the default). `--mute` starts without audio; `--screenshot` runs are always silent.
- **Testing without speakers:** `SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=out.raw` writes the mix (f32 stereo 44.1 kHz) to a file. The log prints every mood change and track.

## Workflow

```bash
python3 -m venv tools/musicgen/.venv && tools/musicgen/.venv/bin/pip install -r tools/musicgen/requirements.txt
tools/musicgen/.venv/bin/python tools/musicgen/build_all.py --review          # everything
tools/musicgen/.venv/bin/python tools/musicgen/build_all.py --only urbs,pax --review
```

`--review` writes a spectrogram and a loudness curve per track to `build/music_review/<id>.png`. Check structure, gaps and spikes there, then listen.

## Sound effects

Sound effects are generated the same way: `tools/sfxgen/` synthesizes them with musicgen's DSP helpers, instruments (the bell, the clap, the cornu) and rooms. No samples. The direction is wood, stone, bronze, cloth and voices, in a room or out of doors: soft attacks, little above 6 kHz, no synth tones, nothing chiptune.

`data/defs/sfx.json` is the source of truth: for each event, its variant files, gain, cooldown, how many copies may sound at once, random pitch and gain spread, and whether it is positional; the positional fall-off; the ambient bed; and the coin threshold.

| Event | Files | Sound | Plays when |
|---|---|---|---|
| ui_click | click_1–3 | a fingertip on a wax tablet's wooden frame | Any button, row, checkbox, slider or field is pressed (one helper reads ImGui's activation each frame; pressing a window's empty space or a scrollbar is quiet) |
| place_building | place_1–3 | a stone block set down, then a mallet tap | A building (or a row of houses) is placed where the ghost was green and the treasury can pay |
| place_road | road_1–2 | a spade of gravel scraped and dropped | A road, bridge, pipe or sewer is laid |
| demolish | crumble_1–2 | a wall pulled down, rubble settling | The demolish tool clears something (a building or a road) |
| coin | coins_1–2 | coins dropped on a table | At least `coin_min_denarii` moved since the last look in exports, imports, grants from Rome, festivals or other payments (taxes and wages trickle and stay quiet) |
| message | scroll_1–2 | a papyrus scroll unrolled | A new letter |
| alert | chime_1–2 | a small bronze bell, struck twice | A riot, slave revolt, disease, angry god, request from Rome or low treasury alert (a letter in the same moment wins: one notification sound at a time) |
| fire | fire_1–2 | a whoosh, then crackling timber | A building catches fire (positional) |
| collapse | collapse_1–2 | a long rumble and falling masonry | A building collapses (positional), or an earthquake (everywhere) |
| battle | clash_1–3 | swords and shields | Soldiers are striking while the city's time runs; heard from the striker nearest the centre of the view (positional) |
| raid | horn_1 | a low cornu call | Raiders sighted |
| legion | horn_2 | a lower cornu call | A cohort leaves on campaign or comes home |
| festival | cheer_1–2 | a crowd cheering and clapping | A festival begins, a monument is dedicated, a campaign is won |
| mission_won | cheer_1–2 | the same | The mission is won |

- **Engine:** `platform::Audio` decodes each effect into memory once at start-up (`load_clip`, 16-bit, about 8 MB in all) and plays it on a fixed pool of 16 voices (`play_sfx`: gain, equal-power pan, playback rate); when every voice is busy, the one nearest its end is cut. Playing allocates nothing. Clips ramp in and out over 32 frames, whatever the encoder left at their edges. Music and effects share one master that bends smoothly toward full scale above 0.9 instead of clipping. With no device nothing is loaded and nothing plays.
- **Director:** `game::SoundDirector` (`src/game/sfx.*`) plays interface sounds when asked, and every 0.25 s compares the world with its last look (`SfxWatch` in `src/game/sfx_rules.*`, which has no SDL dependency and is unit-tested in `tests/test_sfx.cpp`). The sim knows nothing about sound: every cue is derived from state the game already sees (alerts, letters, soldiers, the festival, the ledger, the mission state). A new or loaded city starts a fresh watch, so what is already there makes no sound.
- **Rate limits:** cooldowns and `max_playing` are in real seconds and voices, not game time, so fast-forward never machine-guns a sound; a variant never repeats twice in a row; pitch and gain vary a little per play from a presentation-only RNG (never the sim's streams).
- **Positional events** (fire, collapse, battle) are heard only on or near the screen. Distance is measured in half-screens from the centre of the view: full gain inside `full` (0.8), falling to `far_gain` (0.3) at `max` (2.0, a screen beyond the edge), silent further off; panned up to `pan` (0.6) toward the side they are on.
- **Ambient bed:** `city_1`, a 24 s seamless stereo loop (distant murmur and babble, a far workshop, sparrows, swifts) plays under the city while a mission is on screen. Every 0.5 s the director counts the people living and working in the buildings on screen; the bed is silent below `people_min`, full at `people_full` (square-root curve), and fades over `fade_s`. It fades out on the menus.
- **Volume:** "Sound effects volume" scales the effects and the ambient bed (`sfx_volume`); "Music volume" is separate.

### Workflow

```bash
python3 -m venv tools/sfxgen/.venv && tools/sfxgen/.venv/bin/pip install -r tools/musicgen/requirements.txt
tools/sfxgen/.venv/bin/python tools/sfxgen/build_all.py --review          # everything (~3 min)
tools/sfxgen/.venv/bin/python tools/sfxgen/build_all.py --only click_1,click_2
tools/sfxgen/.venv/bin/python tools/sfxgen/build_all.py --check           # check the files in assets/sfx only
```

Files are `<sound>_<variant>.ogg` in `assets/sfx/` (mono Vorbis q3; the ambient loop stereo q2; all under 1 MB together), each seeded from its name. After encoding, every file is decoded again and checked: 44.1 kHz, peak at most 0.95, no DC, silent first and last samples (no click), a seamless loop seam, and at most 15% of the energy above 6 kHz (harshness). `--review` writes a spectrogram and level curve per file to `build/sfx_review/`. To add a sound: write a function in `tools/sfxgen/sounds.py`, list it in `SOUNDS`, name its files in `data/defs/sfx.json`, render, and trigger the event from `src/game/`.
