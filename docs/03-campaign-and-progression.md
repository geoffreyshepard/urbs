# 03 — Campaign and Progression

## Structure

- **16 missions**, each a **new city on a new map**, spanning the Early Republic to the Empire.
- A persistent **career layer** links the missions: rank, personal savings, unlocked buildings and techniques, and a growing **province map** on which your earlier cities appear as trade partners.
- Mission length grows from about **30 minutes** (mission 1) to about **6 hours** (mission 16). Population goals grow from about **300** to **10,000**.
- History is stylized: real places and eras, loose with dates and details.
- The story (one career told through letters, advisors and events, with a patron, a rival and an ending) is in docs/12-story.md.

## Career ranks

| Rank | Missions | Salary | Notes |
|---|---|---|---|
| Colonus | 1 | Low | Settler-founder |
| Decurio | 2 | | Town councillor |
| Quaestor | 3–4 | | Finance unlocks: per-class tax policy (mission 2 has a flat tax only) |
| Aedile | 5–6 | | Games, markets, public works |
| Praetor | 7–8 | | Command troops, courts |
| Consul | 9–10 | | Full military and law powers |
| Proconsul | 11–12 | | Province governor; imperial cult |
| Legatus Augusti | 13–14 | | Imperial legate on the frontier |
| Praefectus Urbi | 15–16 | High | Prefect of the City (Rome) |

The salary accrues to the governor's **personal savings**, which carry across missions.

## The missions

| # | Mission | Era | Pop goal | Target length | New systems introduced |
|---|---|---|---|---|---|
| 1 | **Latium — The Founding** | Early Republic | 300 | 30 min | Roads, housing (tent → hut), wells, wheat farms, granary, market, shrine, forum and taxes. Tutorial. |
| 2 | **Ostia — River Port** | Early Republic | 600 | 45 min | Warehouses, fishing, salt, first trade route, vigiles (fire), physician, tavern |
| 3 | **Beneventum — Samnite Frontier** | Early Republic | 1,000 | 60 min | Walls, gates, militia, raids, temples and the first gods, paved roads, quarry and stone |
| 4 | **Capua — Campania** | Early Republic | 1,500 | 75 min | Olives, vines, pottery and amphorae; equites; domus; theater; schools; small baths |
| 5 | **Syracuse — Sicilia** | Mid Republic | 2,000 | 90 min | Harbor, sea trade, grain exports, requests from Rome, garum |
| 6 | **Carthago Nova — Hispania** | Mid Republic | 2,500 | 2 h | Mining (iron, silver), smelting, armory, barracks and legionaries, slave markets at scale, Punic threat |
| 7 | **Aquileia — Northern Frontier** | Mid Republic | 3,000 | 2.25 h | Aqueducts and elevation, reservoirs, fountains, pipes, thermae, sewers, forts, barbarian raids |
| 8 | **Narbo Martius — Gallia** | Mid Republic | 3,500 | 2.5 h | Trade hub, carts, dynamic markets, library, basilica, courts, great temple |
| 9 | **Campania — The Servile War** | Late Republic | 4,000 | 3 h | Slave treatment and manumission policy under pressure, servile revolt, amphitheater, hospital, curia |
| 10 | **Corinth — Refounded** | Late Republic | 4,500 | 3.25 h | Greek culture: odeon, academy, marble, dyes, scriptorium; research system expanded |
| 11 | **Lugdunum — Capital of Gaul** | Late Republic → Empire | 5,500 | 3.5 h | Transition to Empire, Emperor's Favor, imperial cult, mint, triumphal arch |
| 12 | **Alexandria — Egypt** | Empire | 5,000 | 4 h | Nile flood cycle, grain fleet, glassworks, circus/hippodrome, Egyptian gods, lighthouse monument |
| 13 | **Colonia Agrippina — The Rhine** | Empire | 7,000 | 4.25 h | Heavy military, limes forts, siege engines, large battles, harsh winters |
| 14 | **Londinium — Britannia** | Empire | 7,500 | 4.5 h | Revolt that burns the city midway, then rebuild under pressure |
| 15 | **Antioch — The East** | Empire | 8,500 | 5 h | Luxury trade, huge markets, earthquake disaster, multi-district management |
| 16 | **Roma — Caput Mundi** | Empire | 10,000 | 6 h | Rebuild Rome after a great fire: Colosseum, Pantheon, Circus Maximus-scale monuments; every system at once |

Alexandria's population goal is lower than its neighbours' because the floodplain limits building land and the lighthouse and circus are large builds.

Each mission has:
- **Win goals:** population, ratings thresholds, specific buildings or monuments, requests fulfilled, sometimes surviving N years.
- **Failure conditions** (on Civitas/Imperium): Favor reaching zero, a city sacked, prolonged debt.
- **Optional goals** that give bonus career rewards.
- A **briefing** with advisor dialogue, and an **epilogue** that also shows notable citizen stories from your city.

## Unlocks

Three layers:
1. **Campaign unlocks (permanent):** each mission adds buildings and systems (table above). Once unlocked, they are available in sandbox mode.
2. **Civic research (per mission):** scholars in schools, libraries and academies generate *Scientia*. Spend it on techniques for this city only, such as crop rotation, hypocaust heating, Roman concrete, arch construction, insula building codes, legion drill reforms. Later missions start with more of the tree available.
3. **Career perks (permanent):** spend personal savings or patronage points earned from optional goals on perks like "Start with 2 extra settler families", "+10% trade prices with your former cities", "Veteran legate: starting cohort".

## Province map

Between missions the player sees a stylized map of the Roman world. Every completed city stays on it as a **trade partner** that sells what it was good at. This is how earlier work pays off later, and it gives the campaign continuity even though each mission is a new city. The map is a painted atlas plate of the Mediterranean (`tools/artgen/province_map_hd.py`); the missions sit on it as numbered medallions, and a line of the same medallions under the map groups them by rank, from Colonus to Praefectus Urbi. Beside it, the selected mission's dossier carries the letter of appointment, the difficulty and the Begin button.

## Sandbox, random maps, editor

- **Sandbox:** pick any unlocked region style, a map size and starting conditions. Optional goals and threats can be toggled.
- **Random map generator:** terrain, rivers, coast, elevation, resources, seeded so maps can be shared.
- **Scenario editor:** paint terrain, place resources and starting buildings, set goals and event scripts (Lua). Scenarios are saved as data and can be shared as mods.

## Mission scripting

Missions are data plus Lua: `data/scenarios/m01_latium/` contains `scenario.json` (map, goals, unlocks, starting state) and `script.lua` (events, advisors, triggers). This keeps the campaign moddable and editable without recompiling.
