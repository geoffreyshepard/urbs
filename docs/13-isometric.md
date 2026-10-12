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
- Icons: goods icons (`goods/<id>`, `goods/<id>_16`) and build-category icons (`ui/cat_<id>`) are
  rendered by `tools/artgen3d/icons.py`; HUD icons (`ui/icon_<id>`), god and advisor portraits
  (`ui/god_<id>`, `ui/advisor_<id>`) and the logo (`ui/logo`) by `tools/artgen3d/ui_art.py` (see
  "Icons" below). The campaign's province map is a painted 2560x1440 picture in `assets/ui/`
  (docs/07-art-pipeline.md, "Province map"); panel chrome comes from `tools/uiart/gen_chrome.py`.
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
| `goods/<id>` | 40 x 40 | on walking citizens' heads: 12 iso px wide (`world_view.cpp`, `kCarriedIso`) |
| `ui/cat_<id>` | 96 x 96 | build-menu category rail, 24-48 points |
| `ui/icon_<id>` | 96 x 96 | top-bar stats and buttons (~21 points), panel headers (~38 points) |
| `ui/god_<id>` | 128 x 128 | religion panel (~48 points), temple info (~24 points) |
| `ui/advisor_<id>` | 128 x 128 | advisors panel buttons and messages (56-64 points) |
| `ui/logo` | 600 x 180 | main menu title, 300 x 90 points (`UiIcons::picture`) |

`game::UiIcons` copies icons larger than 32 texels 1:1 into a separate mipmapped texture (the pixel
art keeps its x6 nearest-neighbour texture), so they shrink cleanly to any layout size. Rules: one
distinct silhouette and colour per good; goods sharing a container differ by shape and colour (wine:
slender amphora, red band, dark-red seal; olive oil: globular greenish amphora, green band, golden
stopper; amphorae: two plain empty jars; garum: one-handled jug with a fish). Category icons are a
miniature on a one-tile diamond of the category's ground, using the category accents below.

`tools/artgen3d/ui_art.py` (family `ui_art`) renders the rest of the UI art in the same look; pictures
that face the viewer get an extra warm key light from the upper left (the world sun only grazes
surfaces turned toward the camera). Its rules:

- HUD icons are one object each, distinct in silhouette: population = man and woman, happiness =
  comedy mask, culture = lyre, prosperity = cornucopia, security = round blue shield, favor = gold
  laurel wreath, rome = eagle standard with an SPQR plaque, advisors = painted counsellor bust,
  finance = purse of gold coins, goods = crate, sack and amphora, trade = merchant ship, labor = saw
  and hammer, religion = altar with fire, military = crested bronze helmet, research = wax tablet and
  stylus, alerts = bell, help = scroll with a red "?", overlay = stacked map tiles, road = paved
  stretch, build = trowel on bricks, demolish = pickaxe and broken bricks.
- Speed controls (pause, play, fast, faster) and the menu button are geometric glyphs, not objects:
  dark bronze faces with a gilt bevel and a heavier contour, so they read on light, ochre, red and
  dark buttons alike. They share one scale (`span`), so the bars and triangles are the same height.
- Portraits are busts on a round medallion (bronze rim, gilt bead, a coloured ground per figure). Gods
  are white marble with an attribute (Jupiter thunderbolt and laurel, Juno diadem and peacock fan,
  Mars helmet and spear, Venus scallop shell, Minerva helmet and owl, Mercury winged petasos and
  caduceus, Neptune trident, Vesta veil and flame, Ceres wheat crown and sheaves, Vulcan cap, hammer
  and anvil, Isis wig with sun disc and horns, Serapis modius and sceptre, Divus laurel and purple
  border). Advisors are painted: quaestor toga, purse and coins; engineer tunic, groma and dividers;
  haruspex veil and lituus; legate helmet, cuirass and red cloak; aedile toga praetexta and scroll.
  The head and headgear are scaled up (`HEAD_SCALE`) so faces survive ~40-point display.
- The logo is "URBS" in Cinzel (`assets/fonts/Cinzel.ttf`), marble on a dark bronze offset copy, between
  two gilt laurel branches. Cinzel is a variable font with overlapping contours, so `text_mesh` fills
  each outer contour separately and merges them with a voxel remesh.
- `game::UiIcons` keeps the logo 1:1 (pixel-art big pictures are still upscaled x3) in a mipmapped
  texture; the religion panel draws god medallions without an extra square frame.

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
  The autoplay bot places everything at rot 0 except houses, which face their street (see House
  facing). Saves store it from version 4.

### House facing

Houses (2x2 `house_plot`, 3x3 `domus_plot`: square footprints, so a turn never moves the footprint)
turn their front to a road by themselves. `World::road_facing_rot(x, y, w, h)` counts the road
tiles along the outside line of each side and returns the rotation whose front touches the most;
ties go to the fronts the camera sees (0 south, 3 east, then 1 west, 2 north); no road gives rot 0.
The result goes into the ordinary place command's rotation bits, so it is replayed like any other
rotation. Who uses it: the player's build tool (the ghost and the door mark follow the street under
the cursor; pressing E / Q switches that house type to the player's own turn, picking it from the
build menu again returns to auto-facing), the autoplay bot, and the town generator's house ring.

## House variants

Every housing tier has four looks so a block of houses does not read as wallpaper. Names:
`bld/<house>_t<k>_v<n>` for n = 1..3; variant 0 keeps the plain `bld/<house>_t<k>` (the build menu
and the placement ghost show it). Rotation comes after the variant, then `_lit`, then the region:
`bld/house_plot_t2_v1`, `bld/house_plot_t2_v1_r3`, `bld/house_plot_t3_v2_r1_lit`,
`bld/domus_plot_t1_v3_r2@east`.

- Rendered for `house_plot` t0-t4 and `domus_plot` t0-t2, every region, all four rotations, `_lit`
  where the tier has night lights (Italian casa and insulae t2-t4, grand domus t1).
- Same footprint and the same tier cues: thatch and canvas up to t1, tiles from t2, one storey (t2),
  two (t3), three (t4), base bands from t3, gardens and peristyles for the domus. Variants change the
  roof form (hip, gable end to the street, lean-to, a light well), the plaster (cream, ochre, pale
  pink, white, light terracotta tones; regional palettes in the provinces), shutters and doors
  (brown, green, red, blue), the roof tone (`kit.TERRACOTTA`, `M["roof_v<n>"]`), and the lot: an
  annex or wing, a vine pergola, a walled garden, a shop with an awning on the street (insulae and
  domus v2), an upper room (domus v3), chimneys, washing lines, animals. A variant keeps its index
  through the tiers (a pink v2 casa grows into a pink v2 insula).
- `WorldView` picks the look from the building's map position (`WorldView::variant_at`, a hash of
  x and y): render only, nothing in the sim or the save. It counts the variants that exist per tier
  at init (contiguous from v1); in a province a variant counts only if it has that province's art
  (or the tier has no regional art at all), so an eastern city never shows an Italian variant. A
  look missing in some rotation falls back to variant 0 in that rotation.

## Roofs

Terracotta, slate, timber, marble and patina roofs use `kind="tiles"` (`kit._tiles`), drawn to stay
readable at medium zoom (atlas 0.625 x zoom 0.5): columns of round imbrex caps over flat tegulae and
courses with a shadow under each lip, scaled to about 4 screen px; every tile takes its own shade
between the two material colours (and `tile_var` recolours a small share as old replaced tiles), so
roofs read as crisp tile rows instead of mottled noise; no grey weathering wash on roofs. `hip`,
`gable`, `ring_roof` and the homes `shed` add ridge and hip caps (`kit.roof_caps`) in a lighter tone
of the roof (`kit.ridge_material`); pass `caps=False` to skip them. House roof tones: `kit.TERRACOTTA`.

## Art kit (`tools/artgen3d/`)

`kit.py` holds the camera, lights, materials (`kit.palette()`), parts (boxes, gable/hip/ring roofs,
columns, steps, fences, sacks, amphorae, carts, trees, lots) and the framing/render helpers.
`run.py` renders one family module: `Blender -b --python tools/artgen3d/run.py -- <family> [names]
[--samples N]` (a name is a prefix; end it with `=` to match exactly, e.g. `bld/house_plot_t2=`
without its `@region` variants). `URBS_ART_OUT=<dir>` renders into another folder (with its own
meta) for test sheets. Each family module declares `SPRITES = {name: spec}` (see `run.py`) and may define
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

## Streets and open ground

Streets must read as the city's grid at every zoom (owner's critique, 2026-10-11):

- **Roads** (`terrain_tex.py: roads()`) fill most of the tile diamond: dirt is light packed earth (half width 0.39
  tile) with a soft worn edge of trodden earth and grass tufts (0.08); paved is light grey polygonal stone (0.35) with
  a kerb of cream travertine blocks (0.105), clearly unlike the cream square slabs of plazas. Each piece (centre `_c`,
  spokes `_n`..`_nw`, diagonal fills `_fill_<corner>`) has a surface sprite and an edge-band sprite
  (`road/<kind>_k_<piece>`, the band only). `build_chunk` draws a tile's bands one ground sub-layer under its
  surfaces (`kLayerKerb` < `kLayerRoad`), so the union of the surfaces hides every inner overlap and the kerb shows
  only along the street's outline, in every autotile shape. Where both side neighbours and the diagonal between them
  are road, a solid quarter (`road/<kind>_q_<corner>`) fills the corner, so wide streets and squares have no kerb
  islands. Surfaces carry no edge shading of their own (they overlap each other).
- **Meadow variation** (`shaders/ground.frag`): on vegetation only, world-space value noise adds broad lush and pale
  swathes a few tiles across, sun-dried patches about two tiles across and darker clover clumps, so open lots stop
  looking flat; the grass art itself repeats every two tiles.
- **Meadow decor** (`terrain.py`: `obj/tuft_<0..2>`, `obj/flowers_<0..3>` poppies / yellow / daisies / violets,
  `obj/pebbles_<0..1>`): scattered by a hash of the tile over empty grass and fertile land in the static chunk layer
  (no per-frame cost), up to two pieces per tile, fewer and no flowers beside roads. All are knee-high at most; tall
  trees grow only on forest terrain (grass and fertile tiles get low bushes instead), so nothing tall stands on
  buildable ground. Built-on and road tiles skip decor, and the chunk rebuild on any map change removes it.
- **Grade:** the day keyframes in `game/lighting.cpp` give a gently warm light, vibrance (saturation 1.10-1.16),
  S-curve contrast 1.12-1.16 and a soft vignette; rain and storms flatten contrast and saturation back down.
- **Contact shadows:** not added. Buildings bake their own shadows and draw their own lot ground, so a dark footprint
  quad under them would be hidden by the lot.
- Screenshots: `--hour H` lights the world at a fixed hour (presentation only), alongside `--weather NAME`.

## Readability rules (from the art test brief)

Each category owns an accent no other category uses: housing is quiet (soft plaster tones, warm
terracotta, no awning stripes); food = saffron awnings and warm ochre; industry = charcoal slate roofs, soot yards,
fire glow; storage = dark timber and tan stone, big doors, paved loading yards; religion = white
marble and gold; civic/government = travertine with bronze/green-patina roofs and red banners;
entertainment = striped awnings, banners, curved seating; health/baths = blue-green water, white
plaster; education = pale stone, scroll/porch; military = Roman red and iron; water infrastructure =
grey stone and visible water. Show function with props on the lot and a category ground (garden,
dirt yard, paving, marble). Civic and monumental buildings are taller and lighter than houses.

Regional variants (`@east`, `@egypt`, `@north`) keep the footprint and function props but change
materials and roofs: east = whitewashed stucco, flat or low roofs, blue accents; egypt = mudbrick and
sandstone, flat roofs, palm trees; north = timber frame, thatch or shingle roofs, darker greens.
