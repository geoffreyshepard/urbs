# 09 — Assumptions and Open Questions

## Decisions defaulted (change any of these freely)

| Topic | Default chosen | Why |
|---|---|---|
| Working title | "Urbs" | Placeholder |
| Campaign structure | New city per mission, linked by a career layer and a province map where past cities become trade partners | You leaned toward "start new cities as you go"; this keeps continuity |
| Placement | Grid-snapped, 8-direction roads, with Syx-style painted workplaces plus fixed-footprint civic buildings | "Yes to both" read as grid + diagonals |
| Time scale | 1 day ≈ 60 s at 1×; 16-day years | Daily life is visible and generations still turn over in long missions |
| Services | Visit-based (citizens go to them); patrols only for vigiles, engineers, guard | Fits "see each citizen going about their life" |
| Military | Unit-level orders, individual soldiers simulated, light tactics (formation, stance) | "Straightforward with light tactics" |
| Difficulty | Per-mission designed difficulty + global Otium/Civitas/Imperium | "Balanced depending on level" |
| Determinism | Same-binary reproducible, not cross-platform bit-exact | No multiplayer |
| Scripting | Lua 5.4 for missions, events and mods | Modding = yes |
| Audio / localization | Not built; architecture leaves hooks | "Not right now" |
| Minimum spec | Apple Silicon M1, 8 GB; comparable Windows/Linux PCs | "Whatever my Mac runs well" |
| License | **Decided:** PolyForm Noncommercial 1.0.0 (code), CC BY-NC-SA 4.0 (assets, data, docs) | Free to use and contribute; no commercial resale |
| Housing gating | Tiers unlock progressively (see catalog "Reachable from") | Fixed contradictions where tiers required buildings that unlock later |
| Taxes | Forum from mission 1 (the city pays wages, so it must be able to tax); per-class rates are a policy | Playtesting with the autoplay bot showed a city without tax collection always goes bankrupt |
| Money flow | Closed loop: wages → households → taxes and market sales → treasury. In steady state the treasury's real income is exports (and Rome's rewards) minus construction, so trade matters from mission 2 | Keeps the economy legible: unsold production is wages spent for nothing |
| Difficulty effects | Otium: threats ×0.5, raids ×0.6, treasury ×1.5, gentler request penalties, no failure. Imperium: threats ×1.5, raids ×1.4, treasury ×0.75, request penalties ×1.5, failure on long debt | docs/02 §15 |
| Stockpiling | Per-good trade mode "stockpile": markets leave the good in storage (for Rome's requests) | Players otherwise can't gather goods that citizens also consume |
| Unreachable raiders | Raiders with no path into the city withdraw | They used to wait forever at the map edge |
| Walking speed | Roads are fast (dirt 1.5, paved 1 tick per tile) | Commutes otherwise ate the working day; the autoplay bot could not grow past ~2,000 |
| Exports at the trade post | Goods staged for export are not handed back out to markets | Markets drained the staged goods and caravans found nothing |
| Housing devolution | After three failed daily checks (was two) | Single-day hiccups caused eviction cascades |

## Open questions (decide before the milestone that needs them)

| Question | Needed by |
|---|---|
| Final game name | M11 (first public release) |
| Should slaves be visibly owned by specific households in the inspector, or shown more abstractly? | M7 |
| How punishing should mission failure be: replay from the start, or reload any save? | M11 |
| Off-map military campaigns: in every mission or only military-focused ones? | M9 |
| Do former cities ever come under threat on the province map (callbacks to earlier missions)? | M10 |
| Naval combat: never, event-only, or real? | M12 |
| Advisor portraits: pixel portraits, or text-only advisors? | M10 |
