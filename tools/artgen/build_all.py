#!/usr/bin/env python3
"""Generate every URBS sprite into assets/sprites/<family>/<name>.png.

Wipes and recreates assets/sprites, validates every pixel against the
locked palette, checks the naming contract, and writes
assets/sprites/meta.json with anchors for sprites whose anchor != (0, 0).

Run from anywhere:  python3 tools/artgen/build_all.py
"""
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from palette import ROOT, check_image          # noqa: E402
import terrain                                  # noqa: E402
import roads                                    # noqa: E402
import objects                                  # noqa: E402
import people                                   # noqa: E402
import buildings                                # noqa: E402
import fx                                       # noqa: E402
import effects                                  # noqa: E402
import units                                    # noqa: E402
import goods                                    # noqa: E402
import ui                                       # noqa: E402
import hud                                      # noqa: E402
import worldmap                                 # noqa: E402
import regions                                 # noqa: E402
from buildings import fixed as bfixed           # noqa: E402

SPRITES_DIR = os.path.join(ROOT, "assets", "sprites")


def required_names():
    """The naming contract the C++ renderer depends on."""
    n = []
    for t in terrain.TERRAINS:
        n += ["terrain/%s_%d" % (t, v) for v in range(4)]
    for t in terrain.DUAL_TERRAINS:
        n += ["terrain/dg_%s_%d" % (t, m) for m in range(1, 15)]
    n += ["terrain/hc_%d" % m for m in range(1, 15)]
    n += ["terrain/water_shimmer_%d" % f for f in range(4)]
    for r in ("dirt", "paved"):
        n.append("road/%s_c" % r)
        n += ["road/%s_%s" % (r, d) for d in ("n", "ne", "e", "se", "s", "sw", "w", "nw")]
    n += ["obj/tree_%d" % i for i in range(6)]
    for o in ("bush", "reeds", "boulder"):
        n += ["obj/%s_%d" % (o, i) for i in range(3)]
    n += ["obj/res_%s" % r for r in ("clay", "stone", "iron", "silver", "marble",
                                     "limestone", "pozzolana", "fish")]
    n += ["bld/placeholder_%dx%d" % (w, h) for w in range(1, 5) for h in range(1, 5)]
    for k in ("patrician", "eques", "pleb", "liberti", "slave", "soldier"):
        for d in "nesw":
            n += ["people/%s_%s_%d" % (k, d, f) for f in range(4)]
            n.append("people/%s_idle_%s" % (k, d))
    for d in "nesw":
        n += ["people/child_%s_%d" % (d, f) for f in range(4)]
    n += ["fx/dot", "fx/white"]
    n += ["fx/select_%dx%d" % (w, h) for w in range(1, 5) for h in range(1, 5)]
    n += phase2_names()
    n += phase3_names()
    return n


HUD_ICONS = ("population happiness culture prosperity security favor build rome advisors finance "
             "goods trade labor religion military research alerts help menu pause play fast "
             "faster road demolish overlay").split()
PLACES = ["Rome", "Ostia", "Beneventum", "Capua", "Syracuse", "Carthago Nova", "Aquileia", "Narbo",
          "Campania", "Corinth", "Lugdunum", "Alexandria", "Colonia Agrippina", "Londinium",
          "Antioch"]


def phase3_names():
    return (["ui/province_map", "ui/logo", "ui/panel_corner", "ui/divider"] +
            ["ui/icon_" + i for i in HUD_ICONS])


GOODS = ("wheat vegetables fish meat olives grapes flour bread olive_oil wine salt garum wood "
         "planks clay bricks stone cut_stone limestone lime pozzolana concrete marble sand "
         "iron_ore iron silver_ore silver gold tools weapons armor pottery amphorae wool "
         "clothing dye dyed_clothing furniture glassware papyrus scrolls jewelry statues "
         "denarii").split()
GODS = "jupiter juno mars venus ceres neptune mercury minerva vulcan vesta isis serapis divus".split()
ADVISORS = "engineer quaestor haruspex legate aedile".split()
LINES = ("wall", "aqueduct", "pipe", "sewer", "bridge")


def phase2_names():
    """Phase 2 contract: buildings, housing, painted, construction, goods,
    effects, gods/advisors, military units."""
    n = []
    defs = bfixed.load_defs()
    for d in defs:
        bid = d["id"]
        if not d.get("painted"):
            n.append("bld/" + bid)
        elif bid in LINES:
            n += ["bld/%s_%s" % (bid, s) for s in ("c", "n", "e", "s", "w")]
        else:
            n += ["bld/%s_tile_%d" % (bid, v) for v in range(4)]
            if d.get("category") == "farm":
                n.append("bld/%s_hut" % bid)
    n += ["bld/house_plot_t%d" % t for t in range(5)]
    n += ["bld/domus_plot_t%d" % t for t in range(3)]
    n += ["fx/scaffold_%d" % v for v in range(4)] + ["fx/foundation"]
    for g in GOODS:
        n += ["goods/" + g, "goods/%s_16" % g]
    n += ["fx/fire_%d" % f for f in range(4)] + ["fx/smoke_%d" % f for f in range(4)]
    n += ["fx/rubble_%d" % v for v in range(4)] + ["fx/plague_mark", "fx/alert"]
    n += ["fx/cart_%s" % d for d in "nesw"] + ["fx/ship_%s" % d for d in "nesw"]
    n += ["ui/god_" + g for g in GODS] + ["ui/advisor_" + a for a in ADVISORS]
    for u in ("legionary", "archer", "cavalry", "barbarian"):
        for d in "nesw":
            n += ["people/%s_%s_%d" % (u, d, f) for f in range(4)]
            n += ["people/%s_atk_%s_%d" % (u, d, f) for f in range(2)]
    n += ["people/ballista_%s" % d for d in "nesw"]
    return n


def generate_all():
    """-> list of (name, PIL image, anchor or None), sorted by name."""
    sprites = []
    for mod in (terrain, roads, objects, people, units, buildings, fx, effects, goods, ui,
                hud, worldmap):
        sprites += mod.generate()
    sprites += regions.variants(sprites)
    names = [s[0] for s in sprites]
    dup = {x for x in names if names.count(x) > 1}
    if dup:
        raise SystemExit("duplicate sprite names: %s" % sorted(dup))
    return sorted(sprites, key=lambda s: s[0])


def main():
    sprites = generate_all()
    names = {s[0] for s in sprites}
    missing = [n for n in required_names() if n not in names]
    if missing:
        raise SystemExit("naming contract violated, missing: %s" % missing)

    if os.path.isdir(SPRITES_DIR):
        shutil.rmtree(SPRITES_DIR)
    os.makedirs(SPRITES_DIR)

    meta = {}
    for name, img, anchor in sprites:
        check_image(img, name)
        path = os.path.join(SPRITES_DIR, name + ".png")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        img.save(path, optimize=False)
        if anchor is not None and tuple(anchor) != (0, 0):
            ax, ay = anchor
            assert 0 <= ax < img.width and 0 <= ay < img.height, name
            meta[name] = {"ax": int(ax), "ay": int(ay)}

    # non-image outputs (e.g. province_places.json)
    places = worldmap.extra_files()["province_places.json"]
    missing_places = [p for p in PLACES if p not in places]
    if missing_places:
        raise SystemExit("province_places.json missing: %s" % missing_places)
    for fname, data in sorted(worldmap.extra_files().items()):
        with open(os.path.join(SPRITES_DIR, fname), "w") as f:
            json.dump(data, f, indent=1, sort_keys=True)
            f.write("\n")

    with open(os.path.join(SPRITES_DIR, "meta.json"), "w") as f:
        json.dump(meta, f, indent=1, sort_keys=True)
        f.write("\n")
    extra = sorted(names - set(required_names()))
    print("wrote %d sprites (%d with anchors) to %s" % (len(sprites), len(meta),
                                                       os.path.relpath(SPRITES_DIR, os.getcwd())))
    if extra:
        print("  extras beyond the contract: %s" % ", ".join(extra))


if __name__ == "__main__":
    main()
