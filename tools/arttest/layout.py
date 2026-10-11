"""Shared test district: the same map for every art option.

Tiles are (x, y) with y growing south (screen-down in top-down, toward the
viewer's lower-left in isometric).  Buildings are anchored at their north-west
tile.  Run directly to print an ASCII check.
"""
W, H = 20, 14
FOOT = {"house_small": 2, "house_large": 2, "bakery": 2, "blacksmith": 2,
        "granary": 3, "temple": 3}
ROAD_ROWS = (4, 9)
ROAD_COLS = (6, 13)

_BLOCKS = {
    (0, 0): [("house_small", 0, 0), ("house_large", 2, 0), ("house_small", 4, 0),
             ("house_large", 0, 2), ("bakery", 2, 2), ("house_small", 4, 2)],
    (7, 0): [("temple", 0, 0), ("house_large", 4, 0), ("house_small", 4, 2)],
    (14, 0): [("house_small", 0, 0), ("house_small", 2, 0), ("blacksmith", 4, 0),
              ("house_large", 0, 2), ("house_small", 2, 2), ("house_large", 4, 2)],
    (0, 5): [("granary", 0, 0), ("bakery", 4, 0), ("house_small", 4, 2)],
    (7, 5): [("house_small", 0, 0), ("house_large", 2, 0), ("house_small", 4, 0),
             ("house_large", 0, 2), ("house_small", 2, 2), ("house_large", 4, 2)],
    (14, 5): [("house_large", 0, 0), ("bakery", 2, 0), ("house_small", 4, 0),
              ("house_small", 0, 2), ("house_large", 2, 2), ("blacksmith", 4, 2)],
    (0, 10): [("house_small", 0, 0), ("house_small", 2, 0), ("house_large", 4, 0),
              ("house_small", 0, 2), ("house_large", 2, 2), ("house_small", 4, 2)],
    (7, 10): [("granary", 0, 0), ("temple", 3, 0)],
    (14, 10): [("house_small", 0, 0), ("house_large", 2, 0), ("house_small", 4, 0),
               ("blacksmith", 0, 2), ("house_small", 2, 2), ("house_large", 4, 2)],
}

BUILDINGS = [(kind, bx + dx, by + dy) for (bx, by), items in _BLOCKS.items()
             for kind, dx, dy in items]


def is_road(x, y):
    return y in ROAD_ROWS or x in ROAD_COLS


def check():
    grid = [["r" if is_road(x, y) else "." for x in range(W)] for y in range(H)]
    for kind, x0, y0 in BUILDINGS:
        n = FOOT[kind]
        for y in range(y0, y0 + n):
            for x in range(x0, x0 + n):
                assert grid[y][x] == ".", (kind, x0, y0, x, y)
                grid[y][x] = kind[0].upper() if kind != "house_large" else "L"
    return "\n".join("".join(r) for r in grid)


if __name__ == "__main__":
    print(check())
