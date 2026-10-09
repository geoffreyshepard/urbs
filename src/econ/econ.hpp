#pragma once
#include <vector>

#include "core/pool.hpp"
#include "core/types.hpp"

namespace urbs::econ {

inline constexpr u32 kNoInv = 0xFFFFFFFFu;
inline constexpr u16 kPorterCarry = 6;

enum class TaskState : u8 { Open, Claimed, Carrying };

struct HaulTask {
  GoodId good;
  u16 amount = 0;
  BuildingId src, dst;
  CitizenId porter;
  TaskState state = TaskState::Open;
  u8 priority = 0;  // higher first
  Tick created = 0;
};

// Building inventories (flat arrays [inv * goods + g]) and haul tasks. Fixed capacity.
struct Econ {
  u32 goods = 0;
  core::SlotAlloc<Id<struct InvTag>> inv_ids;
  std::vector<u16> amount, res_in, res_out;
  std::vector<u8> accept;     // storage policy per good: 0 reject, 1 accept
  std::vector<u16> target;    // desired stock per good (markets, storage stockpile targets)
  std::vector<u16> capacity;  // per inventory, total units
  std::vector<u32> total;     // per inventory: sum of amounts (cached)
  std::vector<BuildingId> owner;

  core::SlotAlloc<HaulTaskId> task_ids;
  std::vector<HaulTask> tasks;
  u32 open_tasks = 0;

  i64 treasury = 0;
  // Ledgers (per game day, rolled at midnight): produced / consumed per good.
  std::vector<u32> produced_today, consumed_today, produced_yday, consumed_yday;
  std::vector<u32> city_stock;  // recomputed daily: sum over storage + markets

  void init(u32 n_goods, u32 max_inv, u32 max_tasks);
  u32 create_inventory(BuildingId owner, u16 cap);
  void destroy_inventory(u32 inv);

  u16& amt(u32 inv, GoodId g) { return amount[static_cast<size_t>(inv) * goods + g.v]; }
  [[nodiscard]] u16 amt(u32 inv, GoodId g) const { return amount[static_cast<size_t>(inv) * goods + g.v]; }
  u16& rin(u32 inv, GoodId g) { return res_in[static_cast<size_t>(inv) * goods + g.v]; }
  u16& rout(u32 inv, GoodId g) { return res_out[static_cast<size_t>(inv) * goods + g.v]; }
  [[nodiscard]] u16 rin(u32 inv, GoodId g) const { return res_in[static_cast<size_t>(inv) * goods + g.v]; }
  [[nodiscard]] u16 rout(u32 inv, GoodId g) const { return res_out[static_cast<size_t>(inv) * goods + g.v]; }
  [[nodiscard]] u16 available(u32 inv, GoodId g) const {
    u16 a = amt(inv, g), r = rout(inv, g);
    return a > r ? static_cast<u16>(a - r) : 0;
  }
  [[nodiscard]] u32 incoming_total(u32 inv) const {
    u32 s = 0;
    for (u32 g = 0; g < goods; ++g) s += res_in[static_cast<size_t>(inv) * goods + g];
    return s;
  }
  [[nodiscard]] u32 free_space(u32 inv) const {
    u32 used = total[inv] + incoming_total(inv);
    return capacity[inv] > used ? capacity[inv] - used : 0;
  }
  void add(u32 inv, GoodId g, u16 n) {
    amt(inv, g) = static_cast<u16>(amt(inv, g) + n);
    total[inv] += n;
  }
  u16 take(u32 inv, GoodId g, u16 n) {
    u16 a = amt(inv, g);
    u16 t = n < a ? n : a;
    amt(inv, g) = static_cast<u16>(a - t);
    total[inv] -= t;
    return t;
  }
};

}  // namespace urbs::econ
