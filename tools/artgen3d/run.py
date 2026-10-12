"""Render one family of iso sprites.

    /Applications/Blender.app/Contents/MacOS/Blender -b --python tools/artgen3d/run.py -- \
        <family> [name-or-prefix[=]...] [--samples N] [--list] [--lit-only] [--rot K[,K..]|all]
        [--resume EPOCH]

<family> is a module in this folder exposing SPRITES = {name: spec}.  name is the atlas name
("bld/bakery", "people/pleb_s_0"); the PNG goes to assets/sprites_iso/<name>.png and the anchor
to assets/sprites_iso/meta/<family>.json.  spec keys:

  build(M)        builds the scene (M = kit.palette()); required
  fp=(fw, fh)     ground-attached sprite: anchor = footprint's bottom corner (see kit.frame_footprint)
  fixed_h=int     canvas height for fp sprites (flat tiles: 64 * (fw + fh) // 2)
  free=True       free-standing sprite: anchor = the origin (foot); optional frame=(w, h, ax, ay)
  lit=True        also render <name>_lit (window glow overlay, see kit.render_lit)
  samples=int     override the sample count

Rotations (docs/13-isometric.md): --rot 1,2,3 (or "all") renders the given quarter turns of the
rotatable sprites only (see rotatable()); rot 0 is the plain run without --rot.  rot k turns the
whole scene k * 90 degrees clockwise seen from above (map north up) about the footprint centre, so
the front faces grid south / west / north / east for k = 0 / 1 / 2 / 3; sun and camera stay put.
The footprint becomes (fh, fw) for odd k.  Names get _r<k> before _lit / @region:
bld/bakery_r1.png, bld/bakery_r1@east.png, bld/bakery_r1_lit.png, bld/house_plot_t2_r3_lit.png.
Anchors are merged into the family meta json under a file lock, so several Blender processes
may render subsets of one family at the same time.
"""
import fcntl
import importlib
import json
import math
import os
import re
import sys
import time

import bpy
from mathutils import Matrix

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit  # noqa: E402

# Not rotated: autotiled line pieces (_c/_n/_e/_s/_w), painted area tiles, area huts, placeholders.
_FIXED = re.compile(r"(_[cnesw]|_tile_\d+|_hut)$|^bld/placeholder_")


def rotatable(name, spec):
    """A one-footprint building sprite in bld/ (base, tier or regional variant)."""
    if not name.startswith("bld/") or spec.get("free") or "fp" not in spec:
        return False
    return not _FIXED.search(name.split("@")[0])


def rot_name(name, k, lit=False):
    """bld/bakery@east, 1 -> bld/bakery_r1@east; lit -> bld/bakery_r1_lit (lit before @region)."""
    base, _, region = name.partition("@")
    out = base + ("_r%d" % k if k else "") + ("_lit" if lit else "")
    return out + ("@" + region if region else "")


def rotate_scene(s, k):
    """Turn every scene object (not the sun/fill lights, not the camera) k quarter turns clockwise
    seen from above about the origin (the footprint centre).  Blender +y is grid north, so
    clockwise from above is a negative angle about +z: front -y (grid south) -> -x (grid west)."""
    if k % 4 == 0:
        return
    R = Matrix.Rotation(-math.pi / 2 * k, 4, "Z")
    for ob in s.objects:
        if ob.parent is not None or ob.type == "CAMERA":
            continue
        if ob.type == "LIGHT" and ob.data.type == "SUN":
            continue
        ob.matrix_world = R @ ob.matrix_world
    bpy.context.view_layer.update()


class MergedMeta:
    """Anchors for one family; each save merges this run's entries into the json under a lock."""

    def __init__(self, family):
        self.path = os.path.join(kit.OUT, "meta", family + ".json")
        self.mine = {}

    def set(self, name, ax, ay):
        self.mine[name] = {"ax": int(ax), "ay": int(ay)}

    def save(self):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        with open(self.path + ".lock", "w") as lk:
            fcntl.flock(lk, fcntl.LOCK_EX)
            try:
                with open(self.path) as f:
                    data = json.load(f)
            except (OSError, ValueError):
                data = {}
            data.update(self.mine)
            tmp = self.path + ".tmp%d" % os.getpid()
            with open(tmp, "w") as f:
                json.dump(data, f, indent=1, sort_keys=True)
                f.write("\n")
            os.replace(tmp, self.path)
        try:
            os.remove(self.path + ".lock")
        except OSError:
            pass


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    samples = 64
    if "--samples" in argv:
        k = argv.index("--samples")
        samples = int(argv[k + 1])
        del argv[k:k + 2]
    rots = [0]
    if "--rot" in argv:
        k = argv.index("--rot")
        v = argv[k + 1]
        rots = [1, 2, 3] if v == "all" else [int(r) for r in v.split(",")]
        del argv[k:k + 2]
    resume = 0.0  # --resume EPOCH: skip sprites whose PNGs were written at or after EPOCH
    if "--resume" in argv:
        k = argv.index("--resume")
        resume = float(argv[k + 1])
        del argv[k:k + 2]
    list_only = "--list" in argv
    lit_only = "--lit-only" in argv  # re-render only the _lit overlays (same framing)
    argv = [a for a in argv if a not in ("--list", "--lit-only")]
    if not argv:
        print("usage: run.py -- <family> [names...]")
        return
    family, wanted = argv[0], argv[1:]
    mod = importlib.import_module(family)
    # a name ending in "=" matches exactly (bld/house_plot_t2= skips the @region variants)
    names = [n for n in mod.SPRITES
             if not wanted or any(n == w.rstrip("=") or (not w.endswith("=") and n.startswith(w)) for w in wanted)]
    if lit_only:
        names = [n for n in names if mod.SPRITES[n].get("lit")]
    if rots != [0]:
        names = [n for n in names if rotatable(n, mod.SPRITES[n])]
    jobs = [(n, r) for n in names for r in rots]
    if resume:
        def done(n, r):
            outs = ([] if lit_only else [rot_name(n, r)]) + ([rot_name(n, r, True)] if mod.SPRITES[n].get("lit") else [])
            paths = [os.path.join(kit.OUT, o + ".png") for o in outs]
            return all(os.path.exists(p) and os.path.getmtime(p) >= resume for p in paths)
        jobs = [j for j in jobs if not done(*j)]
    if list_only:
        out = []
        for n, r in jobs:
            if not lit_only:
                out.append(rot_name(n, r))
            if mod.SPRITES[n].get("lit"):
                out.append(rot_name(n, r, lit=True))
        print("\n".join(out))
        return
    meta = MergedMeta(family)
    report = []
    t_all = time.time()
    for name, r in jobs:
        spec = mod.SPRITES[name]
        out = rot_name(name, r)
        t = time.time()
        s = kit.reset_scene(spec.get("samples", samples))
        spec["build"](kit.palette())
        if spec.get("free"):
            ax, ay = kit.frame_free(s, spec.get("frame"))
        else:
            fw, fh = spec.get("fp", (1, 1))
            if r % 2:
                fw, fh = fh, fw
            rotate_scene(s, r)
            ax, ay = kit.frame_footprint(s, fw, fh, spec.get("fixed_h"))
        if not lit_only:
            kit.render_to(s, os.path.join(kit.OUT, out + ".png"))
            meta.set(out, ax, ay)
        if spec.get("lit"):
            lit = rot_name(name, r, lit=True)
            kit.render_lit(s, os.path.join(kit.OUT, lit + ".png"))
            meta.set(lit, ax, ay)
        meta.save()
        report.append("%s: %.1fs" % (out, time.time() - t))
    print("RENDER REPORT (%d sprites, %.0fs)\n%s" % (len(jobs), time.time() - t_all, "\n".join(report)))


main()
