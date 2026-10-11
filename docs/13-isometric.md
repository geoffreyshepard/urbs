# 13 — Isometric view

The owner chose pre-rendered 3D art on an isometric camera (option C of the art test in
`tools/arttest/`, 2026-10-11): procedural Blender models rendered to sprites, drawn by the existing
2D sprite renderer. The simulation is unchanged: it keeps its top-down tile grid
(`sim::kTilePx` = 16 world px per tile, x east, y south).

## Coordinates (`src/render/iso.hpp`)

- **iso px**: screen-space world units. One tile is a 128 x 64 diamond (`iso::kTileW`, `kTileH`).
  `iso::from_tile(tx, ty, level)` = `((tx - ty) * 64, (tx + ty) * 32 - level * kLevelPx)`.
  `iso::project(wx, wy, level)` does the same from sim world px. `to_tile` / `unproject` invert it
  on a given height level; `pick_tile` resolves the tile under the mouse honouring terrain height.
- Grid directions on screen: **north (-y) = up-right, east (+x) = down-right, south (+y) =
  down-left, west (-x) = up-left**. Grid diagonals: NE = right, SE = down, SW = left, NW = up.
- Terrain height level `h` (0..7) lifts the ground by `h * kLevelPx` (16 iso px). Everything
  standing on a tile (buildings, citizens, trees) is lifted by that tile's level.
- Painter's order: `iso::depth_for_iso(anchor_iso_y, iso::map_iso_h(map))`; nearer (larger iso y)
  draws in front. Ground layers keep the fixed depths in `terrain.hpp`.
- The camera centre and every `SpriteInstance` x/y/w/h are in iso px. `Camera::screen_to_world` /
  `world_to_screen` map window points to and from **iso px** (names kept).

## Sprite conventions

- Art is rendered at **native scale: 128 x 64 px per tile diamond**, by `tools/artgen3d/`
  (Blender, headless). PNGs go to `assets/sprites_iso/<group>/<name>.png` with the same atlas names
  the game already uses (`bld/bakery`, `people/pleb_s_0`, `terrain/grass_0`, ...). Anchors go to
  `assets/sprites_iso/meta/<family>.json` as continuous pixel coordinates `{ax, ay}`.
- **Ground-attached sprites** (buildings, line pieces, farm/garden/plaza/stockpile tiles, terrain,
  roads, selection marks): anchor = the footprint's **bottom (south-east) corner**, i.e. grid corner
  `(x0 + fw, y0 + fh)` = `iso::footprint_anchor(x0, y0, fw, fh, level)`. Canvas width is exactly
  `(fw + fh) * 64`; `ax = fw * 64`, `ay = canvas height`. A flat 1x1 tile is 128 x 64 with anchor
  (64, 64).
- **Free sprites** (citizens, units, trees, carts, ships, fire, smoke): anchor = the foot / ground
  contact point = `iso::project(world_pos, level)`.
- Building footprints: `size: [w, h]` in `data/defs/buildings.json` is `fw` (x, east) by `fh`
  (y, south). Fronts and doors face grid south (the front-left wall on screen).
- Directional sprites keep the grid names: `_n/_e/_s/_w` (and `_ne ...`) mean grid directions, drawn
  in the screen directions above. A citizen sprite `_e` walks toward the screen's lower right.
- Dual-grid terrain masks keep their bits: NW corner = 1 (the diamond's top vertex), NE = 2 (right
  vertex), SW = 4 (left vertex), SE = 8 (bottom vertex).
- `<name>_lit`: night window glow, same canvas and anchor, rendered by `kit.render_lit` (windows
  must use the `window` material).
- Icons: goods icons (`goods/<id>`, `goods/<id>_16`) and build-category icons (`ui/cat_<id>`) are
  rendered by `tools/artgen3d/icons.py` (see "Icons" below). The other `ui/*` art (HUD buttons, gods,
  advisors, logo, province map) is still pixel art in `assets/sprites/ui/`.
- `tools/atlas/pack.py` packs `assets/sprites_iso` plus the legacy `ui/` and `goods/` folders; an
  `assets/sprites_iso` icon wins over a legacy sprite of the same name. World art may be packed below
  native size; `main.json` records `"scale"` (atlas texels per iso px) and the renderer divides by it
  (`Atlas::scale`, `iso_w`, `iso_h`). Icons (`ui/`, `goods/`) are never resampled: legacy ones keep
  `s` = 0.25, rendered ones `s` = 1, and the game draws all of them at explicit sizes.

## Icons (`tools/artgen3d/icons.py`)

Each good and build category is a small model in the world-art look (same camera, sun, palette and
materials), scaled by `fit()` to fill a fixed square canvas, centred (anchor = canvas centre), with a
thin dark Freestyle contour so the silhouette survives small sizes.

| Sprite | Canvas | Drawn at |
|---|---|---|
| `goods/<id>_16` | 64 x 64 | UI: panels, goods chips, trade rows, costs (`denarii_16`), ~20-30 points |
| `goods/<id>` | 40 x 40 | over walking citizens: 20 iso px wide (`world_view.cpp`, `kCarriedIso`) |
| `ui/cat_<id>` | 96 x 96 | build-menu category rail, 24-48 points |

`game::UiIcons` copies icons larger than 32 texels 1:1 into a separate mipmapped texture (the pixel
art keeps its x6 nearest-neighbour texture), so they shrink cleanly to any layout size. Rules: one
distinct silhouette and colour per good; goods sharing a container differ by shape and colour (wine:
slender amphora, red band, dark-red seal; olive oil: globular greenish amphora, green band, golden
stopper; amphorae: two plain empty jars; garum: one-handled jug with a fish). Category icons are a
miniature on a one-tile diamond of the category's ground, using the category accents below.

## Art kit (`tools/artgen3d/`)

`kit.py` holds the camera, lights, materials (`kit.palette()`), parts (boxes, gable/hip/ring roofs,
columns, steps, fences, sacks, amphorae, carts, trees, lots) and the framing/render helpers.
`run.py` renders one family module: `Blender -b --python tools/artgen3d/run.py -- <family> [names]
[--samples N]`. Each family module declares `SPRITES = {name: spec}` (see `run.py`) and may define
its own extra materials and parts. Families never edit `kit.py`; shared fixes go through the lead.

## Readability rules (from the art test brief)

Each category owns an accent no other category uses: housing is quiet (muted plaster, faded
terracotta); food = saffron awnings and warm ochre; industry = charcoal slate roofs, soot yards,
fire glow; storage = dark timber and tan stone, big doors, paved loading yards; religion = white
marble and gold; civic/government = travertine with bronze/green-patina roofs and red banners;
entertainment = striped awnings, banners, curved seating; health/baths = blue-green water, white
plaster; education = pale stone, scroll/porch; military = Roman red and iron; water infrastructure =
grey stone and visible water. Show function with props on the lot and a category ground (garden,
dirt yard, paving, marble). Civic and monumental buildings are taller and lighter than houses.

Regional variants (`@east`, `@egypt`, `@north`) keep the footprint and function props but change
materials and roofs: east = whitewashed stucco, flat or low roofs, blue accents; egypt = mudbrick and
sandstone, flat roofs, palm trees; north = timber frame, thatch or shingle roofs, darker greens.
