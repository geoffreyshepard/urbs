"""Fixed (non-painted) buildings: bld/<id> for every non-painted id in
data/defs/buildings.json (except the housing plots, see housing.py).

Sprite: width = w*16, height = h*16 + 8 + 4*min(w, h); anchor bottom-left."""
import json
import os

from palette import ROOT
from .kit import Bld
from . import works

DEFS = os.path.join(ROOT, "data", "defs", "buildings.json")
SKIP = {"house_plot", "domus_plot"}       # drawn by housing.py (tiers)


def load_defs():
    with open(DEFS) as f:
        return json.load(f)


def recipes():
    r = dict(works.RECIPES)
    try:
        from . import civic
        r.update(civic.RECIPES)
    except ImportError:
        pass
    return r


def fixed_ids():
    return [(d["id"], d["size"][0], d["size"][1]) for d in load_defs()
            if not d.get("painted") and d["id"] not in SKIP]


def generate(only=None):
    out = []
    rec = recipes()
    missing = []
    for bid, w, h in fixed_ids():
        if only and bid not in only:
            continue
        if bid not in rec:
            missing.append(bid)
            continue
        b = Bld(bid, w, h)
        rec[bid](b)
        out.append(b.result())
    if missing and not only:
        raise SystemExit("no recipe for fixed buildings: %s" % ", ".join(missing))
    return out
