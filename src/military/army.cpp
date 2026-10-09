#include "military/army.hpp"

#include <algorithm>

namespace urbs::military {

void Army::init(u32 max_soldiers, i32 map_w, i32 map_h) {
  ids.init(max_soldiers);
  x.assign(max_soldiers, 0);
  y.assign(max_soldiers, 0);
  px.assign(max_soldiers, 0);
  py.assign(max_soldiers, 0);
  hp.assign(max_soldiers, 0);
  unit.assign(max_soldiers, kNoUnit);
  slot.assign(max_soldiers, 0);
  cooldown.assign(max_soldiers, 0);
  target.assign(max_soldiers, kNoSoldier);
  facing.assign(max_soldiers, 2);
  walked.assign(max_soldiers, 0);
  attacking.assign(max_soldiers, 0);
  for (auto& u : units) u = Unit{};
  for (auto& f : fields) {
    f = FlowField{};
    f.dist.assign(static_cast<size_t>(map_w * map_h), 0xFFFFFFFFu);
  }
  projectiles.assign(4096, Projectile{});
  cells_x = (map_w + kCell - 1) / kCell;
  cells_y = (map_h + kCell - 1) / kCell;
  cell_head.assign(static_cast<size_t>(cells_x * cells_y), kNoSoldier);
  next_in_cell.assign(max_soldiers, kNoSoldier);
  deployed = 0;
  next_number = 1;
}

void Army::rebuild_hash() {
  std::fill(cell_head.begin(), cell_head.end(), kNoSoldier);
  for (u32 i = 0; i < ids.high_water(); ++i) {
    if (!ids.alive_index(i)) continue;
    const i32 c = cell_of(x[i], y[i]);
    next_in_cell[i] = cell_head[static_cast<size_t>(c)];
    cell_head[static_cast<size_t>(c)] = i;
  }
}

}  // namespace urbs::military
