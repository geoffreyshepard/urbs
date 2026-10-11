"""Citizens, children, soldiers and military units as iso sprites (family "people").

Every figure is built facing local +x around its foot (the origin), parented to one root that
turns it to the grid direction: e = +x, n = Blender +y (grid north), w = -x, s = -y.  Walk cycles
have 4 frames (legs apart, passing, other leg apart, passing); attacks 2 frames; idles 1.  Each
kind uses one fixed canvas so frames never jitter.  Class reads from clothing colour (see
docs/07-art-pipeline.md): patricians white toga with a broad purple band, equites off-white tunic
with narrow stripes, plebs browns and ochres, liberti muted green/blue with the freedman's cap,
slaves plain grey, soldiers Roman red and iron.
"""
import math

import bpy

import kit

DIRS = {"e": 0.0, "n": 90.0, "w": 180.0, "s": -90.0}
WALK = [(1.0, 0.0), (0.0, 1.0), (-1.0, 0.0), (0.0, 1.0)]  # (leg swing, bob) per frame

# fixed canvases (w, h, ax, ay): foot at (ax, ay)
HUMAN_SCALE = 1.12  # a touch taller than life against the buildings, for readability
F_PERSON = (40, 46, 20, 38)
F_CHILD = (32, 36, 16, 30)
F_UNIT = (52, 54, 26, 44)
F_CAVALRY = (84, 78, 42, 62)
F_BALLISTA = (88, 70, 44, 54)


# ---------------------------------------------------------------- materials

def mats():
    m = kit.material
    return {
        "skin": m("p_skin", "#c98f6b", "#b77d5a", kind="flat", rough=0.7, bump=0),
        "hair": m("p_hair", "#3b2a1e", kind="flat", bump=0),
        "hair_fair": m("p_hair_fair", "#c99a4e", kind="flat", bump=0),
        "foot": m("p_foot", "#4a3324", kind="flat", bump=0),
        "pleb": m("p_pleb", "#8a5228", kind="flat", bump=0),
        "pleb2": m("p_pleb2", "#c99a52", kind="flat", bump=0),
        "toga": m("p_toga", "#f4f1ea", kind="flat", rough=0.8, bump=0),
        "purple": m("p_purple", "#6e2a7e", kind="flat", bump=0),
        "eques": m("p_eques", "#e6dcc2", kind="flat", bump=0),
        "liberti": m("p_liberti", "#4c7f52", kind="flat", bump=0),
        "cap": m("p_cap", "#e3dccb", kind="flat", bump=0),
        "slave": m("p_slave", "#7d7b77", kind="flat", bump=0),
        "child": m("p_child", "#d9a43c", kind="flat", bump=0),
        "trabea": m("p_trabea", "#b0302a", "#6e2a7e", kind="stripes", stripe=0.03, bump=0),
        "red": m("p_red", "#b0281e", kind="flat", rough=0.75, bump=0),
        "red_dark": m("p_red_dark", "#7e1c16", kind="flat", bump=0),
        "iron": m("p_iron", "#9aa0a8", kind="flat", rough=0.35, metal=0.8, bump=0),
        "gold": m("p_gold", "#e0b445", kind="flat", rough=0.3, metal=0.9, bump=0),
        "leather": m("p_leather", "#6b4a2e", kind="flat", bump=0),
        "wood": m("p_wood", "#7a5634", kind="flat", bump=0),
        "wood_dark": m("p_wood_dark", "#4e3522", kind="flat", bump=0),
        "trews": m("p_trews", "#3f6a9a", "#2d4f78", kind="stripes", stripe=0.025, bump=0),
        "fur": m("p_fur", "#5c4430", kind="flat", rough=1.0, bump=0),
        "horse": m("p_horse", "#6e4630", kind="flat", rough=0.6, bump=0),
        "horse_dark": m("p_horse_dark", "#2e211a", kind="flat", bump=0),
        "donkey": m("p_donkey", "#8a7f74", kind="flat", rough=0.7, bump=0),
        "rope": m("p_rope", "#cdb58a", kind="flat", bump=0),
        "shadow": m("p_shadow", "#000000", kind="flat", bump=0, alpha=0.30),
        "shadow2": m("p_shadow2", "#000000", kind="flat", bump=0, alpha=0.22),
    }


# ---------------------------------------------------------------- parts

class Rig:
    """Collects objects and parents them to one root that faces the walking direction."""

    def __init__(self, heading_deg):
        self.root = bpy.data.objects.new("rig", None)
        bpy.context.scene.collection.objects.link(self.root)
        self.heading = math.radians(heading_deg)

    def add(self, ob, loc=(0, 0, 0), rot=(0, 0, 0)):
        ob.location = loc
        ob.rotation_euler = rot
        ob.parent = self.root
        return ob

    def done(self):
        self.root.rotation_euler = (0, 0, self.heading)
        bpy.context.view_layer.update()


def cbox(x0, y0, z0, x1, y1, z1, mat, name="part"):
    return kit.box(x0, y0, z0, x1, y1, z1, mat, bevel=0, name=name)


def taper(x0, x1, y0, y1, z0, z1, k_top, mat, name="taper"):
    """Box whose top face is scaled by k_top (a skirt or robe)."""
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    hx, hy = (x1 - x0) / 2, (y1 - y0) / 2
    v = [(cx - hx, cy - hy, z0), (cx + hx, cy - hy, z0), (cx + hx, cy + hy, z0), (cx - hx, cy + hy, z0)]
    v += [(cx - hx * k_top, cy - hy * k_top, z1), (cx + hx * k_top, cy - hy * k_top, z1),
          (cx + hx * k_top, cy + hy * k_top, z1), (cx - hx * k_top, cy + hy * k_top, z1)]
    f = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    return kit.mesh(name, v, f, mat)


def shadow(P, r=0.075):
    """Soft contact shadow: two stacked translucent discs on the ground."""
    kit.cylinder(0, 0, 0.001, 0.002, r * 1.35, P["shadow2"], n=20, name="shadow", smooth=False)
    kit.cylinder(0, 0, 0.002, 0.003, r, P["shadow"], n=20, name="shadow", smooth=False)


def humanoid(rig, P, look, swing=0.0, bob=0.0, scale=1.0, pose=None):
    """A low-poly person facing +x, foot at the origin.  look keys:

    tunic, legs, hem (z of the garment bottom), sash, stripes, hem_band, hair, headgear,
    armour, cloak, sleeves
    """
    pose = pose or {}
    s = scale * HUMAN_SCALE
    leg_len = 0.15 * s
    hip = leg_len + bob * 0.006 * s
    leg_mat = look.get("legs", P["skin"])
    amp = math.radians(28 if look.get("hem", 0.1) > 0.06 else 12)
    for side, sgn in (("l", 1), ("r", -1)):
        a = -swing * sgn * amp
        rot = (0, a, 0)
        if look.get("seated"):  # astride: thighs forward and apart
            rot = (sgn * math.radians(-38), math.radians(-55), 0)
        leg = cbox(-0.017 * s, -0.016 * s, -leg_len, 0.017 * s, 0.016 * s, 0, leg_mat, "leg")
        rig.add(leg, (0, sgn * 0.022 * s, hip), rot)
        foot = cbox(-0.016 * s, -0.016 * s, -leg_len, 0.03 * s, 0.016 * s, -leg_len + 0.014 * s, P["foot"], "foot")
        rig.add(foot, (0, sgn * 0.022 * s, hip), rot)
    # garment: skirt from hem to the waist, torso above
    hem = look.get("hem", 0.10) * s
    tunic = look["tunic"]
    skirt = taper(-0.04 * s, 0.04 * s, -0.055 * s, 0.055 * s, hem, hip + 0.02 * s, 0.85, tunic, "skirt")
    rig.add(skirt)
    tz0, tz1 = hip + 0.01 * s, hip + 0.125 * s
    rig.add(cbox(-0.032 * s, -0.052 * s, tz0, 0.032 * s, 0.052 * s, tz1, look.get("armour", tunic), "torso"))
    if look.get("hem_band"):
        rig.add(taper(-0.041 * s, 0.041 * s, -0.056 * s, 0.056 * s, hem - 0.001, hem + 0.022 * s, 0.97,
                      look["hem_band"], "hemband"))
    if look.get("sash"):  # broad diagonal band across the chest
        band = cbox(-0.036 * s, -0.012 * s, -0.075 * s, 0.036 * s, 0.012 * s, 0.075 * s, look["sash"], "sash")
        rig.add(band, (0, 0, (tz0 + tz1) / 2), (math.radians(40), 0, 0))
    if look.get("stripes"):  # two narrow vertical stripes on the front
        for y in (-0.022 * s, 0.022 * s):
            rig.add(cbox(0.031 * s, y - 0.006 * s, hem, 0.036 * s, y + 0.006 * s, tz1, look["stripes"], "stripe"))
    if look.get("belt"):
        rig.add(cbox(-0.034 * s, -0.054 * s, hip + 0.005 * s, 0.034 * s, 0.054 * s, hip + 0.02 * s, look["belt"], "belt"))
    if look.get("cloak"):
        rig.add(taper(-0.05 * s, -0.03 * s, -0.06 * s, 0.06 * s, hip - 0.05 * s, tz1, 0.9, look["cloak"], "cloak"))
    # arms swing opposite to the legs unless posed
    sh_z = tz1 - 0.01 * s
    arm_mat = look.get("sleeves", P["skin"])
    for side, sgn in (("l", 1), ("r", -1)):
        rot = pose.get(side)
        if rot is None:
            rot = (0, swing * sgn * math.radians(24), 0)
        arm = cbox(-0.014 * s, -0.013 * s, -0.115 * s, 0.014 * s, 0.013 * s, 0, arm_mat, "arm")
        rig.add(arm, (0, sgn * 0.066 * s, sh_z), rot)
        hand = cbox(-0.014 * s, -0.013 * s, -0.13 * s, 0.014 * s, 0.013 * s, -0.112 * s, P["skin"], "hand")
        rig.add(hand, (0, sgn * 0.066 * s, sh_z), rot)
    # head
    hz = tz1 + 0.05 * s
    rig.add(kit.blob(0, 0, 0, 0.038 * s, 0.036 * s, 0.042 * s, P["skin"], segs=10, rings=6, name="head"), (0, 0, hz))
    gear = look.get("headgear")
    if gear == "helmet":
        rig.add(kit.blob(0, 0, 0, 0.043 * s, 0.041 * s, 0.04 * s, P["iron"], segs=10, rings=6, top_only=True,
                         name="helmet"), (-0.003 * s, 0, hz + 0.006 * s))
        if look.get("crest"):
            rig.add(cbox(-0.008 * s, -0.035 * s, 0, 0.008 * s, 0.035 * s, 0.03 * s, look["crest"], "crest"),
                    (0, 0, hz + 0.035 * s))
    elif gear == "cone":
        rig.add(kit.cylinder(0, 0, 0, 0.05 * s, 0.04 * s, P["iron"], n=10, name="cone"), (0, 0, hz + 0.012 * s))
        rig.add(kit.blob(0, 0, 0, 0.02 * s, 0.02 * s, 0.03 * s, P["iron"], segs=8, rings=4, name="tip"),
                (0, 0, hz + 0.06 * s))
    elif gear == "cap":
        rig.add(kit.blob(0, 0, 0, 0.04 * s, 0.038 * s, 0.05 * s, look.get("cap", P["cap"]), segs=10, rings=6,
                         top_only=True, name="cap"), (-0.004 * s, 0, hz + 0.008 * s))
    else:
        hair = look.get("hair", P["hair"])
        rig.add(kit.blob(0, 0, 0, 0.041 * s, 0.039 * s, 0.03 * s, hair, segs=10, rings=6, top_only=True,
                         name="hair"), (-0.006 * s, 0, hz + 0.012 * s))
        if look.get("long_hair"):
            rig.add(cbox(-0.045 * s, -0.035 * s, -0.05 * s, -0.02 * s, 0.035 * s, 0.03 * s, hair, "mane"),
                    (0, 0, hz))
    return {"hip": hip, "shoulder": sh_z, "head": hz}


def hand_pos(sh_z, side_sgn, rot, s=1.0, reach=0.125):
    """World (local rig) position of a hand given the arm's rotation about y (pitch)."""
    a = rot[1]
    s = s * HUMAN_SCALE
    return (-reach * s * math.sin(a), side_sgn * 0.066 * s, sh_z - reach * s * math.cos(a))


# ---------------------------------------------------------------- citizens

CLASSES = {
    "pleb": lambda P: {"tunic": P["pleb"], "belt": P["leather"], "hem": 0.10},
    "patrician": lambda P: {"tunic": P["toga"], "hem": 0.025, "sash": P["purple"], "hem_band": P["purple"],
                            "hair": P["hair"]},
    "eques": lambda P: {"tunic": P["eques"], "hem": 0.085, "stripes": P["purple"], "belt": P["gold"],
                        "legs": P["skin"], "cloak": P["trabea"]},  # the scarlet-and-purple trabea
    "liberti": lambda P: {"tunic": P["liberti"], "belt": P["leather"], "hem": 0.095, "headgear": "cap"},
    "slave": lambda P: {"tunic": P["slave"], "hem": 0.11, "hair": P["hair"]},
    "child": lambda P: {"tunic": P["child"], "hem": 0.10, "hair": P["hair_fair"]},
    "soldier": lambda P: {"tunic": P["red"], "armour": P["iron"], "belt": P["leather"], "hem": 0.10,
                          "headgear": "helmet", "crest": P["red"]},
}


def citizen(kind, d, frame):
    def build(M):
        P = mats()
        rig = Rig(DIRS[d])
        look = CLASSES[kind](P)
        scale = 0.72 if kind == "child" else 1.0
        swing, bob = WALK[frame] if frame is not None else (0.0, 0.0)
        shadow(P, 0.06 * scale)
        if kind == "soldier":
            j = humanoid(rig, P, look, swing, bob, scale, pose={"r": (0, math.radians(-25), 0)})
            # spear held upright in the right hand
            hx, hy, hz = hand_pos(j["shoulder"], -1, (0, math.radians(-25), 0))
            rig.add(kit.cylinder(0, 0, -0.15, 0.24, 0.006, P["wood"], n=6, name="spear"), (hx, hy, hz))
            rig.add(kit.cylinder(0, 0, 0.24, 0.29, 0.01, P["iron"], n=6, name="tip"), (hx, hy, hz))
        else:
            humanoid(rig, P, look, swing, bob, scale)
            if kind == "pleb":
                pass
        rig.done()
    return build


# ---------------------------------------------------------------- military units

def legionary(d, frame, atk):
    def build(M):
        P = mats()
        rig = Rig(DIRS[d])
        shadow(P, 0.07)
        swing, bob = (0.0, 0.0) if atk is not None else WALK[frame]
        look = {"tunic": P["red"], "armour": P["iron"], "belt": P["leather"], "hem": 0.10,
                "headgear": "helmet", "crest": P["red"]}
        r_rot = (0, math.radians(-85 if atk == 1 else -35 if atk == 0 else 10), 0)
        j = humanoid(rig, P, look, swing if atk is None else 0.4, bob, 1.0,
                     pose={"l": (math.radians(-20), math.radians(-40), 0), "r": r_rot})
        # scutum on the left, in front of the body
        sh = cbox(-0.008, -0.05, -0.08, 0.008, 0.05, 0.08, P["red"], "scutum")
        rig.add(sh, (0.07, 0.06, j["hip"] + 0.05), (0, 0, math.radians(-15)))
        rig.add(kit.cylinder(0, 0, 0, 0.012, 0.016, P["gold"], n=8, axis="x", name="boss"),
                (0.078, 0.06, j["hip"] + 0.05))
        rig.add(cbox(-0.009, -0.052, 0.07, 0.009, 0.052, 0.08, P["gold"], "rim"), (0.07, 0.06, j["hip"] + 0.05),
                (0, 0, math.radians(-15)))
        # gladius
        hx, hy, hz = hand_pos(j["shoulder"], -1, r_rot)
        blade = cbox(0, -0.006, -0.004, 0.09, 0.006, 0.004, P["iron"], "gladius")
        rig.add(blade, (hx, hy, hz), (0, -r_rot[1] - math.radians(90) if atk is not None else math.radians(60), 0))
        rig.done()
    return build


def archer(d, frame, atk):
    def build(M):
        P = mats()
        rig = Rig(DIRS[d])
        shadow(P, 0.065)
        swing, bob = (0.0, 0.0) if atk is not None else WALK[frame]
        look = {"tunic": P["red_dark"], "armour": P["leather"], "belt": P["leather"], "hem": 0.10,
                "headgear": "cone"}
        if atk is None:
            pose = {"l": (0, math.radians(-15), 0)}
        else:
            pose = {"l": (0, math.radians(-90), 0),
                    "r": (0, math.radians(-90 if atk == 1 else -70), math.radians(25 if atk == 0 else 5))}
        j = humanoid(rig, P, look, swing, bob, 1.0, pose=pose)
        # quiver on the back
        rig.add(cbox(-0.02, -0.015, -0.06, 0.0, 0.015, 0.05, P["leather"], "quiver"),
                (-0.04, -0.02, j["shoulder"] - 0.02), (math.radians(20), 0, 0))
        # bow: an arc of thin segments in the left hand
        hx, hy, hz = hand_pos(j["shoulder"], 1, pose["l"])
        vertical = atk is None
        for k in range(-3, 4):
            t = k / 3.0
            a = t * math.radians(55)
            px, pz = 0.02 * math.cos(a) - 0.02, 0.14 * math.sin(a)
            seg = cbox(-0.006, -0.006, -0.026, 0.006, 0.006, 0.026, P["wood_dark"], "bow")
            rig.add(seg, (hx + px, hy, hz + pz), (0, -a * 0.6, 0))
        if not vertical:
            rig.add(cbox(-0.13, -0.003, -0.003, 0.02, 0.003, 0.003, P["wood"], "arrow"), (hx, hy - 0.02, hz))
        rig.done()
    return build


def barbarian(d, frame, atk):
    def build(M):
        P = mats()
        rig = Rig(DIRS[d])
        shadow(P, 0.07)
        swing, bob = (0.0, 0.0) if atk is not None else WALK[frame]
        look = {"tunic": P["fur"], "legs": P["trews"], "hem": 0.13, "hair": P["hair_fair"], "long_hair": True,
                "belt": P["leather"], "sleeves": P["skin"]}
        r_rot = (0, math.radians(-160 if atk == 0 else -40 if atk == 1 else 15), 0)
        j = humanoid(rig, P, look, swing if atk is None else 0.5, bob, 1.05,
                     pose={"l": (math.radians(-25), math.radians(-35), 0), "r": r_rot})
        # round wooden shield on the left
        rig.add(kit.cylinder(0, 0, -0.008, 0.008, 0.065, P["wood"], n=14, axis="x", name="shield"),
                (0.065, 0.065, j["hip"] + 0.06))
        rig.add(kit.cylinder(0, 0, 0.008, 0.018, 0.016, P["iron"], n=8, axis="x", name="boss"),
                (0.065, 0.065, j["hip"] + 0.06))
        # axe in the right hand
        hx, hy, hz = hand_pos(j["shoulder"], -1, r_rot, 1.05)
        handle = cbox(-0.005, -0.005, -0.03, 0.005, 0.005, 0.12, P["wood"], "haft")
        rig.add(handle, (hx, hy, hz), (0, r_rot[1] + math.radians(180), 0))
        head = cbox(-0.03, -0.004, 0.09, 0.012, 0.004, 0.13, P["iron"], "axe")
        rig.add(head, (hx, hy, hz), (0, r_rot[1] + math.radians(180), 0))
        rig.done()
    return build


def horse(rig, P, swing, mat="horse", scale=1.0):
    s = scale
    body_z = 0.2 * s
    rig.add(kit.blob(0, 0, 0, 0.16 * s, 0.06 * s, 0.065 * s, P[mat], segs=12, rings=8, name="barrel"),
            (0, 0, body_z))
    neck = cbox(-0.03 * s, -0.03 * s, 0, 0.03 * s, 0.03 * s, 0.12 * s, P[mat], "neck")
    rig.add(neck, (0.13 * s, 0, body_z + 0.02 * s), (0, math.radians(35), 0))
    head = cbox(0, -0.026 * s, -0.025 * s, 0.1 * s, 0.026 * s, 0.025 * s, P[mat], "hhead")
    rig.add(head, (0.19 * s, 0, body_z + 0.11 * s), (0, math.radians(40), 0))
    rig.add(cbox(-0.01 * s, -0.008 * s, 0, 0.01 * s, 0.008 * s, 0.1 * s, P["horse_dark"], "mane"),
            (0.15 * s, 0, body_z + 0.06 * s), (0, math.radians(35), 0))
    tail = cbox(-0.012 * s, -0.012 * s, -0.12 * s, 0.012 * s, 0.012 * s, 0, P["horse_dark"], "tail")
    rig.add(tail, (-0.16 * s, 0, body_z + 0.03 * s), (0, math.radians(-25), 0))
    amp = math.radians(26)
    for (lx, ly, ph) in ((0.11, 0.035, 1), (0.11, -0.035, -1), (-0.11, 0.035, -1), (-0.11, -0.035, 1)):
        leg = cbox(-0.014 * s, -0.013 * s, -body_z, 0.014 * s, 0.013 * s, 0, P[mat], "hleg")
        rig.add(leg, (lx * s, ly * s, body_z - 0.02 * s), (0, swing * ph * amp, 0))
        hoof = cbox(-0.016 * s, -0.015 * s, -body_z, 0.016 * s, 0.015 * s, -body_z + 0.02 * s, P["horse_dark"], "hoof")
        rig.add(hoof, (lx * s, ly * s, body_z - 0.02 * s), (0, swing * ph * amp, 0))
    return body_z + 0.06 * s


def cavalry(d, frame, atk):
    def build(M):
        P = mats()
        rig = Rig(DIRS[d])
        shadow(P, 0.13)
        swing = 0.0 if atk is not None else WALK[frame][0]
        saddle = horse(rig, P, swing if atk is None else 0.7)
        rig.add(cbox(-0.07, -0.065, saddle - 0.01, 0.06, 0.065, saddle + 0.012, P["red"], "saddlecloth"))
        # rider: torso and head sit on the saddle, legs hang either side
        look = {"tunic": P["red"], "armour": P["iron"], "hem": 0.10, "headgear": "helmet", "crest": P["red"],
                "cloak": P["red_dark"], "seated": True}
        r_rot = (0, math.radians(-95 if atk == 1 else -60 if atk == 0 else -20), 0)
        sub = Rig(0)
        j = humanoid(sub, P, look, 0.0, 0.0, 0.95, pose={"r": r_rot, "l": (0, math.radians(-50), 0)})
        hx, hy, hz = hand_pos(j["shoulder"], -1, r_rot, 0.95)
        lance_pitch = math.radians(-90 + (55 if atk == 1 else 25 if atk == 0 else 0))
        sp = sub.add(kit.cylinder(0, 0, -0.12, 0.26, 0.006, P["wood"], n=6, name="lance"), (hx, hy, hz),
                     (0, lance_pitch if atk is not None else 0, 0))
        sub.add(kit.cylinder(0, 0, 0.26, 0.31, 0.011, P["iron"], n=6, name="tip"), (hx, hy, hz),
                (0, lance_pitch if atk is not None else 0, 0))
        sub.root.parent = rig.root
        sub.root.location = (-0.01, 0, saddle - 0.12)
        rig.done()
    return build


def ballista(d):
    def build(M):
        P = mats()
        rig = Rig(DIRS[d])
        shadow(P, 0.15)
        # carriage and wheels
        rig.add(cbox(-0.18, -0.07, 0.07, 0.14, 0.07, 0.1, P["wood"], "bed"))
        for y in (-0.09, 0.09):
            rig.add(kit.cylinder(0, 0, -0.012, 0.012, 0.07, P["wood_dark"], n=14, axis="y", name="wheel"),
                    (-0.06, y, 0.07))
        rig.add(cbox(-0.02, -0.02, 0.1, 0.02, 0.02, 0.2, P["wood"], "post"))
        # stock, torsion frame and arms
        rig.add(cbox(-0.16, -0.022, 0.2, 0.2, 0.022, 0.235, P["wood"], "stock"))
        rig.add(cbox(0.08, -0.08, 0.185, 0.12, 0.08, 0.26, P["wood_dark"], "frame"))
        for y in (-0.055, 0.055):
            rig.add(kit.cylinder(0.1, y, 0.17, 0.28, 0.016, P["rope"], n=8, name="spring"))
            arm = cbox(-0.008, 0, -0.008, 0.008, 0.11, 0.008, P["wood_dark"], "arm")
            rig.add(arm, (0.1, y, 0.24), (0, 0, math.radians(-35 if y > 0 else 35) + (math.pi if y < 0 else 0)))
        rig.add(cbox(-0.12, -0.004, 0.237, 0.2, 0.004, 0.245, P["iron"], "bolt"))
        rig.done()
    return build


# ---------------------------------------------------------------- sprite table

SPRITES = {}
for kind in ("pleb", "patrician", "eques", "liberti", "slave", "child", "soldier"):
    fr = F_CHILD if kind == "child" else F_PERSON
    for d in DIRS:
        for f in range(4):
            SPRITES["people/%s_%s_%d" % (kind, d, f)] = dict(free=True, frame=fr, build=citizen(kind, d, f))
        SPRITES["people/%s_idle_%s" % (kind, d)] = dict(free=True, frame=fr, build=citizen(kind, d, None))

for kind, fn, fr in (("legionary", legionary, F_UNIT), ("archer", archer, F_UNIT),
                     ("barbarian", barbarian, F_UNIT), ("cavalry", cavalry, F_CAVALRY)):
    for d in DIRS:
        for f in range(4):
            SPRITES["people/%s_%s_%d" % (kind, d, f)] = dict(free=True, frame=fr, build=fn(d, f, None))
        for a in range(2):
            SPRITES["people/%s_atk_%s_%d" % (kind, d, a)] = dict(free=True, frame=fr, build=fn(d, 0, a))

for d in DIRS:
    SPRITES["people/ballista_%s" % d] = dict(free=True, frame=F_BALLISTA, build=ballista(d))
