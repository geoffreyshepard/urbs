#include "econ/econ.hpp"

#include <algorithm>

namespace urbs::econ {

void Econ::init(u32 n_goods, u32 max_inv, u32 max_tasks) {
  goods = n_goods;
  inv_ids.init(max_inv);
  const size_t n = static_cast<size_t>(max_inv) * n_goods;
  amount.assign(n, 0);
  res_in.assign(n, 0);
  res_out.assign(n, 0);
  accept.assign(n, 1);
  target.assign(n, 0);
  capacity.assign(max_inv, 0);
  total.assign(max_inv, 0);
  owner.assign(max_inv, BuildingId{});
  task_ids.init(max_tasks);
  tasks.assign(max_tasks, HaulTask{});
  open_tasks = 0;
  produced_today.assign(n_goods, 0);
  consumed_today.assign(n_goods, 0);
  produced_yday.assign(n_goods, 0);
  consumed_yday.assign(n_goods, 0);
  city_stock.assign(n_goods, 0);
  price.assign(n_goods, 10);
  imported_today.assign(n_goods, 0);
  exported_today.assign(n_goods, 0);
  imported_yday.assign(n_goods, 0);
  exported_yday.assign(n_goods, 0);
}

u32 Econ::create_inventory(BuildingId own, u16 cap) {
  auto id = inv_ids.create();
  if (!id) return kNoInv;
  const u32 i = id.index();
  const size_t base = static_cast<size_t>(i) * goods;
  std::fill(amount.begin() + static_cast<std::ptrdiff_t>(base), amount.begin() + static_cast<std::ptrdiff_t>(base + goods), u16{0});
  std::fill(res_in.begin() + static_cast<std::ptrdiff_t>(base), res_in.begin() + static_cast<std::ptrdiff_t>(base + goods), u16{0});
  std::fill(res_out.begin() + static_cast<std::ptrdiff_t>(base), res_out.begin() + static_cast<std::ptrdiff_t>(base + goods), u16{0});
  std::fill(accept.begin() + static_cast<std::ptrdiff_t>(base), accept.begin() + static_cast<std::ptrdiff_t>(base + goods), u8{1});
  std::fill(target.begin() + static_cast<std::ptrdiff_t>(base), target.begin() + static_cast<std::ptrdiff_t>(base + goods), u16{0});
  capacity[i] = cap;
  total[i] = 0;
  owner[i] = own;
  return i;
}

void Econ::destroy_inventory(u32 inv) {
  if (inv == kNoInv || !inv_ids.alive_index(inv)) return;
  inv_ids.destroy(inv_ids.id_at(inv));
}

}  // namespace urbs::econ
