"""Render one family of iso sprites.

    /Applications/Blender.app/Contents/MacOS/Blender -b --python tools/artgen3d/run.py -- \
        <family> [name-or-prefix...] [--samples N] [--list] [--lit-only]

<family> is a module in this folder exposing SPRITES = {name: spec}.  name is the atlas name
("bld/bakery", "people/pleb_s_0"); the PNG goes to assets/sprites_iso/<name>.png and the anchor
to assets/sprites_iso/meta/<family>.json.  spec keys:

  build(M)        builds the scene (M = kit.palette()); required
  fp=(fw, fh)     ground-attached sprite: anchor = footprint's bottom corner (see kit.frame_footprint)
  fixed_h=int     canvas height for fp sprites (flat tiles: 64 * (fw + fh) // 2)
  free=True       free-standing sprite: anchor = the origin (foot); optional frame=(w, h, ax, ay)
  lit=True        also render <name>_lit (window glow overlay, see kit.render_lit)
  samples=int     override the sample count
"""
import importlib
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit  # noqa: E402


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    samples = 64
    if "--samples" in argv:
        k = argv.index("--samples")
        samples = int(argv[k + 1])
        del argv[k:k + 2]
    list_only = "--list" in argv
    lit_only = "--lit-only" in argv  # re-render only the _lit overlays (same framing)
    argv = [a for a in argv if a not in ("--list", "--lit-only")]
    if not argv:
        print("usage: run.py -- <family> [names...]")
        return
    family, wanted = argv[0], argv[1:]
    mod = importlib.import_module(family)
    names = [n for n in mod.SPRITES if not wanted or any(n == w or n.startswith(w) for w in wanted)]
    if lit_only:
        names = [n for n in names if mod.SPRITES[n].get("lit")]
    if list_only:
        print("\n".join(names))
        return
    meta = kit.Meta(family)
    report = []
    t_all = time.time()
    for name in names:
        spec = mod.SPRITES[name]
        t = time.time()
        s = kit.reset_scene(spec.get("samples", samples))
        spec["build"](kit.palette())
        if spec.get("free"):
            ax, ay = kit.frame_free(s, spec.get("frame"))
        else:
            fw, fh = spec.get("fp", (1, 1))
            ax, ay = kit.frame_footprint(s, fw, fh, spec.get("fixed_h"))
        if not lit_only:
            kit.render_to(s, os.path.join(kit.OUT, name + ".png"))
        meta.set(name, ax, ay)
        if spec.get("lit"):
            kit.render_lit(s, os.path.join(kit.OUT, name + "_lit.png"))
            meta.set(name + "_lit", ax, ay)
        meta.save()
        report.append("%s: %.1fs" % (name, time.time() - t))
    print("RENDER REPORT (%d sprites, %.0fs)\n%s" % (len(names), time.time() - t_all, "\n".join(report)))


main()
