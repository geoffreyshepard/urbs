# CLAUDE.md — Instructions for Claude Code

You are helping build **URBS**, a Roman city builder in C++20 that must simulate 10,000 citizens and tens of thousands of total entities smoothly. Read `docs/05-technical-architecture.md` and `docs/06-simulation-design.md` before touching simulation code. Read the current milestone spec in `docs/milestones/` before starting work.

## Non-negotiable architecture rules

1. **The simulation never depends on rendering, UI, or SDL.** `urbs_sim` is a pure C++ static library. It must build and run headless.
2. **No heap allocation inside the tick.** Pre-size pools and arrays; use arenas or free lists. If you need a dynamic container in a hot path, justify it in a comment.
3. **Hot data is struct-of-arrays.** No virtual calls, no `std::function`, no `shared_ptr` in per-entity loops.
4. **Agents do not tick unless they must.** Use the agent LOD tiers and the event scheduler (see sim design doc). A system that iterates every entity every tick needs an explicit reason.
5. **All randomness comes from the seeded sim RNG streams.** Never use `rand()`, `std::random_device`, or time-based seeds inside the sim.
6. **Same binary + same save + same inputs = same result.** Parallel systems write to per-thread buffers and merge in a stable order.
7. **Content is data.** Buildings, goods, recipes, classes, gods, units, and missions live in `data/defs/*.json`. Never hard-code a building's stats in C++.
8. **Saves are versioned.** Any change to saved state bumps the save version and adds a migration or a clear incompatibility error.

## Workflow

- Work in small, reviewable commits. One concern per commit.
- Before committing: `cmake --build --preset dev` and `ctest --preset dev`. Run `bench` when touching sim, pathfinding, or rendering hot paths, and compare against `bench/baseline.json`.
- New sim systems ship with: a unit test, a headless scenario in `tests/scenarios/`, and a Tracy zone.
- If a design question isn't answered in `docs/`, stop and ask. Don't invent game rules silently. Record the answer in the relevant doc.
- Keep `docs/` updated in the same commit as behavior changes.

## Code style

- C++20. `clang-format` (config in repo) and `clang-tidy` must pass.
- Namespaces match `src/` folders: `urbs::core`, `urbs::sim`, `urbs::path`, `urbs::econ`, `urbs::society`, `urbs::military`, `urbs::script`, `urbs::data`, `urbs::save`, `urbs::render`, `urbs::ui`, `urbs::game`, `urbs::platform`.
- Prefer plain structs and free functions over class hierarchies.
- IDs, not pointers, between entities (`CitizenId`, `BuildingId` as strong typedefs over `uint32_t` with generation counters).
- Fixed-size integer types in all saved data.
- Errors: no exceptions in the sim or render hot paths. Use return codes or `std::expected`-style results. Exceptions are allowed at load time (defs, saves).

## Build commands (macOS dev machine)

```bash
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer  # this Mac: CLT linker is older than the SDK
cmake --preset dev          # configure (Ninja, Debug + ASan/UBSan)
cmake --build --preset dev  # build
ctest --preset dev          # tests
./build/dev/urbs.app/Contents/MacOS/urbs   # run game
./build/dev/urbs_headless --scenario tests/scenarios/stress_50k.json --ticks 12000
./build/dev/bench           # perf benchmarks
cmake --preset release && cmake --build --preset release
ctest --preset long         # long soak tests (release build)
# Mission validation: the autoplay bot plays a mission headless (--daily prints a per-day trace,
# --verbose adds money/request lines, --dump prints every building at the end).
./build/release/urbs_headless --scenario data/scenarios/m04_capua/scenario.json --ticks 192000 --autoplay --daily
URBS_ALLOC_TRAP=1 ./build/dev/urbs_headless ...  # abort with a stack trace at the first allocation inside tick()
tools/bot_missions.sh 300 1 3   # balance: every mission x 3 seeds, win days (release build; does not build)
tools/audit.sh                  # deep audit: consistency checks, chaos player, save round-trips, soak, random maps
                                # (release build; ~1 h; see docs/testing.md)
tools/screens.sh [out_dir]      # game smoke test: every screen and panel in screenshot mode (release build)
```

## Performance targets (Apple Silicon M1 baseline)

- 10,000 citizens + 25,000 other entities at 4× speed: sim ≤ 8 ms/frame on average, 60 FPS render at default zoom.
- Pathfinding: p99 request latency < 2 frames under load.
- Save of a 10k city < 1 s (on a background thread); load < 3 s.

## Art

World art (buildings, terrain, people, effects) is pre-rendered 3D on an isometric camera: Blender scripts in `tools/artgen3d/` render `assets/sprites_iso/` (`/Applications/Blender.app/Contents/MacOS/Blender -b --python tools/artgen3d/run.py -- <family> [names]`), then `python3 tools/atlas/pack.py` rebuilds the atlas. UI and goods icons still come from the pixel-art scripts in `tools/artgen/`. Never hand-edit generated PNGs; change the generator and regenerate. See `docs/13-isometric.md` and `docs/07-art-pipeline.md`.
