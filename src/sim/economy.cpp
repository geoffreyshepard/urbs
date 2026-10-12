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

}  // namespace

bool World::is_porter_job(BuildingId id) const {
  if (!buildings.alive(id)) return false;
  const Building& b = buildings[id];
  if (b.state != BState::Active) return false;
  BuildingKind k = defs->b(b.def).kind;
  return k == BuildingKind::Storage || k == BuildingKind::Market || k == BuildingKind::TradePost || defs->b(b.def).carts;
}

u16 World::carry_for(u32 src_bi, u32 dst_bi) const {
  if (carters_ == 0) return econ::kPorterCarry;
  const int d = manhattan_tiles(map, buildings.b[src_bi].entrance, buildings.b[dst_bi].entrance);
  return d >= econ::kCartMinTiles ? econ::kCartCarry : econ::kPorterCarry;
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
  if (h.meals > 0) {
    --h.meals;
  } else if (h.food > 0) {
    --h.food;
    h.meals = 1;
  } else {
    int best = -1;
    for (int k = 0; k < 8; ++k)
      if (h.fstock[k] > 0 && (best < 0 || h.fstock[k] > h.fstock[best])) best = k;
    if (best < 0) return;  // nothing to eat
    --h.fstock[best];
    const GoodId g = food_slots[static_cast<size_t>(best)];
    ++econ.consumed_today[g.v];
    h.meals = static_cast<u8>(std::max(1, 2 * static_cast<int>(defs->goods[g.v].nutrition)) - 1);
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
  i64 spent_tenths = 0;
  // Units affordable at the market price (tenths of a denarius); savings never go negative.
  // Annona: staples (food worth at most 2 denarii) are free for poor households, or for all.
  const bool dole = policy.rations == 2 || (policy.rations == 1 && h.savings < 10);
  auto afford = [&](GoodId g, u16 want) -> u16 {
    if (dole && defs->goods[g.v].food && defs->goods[g.v].base_price <= 2) return want;
    const u32 unit = std::max<u32>(1, econ.price[g.v]);
    const i64 budget = static_cast<i64>(h.savings) * 10 - spent_tenths;
    if (budget <= 0) return 0;
    return static_cast<u16>(std::min<i64>(want, budget / unit));
  };
  for (size_t k = 0; k < food_slots.size() && k < 8; ++k) {
    GoodId g = food_slots[k];
    if (h.fstock[k] >= per_kind) continue;
    u16 got = econ.take(inv, g, afford(g, std::min<u16>(econ.available(inv, g), static_cast<u16>(per_kind - h.fstock[k]))));
    if (!(dole && defs->goods[g.v].base_price <= 2)) spent_tenths += static_cast<i64>(got) * econ.price[g.v];
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
    u16 got = econ.take(inv, g, afford(g, std::min<u16>(econ.available(inv, g), static_cast<u16>(want - h.goods[k]))));
    spent_tenths += static_cast<i64>(got) * econ.price[g.v];
    h.goods[k] = static_cast<u16>(h.goods[k] + got);
    bought += got;
  }
  (void)bought;
  const i64 paid = (spent_tenths + 9) / 10;
  h.savings -= static_cast<i32>(std::min<i64>(paid, h.savings));
  earn(paid, econ::Money::Sales);
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
  hazards_hourly(id);
  if (b.state == BState::Ruin || b.state == BState::Burning) return;
  if (b.state == BState::Construction) {
    bool have_all = true;
    for (const auto& c : d.cost_goods)
      if (b.inv == econ::kNoInv || econ.amt(b.inv, c.good) < materials_needed_now(b, c.amount)) have_all = false;
    if (!have_all) return;
    const u32 stage = construction_stage(b);
    b.build_progress += static_cast<u32>(b.present) * static_cast<u32>(kTicksPerHour);
    // A monument stage ends only where the next stage's materials are needed.
    if (d.kind == BuildingKind::Monument && stage + 1 < kMonumentStages) {
      const u32 stage_end = static_cast<u32>(static_cast<u64>(b.build_needed) * (stage + 1) / kMonumentStages);
      bool next_ready = true;
      for (const auto& c : d.cost_goods)
        if (econ.amt(b.inv, c.good) < (c.amount * (stage + 2) + kMonumentStages - 1) / kMonumentStages) next_ready = false;
      if (!next_ready) b.build_progress = std::min(b.build_progress, stage_end);
    }
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
    if (effect(Effect::Harvest)) eff = eff * 3 / 2;
    if (tech(Tech::CropRotation)) eff = eff * 6 / 5;
    if (effect(Effect::Blight)) eff = 0;
    if (effect(Effect::Drought)) eff = eff * 3 / 10;
    u32 fert = 0, n = 0;
    for (i32 y = b.y; y < b.y + b.h; ++y)
      for (i32 x = b.x; x < b.x + b.w; ++x) {
        fert += map.fertility[static_cast<size_t>(map.idx(x, y))];
        ++n;
      }
    eff = eff * (n ? fert / n : 128) / 128;
  }
  if (d.kind != BuildingKind::Farm && effect(Effect::Workshops)) eff = eff * 6 / 5;
  if (d.kind != BuildingKind::Farm && tech(Tech::WaterMills)) eff = eff * 23 / 20;
  // Each present worker contributes 75% plus up to 50% more with skill (and work traits).
  const u32 effort = static_cast<u32>(b.present) * 75 + b.skill_sum * 50 / 255;
  b.work_progress += effort * static_cast<u32>(kTicksPerHour) / 100 * eff / 100;
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
    earn(r.coins, econ::Money::Mint);
    // By the Nile, harvests draw down the soil until next year's flood renews it.
    if (nile && d.kind == BuildingKind::Farm)
      for (i32 y = b.y; y < b.y + b.h; ++y)
        for (i32 x = b.x; x < b.x + b.w; ++x) {
          u8& f = map.fertility[static_cast<size_t>(map.idx(x, y))];
          f = static_cast<u8>(std::max(60, f - 1));
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

u32 World::construction_stage(const Building& b) const {
  if (defs->b(b.def).kind != BuildingKind::Monument || b.build_needed == 0) return 0;
  return std::min<u32>(kMonumentStages - 1, static_cast<u32>(static_cast<u64>(b.build_progress) * kMonumentStages / b.build_needed));
}

u32 World::materials_needed_now(const Building& b, u32 amount) const {
  if (defs->b(b.def).kind != BuildingKind::Monument) return amount;
  return (amount * (construction_stage(b) + 1) + kMonumentStages - 1) / kMonumentStages;
}

// A construction site hires builders once its materials (for monuments: the current stage's share)
// are delivered or on the way.
bool World::site_ready(const Building& b) const {
  if (b.state != BState::Construction) return true;
  for (const auto& c : defs->b(b.def).cost_goods)
    if (b.inv == econ::kNoInv || econ.amt(b.inv, c.good) + econ.rin(b.inv, c.good) < materials_needed_now(b, c.amount))
      return false;
  return true;
}

void World::labor_allocate() {
  URBS_ZONE("labor");
  ++labor_hourly_runs;
  auto& C = citizens;
  // Bucket unemployed working-age citizens by the chunk of their home (counting sort, no allocation).
  const i32 cx = nav.chunks_x(), cy = nav.chunks_y(), nchunks = cx * cy;
  labor_head_.assign(static_cast<size_t>(nchunks) + 1, 0);
  scratch_u32_.clear();
  for (u32 i = 0; i < C.ids.high_water(); ++i) {
    if (!C.ids.alive_index(i) || C.work[i].valid() || !C.home[i].valid() || !buildings.alive(C.home[i])) continue;
    const u32 age = C.age_years(i, now);
    if (age < 14 || age >= 60) continue;
    if (C.cls[i] == CitizenClass::Patrician || C.cls[i] == CitizenClass::Soldier) continue;
    if (C.activity[i] == Activity::Leave) continue;
    const i32 he = buildings[C.home[i]].entrance;
    if (he < 0) continue;
    scratch_u32_.push_back(i);
    ++labor_head_[static_cast<size_t>(nav.chunk_of(he % map.w, he / map.w)) + 1];
  }
  if (scratch_u32_.empty()) return;
  for (i32 k = 0; k < nchunks; ++k) labor_head_[static_cast<size_t>(k) + 1] += labor_head_[static_cast<size_t>(k)];
  labor_fill_.assign(labor_head_.begin(), labor_head_.end() - 1);
  scratch_u32b_.resize(scratch_u32_.size());
  for (u32 ci : scratch_u32_) {
    const i32 he = buildings[C.home[ci]].entrance;
    scratch_u32b_[labor_fill_[static_cast<size_t>(nav.chunk_of(he % map.w, he / map.w))]++] = ci;
  }
  // scratch_u32b_ now holds candidates grouped by chunk; a taken candidate is marked kNoIndex.
  // Hire the nearest reachable candidate for building bi; false if there is none.
  auto hire_one = [&](u32 bi) -> bool {
    Building& b = buildings.b[bi];
    const data::BuildingDef& d = defs->b(b.def);
    const u8 classes = b.state == BState::Construction ? static_cast<u8>(data::ClassMask::Workers) : d.worker_classes;
    const i32 bx = b.entrance % map.w, by = b.entrance / map.w;
    const i32 bcx = bx / kChunkTiles, bcy = by / kChunkTiles;
    // Search chunk rings outward; take the nearest candidate in the first ring that has one.
    for (i32 r = 0; r < std::max(cx, cy); ++r) {
      size_t best = kNoIndex;
      int best_d = 1 << 30;
      for (i32 qy = bcy - r; qy <= bcy + r; ++qy)
        for (i32 qx = bcx - r; qx <= bcx + r; ++qx) {
          if (qx < 0 || qy < 0 || qx >= cx || qy >= cy) continue;
          if (std::max(std::abs(qx - bcx), std::abs(qy - bcy)) != r) continue;
          const i32 q = qy * cx + qx;
          for (u32 k = labor_head_[static_cast<size_t>(q)]; k < labor_head_[static_cast<size_t>(q) + 1]; ++k) {
            const u32 ci = scratch_u32b_[k];
            if (ci == kNoIndex) continue;
            if (!(classes & data::class_bit(static_cast<int>(C.cls[ci])))) continue;
            const i32 he = buildings[C.home[ci]].entrance;
            if (!nav.connected(he, b.entrance)) continue;
            const int dd = manhattan_tiles(map, he, b.entrance);
            if (dd < best_d) {
              best_d = dd;
              best = k;
            }
          }
        }
      if (best == kNoIndex) continue;
      const u32 ci = scratch_u32b_[best];
      scratch_u32b_[best] = kNoIndex;
      C.work[ci] = buildings.ids.id_at(bi);
      ++b.employed;
      return true;
    }
    return false;
  };
  // Within a priority level, hire round-robin (one worker per building per round) so every
  // workplace in a production chain gets some staff before any one is filled.
  labor_done_.assign(buildings.ids.high_water(), 0);
  u32 assigned = 0;
  const u32 kMaxAssign = 600;
  for (int prio = 1; prio <= 5 && assigned < kMaxAssign; ++prio) {
    for (bool progress = true; progress && assigned < kMaxAssign;) {
      progress = false;
      for (u32 bi = 0; bi < buildings.ids.high_water() && assigned < kMaxAssign; ++bi) {
        if (!buildings.ids.alive_index(bi) || labor_done_[bi]) continue;
        Building& b = buildings.b[bi];
        if (b.state != BState::Active && b.state != BState::Construction) continue;
        if (b.jobs <= b.employed || b.entrance < 0 || !site_ready(b)) continue;
        if (policy.sector_priority[static_cast<int>(sector_of(b))] != prio) continue;
        if (hire_one(bi)) {
          ++assigned;
          progress = true;
        } else {
          labor_done_[bi] = 1;
        }
      }
    }
  }
  // Priority: sectors with vacancies take workers from lower-priority sectors (lowest first).
  u32 moved = 0;
  for (int prio = 1; prio <= 4 && moved < 60; ++prio) {
    for (u32 bi = 0; bi < buildings.ids.high_water() && moved < 60; ++bi) {
      if (!buildings.ids.alive_index(bi)) continue;
      Building& b = buildings.b[bi];
      if ((b.state != BState::Active && b.state != BState::Construction) || b.jobs <= b.employed || b.entrance < 0) continue;
      if (!site_ready(b)) continue;
      if (policy.sector_priority[static_cast<int>(sector_of(b))] != prio) continue;
      const data::BuildingDef& d = defs->b(b.def);
      const u8 classes = b.state == BState::Construction ? static_cast<u8>(data::ClassMask::Workers) : d.worker_classes;
      for (int low = 5; low >= prio + 2 && b.employed < b.jobs; --low)
        for (u32 ci = 0; ci < C.ids.high_water() && b.employed < b.jobs; ++ci) {
          if (!C.ids.alive_index(ci) || !C.work[ci].valid() || !buildings.alive(C.work[ci])) continue;
          const Building& from = buildings[C.work[ci]];
          if (policy.sector_priority[static_cast<int>(sector_of(from))] != low) continue;
          if (!(classes & data::class_bit(static_cast<int>(C.cls[ci])))) continue;
          if (!C.home[ci].valid() || !buildings.alive(C.home[ci]) || !nav.connected(buildings[C.home[ci]].entrance, b.entrance))
            continue;
          release_worker(ci);
          C.work[ci] = buildings.ids.id_at(bi);
          ++b.employed;
          ++moved;
        }
    }
  }
}

void World::count_carters() {
  carters_ = 0;
  buildings.for_each([&](BuildingId, const Building& b) {
    if (b.state == BState::Active && defs->b(b.def).carts) carters_ += b.employed;
  });
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

void World::expire_stale_tasks() {
  // A haul still holds its reservations, and a reserved delivery is never planned twice, so a stuck
  // haul could leave a site waiting forever for its last few units. One nobody has claimed for a day
  // (no porter can reach its source, or none is free), or any haul two days old (hauls take hours;
  // its porter is lost), is dropped; the next matching pass plans the delivery afresh.
  for (u32 t = 0; t < econ.task_ids.high_water(); ++t) {
    if (!econ.task_ids.alive_index(t)) continue;
    const Tick age = now - econ.tasks[t].created;
    if ((econ.tasks[t].state == econ::TaskState::Open && age > kTicksPerDay) || age > 2 * kTicksPerDay)
      cancel_task(t, true);
  }
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
    // Goods staged for export stay at the trade post (imports there are handed out as usual) while the
    // city has more than the export level and an open route still buys them this year; otherwise
    // nothing is for sale, and what is staged is the city's again (kept goods could sit at the post
    // for good, and unsold ones fill it until no import fits).
    bool supplies = storage && !(d.kind == BuildingKind::TradePost && trade_mode[g.v] == 2 &&
                                 econ.city_stock[g.v] > trade_level[g.v] && open_route_buys(g, d.needs_water_adjacent));
    if (!storage && d.recipe.valid())
      for (const auto& o : defs->recipes[d.recipe.v].outputs)
        if (o.good == g) supplies = true;
    if (supplies && econ.available(b.inv, g) > 0) scratch_u32_.push_back(bi);
    // Sinks for producer output: warehouses/granaries/stockpiles (trade posts only stage trade goods).
    if (d.kind == BuildingKind::Storage && b.state == BState::Active &&
        econ.accept[static_cast<size_t>(b.inv) * econ.goods + g.v] && econ.free_space(b.inv) > 0)
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
    } else if (b.state == BState::Active && d.recipe.valid() && trade_mode[g.v] != 3) {
      // Stockpiled goods stay in storage: workshops do not draw on them either.
      for (const auto& in : defs->recipes[d.recipe.v].inputs)
        if (in.good == g) want = in.amount * 3u;
      prio = 2;
    } else if (b.state == BState::Active && d.kind == BuildingKind::Market && trade_mode[g.v] != 3) {
      want = econ.target[static_cast<size_t>(b.inv) * econ.goods + g.v];
      prio = 1;
    } else if (b.state == BState::Active && d.kind == BuildingKind::Military && !d.accepts.empty() &&
               econ.accept[static_cast<size_t>(b.inv) * econ.goods + g.v]) {
      want = defs->goods[g.v].food ? 30 : 40;  // garrison food, weapons and armor
      prio = 2;
    } else if (b.state == BState::Active && d.kind == BuildingKind::TradePost && trade_mode[g.v] == 2 &&
               econ.city_stock[g.v] > trade_level[g.v] && open_route_buys(g, d.needs_water_adjacent)) {
      want = 80;  // stage export goods at the trade post (only what a visiting partner will buy)
      prio = 0;
    }
    if (want == 0) continue;
    const u32 have = econ.amt(b.inv, g) + econ.rin(b.inv, g);
    if (have >= want) continue;
    if (econ.free_space(b.inv) == 0) continue;
    // Markets restock only from storage, never directly from workshops' raw output piles.
    // Trade posts stage exports only from other storage (never from themselves or other posts).
    u32 si = nearest_supplier(bi, d.kind == BuildingKind::TradePost);
    if (si == kNoIndex) continue;
    u16 amount = static_cast<u16>(std::min<u32>({want - have, econ.available(buildings.b[si].inv, g),
                                                 carry_for(si, bi), econ.free_space(b.inv)}));
    if (amount == 0) continue;
    if (!make_task(si, bi, amount, prio)) break;
  }

  // Pass B: clear producer outputs into storage.
  for (u32 si : scratch_u32_) {
    if (made >= kMaxTasksPerPass) break;
    const Building& sb = buildings.b[si];
    const data::BuildingDef& sd = defs->b(sb.def);
    if (sd.kind == BuildingKind::Storage) continue;
    if (sd.kind == BuildingKind::TradePost) {
      // A trade post is a dock, not a warehouse: when it is three-quarters full, what is not held for
      // a caravan goes to storage so imports still fit.
      if (econ.free_space(sb.inv) * 4 > sd.storage) continue;
    } else {
      if (!sd.recipe.valid()) continue;
      bool is_output = false;
      for (const auto& o : defs->recipes[sd.recipe.v].outputs)
        if (o.good == g) is_output = true;
      if (!is_output) continue;
    }
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
    u16 amount = static_cast<u16>(std::min<u32>({avail, carry_for(si, best), econ.free_space(buildings.b[best].inv)}));
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
  const bool carter = C.work[i].valid() && buildings.alive(C.work[i]) && defs->b(buildings[C.work[i]].def).carts;
  for (u32 t = 0; t < econ.task_ids.high_water(); ++t) {
    if (!econ.task_ids.alive_index(t)) continue;
    const econ::HaulTask& task = econ.tasks[t];
    if (task.state != econ::TaskState::Open || !buildings.alive(task.src)) continue;
    // Cart loads need a cart; carters prefer them.
    const bool cart_load = task.amount > econ::kPorterCarry;
    if (cart_load && !carter) continue;
    const i32 se = buildings[task.src].entrance;
    if (se < 0 || !nav.connected(here, se)) continue;
    int score = manhattan_tiles(map, here, se) - static_cast<int>(task.priority) * 40 -
                static_cast<int>(std::min<Tick>(now - task.created, 400) / 10) - (carter && cart_load ? 80 : 0);
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
  econ.money.roll_day();
  if (day_of_year(now) == 0) econ.money.roll_year();
  std::swap(econ.imported_today, econ.imported_yday);
  std::swap(econ.exported_today, econ.exported_yday);
  std::fill(econ.imported_today.begin(), econ.imported_today.end(), 0u);
  std::fill(econ.exported_today.begin(), econ.exported_today.end(), 0u);
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
  // Market prices follow scarcity: stock measured in days of yesterday's consumption.
  for (u32 g = 0; g < econ.goods; ++g) {
    const u32 base = static_cast<u32>(defs->goods[g].base_price) * 10;
    const u32 use = std::max<u32>(1, econ.consumed_yday[g]);
    const u32 days_x10 = econ.city_stock[g] * 10 / use;  // 30 = three days of stock
    i32 pct = 150 - static_cast<i32>(std::min<u32>(days_x10, 90)) * 100 / 90;  // 150% when empty, 50% at 9+ days
    if (econ.consumed_yday[g] == 0) pct = 100;
    econ.price[g] = static_cast<u16>(std::max<u32>(1, base * static_cast<u32>(std::clamp(pct, 60, 200)) / 100));
  }
  if (econ.treasury < 0) add_alert(AlertKind::TreasuryLow, BuildingId{});
}

// The day's warnings for the alerts feed: buildings that cannot work and why (no workers, an input
// that is not coming, nowhere to put the output, a market with nothing to sell), and the city's people
// problems.  A condition that lasts stays one alert with a day count; each kind names at most a few
// buildings a day, so a big city's list keeps room for fires and raids.
void World::alerts_daily() {
  constexpr int kPerKind = 6;
  int named[static_cast<int>(AlertKind::Count)] = {};
  const auto alert = [&](AlertKind k, BuildingId id, u16 arg) {
    if (named[static_cast<int>(k)]++ < kPerKind) add_alert(k, id, arg);
  };
  buildings.for_each([&](BuildingId id, const Building& b) {
    if (b.state != BState::Active || (b.flags & kBuildingCutOff)) return;  // cut off: the road alert says why
    const auto& d = defs->b(b.def);
    if (d.kind == BuildingKind::House) return;
    if (b.jobs > 0 && b.employed == 0) {
      alert(AlertKind::NoWorkers, id, 0);
      return;
    }
    if (b.inv == econ::kNoInv) return;
    if (d.recipe.valid()) {
      const data::RecipeDef& r = defs->recipes[d.recipe.v];
      if (b.work_progress < r.work) return;  // working, or not yet due
      for (const auto& in : r.inputs)
        if (econ.amt(b.inv, in.good) < in.amount && econ.rin(b.inv, in.good) == 0) {
          alert(AlertKind::InputsMissing, id, in.good.v);
          return;
        }
      bool collecting = false;
      for (const auto& o : r.outputs) collecting |= econ.rout(b.inv, o.good) > 0;
      if (!collecting && !r.outputs.empty())  // inputs are here, so the output has no room
        alert(AlertKind::StorageFull, id, static_cast<u16>(r.outputs[0].good.v + 1));  // arg: output good + 1
      return;
    }
    if (d.kind == BuildingKind::Market && econ.total[b.inv] == 0) {
      bool coming = false;
      for (u32 g = 0; g < econ.goods && !coming; ++g) coming = econ.rin(b.inv, GoodId{static_cast<u16>(g)}) > 0;
      if (!coming) alert(AlertKind::MarketEmpty, id, 0);
    }
    if (d.kind == BuildingKind::Storage && econ.capacity[b.inv] > 0 && econ.total[b.inv] >= econ.capacity[b.inv])
      alert(AlertKind::StorageFull, id, 0);
  });
  u32 adults = 0, unemployed = 0, homeless = 0;
  for (u32 i = 0; i < citizens.ids.high_water(); ++i) {
    if (!citizens.ids.alive_index(i) || (citizens.flags[i] & kFlagVisitor)) continue;
    if (!citizens.home[i].valid()) ++homeless;
    const u32 age = citizens.age_years(i, now);
    if (age < 14 || age >= 60) continue;
    ++adults;
    if (!citizens.work[i].valid()) ++unemployed;
  }
  if (adults >= 20 && unemployed * 100 >= adults * 15)
    add_alert(AlertKind::Unemployment, BuildingId{}, static_cast<u16>(unemployed * 100 / adults));
  if (homeless >= 5) add_alert(AlertKind::Homeless, BuildingId{}, static_cast<u16>(std::min<u32>(homeless, 0xFFFF)));
  if (emigrants_today > 0)
    add_alert(AlertKind::Emigrants, BuildingId{}, static_cast<u16>(std::min<u32>(emigrants_today, 0xFFFF)));
}

}  // namespace urbs::sim
