#!/usr/bin/env python3
"""Writes random sandbox scenarios for the audit (tools/audit.sh sandbox).

Each one is what the in-game Sandbox screen builds (src/game/app.cpp, App::start_sandbox), with random
map settings (size, coast, river, hills, forest, marsh, desert, Nile, building style, entry side)
and a modest goal, so the bot's result says whether the generated map is playable.
Deterministic: the same count gives the same maps.

Usage: tools/sandbox_maps.py OUT_DIR [COUNT=24]
"""
import json
import os
import random
import sys


def scenario(k: int) -> dict:
    r = random.Random(1000 + k)
    river = r.random() < 0.7
    arid = round(r.choice([0, 0, 0.2, 0.4, 0.6]), 2)
    return {
        "id": f"sandbox_{k:02d}",
        "name": f"Random sandbox {k:02d}",
        "mission": 16,
        "difficulty": 1,
        "start_hour": 7,
        "map": {
            "size": r.choice([128, 192, 256]),
            "seed": r.randrange(1, 1_000_000),
            "coast": r.choice([-1, 0, 1, 2, 3]),
            "river": river,
            "hills": round(r.random(), 2),
            "forest": round(r.random() * 0.6, 2),
            "marsh": round(r.random() * 0.4, 2),
            "arid": arid,
        },
        "region": r.choice(["", "north", "east", "egypt"]),
        "nile": river and arid >= 0.4 and r.random() < 0.5,
        "world": {"max_citizens": 16000, "max_buildings": 4096, "max_households": 5000},
        "generator": "scripted",
        "entry_road": r.choice([0, 1, 2, 3]),
        "start": {"treasury": 15000},
        "start_stock": {"wood": 80, "wheat": 60},
        "gods": ["jupiter", "juno", "mars", "venus", "ceres", "neptune", "mercury", "minerva", "vulcan", "vesta"],
        "trade": {"partners": ["latin_villages", "rome", "etruria", "carthage", "massalia", "gallia", "greece"]},
        "goals": {"population": 1500, "culture": 30},
    }


def main() -> None:
    out = sys.argv[1]
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 24
    os.makedirs(out, exist_ok=True)
    for k in range(count):
        with open(os.path.join(out, f"sandbox_{k:02d}.json"), "w") as f:
            json.dump(scenario(k), f, indent=1)


if __name__ == "__main__":
    main()
