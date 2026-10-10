# URBS art generators

All sprites are produced by code, using the locked 56-color palette in
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
| `hud.py` | Logo, meander ornaments, HUD icons |
| `worldmap.py` | Province map (coastline polygons in lon/lat) and `province_places.json` |
| `buildings/kit.py` | Building kit: projection helpers, faces, roofs, colonnades, pediments, domes, arcades, props |
| `buildings/works.py`, `buildings/civic.py` | One recipe per fixed building id (sizes come from `data/defs/buildings.json`) |
| `buildings/fixed.py` | Runs the recipes; stops if a non-painted building has no recipe |
| `buildings/housing.py` | Housing plots and tiers |
| `buildings/painted.py` | Painted tiles, farm huts, line structures |

## Naming contract (the C++ renderer depends on these names)

Anchors are in `assets/sprites/meta.json` and `assets/atlas/main.json`
(`ax`, `ay`). A sprite with no anchor listed uses (0, 0).

- `terrain/<T>_<0..3>`, where T is one of grass, fertile, sand, rock, forest, marsh, shallow, deep. These are opaque 16x16 tiles, and any variant tiles seamlessly with any other.
- Terrain is drawn on a **dual grid**: one 16x16 cell centred on every tile corner (cell (x, y) at pixel
  (x*16-8, y*16-8), its corners on the centres of tiles (x-1, y-1), (x, y-1), (x-1, y), (x, y); cells run to
  x = w and y = h). Blend priority, lowest first: deep < shallow < marsh < sand < rock < grass < fertile < forest.
  - Draw the cell's lowest-priority corner terrain as a full base tile `terrain/<T>_<v>`.
  - Then for each higher terrain L present, lowest first, draw `terrain/dg_<L>_<mask>` (mask 1..14, bit NW=1, NE=2,
    SW=4, SE=8, set for each corner whose terrain has priority >= L). `terrain.dual_layers()` is the reference.
- `terrain/hc_<mask>`: height steps, on the same cells. For each level L with min < L <= max of the four corner
  heights, draw the contour whose mask marks the corners at height >= L. Same contour noise as `dg_`, so cliffs follow
  coastlines.
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
- Buildings, units and objects get a dark outline (`C.outline`). Terrain never has an outline. After generation,
  `outlines.py` recolours the outer silhouette with the dark partner of the bordering material (`C.ol_*`); interior
  lines stay `C.outline`.
- Ground surfaces inside a building sprite (yards, lawns, paving plates: the layers put with `ol=False`, and the
  housing ground layer) are stored with alpha 254 (`primitives.flat`). They look the same; the renderer's cast-shadow
  pass skips them, so only raised parts throw shadows. `check_image` accepts alpha 0, 254 and 255.
- Class reads from clothing:
  - Patrician: white toga with a purple border.
  - Eques: off-white tunic with narrow purple stripes.
  - Pleb: browns.
  - Liberti: muted green with a pileus cap.
  - Slave: gray.
  - Soldier: red tunic, gray armor and helmet.

- Optional: `bld/<name>_lit` (same size and anchor as `bld/<name>`): warm window pixels for night, found by
  `lights.py` (small filled C.outline rectangles set into a wall). The renderer adds them over the graded frame.

### Phase 3

- `ui/province_map` is 512x288, covering lon -10 to 40 and lat 25 to 56 in an equirectangular projection.
  - `assets/sprites/province_places.json` maps each place name to `{"x", "y"}`, normalised 0..1 on that image.
  - Each place is snapped onto land, so it never sits in the sea.
- `ui/logo` (about 200x60), `ui/panel_corner` (8x8, drawn as the top-left corner; mirror it for the others) and `ui/divider` (64x6, tileable).
- `ui/icon_<name>`: 16x16 HUD icons.
