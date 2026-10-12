# 05 — Technical Architecture

## Stack

| Concern | Choice | Why |
|---|---|---|
| Language | C++20 | Performance and control for tens of thousands of entities |
| Build | CMake + Ninja, `CMakePresets.json` | Same commands on all three OSes |
| Dependencies | CPM.cmake, with every dependency pinned to an exact version | No system package managers needed; reproducible builds |
| Window / input / audio | **SDL3** | Mature, cross-platform |
| GPU | **SDL3 GPU API** (Metal on macOS, Vulkan on Linux, D3D12/Vulkan on Windows) | Modern native backends; OpenGL is deprecated on macOS |
| Shaders | Written once in GLSL, compiled at build time by **glslang** to SPIR-V and by **SPIRV-Cross** to MSL, then embedded in the binary (`cmake/Shaders.cmake`) | One shader source; both tools build from source via CPM. Windows uses SDL_GPU's Vulkan backend (no DXIL needed). |
| Entity storage | **EnTT** for low-volume, flexible entities (buildings, events); **custom SoA pools** for hot high-volume data (citizens, soldiers, haul tasks) | ECS flexibility where it's cheap, raw speed where it matters |
| Job system | **enkiTS** | Lightweight task scheduler for parallel sim systems and pathfinding |
| Debug UI | **Dear ImGui** (SDL3 + SDL_GPU backends) | Inspectors, overlays, tuning; compiled out of release builds |
| Game UI | Custom pixel-art UI toolkit on our own sprite renderer | Matches the art style, with no extra rendering backend |
| Data defs | JSON via nlohmann/json (load time only) | Moddable, human-editable |
| Scripting | **Lua 5.4 + sol2** | Missions, events, mod logic; never per-entity per tick |
| Saves | Custom versioned binary + **zstd** | Fast and compact |
| Images | stb_image, stb_rect_pack (atlas tool) | Simple |
| Profiling | **Tracy** | Frame and zone profiling; essential for this game |
| Logging | spdlog | |
| Tests | doctest + headless scenario tests + benchmark suite | |
| CI | GitHub Actions matrix: macOS arm64, Windows x64, Ubuntu x64 | Catch platform breakage per commit |

## Process and module layout

```
urbs/
├── CMakeLists.txt, CMakePresets.json, cmake/
├── CLAUDE.md, README.md, docs/
├── src/
│   ├── core/        # types, strong IDs, math, arenas, containers, RNG, logging
│   ├── sim/         # urbs_sim: world, tick loop, citizens, needs, households, scheduler
│   ├── path/        # grid graph, regions, HPA*, flow fields, path cache
│   ├── econ/        # goods, workplaces, logistics/haul jobs, markets, money, trade
│   ├── society/     # classes, lifecycle, happiness, crime, religion, politics
│   ├── military/    # units, combat, morale
│   ├── script/      # Lua bindings (events, missions)
│   ├── data/        # def loading, validation, mod merging
│   ├── save/        # serialization, versioning, migrations
│   ├── render/      # SDL_GPU renderer, sprite batching, chunk meshes, overlays
│   ├── ui/          # game UI toolkit + screens
│   ├── game/        # glue: input → commands, camera, game states, sim↔render bridge
│   ├── platform/    # SDL3 init, paths, filesystem, threads
│   └── app/         # main() for urbs, urbs_headless
├── data/
│   ├── defs/        # buildings.json, goods.json, recipes.json, classes.json, gods.json, units.json, ...
│   └── scenarios/   # m01_latium/{scenario.json, script.lua, map.bin}, ...
├── assets/          # generated sprites, atlases, palettes, fonts
├── tools/
│   ├── artgen/      # sprite generators (Python + Pillow)
│   └── atlas/       # atlas packer
├── mods/            # user mods (same structure as data/)
├── tests/           # unit tests + scenarios/
└── bench/           # benchmarks + baseline.json
```

**Build targets:**
- `urbs_core`, `urbs_sim` (all sim modules): static libraries **with no SDL dependency**
- `urbs_render`, `urbs_ui`: depend on SDL3
- `urbs`: the game executable
- `urbs_headless`: runs scenarios with no window; used for tests, benchmarks and balancing
- `urbs_tests`, `bench`

## Main loop

```
while running:
    poll input → translate to Commands (never mutate sim directly)
    accumulator += real_dt * game_speed
    while accumulator >= TICK (50 ms):
        sim.apply(commands); sim.tick(); accumulator -= TICK
        (cap ticks per frame; if behind, the sim slows rather than spiral)
    render(sim snapshot, alpha = accumulator / TICK)   # interpolate positions
```

- **Commands** (place building, set policy, order unit) are queued and applied at tick boundaries, so replays and debugging are trivial.
- The renderer reads a **read-only view** of sim state. At first it reads on the main thread after ticks complete. If needed later, double-buffer render-relevant state so the sim can run on a separate thread.

## Determinism

No multiplayer, so bit-exact cross-platform determinism is **not** required. The rule is **the same binary, save and command log reproduces the same game.** This enables bug reproduction from saves, golden-file regression tests and balancing runs.
- Seeded RNG streams per system (`rng::Stream`).
- Parallel work writes to per-thread buffers and is merged in a stable (ID-sorted) order.
- No iteration over unordered containers in sim logic where order affects outcomes.

## Data definitions and modding

- On startup, load `data/defs/*.json`, then each enabled `mods/<name>/defs/*.json`, merging by `id` (override fields, add new). Validate against schemas and fail loudly with file and line.
- Defs compile into compact runtime tables (`DefId` → struct). The hot path never touches JSON.
- Lua runs in a sandbox (no io/os) and is invoked only at **event points**: mission triggers, timed events, dialogue choices, and daily or monthly hooks. Never per entity per tick.

## Saves

- A save is a header (version, mission, seed, timestamp, thumbnail) followed by sections (world, entities per pool, econ, society, scripts, RNG state), each length-prefixed and zstd-compressed.
- **Autosave:** snapshot the pools by memcpy into a buffer at a tick boundary (fast), then serialize and compress on a background thread.
- A save version bump requires a migration function or an explicit "incompatible save" message.
- Saves go to the platform user-data dir (`SDL_GetPrefPath`).

## Rendering

- Pixel art with nearest-neighbor sampling. Zoom levels: **4×, 3×, 2×, 1×** (pixel-perfect) plus **far zooms ½ and ¼**, which use a simplified representation (building silhouettes, citizens as 1-px dots, or density shading).
- **Static layers** (terrain, roads, buildings) are baked per 32×32 chunk into GPU vertex buffers and rebuilt only when dirty.
  Terrain transitions and height steps use a dual grid (cells centred on tile corners, one of 14 noisy corner-mask
  shapes per terrain; see `tools/artgen/README.md`), so coasts, borders and cliffs follow smooth contours instead of
  the tile staircase.
- **Dynamic sprites** (agents, carried goods, effects) are culled by chunk and drawn as instanced quads from one per-frame instance buffer; tens of thousands of instances is cheap.
- **Overlays** are per-tile color layers uploaded as a texture and blended in a shader.
- **Frame passes** (`render/renderer.cpp`): world pass into a scene texture (terrain chunks, dynamic sprites, then
  cast shadows, then translucent sprites and overlays) → grade pass into a final texture → blit to the swapchain →
  UI. The UI is never graded.
- **Cast shadows:** every y-sorted sprite (depth between `kDepthOverlayMax` and `kDepthShadow`) is drawn a second time
  as its silhouette, sheared away from the top-left light (`shaders/shadow.vert`), multiplying the ground. It is
  depth-tested against what is already drawn, so shadows never cover a sprite in front of them, and a stencil test lets
  each pixel darken once. Ground-level sprite pixels are flagged with alpha 254 by the art generators and cast nothing.
  Shadows lengthen toward dawn and dusk and fade under cloud and at night. Far zooms skip them.
- **Ground shading** (`shaders/ground.frag`): the static chunk layer samples a map-sized ground texture (one texel per
  tile: wear, luminance, vegetation, land mask; built per dirty chunk by `build_ground_texels`) with linear filtering at
  each art pixel's centre and quantises it with a 4×4 Bayer pattern, so smooth fields stay pixel-crisp. It gives hills
  smooth shading, broad light/dark and dry/lush variation, worn earth around buildings and along roads, seasonal drying
  and browning, settled snow (built up by snowfall in `game/lighting.cpp`) and lighter shallows by the shore.
- **Night lights:** lived-in houses and staffed workplaces push their `bld/<name>_lit` window mask (plus a faint
  1-px glow) to a light list. It is drawn additively on the graded frame, depth-tested against the world pass, with
  strength from the grade's night amount, so windows glow instead of being darkened with everything else.
- **Colour grade** (`shaders/post.frag`): split toning (cool shadows, warm light), exposure, saturation (above 1 it
  acts as vibrance, sparing colours that are already strong), an S-curve contrast that keeps black and white in range,
  lifted blacks and a soft vignette that leaves the middle of the screen alone. `game/lighting.cpp` keys these by hour (night, dawn, morning, day, golden hour,
  dusk) and adjusts for season and weather, easing between states so weather changes fade in.

## Cross-platform notes

- **macOS (dev):** Apple Clang or Homebrew LLVM, Ninja. If the Command Line Tools linker is older than the SDK (`ld: tapi error ... unknown architecture`), build with `export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer` or run `sudo xcode-select -s /Applications/Xcode.app`; build as a `.app` bundle via CMake. For an open source release, the unsigned app needs the right-click → Open Gatekeeper workaround; document it in the release notes.
- **Windows:** MSVC 2022 or clang-cl; ship a zip.
- **Linux:** GCC 13+ or Clang 17+; ship a tarball, AppImage later.
- Use `std::filesystem` plus SDL path helpers; never hard-code path separators.
- CI builds and runs tests on all three on every push; it also runs the headless stress scenario (shortened) on macOS and fails if it regresses more than 15% versus baseline.

## License

- **Code:** PolyForm Noncommercial 1.0.0 (`LICENSE.md`). Anyone may use, modify, share and contribute for noncommercial purposes; selling the game or a derivative is not allowed.
- **Generated assets, data and docs:** CC BY-NC-SA 4.0 (`LICENSE-ASSETS.txt`).
- Because of the noncommercial clause the project is *source-available*, not OSI "open source". That is deliberate.
- Contributions are accepted under the same licenses (inbound = outbound, see `CONTRIBUTING.md`).
- Because the art is produced by generator scripts, committing the generator source keeps assets reproducible and transparent.
