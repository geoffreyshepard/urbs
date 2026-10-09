// Economy side of the world: construction, production, labor allocation, haul matching, porters,
// household consumption. See docs/06-simulation-design.md sections 8 and 9.
#include <algorithm>

#include "core/profile.hpp"
#include "sim/world.hpp"

namespace urbs::sim {

using data::BuildingKind;

namespace {
constexpr u32 kMaxOpenTasks = 600;
constexpr u32 kMaxTasksPerPass = 48;

int manhattan_tiles(const TileMap& m, i32 a, i32 b) {
  return std::abs(a % m.w - b % m.w) + std::abs(a / m.w - b / m.w);
}

}  // namespace

bool World::pay(i64 d) {
  if (d <= 0) return true;
  if (econ.treasury < d) return false;
  econ.treasury -= d;
  return true;
}

bool World::is_porter_job(BuildingId id) const {
  if (!buildings.alive(id)) return false;
  const Building& b = buildings[id];
  if (b.state != BState::Active) return false;
  BuildingKind k = defs->b(b.def).kind;
  return k == BuildingKind::Storage || k == BuildingKind::Market || k == BuildingKind::TradePost;
}

void World::add_stock(BuildingId id, GoodId g, u16 n) {
  if (!buildings.alive(id) || buildings[id].inv == econ::kNoInv || !g.valid()) return;
  econ.add(buildings[id].inv, g, n);
}

// ------------------------------------------------------------------------------------------------
// Households
// ------------------------------------------------------------------------------------------------

u32 World::household_food(const Household& h) const {
  u32 s = h.food;
  for (u16 v : h.fstock) s += v;
  return s;
}

void World::household_eat(u32 i) {
  auto& C = citizens;
  if (!C.household[i].valid()) return;
  Household& h = households[C.household[i]];
  if (h.food > 0) {
    --h.food;
  } else {
    int best = -1;
    for (int k = 0; k < 8; ++k)
      if (h.fstock[k] > 0 && (best < 0 || h.fstock[k] > h.fstock[best])) best = k;
    if (best < 0) return;  // nothing to eat
    --h.fstock[best];
    ++econ.consumed_today[food_slots[static_cast<size_t>(best)].v];
  }
  C.set_need(i, Need::Food, 255);
}

void World::household_shop(u32 i, BuildingId market) {
  auto& C = citizens;
  if (!C.household[i].valid() || !buildings.alive(market)) return;
  Household& h = households[C.household[i]];
  const u32 inv = buildings[market].inv;
  if (inv == econ::kNoInv) return;
  const u16 per_kind = static_cast<u16>(std::max<u32>(2, h.members * 3u));
  u32 bought = 0;
  for (size_t k = 0; k < food_slots.size() && k < 8; ++k) {
    GoodId g = food_slots[k];
    if (h.fstock[k] >= per_kind) continue;
    u16 got = econ.take(inv, g, std::min<u16>(econ.available(inv, g), static_cast<u16>(per_kind - h.fstock[k])));
    h.fstock[k] = static_cast<u16>(h.fstock[k] + got);
    bought += got;
    if (got && !C.carrying[i]) {
      C.carrying[i] = static_cast<u8>(g.v + 1);
      C.carry_amount[i] = got;
    }
  }
  for (size_t k = 0; k < hh_goods.size() && k < 8; ++k) {
    GoodId g = hh_goods[k];
    const u16 want = static_cast<u16>(std::max<u32>(1, h.members / 2u));
    if (h.goods[k] >= want) continue;
    u16 got = econ.take(inv, g, std::min<u16>(econ.available(inv, g), static_cast<u16>(want - h.goods[k])));
    h.goods[k] = static_cast<u16>(h.goods[k] + got);
    bought += got;
  }
  (void)bought;
}

// ------------------------------------------------------------------------------------------------
// Buildings: construction and production (hourly event per building)
// ------------------------------------------------------------------------------------------------

void World::on_building_event(const Event& e) {
  if (!buildings.ids.alive_index(e.target)) return;
  Building& b = buildings.b[e.target];
  if (b.token != e.token) return;
  if (static_cast<BuildingEvent>(e.type) == BuildingEvent::Hourly) building_hourly(buildings.ids.id_at(e.target));
}

void World::building_hourly(BuildingId id) {
  Building& b = buildings[id];
  const data::BuildingDef& d = defs->b(b.def);
  sched.schedule(Event{now + kTicksPerHour, id.index(), static_cast<u16>(BuildingEvent::Hourly), b.token,
                       EventKind::Building});
  if (b.state == BState::Construction) {
    bool have_all = true;
    for (const auto& c : d.cost_goods)
      if (b.inv == econ::kNoInv || econ.amt(b.inv, c.good) < c.amount) have_all = false;
    if (!have_all) return;
    b.build_progress += static_cast<u32>(b.present) * static_cast<u32>(kTicksPerHour);
    if (b.build_progress >= b.build_needed) {
      for (const auto& c : d.cost_goods) econ.take(b.inv, c.good, c.amount);
      complete_construction(id);
    }
    return;
  }
  if (b.state != BState::Active || !d.recipe.valid() || b.inv == econ::kNoInv) return;
  const data::RecipeDef& r = defs->recipes[d.recipe.v];
  u32 eff = 100;
  if (d.kind == BuildingKind::Farm) {
    if (season(now) == Season::Winter) eff = 0;
    u32 fert = 0, n = 0;
    for (i32 y = b.y; y < b.y + b.h; ++y)
      for (i32 x = b.x; x < b.x + b.w; ++x) {
        fert += map.fertility[static_cast<size_t>(map.idx(x, y))];
        ++n;
      }
    eff = eff * (n ? fert / n : 128) / 128;
  }
  b.work_progress += static_cast<u32>(b.present) * static_cast<u32>(kTicksPerHour) * eff / 100;
  // Painted buildings: recipe output scales with area via more workers, work per batch is fixed.
  while (b.work_progress >= r.work) {
    bool ok = true;
    for (const auto& in : r.inputs)
      if (econ.amt(b.inv, in.good) < in.amount) ok = false;
    u32 out_sum = 0;
    for (const auto& o : r.outputs) out_sum += o.amount;
    if (econ.total[b.inv] + out_sum > econ.capacity[b.inv]) ok = false;
    if (!ok) {
      b.work_progress = r.work;  // stalled: keep one batch of progress banked
      break;
    }
    for (const auto& in : r.inputs) {
      econ.take(b.inv, in.good, in.amount);
      econ.consumed_today[in.good.v] += in.amount;
    }
    for (const auto& o : r.outputs) {
      econ.add(b.inv, o.good, o.amount);
      econ.produced_today[o.good.v] += o.amount;
    }
    b.work_progress -= r.work;
  }
}

// ------------------------------------------------------------------------------------------------
// Labor
// ------------------------------------------------------------------------------------------------

void World::release_worker(u32 i) {
  auto& C = citizens;
  BuildingId w = C.work[i];
  if (w.valid() && buildings.alive(w)) {
    Building& b = buildings[w];
    if (b.employed > 0) --b.employed;
    if ((C.activity[i] == Activity::Work || C.activity[i] == Activity::Build) && C.loc[i] == w && b.present > 0)
      --b.present;
  }
  C.work[i] = BuildingId{};
  if (C.task[i] != kNoIndex) cancel_task(C.task[i], true);
  if (C.activity[i] == Activity::Work || C.activity[i] == Activity::Build || C.activity[i] == Activity::Haul) {
    C.activity[i] = Activity::Home;
    ++C.token[i];
    schedule_citizen(i, now + 1, CitizenEvent::Decide);
  }
}

void World::labor_allocate() {
  URBS_ZONE("labor");
  ++labor_hourly_runs;
  auto& C = citizens;
  // Unemployed working-age citizens with a home.
  scratch_u32_.clear();
  for (u32 i = 0; i < C.ids.high_water(); ++i) {
    if (!C.ids.alive_index(i) || C.work[i].valid() || !C.home[i].valid()) continue;
    const u32 age = C.age_years(i, now);
    if (age < 14 || age >= 60) continue;
    if (C.cls[i] == CitizenClass::Patrician || C.cls[i] == CitizenClass::Soldier) continue;
    scratch_u32_.push_back(i);
  }
  if (scratch_u32_.empty()) return;
  for (int prio = 1; prio <= 5 && !scratch_u32_.empty(); ++prio) {
    for (u32 bi = 0; bi < buildings.ids.high_water() && !scratch_u32_.empty(); ++bi) {
      if (!buildings.ids.alive_index(bi)) continue;
      Building& b = buildings.b[bi];
      if (b.state != BState::Active && b.state != BState::Construction) continue;
      if (b.jobs <= b.employed || b.entrance < 0) continue;
      const data::BuildingDef& d = defs->b(b.def);
      if (policy.sector_priority[static_cast<int>(sector_of(b))] != prio) continue;
      const u8 classes = b.state == BState::Construction ? static_cast<u8>(data::ClassMask::Workers) : d.worker_classes;
      while (b.employed < b.jobs && !scratch_u32_.empty()) {
        // Nearest suitable unemployed citizen by home distance.
        size_t best = scratch_u32_.size();
        int best_d = 1 << 30;
        for (size_t k = 0; k < scratch_u32_.size(); ++k) {
          u32 ci = scratch_u32_[k];
          if (!(classes & data::class_bit(static_cast<int>(C.cls[ci])))) continue;
          const i32 he = buildings[C.home[ci]].entrance;
          if (he < 0 || !nav.connected(he, b.entrance)) continue;
          int dd = manhattan_tiles(map, he, b.entrance);
          if (dd < best_d) {
            best_d = dd;
            best = k;
          }
        }
        if (best == scratch_u32_.size()) break;
        const u32 ci = scratch_u32_[best];
        scratch_u32_[best] = scratch_u32_.back();
        scratch_u32_.pop_back();
        C.work[ci] = buildings.ids.id_at(bi);
        ++b.employed;
      }
    }
  }
}

// ------------------------------------------------------------------------------------------------
// Logistics: haul matching and porters
// ------------------------------------------------------------------------------------------------

void World::cancel_task(u32 t, bool goods_lost) {
  if (!econ.task_ids.alive_index(t)) return;
  econ::HaulTask& task = econ.tasks[t];
  auto unres = [&](BuildingId b, bool in) {
    if (!buildings.alive(b) || buildings[b].inv == econ::kNoInv) return;
    u16& r = in ? econ.rin(buildings[b].inv, task.good) : econ.rout(buildings[b].inv, task.good);
    r = r > task.amount ? static_cast<u16>(r - task.amount) : 0;
  };
  if (task.state != econ::TaskState::Carrying) unres(task.src, false);
  unres(task.dst, true);
  if (task.state == econ::TaskState::Open && econ.open_tasks > 0) --econ.open_tasks;
  (void)goods_lost;
  if (task.porter.valid() && citizens.alive(task.porter)) {
    const u32 i = task.porter.index();
    if (citizens.task[i] == t) {
      citizens.task[i] = kNoIndex;
      citizens.carrying[i] = 0;
      if (citizens.activity[i] == Activity::Walk || citizens.activity[i] == Activity::Haul) {
        ++citizens.token[i];
        schedule_citizen(i, now + 1, CitizenEvent::Decide);
      }
    }
  }
  econ.task_ids.destroy(econ.task_ids.id_at(t));
}

void World::match_good(GoodId g) {
  if (econ.open_tasks >= kMaxOpenTasks) return;
  // Suppliers (storage, producers of g) and active demands (sites, producer inputs, markets).
  scratch_u32_.clear();   // suppliers: building index
  scratch_u32b_.clear();  // storage sinks accepting g
  for (u32 bi = 0; bi < buildings.ids.high_water(); ++bi) {
    if (!buildings.ids.alive_index(bi)) continue;
    const Building& b = buildings.b[bi];
    if (b.inv == econ::kNoInv || b.entrance < 0) continue;
    const data::BuildingDef& d = defs->b(b.def);
    if (b.state == BState::Construction) continue;
    const bool storage = d.kind == BuildingKind::Storage || d.kind == BuildingKind::TradePost;
    bool supplies = storage;
    if (!storage && d.recipe.valid())
      for (const auto& o : defs->recipes[d.recipe.v].outputs)
        if (o.good == g) supplies = true;
    if (supplies && econ.available(b.inv, g) > 0) scratch_u32_.push_back(bi);
    if (storage && b.state == BState::Active && econ.accept[static_cast<size_t>(b.inv) * econ.goods + g.v] &&
        econ.free_space(b.inv) > 0)
      scratch_u32b_.push_back(bi);
  }
  u32 made = 0;
  auto make_task = [&](u32 src_bi, u32 dst_bi, u16 amount, u8 prio) {
    auto id = econ.task_ids.create();
    if (!id) return false;
    econ::HaulTask& t = econ.tasks[id.index()];
    t = econ::HaulTask{};
    t.good = g;
    t.amount = amount;
    t.src = buildings.ids.id_at(src_bi);
    t.dst = buildings.ids.id_at(dst_bi);
    t.state = econ::TaskState::Open;
    t.priority = prio;
    t.created = now;
    econ.rout(buildings.b[src_bi].inv, g) = static_cast<u16>(econ.rout(buildings.b[src_bi].inv, g) + amount);
    econ.rin(buildings.b[dst_bi].inv, g) = static_cast<u16>(econ.rin(buildings.b[dst_bi].inv, g) + amount);
    ++econ.open_tasks;
    ++made;
    return true;
  };
  auto nearest_supplier = [&](u32 dst_bi, bool storage_only) -> u32 {
    const Building& db = buildings.b[dst_bi];
    u32 best = kNoIndex;
    int best_d = 1 << 30;
    for (u32 si : scratch_u32_) {
      if (si == dst_bi) continue;
      const Building& sb = buildings.b[si];
      if (econ.available(sb.inv, g) == 0) continue;
      if (storage_only) {
        BuildingKind k = defs->b(sb.def).kind;
        if (k != BuildingKind::Storage && k != BuildingKind::TradePost) continue;
      }
      if (!nav.connected(sb.entrance, db.entrance)) continue;
      int dd = manhattan_tiles(map, sb.entrance, db.entrance);
      if (dd < best_d) {
        best_d = dd;
        best = si;
      }
    }
    return best;
  };

  // Pass A: active demands.
  for (u32 bi = 0; bi < buildings.ids.high_water() && made < kMaxTasksPerPass; ++bi) {
    if (!buildings.ids.alive_index(bi)) continue;
    const Building& b = buildings.b[bi];
    if (b.inv == econ::kNoInv || b.entrance < 0) continue;
    const data::BuildingDef& d = defs->b(b.def);
    u32 want = 0;
    u8 prio = 1;
    if (b.state == BState::Construction) {
      for (const auto& c : d.cost_goods)
        if (c.good == g) want = c.amount;
      prio = 3;
    } else if (b.state == BState::Active && d.recipe.valid()) {
      for (const auto& in : defs->recipes[d.recipe.v].inputs)
        if (in.good == g) want = in.amount * 3u;
      prio = 2;
    } else if (b.state == BState::Active && d.kind == BuildingKind::Market) {
      want = econ.target[static_cast<size_t>(b.inv) * econ.goods + g.v];
      prio = 1;
    }
    if (want == 0) continue;
    const u32 have = econ.amt(b.inv, g) + econ.rin(b.inv, g);
    if (have >= want) continue;
    if (econ.free_space(b.inv) == 0) continue;
    // Markets restock only from storage, never directly from workshops' raw output piles.
    u32 si = nearest_supplier(bi, false);
    if (si == kNoIndex) continue;
    u16 amount = static_cast<u16>(std::min<u32>({want - have, econ.available(buildings.b[si].inv, g),
                                                 econ::kPorterCarry, econ.free_space(b.inv)}));
    if (amount == 0) continue;
    if (!make_task(si, bi, amount, prio)) break;
  }

  // Pass B: clear producer outputs into storage.
  for (u32 si : scratch_u32_) {
    if (made >= kMaxTasksPerPass) break;
    const Building& sb = buildings.b[si];
    const data::BuildingDef& sd = defs->b(sb.def);
    if (sd.kind == BuildingKind::Storage || sd.kind == BuildingKind::TradePost || !sd.recipe.valid()) continue;
    bool is_output = false;
    for (const auto& o : defs->recipes[sd.recipe.v].outputs)
      if (o.good == g) is_output = true;
    if (!is_output) continue;
    u16 avail = econ.available(sb.inv, g);
    if (avail == 0) continue;
    u32 best = kNoIndex;
    int best_d = 1 << 30;
    for (u32 ti : scratch_u32b_) {
      const Building& tb = buildings.b[ti];
      if (econ.free_space(tb.inv) == 0 || !nav.connected(sb.entrance, tb.entrance)) continue;
      int dd = manhattan_tiles(map, sb.entrance, tb.entrance);
      if (dd < best_d) {
        best_d = dd;
        best = ti;
      }
    }
    if (best == kNoIndex) continue;
    u16 amount = static_cast<u16>(std::min<u32>({avail, econ::kPorterCarry, econ.free_space(buildings.b[best].inv)}));
    if (amount > 0 && !make_task(si, best, amount, 0)) break;
  }
}

bool World::claim_task(u32 i) {
  auto& C = citizens;
  if (econ.open_tasks == 0) return false;
  const i32 here = current_tile(i);
  if (here < 0) return false;
  u32 best = kNoIndex;
  int best_score = 1 << 30;
  for (u32 t = 0; t < econ.task_ids.high_water(); ++t) {
    if (!econ.task_ids.alive_index(t)) continue;
    const econ::HaulTask& task = econ.tasks[t];
    if (task.state != econ::TaskState::Open || !buildings.alive(task.src)) continue;
    const i32 se = buildings[task.src].entrance;
    if (se < 0 || !nav.connected(here, se)) continue;
    int score = manhattan_tiles(map, here, se) - static_cast<int>(task.priority) * 40 -
                static_cast<int>(std::min<Tick>(now - task.created, 400) / 10);
    if (score < best_score) {
      best_score = score;
      best = t;
    }
  }
  if (best == kNoIndex) return false;
  econ::HaulTask& task = econ.tasks[best];
  task.state = econ::TaskState::Claimed;
  task.porter = C.ids.id_at(i);
  --econ.open_tasks;
  C.task[i] = best;
  request_walk(i, here, task.src, buildings[task.src].entrance, Activity::Haul);
  return true;
}

void World::haul_step(u32 i) {
  auto& C = citizens;
  const u32 t = C.task[i];
  auto done = [&]() {
    C.task[i] = kNoIndex;
    C.carrying[i] = 0;
    C.activity[i] = Activity::Home;
    ++C.token[i];
    schedule_citizen(i, now + 1, CitizenEvent::Decide);
  };
  if (t == kNoIndex || !econ.task_ids.alive_index(t) || econ.tasks[t].porter != C.ids.id_at(i)) {
    C.task[i] = kNoIndex;
    done();
    return;
  }
  econ::HaulTask& task = econ.tasks[t];
  if (!buildings.alive(task.src) || !buildings.alive(task.dst)) {
    cancel_task(t, true);
    done();
    return;
  }
  if (task.state == econ::TaskState::Claimed) {
    const u32 sinv = buildings[task.src].inv;
    u16& ro = econ.rout(sinv, task.good);
    ro = ro > task.amount ? static_cast<u16>(ro - task.amount) : 0;
    u16 got = econ.take(sinv, task.good, task.amount);
    if (got < task.amount) {
      u16& ri = econ.rin(buildings[task.dst].inv, task.good);
      u16 diff = static_cast<u16>(task.amount - got);
      ri = ri > diff ? static_cast<u16>(ri - diff) : 0;
      task.amount = got;
    }
    if (got == 0) {
      task.state = econ::TaskState::Carrying;  // nothing to carry: release dst reservation (already 0)
      econ.task_ids.destroy(econ.task_ids.id_at(t));
      done();
      return;
    }
    task.state = econ::TaskState::Carrying;
    C.carrying[i] = static_cast<u8>(task.good.v + 1);
    C.carry_amount[i] = got;
    request_walk(i, current_tile(i), task.dst, buildings[task.dst].entrance, Activity::Haul);
    return;
  }
  // Carrying: deliver.
  const u32 dinv = buildings[task.dst].inv;
  u16& ri = econ.rin(dinv, task.good);
  ri = ri > task.amount ? static_cast<u16>(ri - task.amount) : 0;
  econ.add(dinv, task.good, task.amount);
  econ.task_ids.destroy(econ.task_ids.id_at(t));
  done();
}

void World::daily_update() {
  std::swap(econ.produced_today, econ.produced_yday);
  std::swap(econ.consumed_today, econ.consumed_yday);
  std::fill(econ.produced_today.begin(), econ.produced_today.end(), 0u);
  std::fill(econ.consumed_today.begin(), econ.consumed_today.end(), 0u);
  std::fill(econ.city_stock.begin(), econ.city_stock.end(), 0u);
  buildings.for_each([&](BuildingId, const Building& b) {
    if (b.inv == econ::kNoInv || b.state != BState::Active) return;
    BuildingKind k = defs->b(b.def).kind;
    if (k != BuildingKind::Storage && k != BuildingKind::Market && k != BuildingKind::TradePost) return;
    for (u32 g = 0; g < econ.goods; ++g) econ.city_stock[g] += econ.amount[static_cast<size_t>(b.inv) * econ.goods + g];
  });
}

}  // namespace urbs::sim
