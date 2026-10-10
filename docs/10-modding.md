# 10 — Modding

Urbs is data-driven. Goods, recipes, buildings, housing tiers, services, gods, units, trade partners, traits and techniques are JSON. Missions are JSON plus Lua. You can mod without touching C++.

Licence reminder: mods of Urbs fall under the same terms (PolyForm Noncommercial for code, CC BY-NC-SA for data and art). Share them freely; don't sell them.

## Where mods live

```
mods/
  my_mod/
    mod.json            {"name": "My mod", "version": "1.0", "description": "..."}
    defs/               any of the files below; each merges into the base game by "id"
      goods.json  recipes.json  services.json  buildings.json  housing.json
      gods.json  units.json  trade_partners.json  traits.json  techniques.json
    scenarios/
      my_scenario/scenario.json  script.lua  [map.urbsmap]
```

Urbs looks for `mods/` in two places:
- next to `data/`, for mods shipped with the game;
- in the user data folder, `~/Library/Application Support/urbs/urbs/mods/` on macOS.

To enable mods, open **Main menu → Mods**. Enabled mods load in list order after the base game:
- An entry whose `id` already exists overrides only the fields it gives.
- A new `id` adds a new entry.

Errors name the file and the entry, and the game refuses to start a broken mod rather than guessing.

`mods/example_vintners_guild/` is a small working example. It adds amber wine, a vintner's workshop and a new trade partner.

## Definition files

All files are arrays of objects with a unique `"id"`. Amounts are integers.

### goods.json
- `id`, `name`, `category` (free text), `price` (base price in denarii), `unlock` (mission number).
- `food: true` makes it edible; `nutrition` sets how filling it is (1–3).
- Households can hold up to eight food kinds and eight household goods. The household goods are pottery, clothing, olive_oil, wine, furniture, glassware, dyed_clothing and jewelry.

### recipes.json
`id`, `inputs` {good: amount}, `outputs` {good: amount}, `work` (worker-ticks per batch). Optionally `coins` (denarii struck into the treasury per batch, as the mint does). A recipe needs outputs or coins.

### services.json
- `id`, `name`.
- `need`: water, hygiene, health, religion, entertainment or education.
- `quality`: how long a visit satisfies the need (0–255).
- `range`: how far, in walking ticks, a house may be from it. A road costs 1–1.5 ticks per tile.
- `visit_ticks`: how long a visit takes.
- `patrol: true`: the service is a patrol, as for vigiles, engineers and the guard.

### buildings.json
Common fields:
- `id`, `name`, `category` (build-menu group), `kind`, `size` [w, h], `unlock`.
- `cost` {"denarii": n, good: n}, `build_work`.
- `workers`, `worker_classes`.
- `desirability` [value, range], `fire_risk`, `collapse_risk`.

`kind` is one of house, workplace, farm, storage, market, service, patrol, water, civic, military, trade_post, infrastructure, decor, monument.

Kind-specific fields:

| Field | Meaning |
|---|---|
| `recipe` | What a workplace or farm makes |
| `storage`, `accepts` | Inventory size and accepted goods (storage, military) |
| `service`, `service_capacity` | Service buildings |
| `housing`, `house_line` | Houses: base residents; `pleb`, `patrician` or `slave` |
| `resource` | Needs a deposit under the footprint: clay, stone, iron, silver, marble, limestone, pozzolana, fish |
| `needs_water` | Must touch water (wharf, harbor, lighthouse) |
| `needs_road` | Default true (false for farms and decor) |
| `painted`, `min_size`, `max_size` | Area buildings (farms, aqueducts, pipes); cost and workers scale with area |
| `layer` | `pipe`, `sewer` or `bridge` for underground and line structures |
| `passable` | Citizens walk through it (aqueduct arches) |
| `needs_pipe` | Works only with piped water (fountain, thermae) |
| `water_radius` | Wells and fountains: where houses count as watered |
| `carts` | Workers are carters who haul long-distance loads of 40 |
| `crime_cut`, `favor_days` | Civic institutions: % less crime city-wide while staffed; +1 favor every N days |
| `dedication` | Monuments: `{"favor", "mood", "days", "god", "god_mood", "culture"}` applied when completed |

Monuments (`kind: monument`) rise in four stages, each needing a quarter of the materials.

Sprites: `bld/<id>` in the atlas, or a grey placeholder of the right size if the atlas has none. Regional variants are named `bld/<id>@north`, `@east` and `@egypt`.

### housing.json
- `id`, `line` (pleb or patrician), `tier` (0 upward), `name`, `capacity`.
- `requires`: a list, checked in order. Each entry is one of:
  - `{"service": [ids]}`: any of these services.
  - `{"food_kinds": n}`
  - `{"food": "bread"}`
  - `{"goods": [ids]}`
  - `{"desirability": n}`
  - `{"piped_water": true}`
  - `{"sewer": true}`
  - `{"patrol": "guard"}`
- A house rises one tier when it meets every requirement of the next tier. It falls back after three failed daily checks.

### gods.json
- `id`, `name`, `domain`, `festival_day` (day of the 16-day year).
- `blessing` and `wrath` effects, with the text shown for each: `blessing_text`, `wrath_text`.

### units.json
- `id`, `name`, `side` (city or enemy), `sprite`.
- Stats: `hp`, `attack`, `defense`, `speed` (tiles per tick), `range` (above 2 means ranged), `size` (soldiers per unit), `upkeep`.
- `gear`: goods per soldier, used when the unit is raised.
- `morale`, `charge`, `siege`, `unlock`.
- `trained_at`: building ids that can raise the unit.

### trade_partners.json
- `id`, `name`, `route` (land or sea), `open_cost`, `interval_days`.
- `buys` and `sells`: {good: amount per year}.

### traits.json, techniques.json
- Traits multiply rates: `work`, `rest_rate`, `religion_rate`, `hygiene_rate`, `entertainment_rate`, `education_rate`, `skill_rate`, `crime`, `spend`, `lifespan`, `fertility`.
- Techniques: `id`, `name`, `cost`, `desc`. Their effects are built into the game, so mods can rename or reprice them but not add new effects.

## Scenarios

A scenario is a folder with `scenario.json` and an optional `script.lua`.

**Identity**
- `id`, `name`, `mission` (unlock level: buildings with `unlock` ≤ mission are available), `era` (republic, late_republic, empire), `region` (art style: north, east, egypt), `start_hour`.

**Map**
- `map`: `{"size", "seed", "coast": -1 or 0–3 (n/e/s/w), "river", "hills", "forest", "marsh", "arid"}`, generated by the map generator.
- Or `"map_file": "map.urbsmap"` for a map painted in the editor.
- `nile: true` turns on the yearly inundation.
- `entry_road`: the side the road enters from.

**Limits**
- `world`: `{"max_citizens", "max_buildings", "max_households", "max_soldiers"}`.

**Starting conditions**
- `start`: `{"treasury"}`.
- `start_stock`: {good: n}, stored in a stockpile at the end of the entry road.
- `start_slaves`.
- `policy`: `{"wage", "rations", "slave_treatment", "manumission"}`.

**Gods and trade**
- `gods`: the gods worshipped.
- `trade`: `{"partners": [...], "open": [...], "import": {good: level}, "export": {...}, "stockpile": {...}}`.

**Goals**
- `goals`: `{"population", "culture", "prosperity", "security", "favor", "requests", "survive_days", "buildings": {id: n}}`. All must be met at once to win.

**Script**
- `script`: the Lua file.

### Lua API (`urbs.*`)

Scripts are sandboxed: there is no `io`, `os`, `load` or `require`. The game calls `on_start()`, `on_day(day)`, `on_request(id, ok)` and choice callbacks. Store anything you need later in `urbs.state` (a table that is saved with the game).

| Function | Meaning |
|---|---|
| `message(title, text, advisor)` | Advisor 1 engineer, 2 quaestor, 3 haruspex, 4 legate, 5 aedile |
| `choice(title, text, {options}, "callback")` | The callback receives the option number |
| `request{kind="goods", good, amount, days, reward, favor, penalty}` | Returns an id |
| `raid{unit, count, side="north"\|"east"\|"south"\|"west"}` | |
| `trigger(name[, value])` | fire, collapse, plague, riot, revolt, flood, earthquake, drought, storm, blight, blessing, wrath, raid |
| `population()`, `stock(good)`, `treasury()`, `favor()`, `rating(name)`, `happiness()`, `count(building)` | Queries |
| `add_money(n)`, `take_money(n)`, `give(good, n)`, `change_favor(n)` | Changes |
| `unlock(building)`, `lock(building)`, `open_route(id)`, `offer_route(id)` | |
| `change_god(id, delta)`, `god_mood(id)`, `worship(id)`, `festival_day(id)` | |
| `set_empire()` | The Republic becomes an Empire |
| `day()`, `mission_day()`, `year()`, `season()` ("Spring"…"Winter"), `random(n)` (deterministic) | |
| `win()`, `lose(reason)`, `log(text)` | |

### Testing scenarios headless

```bash
./build/release/urbs_headless --scenario path/to/scenario.json --ticks 360000 --autoplay --daily
```

The autoplay bot plays the scenario like a player, which quickly shows whether goals are reachable. `--verbose` adds money, mood and request traces; `--dump` lists every building at the end.

## Art

All sprites are generated by code (`tools/artgen/`, see its README) from a locked 48-colour palette. A mod building without a sprite is drawn as a placeholder of its size. Shipping custom sprites in a mod is planned but not yet supported.
