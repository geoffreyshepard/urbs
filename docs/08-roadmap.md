# 08 — Roadmap

Milestones are sequential. Each has **exit criteria**, and you don't start the next one until they pass. At the start of each milestone, ask Claude Code to write `docs/milestones/Mxx.md` (scope, tasks, tests, risks) from the entry below.

There are no dates: this is a hobby project. Track progress by milestones completed.

---

### M0 — Foundation
Repo, CMake presets, CPM dependencies pinned, CI on macOS/Windows/Linux, SDL3 window with SDL_GPU clear color, ImGui debug overlay, logging, Tracy, doctest, `urbs_headless` stub, clang-format/clang-tidy.
**Exit:** CI green on all three OSes; the window opens on all three.

### M1 — World and rendering
Tile map data, procedural terrain test maps with elevation, chunked static rendering, camera pan and all zoom levels, overlay layer, palette locked, artgen pipeline and atlas packer working, first terrain tiles and placeholder buildings.
**Exit:** a 768² map renders at 60 FPS at every zoom; the art style is locked on a contact sheet.

### M2 — Simulation core + performance gate ⚠️
Fixed tick and speeds, command queue, SoA citizen pool, timing-wheel scheduler, agent LOD tiers, lazy needs, connectivity regions, HPA* + path cache + async queue, spatial grid, service maps v1, enkiTS integration, **save/load v1**, allocation tracking.
**Exit:** `stress_50k` passes the gate in sim design §13; save/load round-trips the stress scenario exactly.

### M3 — Building and logistics
Placement (fixed and painted), roads with diagonals, physical construction by builders, stockpiles and warehouses, haul matcher with reservations, porters, first chain (wheat → flour → bread), data defs loading with validation.
**Exit:** in a headless scenario a small town builds itself from orders and bread reaches storage; all content comes from JSON.

### M4 — Citizens' daily life
Households, plebeian housing tiers 1–3, schedules, errands (shopping, water), utility decisions, employment and the labor screen, immigration, citizen inspector and follow cam, alerts feed v1.
**Exit:** a 1,000-person town lives a believable day; clicking any citizen explains what they're doing and why.

### M5 — Economy depth
All production chains through mission 6, markets and pricing, money (treasury, wages, household savings, taxes), the goods ledger, trade posts and land trade, the harbor and sea trade.
**Exit:** a balanced 3,000-person economy runs 10 game years headless without collapsing; the ledgers explain every flow.

### M6 — Services and housing evolution
Desirability, water (wells → aqueducts with elevation → pipes → fountains), sewers, health, baths, education, entertainment, full housing lines including domus, coverage overlays.
**Exit:** housing visibly evolves and devolves with clear, inspectable causes.

### M7 — Society
Classes (all five), slavery and manumission, liberti mobility, lifecycle (births, aging, marriage, death, inheritance), traits and skills, life goals, happiness breakdown, crime, riots.
**Exit:** generations turn over in a 20-year headless run; population composition is stable and responsive to policy.

### M8 — Religion, threats, seasons
Gods and moods, festivals and calendar, seasons and weather, fire, collapse, disease, servile revolt, disasters (flood, earthquake, drought), vigiles and engineers.
**Exit:** each threat can be triggered from debug UI and countered through gameplay.

### M9 — Military
Barracks, recruitment, upkeep, armory chain hookup, unit types, formations, morale, walls/gates/towers, raids and invasions, battles with 2,000+ soldiers, off-map campaigns.
**Exit:** 3,000 soldiers fight in a 10k city at ≥ 50 FPS at 1×.

### M10 — Governance and narrative
Ratings, Favor (Senate/Emperor), requests from Rome, laws and policies, advisors, Lua event system and dialogue UI, the province map.
**Exit:** a full mission can be scripted entirely in data + Lua.

### M11 — Campaign vertical slice
Campaign framework (career, ranks, savings, unlocks, civic research), **missions 1–4** fully playable, game UI skin, main menu, settings, save slots.
**Exit:** someone else can play missions 1–4 start to finish without help. **First public release (v0.1).**

### M12 — Campaign: Republic
Missions 5–10, remaining buildings and chains, monuments framework.
**Exit:** missions 1–10 completable; balance pass.

### M13 — Campaign: Empire
Missions 11–16, regional art variants, the large monuments, a performance pass at 10k population and full systems.
**Exit:** mission 16 runs at 10k population at 4× within budget; the full campaign is completable.

### M14 — Sandbox, editor, modding, 1.0
Sandbox mode, random map generator, scenario editor, mod loader UI, modding docs, polish, and accessibility basics (UI scale, colorblind-safe overlays).
**Exit:** v1.0 released on GitHub for all three platforms.

---

## Standing rules
- A perf regression > 15% on `bench` blocks merging.
- Each milestone ends with a short `docs/milestones/Mxx-retro.md`: what changed in the design, what to fix.
- Playtest your own build at the end of every milestone, not only through headless runs.
