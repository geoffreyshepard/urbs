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
  (y, south). Unrotated, fronts and doors face grid south (the front-left wall on screen); see
  Rotation below.
- Directional sprites keep the grid names: `_n/_e/_s/_w` (and `_ne ...`) mean grid directions, drawn
  in the screen directions above. A citizen sprite `_e` walks toward the screen's lower right.
- Dual-grid terrain masks keep their bits: NW corner = 1 (the diamond's top vertex), NE = 2 (right
  vertex), SW = 4 (left vertex), SE = 8 (bottom vertex).
- `<name>_lit`: night window glow, same canvas and anchor, rendered by `kit.render_lit` (windows
  must use the `window` material).
- `ui/*` and `goods/*` icons stay as they are in `assets/sprites/` (UI art, not world art).
- `tools/atlas/pack.py` packs `assets/sprites_iso` plus the legacy `ui/` and `goods/` folders. It may
  pack below native size; `main.json` records `"scale"` (atlas texels per iso px) and the renderer
  divides by it (`Atlas::scale`, `iso_w`, `iso_h`).

## Rotation

Players turn a building before placing it (E clockwise, Q or Shift+E counter-clockwise; R stays the
road tool). The contract between the sim, the game and the art:

- `Building::rot` 0..3 = quarter turns **clockwise as seen from above (map north up)**. The front /
  door faces 0 = south (+y, the unrotated default), 1 = west (-x), 2 = north (-y), 3 = east (+x).
  On screen: 0 = down-left, 1 = up-left, 2 = up-right, 3 = down-right.
- Footprint on the map: `size: [fw, fh]` for even rot, `(fh, fw)` for odd rot
  (`data::rotated_size`). `Building::w/h` always hold the footprint as placed.
- The entrance (`World::compute_entrance`) prefers the outside tiles along the front side, nearest
  its middle, then the two sides, then the back (roads and reachability still outrank sides).
- Sprite names: rot 0 is unchanged. For rot k > 0 insert `_r<k>` after the base / tier part and
  before `_lit` / `@region`: `bld/bakery_r1`, `bld/bakery_r1@east`, `bld/bakery_r1_lit`,
  `bld/house_x_t2_r3`, `bld/house_x_t2_r3_lit`. The canvas is that of the rotated footprint.
- Anchor: unchanged, the bottom (SE) corner of the actual map footprint,
  `iso::footprint_anchor(b.x, b.y, b.w, b.h, level)` with the rotated w/h.
- Rotatable = buildings drawn as one footprint sprite (base or `_t<k>` tiers). Not rotatable: painted
  area buildings (farms, plazas, gardens, stockpiles: `_tile_<v>`), line / autotiled structures
  (aqueducts, walls, pipes, sewers, bridges: `_c`, `_n`...). The default is
  `!painted && layer == none`; `"rotatable": true/false` in `buildings.json` overrides it. The sim
  stores rot 0 for non-rotatable defs whatever the command says.
- Missing art: `WorldView` resolves every rotated sprite once at init. A missing `_r<k>` sprite
  falls back to the unrotated sprite for square footprints and to the size-matched placeholder
  (`bld/placeholder_WxH`, rotated size) for non-square ones.
- Commands carry the rotation in `Command::flags` bits 1-2 (x1, y1 = the rotated w, h), so replays
  and the chaos player reproduce it; scenario `start.buildings` and `orders` accept `"rot"`.
  The autoplay bot places everything at rot 0. Saves store it from version 4.

## Art kit (`tools/artgen3d/`)

`kit.py` holds the camera, lights, materials (`kit.palette()`), parts (boxes, gable/hip/ring roofs,
columns, steps, fences, sacks, amphorae, carts, trees, lots) and the framing/render helpers.
`run.py` renders one family module: `Blender -b --python tools/artgen3d/run.py -- <family> [names]
[--samples N]`. Each family module declares `SPRITES = {name: spec}` (see `run.py`) and may define
its own extra materials and parts. Families never edit `kit.py`; shared fixes go through the lead.

### Rotated buildings

Buildings can be placed in four rotations. `rot` 0..3 = quarter turns **clockwise seen from above
(map north up)**; the front faces grid south / west / north / east (screen down-left / up-left /
up-right / down-right) for rot 0 / 1 / 2 / 3. The map footprint is `(fw, fh)` for even rot and
`(fh, fw)` for odd rot. Sprite names insert `_r<k>` after the base/tier part and before `_lit` /
`@region`: `bld/bakery_r1`, `bld/bakery_r1@east`, `bld/bakery_r1_lit`, `bld/house_plot_t2_r3_lit`.
rot 0 keeps the plain names. Canvas and anchor follow the usual rule on the rotated footprint
(width `(w + h) * 64`, `ax = w * 64`, `ay` = canvas height); anchors live in the family meta json.

Only one-footprint building sprites are rotated: bld/ base, tier and regional sprites and their
`_lit` overlays. Line pieces (`_c/_n/_e/_s/_w`), area tiles (`_tile_<v>`), area huts (`_hut`),
placeholders, roads, terrain, people, fx and `obj/` sprites are not (`run.py: rotatable()`).

Render them with `--rot`: `Blender -b --python tools/artgen3d/run.py -- <family> [names] --rot all`
(or `--rot 1,3`); `--list` with `--rot` prints the file names it would write. The scene is built
as usual, then every object except the sun/fill lights and the camera is turned `-90 * k` degrees
about z through the footprint centre (Blender +y is grid north), and framed with the rotated
footprint. Sun and camera stay fixed, so light and shadows match the unrotated art; the backs of
buildings become visible in rot 1 and 2. Anchors are merged into the meta json under a file lock,
so several Blender processes may render subsets of one family at once. Sample counts match the
originals: 48 for `civic` and `logistics`, the default 64 for the others.

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
