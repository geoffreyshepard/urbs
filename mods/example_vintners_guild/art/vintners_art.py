"""The Vintners' Guild's own sprites, rendered with the game's Blender kit (tools/artgen3d).

From the repository root (writes PNGs to mods/example_vintners_guild/sprites/ and anchors to
sprites/meta/vintners_art.json; see docs/10-modding.md, "Sprites"):

    URBS_ART_OUT=mods/example_vintners_guild/sprites /Applications/Blender.app/Contents/MacOS/Blender -b \
        --python tools/artgen3d/run.py -- mods/example_vintners_guild/art/vintners_art.py
    URBS_ART_OUT=mods/example_vintners_guild/sprites /Applications/Blender.app/Contents/MacOS/Blender -b \
        --python tools/artgen3d/run.py -- mods/example_vintners_guild/art/vintners_art.py bld/ --rot all

bld/vintner (+ _r1.._r3 and their _lit night overlays): the vintner's cellar, 2x2.  Food building,
so warm ochre walls, but its accent is amber: an amber-striped awning, barrels of aged wine and
amber-sealed amphorae.  goods/amber_wine_16 and goods/amber_wine: the slender wine amphora of
goods/wine with an amber band and a honey-gold seal.
"""
import icons
import kit
import works
from kit import box, cylinder, lot_rect, material


def mats(M):
    works.ext(M)
    M["awning_amber"] = material("awning_amber", "#d98b1a", "#f6e7c4", kind="stripes", stripe=0.09,
                                 bump=0.0, rough=0.9)
    M["amber"] = material("amber", "#c7811a", "#dda03a", scale=8, rough=0.15, bump=0.02)
    M["barrel"] = material("barrel", "#93613a", "#7d512e", scale=14, bump=0.2)
    M["hoop"] = material("hoop", "#3e3d40", "#2e2d30", scale=20, rough=0.45, metal=0.6, bump=0.0)
    return M


def barrel(M, x0, x1, y, zc, r=0.1):
    """A barrel lying along x, with three iron hoops."""
    cylinder(zc, y, x0, x1, r, M["barrel"], n=16, axis="x")
    for t in (0.12, 0.5, 0.88):
        x = x0 + (x1 - x0) * t
        cylinder(zc, y, x - 0.015, x + 0.015, r * 1.04, M["hoop"], n=16, axis="x")


def vintner(M):
    mats(M)
    lot_rect(2, 2, M["paving"])
    # the cellar house at the back; its door opens on the yard under an amber awning
    works.house_block(M, -0.92, 0.05, 0.5, 0.92, 0.6, M["plaster_ochre"], M["roof_house"], axis="x",
                      door_x=-0.2)
    works.awning(M, -0.88, 0.46, 0.05, 0.28, 0.52, mat=M["awning_amber"])
    # aged wine in barrels, two below and one on top, on the east side of the yard
    barrel(M, 0.5, 0.9, -0.62, 0.1)
    barrel(M, 0.5, 0.9, -0.38, 0.1)
    barrel(M, 0.52, 0.88, -0.5, 0.27, r=0.095)
    box(0.47, -0.76, 0.0, 0.93, -0.24, 0.02, M["timber"], bevel=0)
    # an open dolium of amber wine and a row of amber-sealed amphorae
    works.vat(M, -0.2, -0.5, 0.15, M["amber"], h=0.16, rim=M["amphora"])
    for i in range(6):
        x = -0.85 + i * 0.12
        kit.amphora(x, -0.78, M, 1.2)
        cylinder(x, -0.78, 0.095, 0.12, 0.056, M["amber"], n=12)
    for i in range(3):
        kit.amphora(-0.85 + i * 0.12, -0.6, M, 1.2)
        cylinder(-0.85 + i * 0.12, -0.6, 0.095, 0.12, 0.056, M["amber"], n=12)
    # a cypress by the yard wall, and a lantern for the night
    kit.cypress(0.85, 0.2, 0.55, M)
    works.lantern(M, 0.25, -0.85, 0.32)


def g_amber_wine(M):
    """goods/wine's slender amphora with an amber band and a honey-gold wax seal."""
    icons.amphora_shape(M["terracotta"])
    icons.handles(M["terracotta"], 0.43, 0.57, 0.1)
    band = material("band_amber", "#d8901c", "#c27c12", scale=20, rough=0.5, bump=0.02)
    seal = material("wax_amber", "#e8b13c", "#cf9526", scale=20, rough=0.35, bump=0.02)
    icons.lathe([(0.141, 0.3), (0.152, 0.36), (0.143, 0.42)], band, n=24, top=False, bottom=False)
    icons.lathe([(0.06, 0.6), (0.068, 0.64), (0.04, 0.68), (0.0, 0.69)], seal, n=16, bottom=False)


UI, WORLD = icons.UI, icons.WORLD
SPRITES = {
    "bld/vintner": dict(fp=(2, 2), build=vintner, lit=True),
    "goods/amber_wine_16": dict(free=True, frame=(UI, UI, UI // 2, UI // 2), build=icons.icon(g_amber_wine, UI)),
    "goods/amber_wine": dict(free=True, frame=(WORLD, WORLD, WORLD // 2, WORLD // 2),
                             build=icons.icon(g_amber_wine, WORLD)),
}
