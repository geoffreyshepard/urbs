#include "sim/world.hpp"

#include <TaskScheduler.h>

#include <algorithm>
#include <chrono>

#include "core/alloc_track.hpp"
#include "core/log.hpp"
#include "core/profile.hpp"

namespace urbs::sim {

World::World() = default;
World::~World() {
  if (jobs_) jobs_->WaitforAllAndShutdown();
}

void World::init(const WorldConfig& c, const data::Defs& d) {
  cfg = c;
  defs = &d;
  for (u64 s = 0; s < static_cast<u64>(rng::StreamId::Count); ++s) rng[s].seed(cfg.seed, s);
  jobs_ = std::make_unique<enki::TaskScheduler>();
  enki::TaskSchedulerConfig tc;
  if (cfg.threads > 0) tc.numTaskThreadsToCreate = cfg.threads - 1;
  jobs_->Initialize(tc);

  nav.init(map);
  paths.init(nav, jobs_.get(), std::max<u32>(cfg.max_citizens, 1024), cfg.path_budget);
  sched.init(cfg.max_citizens * 3 + cfg.max_buildings * 4 + 1024);
  sched.set_now(now);
  services.init(map.w, map.h, static_cast<u32>(d.services.size()), cfg.max_buildings);
  buildings.init(cfg.max_buildings);
  citizens.init(cfg.max_citizens);
  households.init(cfg.max_households);
  due_.init(sched.capacity());
  results_.init(std::max<u32>(cfg.path_budget, cfg.max_citizens) + 16);
  scratch_ids_.reserve(cfg.max_buildings);
  scratch_tiles_.reserve(cfg.max_buildings);
  scratch_u32_.reserve(std::max(cfg.max_citizens, cfg.max_buildings));
  scratch_u32b_.reserve(std::max(cfg.max_citizens, cfg.max_buildings));
  water_queue_.reserve(static_cast<size_t>(map.w * map.h));
  reservoir_full_.assign(cfg.max_buildings, 0);
  desir_acc_.assign(static_cast<size_t>(map.w * map.h), 0);
  water_dirty = desir_dirty = true;
  labor_head_.reserve(static_cast<size_t>(map.chunks_x() * map.chunks_y()) + 2);
  labor_fill_.reserve(static_cast<size_t>(map.chunks_x() * map.chunks_y()) + 2);
  labor_done_.reserve(static_cast<size_t>(cfg.max_buildings) + 1);
  econ.init(static_cast<u32>(d.goods.size()), cfg.max_buildings, 4096);
  routes.assign(d.partners.size(), TradeRouteState{});
  for (auto& r : routes) {
    r.bought.assign(d.goods.size(), 0);
    r.sold.assign(d.goods.size(), 0);
  }
  army.init(cfg.max_soldiers, map.w, map.h);
  field_heap_.init(static_cast<u32>(map.w * map.h));
  req_physician_ = data::HousingReq{data::HousingReq::Service, {d.service("physician"), d.service("hospital")}, {}, 0};
  req_fountain_ = data::HousingReq{data::HousingReq::Service, {d.service("fountain")}, {}, 0};
  requests.assign(8, Request{});
  tech_known.assign(d.techniques.size(), 0);
  unlock_up_to(16);
  messages.reserve(80);
  god_mood.assign(d.gods.size(), 50);
  god_available.assign(d.gods.size(), 0);
  god_offerings.assign(d.gods.size(), 0);
  for (const char* g : {"jupiter", "juno", "mars", "venus", "ceres", "vesta"}) {
    const u8 gi = d.god(g);
    if (gi != 0xFF) god_available[gi] = 1;
  }
  trade_mode.assign(d.goods.size(), 0);
  trade_level.assign(d.goods.size(), 50);
  for (auto& v : need_services) v.clear();
  for (u16 sv = 0; sv < d.services.size(); ++sv)
    if (d.services[sv].need != data::NeedKind::None) need_services[static_cast<int>(d.services[sv].need)].push_back(ServiceId{sv});
  alerts.reserve(160);
  food_slots.clear();
  hh_goods.clear();
  good_food_slot.assign(d.goods.size(), -1);
  good_hh_slot.assign(d.goods.size(), -1);
  for (u16 g = 0; g < d.goods.size(); ++g)
    if (d.goods[g].food && food_slots.size() < 8) {
      good_food_slot[g] = static_cast<i8>(food_slots.size());
      food_slots.push_back(GoodId{g});
    }
  for (const char* n : {"pottery", "clothing", "olive_oil", "wine", "furniture", "glassware", "dyed_clothing", "jewelry"}) {
    GoodId g = d.good(n);
    if (g.valid() && hh_goods.size() < 8) {
      good_hh_slot[g.v] = static_cast<i8>(hh_goods.size());
      hh_goods.push_back(g);
    }
  }
  commands.reserve(1024);
  providers_dirty_ = true;
}

// ------------------------------------------------------------------------------------------------
// Structure
// ------------------------------------------------------------------------------------------------

bool World::can_place(BuildingDefId def, i32 x, i32 y, i32 w, i32 h) const {
  if (x < 0 || y < 0 || x + w > map.w || y + h > map.h) return false;
  const data::BuildingDef& bd = defs->b(def);
  bool has_resource = bd.resource == 0;
  bool near_water = !bd.needs_water_adjacent;
  for (i32 ty = y; ty < y + h; ++ty)
    for (i32 tx = x; tx < x + w; ++tx) {
      size_t i = static_cast<size_t>(map.idx(tx, ty));
      if (map.building[i].valid()) return false;
      if (terrain_water(map.terrain[i]) || map.road[i] != Road::None) return false;
      if (static_cast<u8>(map.resource[i]) == bd.resource) has_resource = true;
    }
  if (!near_water || !has_resource)
    for (i32 ty = y - 1; ty <= y + h; ++ty)
      for (i32 tx = x - 1; tx <= x + w; ++tx) {
        if (!map.in_bounds(tx, ty)) continue;
        const size_t ri = static_cast<size_t>(map.idx(tx, ty));
        if (terrain_water(map.terrain[ri])) near_water = true;
        // Waterside buildings (fishing wharves) use resources in the water next to them.
        if (bd.needs_water_adjacent && static_cast<u8>(map.resource[ri]) == bd.resource) has_resource = true;
      }
  return has_resource && near_water;
}

void World::update_nav_rect(i32 x, i32 y, i32 w, i32 h) {
  nav_changed_tick_ = now;
  for (i32 ty = y; ty < y + h; ++ty)
    for (i32 tx = x; tx < x + w; ++tx)
      if (map.in_bounds(tx, ty)) {
        nav.update_tile(map, tx, ty, now);
        map.mark_dirty(tx, ty);
      }
}

void World::compute_entrance(Building& b) {
  // Perimeter scan: bottom side first (buildings face south), then sides, then top. Prefer tiles
  // connected to the rest of the map (not a pocket cut off by water), then roads.
  i32 best = -1;
  int best_score = -1;
  auto consider = [&](i32 tx, i32 ty, int side_pref) {
    if (!map.in_bounds(tx, ty)) return;
    i32 i = map.idx(tx, ty);
    if (nav.cost[static_cast<size_t>(i)] == 0) return;
    int score = side_pref + (map.road[static_cast<size_t>(i)] != Road::None ? 100 : 0);
    if (entry_tile >= 0 && nav.connected(i, entry_tile)) score += 1000;
    if (score > best_score) {
      best_score = score;
      best = i;
    }
  };
  for (i32 tx = b.x; tx < b.x + b.w; ++tx) consider(tx, b.y + b.h, 30 - std::abs(tx - (b.x + b.w / 2)));
  for (i32 ty = b.y; ty < b.y + b.h; ++ty) {
    consider(b.x - 1, ty, 10);
    consider(b.x + b.w, ty, 10);
  }
  for (i32 tx = b.x; tx < b.x + b.w; ++tx) consider(tx, b.y - 1, 0);
  b.entrance = best;
}

namespace {
const data::HousingTierDef* tier0(const data::Defs& d, const data::BuildingDef& bd) { return d.tier(bd.house_line, 0); }
}  // namespace

BuildingId World::place_building(BuildingDefId def, i32 x, i32 y, i32 w, i32 h, bool instant) {
  if (!can_place(def, x, y, w, h)) return BuildingId{};
  BuildingId id = buildings.ids.create();
  if (!id) return id;
  const data::BuildingDef& bd = defs->b(def);
  Building& b = buildings[id];
  b = Building{};
  b.def = def;
  b.x = static_cast<i16>(x);
  b.y = static_cast<i16>(y);
  b.w = static_cast<u8>(w);
  b.h = static_cast<u8>(h);
  const u32 area = static_cast<u32>(w * h);
  const u32 base_area = static_cast<u32>(bd.w * bd.h);
  b.jobs = bd.painted ? static_cast<u16>(std::max<u32>(1, bd.workers * area / std::max<u32>(1, base_area))) : bd.workers;
  b.capacity = bd.housing;
  if (bd.kind == data::BuildingKind::House)
    if (const auto* t = tier0(*defs, bd)) b.capacity = t->capacity;
  b.build_needed = static_cast<u32>(bd.build_work) * (bd.painted ? std::max<u32>(1, area / base_area) : 1);
  if (tech(Tech::RomanConcrete)) b.build_needed = b.build_needed * 3 / 4;
  for (i32 ty = y; ty < y + h; ++ty)
    for (i32 tx = x; tx < x + w; ++tx) {
      const size_t ti = static_cast<size_t>(map.idx(tx, ty));
      map.building[ti] = id;
      if (bd.passable) map.water[ti] |= 0x80;
    }
  update_nav_rect(x, y, w, h);
  compute_entrance(b);
  water_dirty = desir_dirty = true;
  b.state = BState::Construction;
  ++b.token;
  // Inventory: storage capacity, producer buffer, or construction materials.
  u32 cost_units = 0;
  for (const auto& c : bd.cost_goods) cost_units += c.amount;
  u32 cap = bd.storage ? static_cast<u32>(bd.storage) * (bd.painted ? std::max<u32>(1, area / base_area) : 1) : 0;
  if (bd.recipe.valid() && cap == 0) cap = 30;
  if (bd.kind == data::BuildingKind::Market && cap == 0) cap = 200;
  cap = std::max(cap, cost_units);
  if (cap > 0) {
    b.inv = econ.create_inventory(id, static_cast<u16>(std::min<u32>(cap, 60000)));
    if (b.inv != econ::kNoInv) {
      if (!bd.accepts.empty())
        for (u32 g = 0; g < econ.goods; ++g) {
          bool acc = std::find(bd.accepts.begin(), bd.accepts.end(), GoodId{static_cast<u16>(g)}) != bd.accepts.end();
          econ.accept[static_cast<size_t>(b.inv) * econ.goods + g] = acc ? 1 : 0;
        }
      if (bd.kind == data::BuildingKind::Market) {
        for (GoodId g : food_slots) econ.target[static_cast<size_t>(b.inv) * econ.goods + g.v] = 40;
        for (GoodId g : hh_goods) econ.target[static_cast<size_t>(b.inv) * econ.goods + g.v] = 20;
      }
    }
  }
  if (instant || (b.build_needed == 0 && bd.cost_goods.empty())) {
    complete_construction(id);
  } else {
    b.jobs = bd.kind == data::BuildingKind::Monument ? static_cast<u16>(std::clamp<u32>(area / 2, 8, 40))
                                                     : static_cast<u16>(std::clamp<u32>(2 + area / 4, 2, 16));
  }
  sched.schedule(Event{now + 1 + id.index() % kTicksPerHour, id.index(), static_cast<u16>(BuildingEvent::Hourly), b.token,
                       EventKind::Building});
  return id;
}

void World::complete_construction(BuildingId id) {
  Building& b = buildings[id];
  const data::BuildingDef& bd = defs->b(b.def);
  // Builders are released; the finished building hires its own workforce.
  if (b.employed > 0)
    for (u32 i = 0; i < citizens.ids.high_water(); ++i)
      if (citizens.ids.alive_index(i) && citizens.work[i] == id) release_worker(i);
  b.state = BState::Active;
  b.build_progress = b.build_needed;
  b.employed = 0;
  b.present = 0;
  const u32 area = static_cast<u32>(b.w * b.h), base_area = static_cast<u32>(bd.w * bd.h);
  b.jobs = bd.painted ? static_cast<u16>(std::max<u32>(bd.workers ? 1u : 0u, bd.workers * area / std::max<u32>(1, base_area)))
                      : bd.workers;
  providers_dirty_ = true;
  water_dirty = desir_dirty = true;
  if (bd.kind == data::BuildingKind::Monument) dedicate(id);
}

void World::dedicate(BuildingId id) {
  const auto& ded = defs->b(buildings[id].def).dedication;
  change_favor(ded.favor);
  if (ded.mood != 0 && ded.days > 0) {
    const u16 until = static_cast<u16>(day_index(now) + ded.days);
    for (u32 i = 0; i < citizens.ids.high_water(); ++i)
      if (citizens.ids.alive_index(i)) {
        citizens.bonus[i] = ded.mood;
        citizens.bonus_until[i] = until;
      }
  }
  if (!ded.god.empty()) {
    const u8 g = defs->god(ded.god);
    if (g < god_mood.size()) god_mood[g] = static_cast<u8>(std::clamp(static_cast<int>(god_mood[g]) + ded.god_mood, 0, 100));
  }
  add_alert(AlertKind::Dedicated, id);
}

void World::remove_building(BuildingId id) {
  if (!buildings.alive(id)) return;
  Building& b = buildings[id];
  for (i32 ty = b.y; ty < b.y + b.h; ++ty)
    for (i32 tx = b.x; tx < b.x + b.w; ++tx) {
      const size_t ti = static_cast<size_t>(map.idx(tx, ty));
      map.building[ti] = BuildingId{};
      map.water[ti] &= static_cast<u8>(~0x80);
    }
  water_dirty = desir_dirty = true;
  update_nav_rect(b.x, b.y, b.w, b.h);
  // Evict residents and workers.
  for (u32 i = 0; i < citizens.ids.high_water(); ++i) {
    if (!citizens.ids.alive_index(i)) continue;
    if (citizens.work[i] == id) release_worker(i);
    if (citizens.home[i] == id) citizens.home[i] = BuildingId{};
    if (citizens.loc[i] == id) {
      citizens.loc[i] = BuildingId{};
      citizens.dest_tile[i] = b.entrance;
      ++citizens.token[i];
      schedule_citizen(i, now + 1, CitizenEvent::Decide);
    }
    if (citizens.dest[i] == id && citizens.activity[i] == Activity::Walk) citizens.dest[i] = BuildingId{};
  }
  for (u32 t = 0; t < econ.task_ids.high_water(); ++t)
    if (econ.task_ids.alive_index(t) && (econ.tasks[t].src == id || econ.tasks[t].dst == id)) cancel_task(t, true);
  econ.destroy_inventory(b.inv);
  b.inv = econ::kNoInv;
  ++b.token;
  for (u32 hi = b.first_household; hi != kNoIndex;) {
    Household& hh = households.h[hi];
    u32 next = hh.next_in_house;
    hh.home = BuildingId{};
    hh.next_in_house = kNoIndex;
    hi = next;
  }
  buildings.ids.destroy(id);
  providers_dirty_ = true;
}

void World::place_road(i32 x, i32 y, Road type) {
  if (!map.in_bounds(x, y)) return;
  size_t i = static_cast<size_t>(map.idx(x, y));
  if (map.building[i].valid()) return;
  if (terrain_water(map.terrain[i]) && type != Road::Bridge) return;
  if (map.road[i] == type) return;
  map.road[i] = type;
  nav_changed_tick_ = now;
  nav.update_tile(map, x, y, now);
  map.mark_dirty(x, y);
}

void World::place_road_line(i32 x0, i32 y0, i32 x1, i32 y1, Road type) {
  // 8-directional line: diagonal first, then straight.
  i32 x = x0, y = y0;
  place_road(x, y, type);
  while (x != x1 || y != y1) {
    i32 dx = (x1 > x) - (x1 < x), dy = (y1 > y) - (y1 < y);
    if (std::abs(x1 - x) != 0 && std::abs(y1 - y) != 0) {
      x += dx;
      y += dy;
    } else {
      x += dx;
      y += dy;
    }
    place_road(x, y, type);
  }
}

void World::demolish(i32 x0, i32 y0, i32 x1, i32 y1) {
  if (x0 > x1) std::swap(x0, x1);
  if (y0 > y1) std::swap(y0, y1);
  for (i32 y = y0; y <= y1; ++y)
    for (i32 x = x0; x <= x1; ++x) {
      if (!map.in_bounds(x, y)) continue;
      size_t i = static_cast<size_t>(map.idx(x, y));
      if (map.building[i].valid()) remove_building(map.building[i]);
      if (map.road[i] != Road::None) {
        map.road[i] = Road::None;
        nav.update_tile(map, x, y, now);
        map.mark_dirty(x, y);
      }
      if (map.under[i]) {
        map.under[i] = 0;
        water_dirty = true;
      }
    }
}

// ------------------------------------------------------------------------------------------------
// Citizens
// ------------------------------------------------------------------------------------------------

HouseholdId World::create_household(BuildingId home, CitizenClass cls) {
  HouseholdId id = households.ids.create();
  if (!id) return id;
  Household& h = households[id];
  h = Household{};
  h.home = home;
  h.cls = cls;
  h.nomen = static_cast<u16>(stream(rng::StreamId::Names).below(static_cast<u32>(std::max<size_t>(1, defs->names.nomina.size()))));
  if (home.valid()) {
    Building& b = buildings[home];
    h.next_in_house = b.first_household;
    b.first_household = id.index();
  }
  return id;
}

CitizenId World::spawn_citizen(CitizenClass cls, BuildingId at, HouseholdId hh, u32 age_years, i32 at_tile) {
  CitizenId id = citizens.ids.create();
  if (!id) return id;
  const u32 i = id.index();
  auto& C = citizens;
  ++C.token[i];
  C.activity[i] = Activity::Home;
  C.next[i] = Activity::None;
  C.loc[i] = at_tile >= 0 ? BuildingId{} : at;
  C.dest[i] = BuildingId{};
  C.dest_tile[i] = at_tile >= 0 ? at_tile : (at.valid() ? buildings[at].entrance : -1);
  C.reason[i] = Reason::None;
  C.path[i] = path::kNoHandle;
  C.home[i] = at;
  C.work[i] = BuildingId{};
  C.household[i] = hh;
  C.cls[i] = cls;
  rng::Stream& r = stream(rng::StreamId::Citizens);
  C.sex[i] = static_cast<u8>(r.below(2));
  {
    rng::Stream& nr = stream(rng::StreamId::Names);
    const auto& N = defs->names;
    C.name0[i] = static_cast<u16>(nr.below(static_cast<u32>(cls == CitizenClass::Slave ? N.slave.size() : N.praenomina.size())));
    C.name1[i] = hh.valid() ? households[hh].nomen : static_cast<u16>(nr.below(static_cast<u32>(N.nomina.size())));
    C.name2[i] = static_cast<u16>(nr.below(static_cast<u32>(N.cognomina.size())));
  }
  C.born[i] = static_cast<i64>(now) - static_cast<i64>(age_years) * static_cast<i64>(kTicksPerYear) -
              static_cast<i64>(r.below(static_cast<u32>(kTicksPerYear)));
  C.need_t0[i] = now;
  for (int n = 0; n < kNeedCount; ++n) {
    C.need_base[static_cast<size_t>(i) * kNeedCount + n] = static_cast<u8>(160 + r.below(80));
    C.need_rate[static_cast<size_t>(i) * kNeedCount + n] = -4;
  }
  C.happiness[i] = 140;
  C.health[i] = 255;
  C.carrying[i] = 0;
  C.carry_amount[i] = 0;
  C.flags[i] = 0;  // slots are reused: never inherit a previous occupant's flags (visitor, schooled, ...)
  C.task[i] = kNoIndex;
  C.memory_head[i] = 0;
  for (int k = 0; k < kMemoryEntries; ++k) C.memory[static_cast<size_t>(i) * kMemoryEntries + k] = MemoryEntry{};
  C.spouse[i] = C.mother[i] = C.father[i] = CitizenId{};
  C.bonus[i] = 0;
  C.bonus_until[i] = 0;
  C.goal[i] = 0;
  assign_traits(i, kNoIndex, kNoIndex);
  if (age_years >= 16) assign_goal(i);
  if (hh.valid()) {
    Household& h = households[hh];
    if (!h.head.valid()) h.head = id;
    ++h.members;
  }
  if (at.valid()) ++buildings[at].residents;
  schedule_citizen(i, now + 1 + (at_tile >= 0 ? 0 : r.below(static_cast<u32>(kTicksPerHour))), CitizenEvent::Decide);
  return id;
}

void World::despawn_citizen(CitizenId id) {
  if (!citizens.alive(id)) return;
  const u32 i = id.index();
  auto& C = citizens;
  if (C.path[i] != path::kNoHandle) {
    paths.store().unref(C.path[i]);
    C.path[i] = path::kNoHandle;
    if (walking > 0) --walking;
  }
  if (C.task[i] != kNoIndex) cancel_task(C.task[i], true);
  if ((C.activity[i] == Activity::Work || C.activity[i] == Activity::Build) && C.loc[i].valid() &&
      buildings.alive(C.loc[i]) && buildings[C.loc[i]].present > 0)
    --buildings[C.loc[i]].present;
  if (C.work[i].valid() && buildings.alive(C.work[i]) && buildings[C.work[i]].employed > 0)
    --buildings[C.work[i]].employed;
  if (C.home[i].valid() && buildings.alive(C.home[i]) && buildings[C.home[i]].residents > 0)
    --buildings[C.home[i]].residents;
  if (C.household[i].valid() && households.ids.alive(C.household[i])) {
    Household& h = households[C.household[i]];
    if (h.members > 0) --h.members;
    if (h.head == id) h.head = CitizenId{};
    if (h.shopper == id) h.shopper = CitizenId{};
    if (h.fetcher == id) h.fetcher = CitizenId{};
  }
  ++C.token[i];
  C.activity[i] = Activity::Dead;
  citizens.ids.destroy(id);
}

i32 World::jitter(u32 i) const {
  // Stable per-citizen offset in [-24, +24] ticks (about +-30 game minutes) to flatten dawn/shift spikes.
  return static_cast<i32>(rng::hash64(static_cast<u64>(i) * 0x9E37u + cfg.seed) % 49) - 24;
}

i32 World::current_tile(u32 i) const {
  BuildingId l = citizens.loc[i];
  if (l.valid() && buildings.alive(l)) return buildings[l].entrance;
  return citizens.dest_tile[i];
}

void World::schedule_citizen(u32 i, Tick when, CitizenEvent ev) {
  sched.schedule(Event{when, i, static_cast<u16>(ev), citizens.token[i], EventKind::Citizen});
}

void World::request_walk(u32 i, i32 from, BuildingId to, i32 to_tile, Activity next) {
  auto& C = citizens;
  ++C.token[i];
  C.next[i] = next;
  C.dest[i] = to;
  C.dest_tile[i] = to_tile;
  if (from < 0 || to_tile < 0) {
    C.activity[i] = Activity::Home;
    schedule_citizen(i, now + kTicksPerHour, CitizenEvent::Decide);
    return;
  }
  if (from == to_tile) {
    if (next == Activity::Leave) {
      finish_leave(i);
      return;
    }
    C.loc[i] = to;
    start_activity(i, next);
    return;
  }
  C.activity[i] = Activity::Walk;
  paths.request(path::PathRequest{i, C.token[i], 0, from, to_tile});
  ++stats.path_requests;
}

bool World::citizen_position(u32 i, f32 alpha, Vec2& out, int* facing) const {
  const auto& C = citizens;
  if (C.activity[i] != Activity::Walk || C.path[i] == path::kNoHandle) return false;
  const f32 el = (static_cast<f32>(now - C.walk_start[i]) + alpha) * 10.0f;
  const u32 e = el < 0 ? 0 : static_cast<u32>(el);
  Vec2 t = paths.store().position(C.path[i], C.path_rev[i] != 0, e);
  out = {t.x * static_cast<f32>(kTilePx), t.y * static_cast<f32>(kTilePx)};
  if (facing) *facing = paths.store().facing(C.path[i], C.path_rev[i] != 0, e);
  return true;
}

void World::on_citizen_event(const Event& e) {
  const u32 i = e.target;
  if (!citizens.ids.alive_index(i) || citizens.token[i] != e.token) return;  // stale
  switch (static_cast<CitizenEvent>(e.type)) {
    case CitizenEvent::Decide: decide(i); break;
    case CitizenEvent::Arrive: arrive(i); break;
  }
}

void World::apply_path_results() {
  auto& C = citizens;
  for (const path::PathResult& r : results_) {
    const u32 i = r.requester;
    const bool live = citizens.ids.alive_index(i) && C.token[i] == r.token && C.activity[i] == Activity::Walk &&
                      C.path[i] == path::kNoHandle;
    if (!live) {
      if (r.ok) paths.store().unref(r.handle);
      continue;
    }
    ++stats.path_results;
    if (!r.ok) {
      // Unreachable: stay put and try again later.
      C.activity[i] = Activity::Home;
      ++C.token[i];
      if (C.household[i].valid() && households.ids.alive(C.household[i])) {
        Household& h = households[C.household[i]];
        if (C.next[i] == Activity::Shop && h.shopper == C.ids.id_at(i)) h.shopper = CitizenId{};
        if (C.next[i] == Activity::Water && h.fetcher == C.ids.id_at(i)) h.fetcher = CitizenId{};
      }
      schedule_citizen(i, now + kTicksPerHour, CitizenEvent::Decide);
      continue;
    }
    C.path[i] = r.handle;
    C.path_rev[i] = r.reversed ? 1 : 0;
    C.walk_start[i] = now;
    C.loc[i] = BuildingId{};
    ++walking;
    schedule_citizen(i, now + std::max<u32>(1, paths.store().total_ticks(r.handle)), CitizenEvent::Arrive);
  }
  results_.clear();
}

// ------------------------------------------------------------------------------------------------
// Tick
// ------------------------------------------------------------------------------------------------

void World::refresh_service_providers() {
  for (u32 s = 0; s < services.services(); ++s) {
    scratch_ids_.clear();
    scratch_tiles_.clear();
    buildings.for_each([&](BuildingId id, const Building& b) {
      if (b.state != BState::Active || b.entrance < 0) return;
      if (defs->b(b.def).service.v != s) return;
      if (defs->b(b.def).needs_pipe && !(b.flags & 0x20)) return;  // dry fountain / baths
      scratch_ids_.push_back(id);
      scratch_tiles_.push_back(b.entrance);
    });
    services.set_providers(s, scratch_ids_, scratch_tiles_);
  }
  providers_dirty_ = false;
}

void World::apply_commands() {
  for (const Command& c : commands) {
    switch (c.type) {
      case CmdType::PlaceBuilding: {
        const BuildingDefId d{c.def};
        if (!is_unlocked(d) || !can_place(d, c.x0, c.y0, c.x1, c.y1)) break;
        const auto& bd = defs->b(d);
        const i64 area = static_cast<i64>(c.x1) * c.y1, base = static_cast<i64>(bd.w) * bd.h;
        i64 cost = bd.painted ? std::max<i64>(1, bd.cost_denarii * area / std::max<i64>(1, base)) : bd.cost_denarii;
        if (tech(Tech::Arches) && (bd.id == "aqueduct" || bd.id == "grand_aqueduct")) cost /= 2;
        if (!pay(cost, econ::Money::Construction)) break;
        BuildingId nb = place_building(d, c.x0, c.y0, c.x1, c.y1, (c.flags & 1) != 0);
        // Temples are dedicated to a god (Command::value); default: the first worshipped god.
        if (nb.valid() && bd.service.valid() && defs->services[bd.service.v].need == data::NeedKind::Religion &&
            bd.id != "shrine") {
          u8 g = static_cast<u8>(c.value);
          if (g >= god_available.size() || !god_available[g])
            for (g = 0; g < god_available.size() && !god_available[g]; ++g) {
            }
          buildings[nb].extra = g;
        }
        break;
      }
      case CmdType::PlaceRoad: {
        const i64 len = std::max(std::abs(c.x1 - c.x0), std::abs(c.y1 - c.y0)) + 1;
        if (!pay(len * (static_cast<Road>(c.def) == Road::Paved ? 4 : 1), econ::Money::Construction)) break;
        place_road_line(c.x0, c.y0, c.x1, c.y1, static_cast<Road>(c.def));
        break;
      }
      case CmdType::Demolish: demolish(c.x0, c.y0, c.x1, c.y1); break;
      case CmdType::PlaceUnder: {
        const i64 len = std::abs(c.x1 - c.x0) + std::abs(c.y1 - c.y0) + 1;
        if (c.value && !pay(len * (c.def == 2 ? 6 : 2), econ::Money::Construction)) break;
        place_underground_line(c.x0, c.y0, c.x1, c.y1, static_cast<u8>(c.def == 2 ? 2 : 1), c.value != 0);
        break;
      }
      case CmdType::BuySlaves: buy_public_slaves(static_cast<u32>(std::clamp(c.value, 0, 50))); break;
      case CmdType::Festival: hold_festival(static_cast<u8>(c.def), c.value); break;
      case CmdType::Trigger: trigger(static_cast<TriggerId>(c.def), c.x0, c.y0, c.value); break;
      case CmdType::RaiseUnit: raise_unit(UnitDefId{c.def}, BuildingId{static_cast<u32>(c.value)}); break;
      case CmdType::UnitOrder:
        order_unit(c.def, static_cast<military::Order>(c.value), map.in_bounds(c.x0, c.y0) ? map.idx(c.x0, c.y0) : -1);
        break;
      case CmdType::UnitFormation:
        if (c.def < military::kMaxUnits) army.units[c.def].formation = static_cast<military::Formation>(c.value % 4);
        break;
      case CmdType::UnitStance:
        if (c.def < military::kMaxUnits) army.units[c.def].stance = static_cast<military::Stance>(c.value % 2);
        break;
      case CmdType::DisbandUnit: disband_unit(c.def); break;
      case CmdType::Campaign:
        if (c.def < military::kMaxUnits && army.units[c.def].alive && army.units[c.def].side == 0) {
          army.units[c.def].campaign_level = static_cast<u8>(std::clamp(c.value, 1, 3));
          order_unit(c.def, military::Order::Campaign, -1);
        }
        break;
      case CmdType::FulfillRequest: fulfill_request(c.def); break;
      case CmdType::Research: research(c.def); break;
      case CmdType::Grant: earn(c.value, econ::Money::Tribute); break;
      case CmdType::TakeLoan: take_loan(c.value); break;
      case CmdType::Answer: answer(c.def, c.value); break;
      case CmdType::OpenRoute:
        if (c.def < routes.size() && !routes[c.def].open && routes[c.def].available &&
            pay(defs->partners[c.def].open_cost, econ::Money::Other)) {
          routes[c.def].open = 1;
          routes[c.def].next_visit = now + kTicksPerHour * 4;
        }
        break;
      case CmdType::SetTrade:
        if (c.def < trade_mode.size()) {
          trade_mode[c.def] = static_cast<u8>(std::clamp(c.value, 0, 3));
          trade_level[c.def] = static_cast<u16>(std::max(0, c.x0));
        }
        break;
      case CmdType::SetPolicy: {
        const i32 v = c.value;
        switch (static_cast<PolicyId>(c.def)) {
          case PolicyId::Wage: policy.wage = static_cast<u16>(std::clamp(v, 0, 100)); break;
          case PolicyId::TaxPatrician: policy.tax_rate[0] = static_cast<u8>(std::clamp(v, 0, 50)); break;
          case PolicyId::TaxEques: policy.tax_rate[1] = static_cast<u8>(std::clamp(v, 0, 50)); break;
          case PolicyId::TaxPleb: policy.tax_rate[2] = static_cast<u8>(std::clamp(v, 0, 50)); break;
          case PolicyId::TaxLibertus: policy.tax_rate[3] = static_cast<u8>(std::clamp(v, 0, 50)); break;
          case PolicyId::Rations: policy.rations = static_cast<u8>(std::clamp(v, 0, 2)); break;
          case PolicyId::SlaveTreatment: policy.slave_treatment = static_cast<u8>(std::clamp(v, 0, 2)); break;
          case PolicyId::Manumission: policy.manumission = static_cast<u8>(std::clamp(v, 0, 2)); break;
          case PolicyId::Curfew: policy.curfew = static_cast<u8>(v != 0); break;
          case PolicyId::Conscription: policy.conscription = static_cast<u8>(std::clamp(v, 0, 2)); break;
          case PolicyId::MaxInsula: policy.max_insula_tier = static_cast<u8>(std::clamp(v, 2, 4)); break;
          case PolicyId::Immigration: policy.immigration_open = static_cast<u8>(v != 0); break;
          default: break;
        }
        break;
      }
      default: break;
    }
  }
  commands.clear();
}

void World::process_events() {
  due_.clear();
  sched.pop_due(now, cfg.decision_budget, due_);
  stats.events = static_cast<u32>(due_.size());
  for (const Event& e : due_) {
    switch (e.kind) {
      case EventKind::Citizen: on_citizen_event(e); break;
      case EventKind::Building: on_building_event(e); break;
      default: break;
    }
  }
}

void World::tick() {
  URBS_ZONE("tick");
  using clock = std::chrono::steady_clock;
  const auto t0 = clock::now();
  auto lap = [&, last = t0](int k) mutable {
    const auto t = clock::now();
    stats.sys_ms[k] = std::chrono::duration<f64, std::milli>(t - last).count();
    last = t;
  };
  core::alloc::TickScope no_alloc;
  stats = TickStats{};
  ++now;
  sched.set_now(now);
  apply_commands();
  if (nav.regions_dirty) nav.rebuild_regions();
  if (water_dirty) recompute_water();
  if (desir_dirty) recompute_desirability();
  if (providers_dirty_) refresh_service_providers();
  if (nav.regions_dirty || std::any_of(nav.chunk_dirty.begin(), nav.chunk_dirty.end(), [](u8 d) { return d != 0; }))
    services.mark_all_dirty();
  lap(0);
  services.step(nav, cfg.service_budget);
  lap(1);
  if (now % kTicksPerDay == 0) {
    daily_update();
    weather_daily();
    inundation_daily();
    religion_daily();
    riot_check();
    revolt_check(false);
    military_daily();
    governance_daily();
    crimes_yday = crimes_today;
    crimes_today = births_today = deaths_today = marriages_today = freed_today = 0;
    avg_happiness = happy_count_today ? static_cast<u8>(happy_sum_today / static_cast<i32>(happy_count_today)) : avg_happiness;
    happy_sum_today = 0;
    happy_count_today = 0;
    immigrants_today = emigrants_today = 0;
  }
  if (now % kTicksPerHour == 20) immigration_hourly();
  if (now % kTicksPerHour == 33) trade_daily();
  {
    // Staggered daily passes: each citizen and each building once per game day.
    const u32 tod = tick_of_day(now);
    for (u32 i = tod; i < citizens.ids.high_water(); i += static_cast<u32>(kTicksPerDay))
      if (citizens.ids.alive_index(i)) citizen_daily(i);
    for (u32 bi = tod; bi < buildings.ids.high_water(); bi += static_cast<u32>(kTicksPerDay))
      if (buildings.ids.alive_index(bi)) house_daily(buildings.ids.id_at(bi));
  }
  lap(2);
  if (now % kTicksPerHour == 7) {
    labor_allocate();
    count_carters();
  }
  lap(3);
  if (econ.goods > 0) {
    // Two goods per tick: every good is matched about every 20 ticks.
    match_good(GoodId{static_cast<u16>((now * 2) % econ.goods)});
    match_good(GoodId{static_cast<u16>((now * 2 + 1) % econ.goods)});
  }
  lap(4);
  process_events();
  military_tick();
  lap(5);
  paths.process(now, results_);
  apply_path_results();
  lap(6);
  stats.walking = walking;
  stats.tick_ms = std::chrono::duration<f64, std::milli>(clock::now() - t0).count();
}

}  // namespace urbs::sim
