# Testing

Urbs is tested in layers, from seconds to hours. The autoplay bot (`src/sim/autoplay.cpp`) stands in for a player wherever a whole game has to be played. It proves missions can be won and finds bugs, but it cannot judge pacing, clarity or fun: that is the human playtest (docs/playtest-checklist.md).

## 1. `ctest --preset dev` (about a minute, every commit)

- **Unit tests** (`tests/test_*.cpp`): determinism and save round-trips, pathfinding, the water network, military, Lua scripts, monuments, map files, entrances, defences, expiring deliveries, every good having a source and a use, and music rules.
- **Chaos on a small town:** a town played by the chaos player for ten days stays consistent and reloads exactly.
- **Scenario tests** (`tests/scenarios/*.json` with a `"test"` block): small towns run headless with expectations. These include:
  - minimum citizens, stock, production and births;
  - no allocation in the tick;
  - the mission outcome (`"mission": "won" | "lost" | "running"`) and the loss reason (`"lost_reason"`).
- **Losing on purpose:**
  - `lose_favor`: Rome's demands go unmet and favor runs out.
  - `lose_debt`: debt on Imperium past `max_debt_days`.
  - `otium_no_loss`: the same as `lose_favor` on Otium, where a mission cannot be lost.
- **Saves and mods:** old saves (`tests/saves/`) load and run, and the example mod loads.
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
- **`--roundtrip D`:** every D days, save, load into a fresh world, and run both for a game day. They must stay identical (compared every game hour). On a mismatch the run names the parts that differ (`save::world_hash_parts`). It also reports the slowest save and load and the largest file.
- **`--keep-playing`:** keep going after the mission is won or lost.
- **Diagnostics:** `--watch goods`, `--ascii x0 y0 x1 y1`, `--verbose`, `--dump`.

Any failure gives exit status 3. Unknown flags are rejected.

## Bugs these tests found

- **Porters dropped hauls:** a porter interrupted mid-haul kept the haul reserved, so a site could wait forever for its last units.
- **Stale flow fields:** raiders kept marching on flow fields computed before the map changed (walls breached, roads built).
- **Estates:** an heirless household's estate was booked in the ledger as an expense.
- **Blocked entrances:** buildings whose entrance was built over could never be staffed or supplied.
