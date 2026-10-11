"""Smoke test for the kit: one 2x2 house, one tree, one flat tile."""
import kit


def house(M):
    kit.lot_rect(2, 2, M["garden"])
    kit.box(-0.6, -0.5, 0, 0.6, 0.5, 0.55, M["plaster_house"])
    kit.hip(-0.6, -0.5, 0.6, 0.5, 0.55, 0.3, M["roof_house"])
    kit.windows_y(-0.5, [-0.3, 0.3], 0.25, 0.4, M)


def tree(M):
    kit.cypress(0, 0, 0.9, M)


def grass(M):
    kit.lot_rect(1, 1, M["grass"])


SPRITES = {
    "test/house": dict(fp=(2, 2), build=house, lit=True),
    "test/house_wide": dict(fp=(3, 2), build=lambda M: (kit.lot_rect(3, 2, M["paving"]),
                                                       kit.box(-1.2, -0.6, 0, 1.2, 0.6, 0.6, M["travertine"]))),
    "test/tree": dict(free=True, build=tree),
    "test/grass": dict(fp=(1, 1), fixed_h=64, build=grass),
}
