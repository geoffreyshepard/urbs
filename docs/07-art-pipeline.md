# 07 — Art Pipeline

> **2026-10-11: world art is now pre-rendered 3D on an isometric camera.** Buildings, terrain, roads,
> trees, people, units and effects are Blender models built by Python scripts in `tools/artgen3d/`
> (`kit.py` holds the camera, lights, materials and parts; one module per family; `run.py` renders
> them headless into `assets/sprites_iso/`; ground textures come from `terrain_tex.py`). See
> `docs/13-isometric.md` for the projection, sprite conventions and readability rules. Goods icons
> (`goods/*`) and build-category icons (`ui/cat_*`) are rendered too (`tools/artgen3d/icons.py`). The
> pixel-art generators below still produce the other UI icons (`ui/*`: HUD, gods, advisors, logo);
> their world sprites, goods and category icons in `assets/sprites/` are no longer packed. The rest
> of this document describes the original pixel-art pipeline.


## Approach: art is code

Claude Code doesn't paint images. It writes **programs that draw sprites**. All art is produced by generator scripts in `tools/artgen/` (Python + Pillow), using a locked palette and shared templates. This gives:
- **Consistency:** every sprite uses the same palette, light direction, outline rules and proportions.
- **Regenerability:** change a rule once (say, roof color), regenerate everything.
- **Variation for free:** class clothing, building wear, seasonal tints and color variants come from parameters, not new drawings.
- **Reviewable diffs:** art changes show up as code changes.

Expect iteration. Programmatic pixel art reaches "clean and stylized" reliably, but hero assets such as monuments and portraits will take several rounds. Treat contact-sheet review as a normal part of the work.

## Style guide

| Rule | Value |
|---|---|
| Camera | Fixed top-down with a slight 3/4 tilt for buildings (front faces visible), as in Songs of Syx |
| Tile size | 16×16 px |
| Citizen size | About 6×10 px; readable silhouette at 2×, a colored dot at far zoom |
| Light | From the top-left; one highlight shade and one shadow shade per material. The renderer adds cast shadows, ground shading and a colour grade on top; those are not palette colours |
| Outlines | Dark outline on buildings and units only; terrain has none. The outer silhouette uses the dark partner of the material it borders (selective outline, `outlines.py`); interior detail lines stay near-black |
| Palette | 56-color locked palette (v2: hue-shifted ramps with cool shadows and warm lights, sun-baked sage grass, plus 8 dark outline partners) (`assets/palettes/urbs.pal`) built on a Mediterranean feel: terracotta, ochre, travertine, marble white, olive and cypress greens, sea blues, sun-baked earth, Roman red, Tyrian purple |
| Animation | Citizens: 4 directions × 4-frame walk, plus a work loop and idle. Water and fire use palette cycling. |

**Class readability:** clothing color encodes class at a glance.

| Class | Clothing |
|---|---|
| Patricians | White toga with a purple stripe |
| Equites | Off-white with a narrow stripe |
| Plebeians | Undyed browns and ochres |
| Freedmen | Muted greens and blues |
| Slaves | Plain gray tunic |
| Soldiers | Roman red with metal gray |

**UI type:** Alegreya Sans for all UI text and Cinzel (Roman inscriptional capitals) for window titles and headings,
both SIL OFL fonts in `assets/fonts` (the one exception to "art is code"). `game/style.cpp` loads them;
`begin_window()` and `heading()` apply the title face.

**Build menu:** the left-hand rail shows one 24x24 picture per building group (`ui/cat_<id>` from
`tools/artgen/categories.py`). Each symbol has a distinct shape and a dominant colour, so the rail reads without text:
basalt road, terracotta house, golden wheat sheaf, Roman loaf, iron anvil, amphora and crate, water drop, rod of
Asclepius, temple front, lyre, SPQR standard, scutum, triumphal arch. The rows that pop out show each building's own
sprite. `game/ui_icons.cpp` composes these thumbnails at startup: a 2x2 field with its hut for farms, a three-piece
run for walls, aqueducts and roads, and the city's regional variant when there is one.

**Panels:** every panel is built from the pieces in `game/panel_kit.hpp`: a carved frame with no title bar, a
header (the title on the red band, a framed picture hanging from it: the panel's icon on a medallion, the building's
own thumbnail or a portrait on a tile, and one summary line), small-caps section labels in Pompeian red under laurel
rules, inlaid bars (green when a goal is met, amber when short, red when failing, ochre with no goal) and goods named
with their icons. The building and citizen inspector sits beside the build rail, where the build lists pop out, so
opening a list closes it. The report panels (Rome, Finance, Goods and the rest) hang from their top-bar button at the
right edge, with a little stone gable pointing at it, one at a time; the latest-news card moves left of an open panel.

## UI chrome (generated)

The frames, buttons and ornaments round the UI are drawn by `tools/uiart/gen_chrome.py` (Python, numpy and Pillow;
no Blender). Each piece is a small height field (a moulding profile, a bead, a dish, a fillet) shaded with the
world's sun from the upper left, coloured with procedural materials (travertine with faint layers and pores, bronze
and gilt from colour ramps with a specular glint, parchment with fibres and warm age stains, fresco-red with
mottling), and packed into one atlas at 2 texels per UI point (retina):

```
python3 tools/uiart/gen_chrome.py [--preview sheet.png]
  -> assets/sprites_iso/ui/chrome/chrome.png    the atlas (not part of the world atlas; loaded directly)
  -> assets/sprites_iso/ui/chrome/chrome.json   per piece: rect, 9-slice margins `m`, outset `o` (shadows reach
                                                 beyond the rectangle), tiling `t`, frame border width `i`
```

Centres and edges that tile (stone grain, parchment) come from periodic noise, so a 9-slice repeats them instead of
stretching. Never edit the PNG; change the script and run it again. `game/ui_skin.{hpp,cpp}` loads the atlas
(mipmapped) and draws the pieces.

## UI style

One look everywhere, matching the lit 3D world:

| Element | Look |
|---|---|
| Panels, dialogs | Carved travertine frame (torus, cove, fillet), a bronze bead inside it, bronze rosettes at the corners, warm parchment with an inner shadow, a soft drop shadow on the world |
| Titles | Report panels and the build list: Cinzel in cream on a Pompeian-red band (#8e3324) with gilt hairlines across the top of the frame. Menus and dialogs: the same red on a tabula ansata plaque, centred |
| Buttons | Raised polished stone with a bronze rim; hover warms the stone and gilds the rim; pressed sinks; selected is Pompeian red with a gilt rim and cream text; disabled is greyed and flat |
| Icon buttons | Top bar: round medallions with a slim bronze rim and a Pompeian-black enamel face, a little lighter behind the icon, which fills about three quarters of it (hover: a lighter face and a gilt rim; pressed: darker and sunk; on: Pompeian red with a gilt rim). Icons that must read on the black are light or gilt (the alerts bell is gilt, the research tablets honey-coloured wax). Build rail and portraits: square tiles with a slim bronze rim and an ivory face; hover warms the face to honey |
| Top bar | A marble cornice across the screen (fillet, frieze, a small cyma, a slim bronze fillet with a gilt edge, a soft shadow) holding recessed slots: money and people; the ratings (Mood, Culture, Prosperity, Security, Favor), each an icon, its name in the caption colour with the value right-aligned after it, and a bar under both; date and the (slightly smaller) speed buttons; then the overlay, the panels, alerts, help and menu in one slot. When the screen is too narrow for the ratings' names (decided with the widest date of the year, so they do not flicker), they drop out and the tooltips still give them. Every button's tooltip names it in bold with its hotkey, then a line of explanation |
| Build rail | A slim travertine pilaster (low capital and base, shallow flutes) holding the group tiles, with little padding: the pictures fill most of each tile |
| Tooltips, popups, toasts | Small parchment cards with a bronze edge. Toasts (the latest news under the top bar, notices at the bottom) wrap to a few lines with the lines balanced, and a longer message ends in an ellipsis on a word, with the whole text in a tooltip; they are sized before they are drawn, so a new message never shows clipped |
| Inputs, checkboxes, sliders, scrollbars | Sunken parchment wells with a bronze hairline; bronze knobs and rods; red tick |
| Dividers | A bronze rule fading at both ends with a lozenge between laurel sprigs |
| Text | Ink #2b1c12 on parchment, captions #68543e, section labels Pompeian red; spacing follows the font size, so the UI scale setting applies to the chrome too |

**How ImGui gets the skin.** The style gives every ImGui colour that fills a frame (window, popup and child
backgrounds, buttons, frames, headers, slider grabs, scrollbars) a *skin colour*: RGB (1, 3, piece). After
`ImGui::Render()`, `UiSkin::process()` walks the draw lists and replaces each rectangle filled with a skin colour by
that piece's 9-slice, in place, so the draw order is kept; a faded colour means a disabled widget and picks the
disabled picture. Rounding and ImGui's own borders are off so these stay plain rectangles; a skin colour on any
other shape (a radio button's circle) is repainted with a plain colour. Custom drawing (title bands, plaques, bars,
dividers, medallion pictures, the top bar's slots) calls `UiSkin::draw()` directly. `begin_window()` takes the piece a
window is drawn as (`Panel`, `Card`, `Cornice`, `Column`, or `None`) and its padding; panels also get an invisible
border as wide as the stone, which keeps scrolled content and the scrollbar inside the frame. Without the chrome
files the same code falls back to flat colours.

**Title screens:** the main menu plays over a living city (`data/menu/backdrop`), a scripted scenario frozen from a
city the autoplay bot built in a campaign mission. `tools/menu_backdrop.sh [mission] [ticks]` (release build; defaults
`m05_syracuse`, 110000 ticks, about 45 s, deterministic) runs `urbs_headless --export-city`, which writes the map with
its roads (`map.urbsmap`) and every finished building with its house tier, stock and families (`scenario.json`; see
`start` in docs/10-modding.md). The game loads it in about 0.25 s (release, 667 buildings and 3,500 people, including a
15-second head start so the streets are busy), lets it tick at normal speed behind the menus under a fixed
late-afternoon light and clear weather, and drifts the camera in a slow ellipse around it. A dark column on the left
carries the logo and the menu; the sub-screens (campaign, sandbox, custom scenarios, mods, dialogs) dim it. If the
backdrop will not load (a defs change or a mod removed one of its buildings; ctest `menu_backdrop` catches the first),
a plain generated landscape stands in; regenerate it with the script.

**Province map (campaign screen):** `tools/artgen/province_map_hd.py` (numpy + Pillow, deterministic, about 8 s)
paints `assets/ui/province_map.png`, 2560x1440, with the same projection and coastline polygons as the pixel map in
`worldmap.py`, so the campaign's normalised mission positions still fit: a relief built from distance inland, fractal
noise and the mountain ranges, hill-shaded from the north-west; land coloured by climate (green Gaul and Italy, olive
Iberia and Greece, ochre and sand Africa, a green Nile valley), forest masses, rock and snow on the high ranges; sea by
depth with a line of surf; rivers; Latin names for seas and lands in Cinzel; paper grain and a vignette. It is not
packed into the atlas: `game/ui_picture.cpp` loads it into its own mipmapped texture the first time the campaign
screen opens (the 512x288 pixel map stays in the atlas as a fallback). The screen draws the missions over it as
medallions numbered in Roman numerals (gold: won; terracotta with a pulse: the next post; grey: locked; a light ring:
selected), nudged apart where they crowd with a leader line to the true place, a dashed road through the career so
far, and below the map the cursus honorum as a line of medallions under their ranks. The dossier on the right holds
the mission's title, province, rank and salary, the blurb, the letter of appointment as a scroll, the difficulty and
the Begin button.

## Generator structure

```
tools/artgen/
├── palette.py          # loads urbs.pal, named color roles (roof_terracotta, marble_light, ...)
├── primitives.py       # pixel-level helpers: shaded box, roof, column, dither, outline
├── terrain.py          # tiles + autotile transitions (grass↔sand, shore, roads incl. diagonals)
├── buildings/          # one module per building family, composed from parts (walls, roofs, columns, doors)
├── people.py           # body template × clothing palette × pose × direction × frame
├── units.py            # soldiers, cavalry, siege
├── goods.py            # carried-goods icons and UI icons
├── hud.py              # HUD icons (16x16), logo, panel ornaments
├── categories.py       # build-menu category icons (24x24)
├── ui.py               # panels, buttons, frames, fonts (bitmap)
└── build_all.py        # generates everything → assets/sprites/
tools/atlas/            # packs sprites into atlases + JSON metadata (stb_rect_pack)
```

## Workflow

1. Lock the palette and the style rules first, in milestone M1, before mass-producing anything.
2. For each new asset family, ask Claude Code to write or extend a generator, then run `build_all.py`.
3. Generate a **contact sheet** (`tools/artgen/contact_sheet.py`) showing every sprite at 1×, 2× and 4×, against grass and against paving. Review it, and check in-game at all zoom levels.
4. Iterate on the generator, never on the PNG.
5. The atlas packer runs as a CMake custom step, so the game always loads up-to-date atlases.

## Asset backlog (by milestone)

| Milestone | Assets |
|---|---|
| M1 | Palette, terrain tiles and transitions, dirt road, placeholder building blocks, a citizen template |
| M3–M4 | First 15 buildings, roads incl. diagonals, carried goods, all class clothing variants |
| M5–M7 | All production buildings, housing evolution stages, service buildings, aqueduct pieces |
| M8–M9 | Fire/smoke/collapse effects, soldiers, walls/gates/towers, combat effects |
| M10 | UI skin, advisor portraits (small pixel portraits), god icons |
| M12–M13 | Monuments, regional variants (Egyptian, Gallic, Germanic, Greek) |

**Regional variants** (`tools/artgen/regions.py`) swap material ramps for common buildings and also reshape housing: flat earthen roof terraces with parapets in Egypt, flat whitewashed terraces in the east, half-timbered daub walls under slate in the north.
