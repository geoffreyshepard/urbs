#include "sim/invariants.hpp"

#include <cstdio>

#include "sim/world.hpp"

namespace urbs::sim {

namespace {
i64 signed_sum(const i64 (&col)[econ::kMoneyCount]) {
  i64 s = 0;
  for (int k = 0; k < econ::kMoneyCount; ++k) s += econ::money_is_income(static_cast<econ::Money>(k)) ? col[k] : -col[k];
  return s;
}
}  // namespace

u32 InvariantChecker::check(const World& w, std::vector<std::string>& out, u32 limit) {
  u32 n = 0;
  char buf[256];
  auto fail = [&](const char* fmt, auto... args) {
    ++n;
    if (out.size() >= limit) return;
    std::snprintf(buf, sizeof buf, fmt, args...);
    out.emplace_back(buf);
  };
  const auto& B = w.buildings;
  const auto& C = w.citizens;
  const auto& E = w.econ;
  auto bname = [&](BuildingId id) { return w.defs->b(B[id].def).id.c_str(); };

  // 1. Inventories: the cached total is the sum of the amounts.
  B.for_each([&](BuildingId id, const Building& b) {
    if (b.inv == econ::kNoInv) return;
    u32 s = 0;
    for (u32 g = 0; g < E.goods; ++g) s += E.amount[static_cast<size_t>(b.inv) * E.goods + g];
    if (s != E.total[b.inv]) fail("inventory total: %s #%u holds %u but caches %u", bname(id), id.index(), s, E.total[b.inv]);
  });

  // 2. Hauls: reservations equal what live hauls carry; the open count matches; hauls point at live things.
  const size_t cells = static_cast<size_t>(E.amount.size());
  std::vector<u32> want_in(cells, 0), want_out(cells, 0);
  u32 open = 0;
  for (u32 t = 0; t < E.task_ids.high_water(); ++t) {
    if (!E.task_ids.alive_index(t)) continue;
    const econ::HaulTask& task = E.tasks[t];
    if (task.state == econ::TaskState::Open) ++open;
    const bool src_ok = B.alive(task.src) && B[task.src].inv != econ::kNoInv;
    const bool dst_ok = B.alive(task.dst) && B[task.dst].inv != econ::kNoInv;
    if (!dst_ok || (!src_ok && task.state != econ::TaskState::Carrying)) {
      fail("haul %u (%s x%u) points at a demolished building", t, w.defs->goods[task.good.v].id.c_str(), task.amount);
      continue;
    }
    want_in[static_cast<size_t>(B[task.dst].inv) * E.goods + task.good.v] += task.amount;
    if (task.state != econ::TaskState::Carrying)
      want_out[static_cast<size_t>(B[task.src].inv) * E.goods + task.good.v] += task.amount;
    if (task.state != econ::TaskState::Open) {
      const bool porter_ok = task.porter.valid() && C.alive(task.porter) && C.task[task.porter.index()] == t;
      if (!porter_ok) fail("haul %u (%s x%u) is claimed by a porter who is gone or doing something else", t,
                           w.defs->goods[task.good.v].id.c_str(), task.amount);
    }
  }
  if (open != E.open_tasks) fail("open hauls: %u counted, %u cached", open, E.open_tasks);
  B.for_each([&](BuildingId id, const Building& b) {
    if (b.inv == econ::kNoInv) return;
    for (u32 g = 0; g < E.goods; ++g) {
      const size_t c = static_cast<size_t>(b.inv) * E.goods + g;
      if (E.res_in[c] != want_in[c])
        fail("reserved inbound: %s #%u %s %u, live hauls bring %u", bname(id), id.index(), w.defs->goods[g].id.c_str(),
             E.res_in[c], want_in[c]);
      if (E.res_out[c] != want_out[c])
        fail("reserved outbound: %s #%u %s %u, live hauls take %u", bname(id), id.index(), w.defs->goods[g].id.c_str(),
             E.res_out[c], want_out[c]);
    }
  });

  // 3. People: building and household counters match a recount; references point at live things.
  std::vector<u32> employed(B.ids.high_water(), 0), residents(B.ids.high_water(), 0);
  std::vector<u32> members(w.households.ids.high_water(), 0);
  for (u32 i = 0; i < C.ids.high_water(); ++i) {
    if (!C.ids.alive_index(i)) continue;
    if (C.work[i].valid()) {
      if (!B.alive(C.work[i])) fail("citizen %u works at a demolished building", i);
      else ++employed[C.work[i].index()];
    }
    if (C.home[i].valid()) {
      if (!B.alive(C.home[i])) fail("citizen %u lives in a demolished building", i);
      else ++residents[C.home[i].index()];
    }
    if (C.household[i].valid()) {
      if (!w.households.ids.alive(C.household[i])) fail("citizen %u belongs to a dissolved household", i);
      else ++members[C.household[i].index()];
    }
    if (C.loc[i].valid() && !B.alive(C.loc[i])) fail("citizen %u is inside a demolished building", i);
  }
  B.for_each([&](BuildingId id, const Building& b) {
    if (b.employed != employed[id.index()])
      fail("workers: %s #%u counts %u, %u citizens work there", bname(id), id.index(), b.employed, employed[id.index()]);
    if (b.residents != residents[id.index()])
      fail("residents: %s #%u counts %u, %u citizens live there", bname(id), id.index(), b.residents,
           residents[id.index()]);
  });
  for (u32 k = 0; k < w.households.ids.high_water(); ++k) {
    if (!w.households.ids.alive_index(k)) continue;
    const Household& h = w.households.h[k];
    if (h.members != members[k]) fail("household %u counts %u members, %u citizens belong to it", k, h.members, members[k]);
    if (h.home.valid() && !B.alive(h.home)) fail("household %u lives in a demolished building", k);
  }

  // 4. Map: every building's footprint points back at it, and no tile points at a dead building.
  B.for_each([&](BuildingId id, const Building& b) {
    for (i32 y = b.y; y < b.y + b.h; ++y)
      for (i32 x = b.x; x < b.x + b.w; ++x)
        if (w.map.building[static_cast<size_t>(w.map.idx(x, y))] != id) {
          fail("map: tile %d,%d of %s #%u points elsewhere", x, y, bname(id), id.index());
          return;
        }
  });
  for (size_t i = 0; i < w.map.building.size(); ++i) {
    const BuildingId b = w.map.building[i];
    if (b.valid() && !B.alive(b)) {
      fail("map: tile %d,%d points at a demolished building", static_cast<int>(i % static_cast<size_t>(w.map.w)),
           static_cast<int>(i / static_cast<size_t>(w.map.w)));
      break;
    }
  }

  // 5. Entrances (once pending map changes have been applied): on a walkable tile.
  if (!w.entrances_dirty_)
    B.for_each([&](BuildingId id, const Building& b) {
      if (b.state == BState::Ruin || b.entrance < 0) return;
      if (w.nav.cost[static_cast<size_t>(b.entrance)] == 0)
        fail("entrance of %s #%u at %d,%d is not walkable", bname(id), id.index(), b.entrance % w.map.w,
             b.entrance / w.map.w);
    });

  // 6. Money: every change to the treasury since the last call is in the ledger.
  const u32 day = static_cast<u32>(w.now / kTicksPerDay);
  const i64 today = signed_sum(E.money.today);
  if (primed_) {
    i64 booked = today - last_today_;
    if (day == last_day_ + 1) booked = signed_sum(E.money.yesterday) - last_today_ + today;
    if (day <= last_day_ + 1 && E.treasury - last_treasury_ != booked)
      fail("money: the treasury moved by %lld but the ledger booked %lld",
           static_cast<long long>(E.treasury - last_treasury_), static_cast<long long>(booked));
  }
  primed_ = true;
  last_treasury_ = E.treasury;
  last_today_ = today;
  last_day_ = day;
  return n;
}

}  // namespace urbs::sim
