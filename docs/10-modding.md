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
    sprites/            optional PNGs named like atlas sprites (see "Sprites" below)
      bld/my_temple.png  bld/my_temple_r1.png  goods/olive_paste_16.png  sprites.json
```

Urbs looks for `mods/` in two places:
- next to `data/`, for mods shipped with the game;
- in the user data folder, `~/Library/Application Support/urbs/urbs/mods/` on macOS.

To enable mods, open **Main menu → Mods**. Enabled mods load in list order after the base game:
- An entry whose `id` already exists overrides only the fields it gives.
- A new `id` adds a new entry.

Errors name the file and the entry, and the game refuses to start a broken mod rather than guessing.

`mods/example_vintners_guild/` is a small working example. It adds amber wine, a vintner's cellar with its own sprites (rendered by `art/vintners_art.py`) and a new trade partner.

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
| `rotatable` | Can be turned before placing (default: true unless `painted` or `layer`); needs `_r1`..`_r3` sprites (docs/13-isometric.md) |
| `passable` | Citizens walk through it (aqueduct arches) |
| `needs_pipe` | Works only with piped water (fountain, thermae) |
| `water_radius` | Wells and fountains: where houses count as watered |
| `carts` | Workers are carters who haul long-distance loads of 40 |
| `crime_cut`, `favor_days` | Civic institutions: % less crime city-wide while staffed; +1 favor every N days |
| `dedication` | Monuments: `{"favor", "mood", "days", "god", "god_mood", "culture"}` applied when completed |

Monuments (`kind: monument`) rise in four stages, each needing a quarter of the materials.

Sprites: `bld/<id>` (or the def's `"sprite"` name) from the atlas or a mod's `sprites/` folder (see "Sprites" below), or a grey placeholder of the right size if there is none. Regional variants are named `bld/<id>@north`, `@east` and `@egypt`.

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
- `start`: `{"treasury", "roads", "buildings", "households"}`. Each pre-placed building is `{"def", "x", "y"}` (top-left tile), with optional `"rot"` (0–3 quarter turns clockwise; 0 faces south), `"size"` (painted buildings), `"stock"` and, for houses, `"tier"` (starts evolved; it devolves as usual if its needs go unmet). Each household entry is `{"building": index into "buildings", "count", "members", "food", "savings"}`, with optional `"class"` (`"pleb"` by default, `"patrician"`, `"eques"`, `"libertus"`).
- `start_stock`: {good: n}, stored in a stockpile at the end of the entry road.
- `start_slaves`.
- `policy`: `{"wage", "rations", "slave_treatment", "manumission"}`.

**Gods and trade**
- `gods`: the gods worshipped.
- `trade`: `{"partners": [...], "open": [...], "import": {good: level}, "export": {...}, "stockpile": {...}}`.

**Goals**
- `goals`: `{"population", "culture", "prosperity", "security", "favor", "requests", "survive_days", "buildings": {id: n}}`. All must be met at once to win.
- `goals.max_debt_days` (default 32): on Imperium, the mission is lost after this many days in debt. On Civitas and Imperium it is also lost when favor reaches zero; on Otium a mission cannot be lost.

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

## Sprites

The game's world art (buildings, terrain, people, effects) is pre-rendered 3D seen from an isometric camera: procedural Blender models in `tools/artgen3d/` rendered to PNGs, packed into the atlas by `tools/atlas/pack.py` (docs/13-isometric.md, docs/07-art-pipeline.md). Goods and build-category icons are rendered the same way; the other UI icons come from the pixel-art scripts in `tools/artgen/`. A mod building with no sprite anywhere is drawn as a grey placeholder of its size.

A mod can ship its own sprites. They are packed when the mods load (at start and on **Mods → Apply**), not when the atlas is built.

**Files.** Put PNGs (RGBA, transparent background) in the mod's `sprites/` folder. The path below `sprites/` is the sprite's atlas name:
- `sprites/bld/my_temple.png` is `bld/my_temple`.
- `sprites/goods/olive_paste_16.png` is `goods/olive_paste_16`.

The usual name suffixes apply:
- `_r1`..`_r3` for rotations.
- `_t<k>` for house tiers and `_v<n>` for house looks.
- `_lit` for night window glow.
- `@north`, `@east`, `@egypt` for regions.

Their order is `bld/<id>[_t<k>][_v<n>][_r<k>][_lit][@region]` (docs/13-isometric.md, "Rotation"). Sprites live in group folders such as `bld/`, `goods/`, `ui/`, `people/`, `obj/` and `fx/`. A PNG directly in `sprites/` is refused.

**Scale.** Draw world sprites at the game's native scale: one tile is a 128 x 64 px diamond. The game resamples them to the atlas scale when it packs them (0.625 today), as the packer does for the base art. Icons (`goods/`, `ui/`) are packed 1:1.

**Building sprites** are anchored on the bottom (south) corner of their footprint. For a building of `size: [N, M]` (N east, M south):

| | |
|---|---|
| Canvas width | Exactly `(N + M) x 64` px: 128 for 1x1, 256 for 2x2, 320 for 3x2 or 2x3, 384 for 3x3, 512 for 4x4 |
| Canvas height | At least `(N + M) x 32` px (the flat footprint diamond), plus whatever the building rises above it. The footprint's bottom corner is the canvas's bottom edge |
| Anchor | Inferred: `ax = N x 64`, `ay` = canvas height. For `_r1` and `_r3` the footprint is turned, so `ax = M x 64`; the width is the same |
| Front | Unrotated, the front and door face grid south (screen lower left); `_r1` faces west, `_r2` north, `_r3` east |

The footprint comes from the building def whose `sprite` name the file is named after, so it includes buildings the mod itself adds. A building sprite of the wrong width is refused. `tools/artgen3d` frames sprites exactly this way, with `fp=(N, M)`.

**Other sprites.**
- Icons (`goods/`, `ui/`) are anchored at their centre. Sizes:
  - `goods/<id>_16`: 64 x 64. The UI draws it at about 20-30 points.
  - `goods/<id>`: 40 x 40. It is carried over walking citizens' heads.
  - `ui/cat_<id>` and `ui/icon_<id>`: 96 x 96.
- Everything else (props, people, line and area pieces such as `bld/<x>_c` or `bld/<x>_tile_0`) is anchored at its bottom centre. That is right for any square footprint and for a sprite standing on one point.

**Anchors by hand.** Give an anchor in `sprites/sprites.json` when the inferred one is wrong. Coordinates are native PNG pixels; an entry overrides the inference and the width check:

```json
{"obj/my_statue": {"ax": 31, "ay": 88}, "bld/my_odd_hall": {"ax": 200, "ay": 240}}
```

`sprites/meta/*.json` files, which the Blender kit writes, are read the same way, before `sprites.json`.

**Overrides and load order.** Sprites follow the mods' enable switches and load order, as defs do:
- A mod sprite replaces the base sprite of the same name.
- A later mod's sprite replaces an earlier mod's.
- Replacing a building's art also hides the base versions the mod does not ship itself. A mod that ships only `bld/bakery` hides the base `bld/bakery_r1`..`_r3`, `bld/bakery_lit` and `bld/bakery@east`. The new bakery is then shown turned (turns fall back to the unrotated sprite for square footprints, and to the placeholder for others), at night and in every province. Likewise, `bld/bakery@east` hides `bld/bakery_r1@east`.
- Tiers (`_t<k>`) are separate art. Replacing `bld/house_plot_t2` leaves the other tiers alone, but it hides that tier's base looks (`_t2_v1`..`_v3`).

**Limits and errors.** Mod sprites never stop the game. A file that cannot be used is skipped with a warning naming the file and the reason; the base sprite, or the placeholder, is drawn instead. The first warning shows in the status line and the rest go to the console log. A file is refused when it:
- is not a readable PNG;
- is larger than 32 MB;
- has a side longer than 2048 px (1024 for icons);
- is the wrong width for its footprint;
- does not fit the space left.

All enabled mods share up to four extra atlas pages of 4096 x 2048 texels (32 MB of video memory each, allocated only when a mod ships sprites). At the atlas scale, that is room for several hundred building sprites.

**Making sprites with the game's kit.** Write a family module like those in `tools/artgen3d/` (one `SPRITES = {name: spec}` dict; see `tools/artgen3d/run.py`), keep it in your mod, and render into the mod's `sprites/` folder:

```bash
URBS_ART_OUT=mods/my_mod/sprites /Applications/Blender.app/Contents/MacOS/Blender -b \
    --python tools/artgen3d/run.py -- mods/my_mod/art/my_art.py              # rot 0, icons
URBS_ART_OUT=mods/my_mod/sprites /Applications/Blender.app/Contents/MacOS/Blender -b \
    --python tools/artgen3d/run.py -- mods/my_mod/art/my_art.py bld/ --rot all   # _r1.._r3
```

`mods/example_vintners_guild/art/vintners_art.py` renders the vintner's cellar (with its turns and night lights) and the amber wine icons this way. It reuses `works.py`'s helpers and `icons.py`'s amphora. Any other tool works too, as long as it follows the sizes above. Render, don't hand-paint: change the script and render again.
