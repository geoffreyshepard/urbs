# 07 — Art Pipeline (AI-Generated Pixel Art via Claude Code)

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
