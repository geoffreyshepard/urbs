# Testing

Urbs is tested in layers, from seconds to hours. The autoplay bot (`src/sim/autoplay.cpp`) stands in for a player wherever a whole game has to be played. It proves missions can be won and finds bugs, but it cannot judge pacing, clarity or fun: that is the human playtest (docs/playtest-checklist.md).

## 1. `ctest --preset dev` (about a minute, every commit)

- **Unit tests** (`tests/test_*.cpp`): determinism and save round-trips, pathfinding, the water network, military, Lua scripts, monuments, map files, entrances, defences, expiring deliveries, exports and a crowded trade post, every good having a source and a use, and music rules.
- **Chaos on a small town:** a town played by the chaos player for ten days stays consistent and reloads exactly.
- **Scenario tests** (`tests/scenarios/*.json` with a `"test"` block): small towns run headless with expectations. These include:
  - minimum citizens, stock, production and births;
  - no allocation in the tick;
  - the mission outcome (`"mission": "won" | "lost" | "running"`) and the loss reason (`"lost_reason"`).
- **Losing on purpose:**
  - `lose_favor`: Rome's demands go unmet and favor runs out.
  - `lose_debt`: debt on Imperium past `max_debt_days`.
  - `otium_no_loss`: the same as `lose_favor` on Otium, where a mission cannot be lost.
- **Saves and mods:** old saves (`tests/saves/`: a v1 save and a v3 save written by v1.0, before building rotation) load and run, and the example mod loads.
- **Menu backdrop:** the main menu's city (`data/menu/backdrop/`) loads and runs a day with checks.
- **`audit_town`:** the audit flags (below) on a small town.

## 2. `ctest --preset long` (about ten minutes on 6 cores, release build)

- **Campaign:** the bot must win all 16 missions within 300 days.
- **Stress:** stress and multi-year runs (50,000 citizens, 20 years of generations, a 3,000-citizen economy).
- **Audits:** `audit_m04_chaos` and `audit_m09_check`, two missions under the full audit.

The release workflow runs everything. Pushes run only the dev set.

## 3. `tools/audit.sh` (about an hour, before a release or after deep changes)

Needs a release build: the script does not build, and a stale binary would be tested silently. Parts can be run alone: `tools/audit.sh missions chaos soak sandbox`.

| Part | What it does |
|---|---|
| `missions` | Every mission played by the bot with `--check` and `--roundtrip 25` |
| `chaos` | 7 missions × 2 seeds: the bot builds while the chaos player sends 8 random commands per game hour, for 60 days, with checks and round-trips |
| `soak` | Rome for 1,000 days (about 60 game years) past victory, with checks and round-trips every 100 days |
| `sandbox` | 24 random sandbox maps (`tools/sandbox_maps.py`) played by the bot to a modest goal: are generated maps playable? |

## 4. `tools/screens.sh` (about five minutes, after rendering or UI changes)

The game itself in screenshot mode, one window per shot: the main menu, the campaign, mods, custom-game and editor screens, a city the bot has built for 20 days with every panel and dialog opened over it, two zoom levels, night, an inspected building, a frontier mission and the tutorial's first message. A shot fails if the game exits with an error, logs an `[error]`, or writes a missing or near-empty image. The images are kept (`tools/screens.sh out_dir`) for a look by eye: the script proves the screens render, not that they look right.

## 5. `tools/playtest.sh` (about a minute, after UI or gameplay changes)

Plays the scripts in `tests/play/` through the game's real interface: the game runs with `--play SCRIPT`, and the script's clicks, drags and keys go through the same event handling as a player's, so the build menu, placement and its ghost, letters, panels, dialogs, saving and the campaign are exercised for real. Real mouse and keyboard input is ignored meanwhile, the window opens in the background at 1280×720 so coordinates mean the same everywhere, and saves go to their own folder (`--user-dir`). A script fails on a failed `expect`, a missing button, or any `[error]` in the log; screenshots and logs land in `out_dir/<script>/`.

| Script | What it plays |
|---|---|
| `m01_first_days` | Latium's tutorial: letters, building while paused, a dragged row of houses, wells, food, the forum, and the merchants' choice reaching the mission script |
| `panels` | Every report panel, the overlay menu, the pause menu and settings, opened from their buttons over a town the bot built |
| `save_load` | Save to a slot from the pause menu, build more, load: the town, the treasury and read letters come back |
| `campaign` | From the title screen: begin mission I, win it, back to the province map, begin mission II |

A script's first line may give game arguments (`# args: --scenario ...`). Commands, one per line (`#` starts a comment); coordinates are window points, tiles are map tiles:

- `wait N` (frames), `mouse X Y`, `click X Y [right]`, `press`/`release X Y [button]`, `drag X0 Y0 X1 Y1 [right]`, `wheel N`
- `tile TX TY` (point at a tile), `clicktile TX TY`, `dragtile TX0 TY0 TX1 TY1`, `cam TX TY`, `zoom Z`
- `key NAME [shift]` (SDL key names: `space`, `escape`, `r`, `F5`), `type TEXT`
- `button LABEL`: click the button showing that text, or an icon by its id (`Rome`, `Build Housing`, `Housing plot`, `Slot 1`); it waits up to a second for it. `button? LABEL` clicks it only if it is there. `read` answers "Very well" to every plain letter. `items` lists what can be clicked.
- `advance TICKS` (run the clock without waiting), `autoplay` (the bot takes over), `shot FILE`, `state` (one line of the game's state)
- `mark`, then `expect KEY OP VALUE`: numbers `treasury`, `spent` (since `mark`), `pop`, `buildings`, `day`, `favor`, `speed`, `messages`, `unread` with `< <= > >= == !=`; words `screen`, `mission`, `tool`, `selected` with `== !=`
- `quit`

The script is read as it grows, so a game started with `--play live.txt` can also be driven by hand (append commands, look at the screenshots). A script that stops growing for fifteen minutes ends the game.

Balance is measured separately with `tools/bot_missions.sh [days] [difficulty] [seeds]`, which reports win days per mission.

### The audit flags (`urbs_headless`)

- **`--check`:** a consistency check every game day (`src/sim/invariants.cpp`). Every counter the simulation keeps incrementally is recounted from scratch:
  - inventory totals;
  - goods reserved for deliveries against live hauls;
  - the open-haul count, and hauls pointing at live buildings and porters;
  - workers, residents and household members;
  - footprints on the map;
  - walkable entrances;
  - every treasury change booked in the ledger.
- **`--chaos N`:** N random player commands per game hour (`src/sim/chaos.cpp`), sent through the same command queue as the UI:
  - build anything anywhere, demolish anything, lay roads and pipes;
  - extreme policies, any trade settings and routes;
  - raise, order and disband armies;
  - call disasters, buy slaves, take loans, answer messages;
  - 3% garbage commands with out-of-range ids and coordinates.

  The chaos player has its own random stream, so a run is reproducible.
- **`--roundtrip D`:** every D days, save, load into a fresh world, and run both for a game day with the same inputs: the bot and the chaos player play the original and their commands are replayed into the copy. They must stay identical (compared every game hour), and the run goes on exactly as it would without the round-trip, so an audit run is the same game as a plain one. On a mismatch the run names the parts that differ (`save::world_hash_parts`). It also reports the slowest save and load and the largest file.
- **`--keep-playing`:** keep going after the mission is won or lost.
- **Diagnostics:** `--watch goods`, `--ascii x0 y0 x1 y1`, `--verbose`, `--dump`.
- **`--slow-ticks MS`:** print every tick slower than MS with its per-system laps (`TickStats::sys_ms`: 0 dirty rebuilds, 1 services, 2 daily/needs, 3 hourly labor, 4 goods matching, 5 events + military, 6 pathfinding). Use it to find the source of hitches.

Any failure gives exit status 3. Unknown flags are rejected.

## Bugs these tests found

- **Porters dropped hauls:** a porter interrupted mid-haul kept the haul reserved, so a site could wait forever for its last units.
- **Stale flow fields:** raiders kept marching on flow fields computed before the map changed (walls breached, roads built).
- **Estates:** an heirless household's estate was booked in the ledger as an expense.
- **Blocked entrances:** buildings whose entrance was built over could never be staffed or supplied.
- **Exports stranded at the trade post:** goods staged for export were never handed back out, even when the city's stock fell to the export level, so the goods the player chose to keep could sit at the post while construction sites waited for them. The bot abandoned 428 sites in one Antioch run and lost to debt; with the fix it won in 141 days.
- **A trade post full of unsold goods:** exports a partner had stopped buying for the year stayed at the post until it was full, after which no import fit. In one Colonia run the siege workshop waited 150 days for 10 iron that two open routes sold.
- **The bot's jobs:** with every good above its stock limit the bot found nothing to build for the unemployed, so big cities stalled short of their goal while the treasury grew past 100,000. It now tries further export goods made from local materials, then services.
- **The audit changed the game it audited:** during each round-trip day the original world played without the bot, so audit runs drifted from plain runs (Londinium lost under `--roundtrip` and won without it). Both worlds now get the same inputs.
- **Letter choices did nothing (playtest):** the letter marked itself answered before its answer reached the simulation, which then ignored the answer as a repeat, so no mission script ever heard a player's choice (paying the merchants, festivals, the Senate's offers). The bot answers through the command queue and never noticed. `m01_first_days` now checks that paying the merchants costs 200 denarii.
- **Building while paused (playtest):** orders waited for the clock, so a paused player's roads and houses did not appear and were not paid for until the game ran. They now take effect at once.
- **Smaller windows (playtest):** at 1280 points wide the top bar ran off the screen, hiding the menu button. It now shrinks to fit. A first version re-measured itself every frame and wobbled by a few pixels, which made held clicks on its buttons miss; the `panels` script caught that.
- **Other playtest finds:** the alerts that name a building that cannot work (no workers, a missing input, nowhere to store its output, an empty market), homelessness, unemployment and emigration existed in the UI but were never raised, and an "0 immigrants arrived" alert was; read letters came back after loading a save; panels covered open letters and the pause menu covered its own settings; the build list stayed open over the map after a pick; the alert feed was out of order; Esc did not leave the menu screens; the well's "water within 4 tiles" read as a radius (it is a walk from each house's door); and a Latium letter recommended a Tavern and a Physician, which unlock in mission II.
- **The bot's reservoirs:** the bot rebuilt a reservoir on the same cut-off shore spot again and again (45 times in one Antioch run), draining the treasury. Its stalled-site notes now say why a site was cleared (`cut off`, `no builders`, `no materials`, `waited too long`) and where.

## Frame-time measurement (the game)

`urbs --scenario S --cam X Y --zoom Z --speed N --perf 600 --no-vsync --mute --show nomsg,noend` skips a 120-frame warm-up,
records 600 frames and logs avg / p50 / p95 / p99 / max for the whole frame and its parts (sim ticks, UI build, world
draw-list build, render submit), plus the sprite instance counts, then quits. Without `--no-vsync` the frame time is
what the player sees (capped at the display rate). Measure on an idle machine: other heavy processes inflate the
tails several times over.
