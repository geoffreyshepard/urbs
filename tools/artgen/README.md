# URBS art generators

All sprites are produced by code, using the locked 48-color palette in
`assets/palettes/urbs.pal`. Never edit a generated PNG. Change the generator
and regenerate instead. Output is deterministic: every random choice comes from
`primitives.rng(key)`, which is `random.Random` seeded with a CRC of a fixed
string, so reruns are byte-identical.

## Run (from the repo root)

```bash
python3 tools/artgen/build_all.py     # -> assets/sprites/<family>/<name>.png + meta.json
python3 tools/atlas/pack.py           # -> assets/atlas/main.png + main.json
python3 tools/artgen/contact_sheet.py # -> build/contact_sheet.png, build/contact/*.png,
                                      #    build/scene_test.png, build/scene_city.png
```

`build_all.py` wipes `assets/sprites/` first. It then checks every pixel
against the palette (each pixel must be an exact palette color or fully
transparent) and checks the naming contract below. It fails if either check
fails.

## Files

| File | Contents |
|---|---|
| `palette.py` | Loads `urbs.pal`; named ramps ordered dark to light (`C.grass[0..3]`, `C.roof_terracotta`, `C.marble`, `C.sea`, ...); `C.outline`; `check_image()` |
| `primitives.py` | `rng`, `hash2`, `px`, `rect`, `line`, `shaded_box`, `roof`, `column`, `dither`, `speckle`, `outline`, `ellipse_mask`, `shade_blob` |
| `terrain.py` | Base tiles, edges, corners, cliffs, water shimmer |
| `roads.py` | Dirt and paved road pieces, built from one 16-periodic texture per road type |
| `objects.py` | Trees, bushes, reeds, boulders, resource overlays |
| `people.py` | Citizens: template x outfit x direction x frame |
| `buildings/placeholder.py` | Generic building blocks for footprints 1x1 to 4x4 |
| `fx.py` | Tint dot, white block, selection frames |
| `effects.py` | Fire, smoke, rubble, scaffolding, foundation, plague/alert marks, carts, ships |
| `units.py` | Legionary, archer, cavalry, barbarian (walk + attack), ballista |
| `goods.py` | Goods icons: 7x7 carried and 16x16 UI |
| `ui.py` | God medallions and advisor portraits |
| `buildings/kit.py` | Building kit: projection helpers, faces, roofs, colonnades, pediments, domes, arcades, props |
| `buildings/works.py`, `buildings/civic.py` | One recipe per fixed building id (sizes come from `data/defs/buildings.json`) |
| `buildings/fixed.py` | Runs the recipes; stops if a non-painted building has no recipe |
| `buildings/housing.py` | Housing plots and tiers |
| `buildings/painted.py` | Painted tiles, farm huts, line structures |

## Naming contract (the C++ renderer depends on these names)

Anchors are in `assets/sprites/meta.json` and `assets/atlas/main.json`
(`ax`, `ay`). A sprite with no anchor listed uses (0, 0).

- `terrain/<T>_<0..3>`, where T is one of grass, fertile, sand, rock, forest, marsh, shallow, deep. These are opaque 16x16 tiles, and any variant tiles seamlessly with any other.
- `terrain/edge_<T>_<n|e|s|w>`: terrain T bleeding in from that side. `terrain/corner_<T>_<ne|se|sw|nw>`: the same for a diagonal-only neighbour.
  Blend priority, lowest first: deep < shallow < marsh < sand < rock < grass < fertile < forest. On each tile, draw the edges of every higher-priority neighbour terrain, lowest priority first. Draw a corner only when the diagonal neighbour is T and neither orthogonal neighbour next to that corner is T.
- `terrain/cliff_<s|e|w>`: drawn on a tile whose south, east or west neighbour is lower.
- `terrain/water_shimmer_<0..3>`: animation overlay frames for water tiles.
- `road/<dirt|paved>_c` plus `road/<R>_<n|ne|e|se|s|sw|w|nw>`: the center piece plus one spoke per connected neighbour. Pieces can be drawn in any order.
  - Extra, optional: `road/<R>_fill_<ne|se|sw|nw>`. Draw it on a tile whose two orthogonal neighbours at that corner are joined by a diagonal road. For example, draw `fill_nw` when the N neighbour has a `sw` spoke to the W neighbour. Without it, diagonal roads narrow to a single pixel at every tile corner.
- `obj/tree_<0..5>`, `obj/bush_<0..2>`, `obj/reeds_<0..2>`, `obj/boulder_<0..2>`: anchor at the bottom center, `(w//2, h-1)`.
- `obj/res_<clay|stone|iron|silver|marble|limestone|pozzolana|fish>`: 16x16 overlays, anchor (0, 0).
- `bld/placeholder_<w>x<h>`, w and h from 1 to 4. The sprite is `w*16` wide and `h*16 + 8 + 4*min(w,h)` tall. Anchor at the bottom left, `(0, H-1)`.
- `people/<K>_<n|e|s|w>_<0..3>` and `people/<K>_idle_<D>`, where K is one of patrician, eques, pleb, liberti, slave, soldier. Also `people/child_<D>_<0..3>`, plus the extra `people/child_idle_<D>`. Each is an 8x12 canvas with the anchor at (4, 11).
- `fx/dot` (2x2 white), `fx/white` (4x4 white), and `fx/select_<w>x<h>` (footprint-sized frame, anchor (0, 0)).

### Phase 2

`build_all.required_names()` checks all of these names too.

- `bld/<id>` for every non-painted building in `data/defs/buildings.json`. Width is `w*16`, height is `h*16 + 8 + 4*min(w,h)`, and the anchor is the bottom-left pixel.
- Housing tiers, anchor at the bottom left:
  - `bld/house_plot` (an empty surveyed plot) and `bld/house_plot_t0..t4`. All are 32x48 except `t4`, which is 32x64 (16 px taller).
  - `bld/domus_plot` and `bld/domus_plot_t0..t2`, all 48x68.
- Painted tiles: `bld/<id>_tile_<0..3>`, 16x16. Farm-category ids also get `bld/<id>_hut`, 16x28 with the anchor at the bottom left.
- Line structures: `bld/<wall|aqueduct|pipe|sewer|bridge>_<c|n|e|s|w>`.
  - Draw the center first, then the spokes. Draw tiles north to south.
  - Wall (16x26) and aqueduct (16x34) are tall, anchored at the bottom left. Pipe, sewer and bridge are flat 16x16 overlays.
- Construction: `fx/scaffold_<0..3>` and `fx/foundation`, 16x16.
- Effects:
  - `fx/fire_<0..3>`: 16x24, anchor (0, 23).
  - `fx/smoke_<0..3>`, `fx/rubble_<0..3>`: 16x16.
  - `fx/plague_mark`, `fx/alert`: 8x8.
  - `fx/cart_<d>`: 16x12, anchor (8, 11).
  - `fx/ship_<d>`: 24x24, anchor (12, 23).
- Goods: `goods/<id>` (7x7) and `goods/<id>_16` (16x16) for every good, plus `goods/denarii` and `goods/denarii_16`.
- UI: `ui/god_<name>` (16x16) and `ui/advisor_<name>` (32x32).
- Units, where unit is one of legionary, archer, cavalry, barbarian:
  - `people/<unit>_<d>_<0..3>` (walk) and `people/<unit>_atk_<d>_<0..1>` (attack).
  - Legionary, archer and barbarian are 8x12 with anchor (4, 11). Cavalry is 12x14 with anchor (6, 13).
  - `people/ballista_<d>`: 16x16, anchor (8, 15).

## Style rules

- Tiles are 16x16. Light comes from the top-left. Each material uses one highlight shade and one shadow shade.
- Buildings, units and objects get a dark outline (`C.outline`). Terrain never has an outline.
- Class reads from clothing:
  - Patrician: white toga with a purple border.
  - Eques: off-white tunic with narrow purple stripes.
  - Pleb: browns.
  - Liberti: muted green with a pileus cap.
  - Slave: gray.
  - Soldier: red tunic, gray armor and helmet.
