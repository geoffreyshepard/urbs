# URBS (working title)

A Roman city-building and management game in the tradition of *Caesar III* and *Pharaoh*, with the living, fully simulated citizens of *Songs of Syx*. You start as a minor official founding a hamlet in Latium and finish, sixteen missions later, rebuilding Rome itself, a city of 10,000 people in which every one of them is a real, simulated individual going about their day.

- **Genre:** City builder / management sim with light military
- **Setting:** Stylized Rome, from the Early Republic to the Empire, with gods that matter
- **Scale:** 10,000 citizens and tens of thousands of simulated entities at once
- **Tech:** C++20, custom simulation, SDL3 (GPU API: Metal / Vulkan / D3D12)
- **Platforms:** macOS (Apple Silicon), Windows, Linux. Developed on a Mac.
- **Art:** Stylized pixel art, generated as code through Claude Code
- **License:** PolyForm Noncommercial 1.0.0 (code) and CC BY-NC-SA 4.0 (assets, data). Free to use, modify and contribute; not for commercial resale.
- **Team:** Solo developer + Claude Code

> "Urbs" is a placeholder (Latin for "the city"). Rename freely; the code namespace is `urbs`.

## Status

**v1.0.0**, which includes:
- the full 16-mission campaign;
- a sandbox with generated maps;
- a scenario editor;
- mod support.

See [RELEASE_NOTES.md](RELEASE_NOTES.md).

## Playing

Download a release archive, or build from source and run `urbs`. On a Mac, `tools/package/package_macos.sh` builds `dist/Urbs.app` and a zip.

## Building

Needs CMake 3.25+, Ninja and a C++20 compiler. Dependencies are fetched by CPM on first configure.

```bash
cmake --preset release && cmake --build --preset release
ctest --preset dev        # quick tests (use the dev preset build)
ctest --preset long       # campaign missions played by the autoplay bot, stress runs (release build)
./build/release/urbs_headless --scenario data/scenarios/m01_latium/scenario.json --ticks 360000 --autoplay --daily
```

On macOS, set `DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer` if CMake picks the command-line tools.

## Modding

Goods, buildings, housing, gods, units, trade partners and scenarios are JSON and Lua. Enable mods under **Main menu → Mods**. See [docs/10-modding.md](docs/10-modding.md) and the example in `mods/example_vintners_guild/`.

## Contributing

Contributions are welcome. Read [CLAUDE.md](CLAUDE.md) for the engineering rules (deterministic simulation, no allocation in the tick, data-driven content, versioned saves). Run `ctest --preset dev` before sending changes. Changes to simulation or balance should also pass `ctest --preset long`.

## Documents

| File | What it covers |
|---|---|
| [CLAUDE.md](CLAUDE.md) | Rules and context for Claude Code. Read before every session. |
| [docs/01-vision.md](docs/01-vision.md) | Pitch, design pillars, references, non-goals |
| [docs/02-game-design.md](docs/02-game-design.md) | Full game design: citizens, needs, economy, housing, gods, politics, military, threats, UI |
| [docs/03-campaign-and-progression.md](docs/03-campaign-and-progression.md) | The 16-mission campaign, career ranks, unlocks, sandbox and editor |
| [docs/04-content-catalog.md](docs/04-content-catalog.md) | Buildings, goods, production chains, class consumption tables |
| [docs/05-technical-architecture.md](docs/05-technical-architecture.md) | Stack, libraries, module layout, build, cross-platform, saves, modding |
| [docs/06-simulation-design.md](docs/06-simulation-design.md) | How tens of thousands of entities run fast: agent LOD, scheduling, pathfinding, logistics |
| [docs/07-art-pipeline.md](docs/07-art-pipeline.md) | Generating consistent pixel art with Claude Code; style guide |
| [docs/08-roadmap.md](docs/08-roadmap.md) | Milestones M0–M14 with exit criteria |
| [docs/09-assumptions-and-open-questions.md](docs/09-assumptions-and-open-questions.md) | Decisions defaulted on your behalf, and what is still open |
| [docs/10-modding.md](docs/10-modding.md) | Modding guide: definition files, scenarios, the Lua API |
| [docs/11-music.md](docs/11-music.md) | The generated soundtrack |
| [docs/12-story.md](docs/12-story.md) | The campaign's story, cast and writing rules |
| [docs/testing.md](docs/testing.md) | Test layers: unit and scenario tests, the campaign, the audit |
| [docs/milestones/](docs/milestones/) | What each milestone delivered, M0–M14 |

## How to use these docs with Claude Code

1. Put this folder at the root of the game repo. `CLAUDE.md` lives at the repo root.
2. At the start of each milestone, ask Claude Code to write `docs/milestones/Mxx.md` from the roadmap entry. Review it, then build against it.
3. When a design decision changes, update the relevant doc in the same commit as the code. The docs are the source of truth, not chat history.
4. Never skip the M2 performance gate (see roadmap). Scale is the whole point of this game.
