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
  if (!near_water)
    for (i32 ty = y - 1; ty <= y + h && !near_water; ++ty)
      for (i32 tx = x - 1; tx <= x + w; ++tx)
        if (map.in_bounds(tx, ty) && terrain_water(map.terrain[static_cast<size_t>(map.idx(tx, ty))])) {
          near_water = true;
          break;
        }
  return has_resource && near_water;
}

void World::update_nav_rect(i32 x, i32 y, i32 w, i32 h) {
  for (i32 ty = y; ty < y + h; ++ty)
    for (i32 tx = x; tx < x + w; ++tx)
      if (map.in_bounds(tx, ty)) {
        nav.update_tile(map, tx, ty, now);
        map.mark_dirty(tx, ty);
      }
}

void World::compute_entrance(Building& b) {
  // Perimeter scan: bottom side first (buildings face south), then sides, then top. Prefer roads.
  i32 best = -1;
  int best_score = -1;
  auto consider = [&](i32 tx, i32 ty, int side_pref) {
    if (!map.in_bounds(tx, ty)) return;
    i32 i = map.idx(tx, ty);
    if (nav.cost[static_cast<size_t>(i)] == 0) return;
    int score = side_pref + (map.road[static_cast<size_t>(i)] != Road::None ? 100 : 0);
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
  b.build_needed = static_cast<u32>(bd.build_work) * (bd.painted ? std::max<u32>(1, area / base_area) : 1);
  for (i32 ty = y; ty < y + h; ++ty)
    for (i32 tx = x; tx < x + w; ++tx) map.building[static_cast<size_t>(map.idx(tx, ty))] = id;
  update_nav_rect(x, y, w, h);
  compute_entrance(b);
  b.state = BState::Construction;
  if (instant) complete_construction(id);
  return id;
}

void World::complete_construction(BuildingId id) {
  Building& b = buildings[id];
  b.state = BState::Active;
  b.build_progress = b.build_needed;
  providers_dirty_ = true;
}

void World::remove_building(BuildingId id) {
  if (!buildings.alive(id)) return;
  Building& b = buildings[id];
  for (i32 ty = b.y; ty < b.y + b.h; ++ty)
    for (i32 tx = b.x; tx < b.x + b.w; ++tx) map.building[static_cast<size_t>(map.idx(tx, ty))] = BuildingId{};
  update_nav_rect(b.x, b.y, b.w, b.h);
  // Evict residents and workers.
  for (u32 i = 0; i < citizens.ids.high_water(); ++i) {
    if (!citizens.ids.alive_index(i)) continue;
    if (citizens.work[i] == id) citizens.work[i] = BuildingId{};
    if (citizens.home[i] == id) citizens.home[i] = BuildingId{};
    if (citizens.loc[i] == id) {
      citizens.loc[i] = BuildingId{};
      citizens.dest_tile[i] = b.entrance;
      ++citizens.token[i];
      schedule_citizen(i, now + 1, CitizenEvent::Decide);
    }
    if (citizens.dest[i] == id && citizens.activity[i] == Activity::Walk) citizens.dest[i] = BuildingId{};
  }
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
  if (home.valid()) {
    Building& b = buildings[home];
    h.next_in_house = b.first_household;
    b.first_household = id.index();
  }
  return id;
}

CitizenId World::spawn_citizen(CitizenClass cls, BuildingId at, HouseholdId hh, u32 age_years) {
  CitizenId id = citizens.ids.create();
  if (!id) return id;
  const u32 i = id.index();
  auto& C = citizens;
  ++C.token[i];
  C.activity[i] = Activity::Home;
  C.next[i] = Activity::None;
  C.loc[i] = at;
  C.dest[i] = BuildingId{};
  C.dest_tile[i] = at.valid() ? buildings[at].entrance : -1;
  C.path[i] = path::kNoHandle;
  C.home[i] = at;
  C.work[i] = BuildingId{};
  C.household[i] = hh;
  C.cls[i] = cls;
  rng::Stream& r = stream(rng::StreamId::Citizens);
  C.sex[i] = static_cast<u8>(r.below(2));
  C.born[i] = now - static_cast<Tick>(age_years) * kTicksPerYear - r.below(static_cast<u32>(kTicksPerYear));
  C.need_t0[i] = now;
  for (int n = 0; n < kNeedCount; ++n) {
    C.need_base[static_cast<size_t>(i) * kNeedCount + n] = static_cast<u8>(160 + r.below(80));
    C.need_rate[static_cast<size_t>(i) * kNeedCount + n] = -4;
  }
  C.happiness[i] = 140;
  C.health[i] = 255;
  C.carrying[i] = 0;
  C.task[i] = kNoIndex;
  C.memory_head[i] = 0;
  for (int k = 0; k < kMemoryEntries; ++k) C.memory[static_cast<size_t>(i) * kMemoryEntries + k] = MemoryEntry{};
  C.spouse[i] = C.mother[i] = C.father[i] = CitizenId{};
  if (hh.valid()) {
    Household& h = households[hh];
    if (!h.head.valid()) h.head = id;
    ++h.members;
  }
  if (at.valid()) ++buildings[at].residents;
  schedule_citizen(i, now + 1 + r.below(static_cast<u32>(kTicksPerHour)), CitizenEvent::Decide);
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
  if (C.activity[i] == Activity::Work && C.work[i].valid() && buildings.alive(C.work[i]) &&
      buildings[C.work[i]].present > 0)
    --buildings[C.work[i]].present;
  if (C.work[i].valid() && buildings.alive(C.work[i]) && buildings[C.work[i]].employed > 0)
    --buildings[C.work[i]].employed;
  if (C.home[i].valid() && buildings.alive(C.home[i]) && buildings[C.home[i]].residents > 0)
    --buildings[C.home[i]].residents;
  if (C.household[i].valid() && households.ids.alive(C.household[i])) {
    Household& h = households[C.household[i]];
    if (h.members > 0) --h.members;
    if (h.head == id) h.head = CitizenId{};
    if (h.shopper == id) h.shopper = CitizenId{};
  }
  ++C.token[i];
  C.activity[i] = Activity::Dead;
  citizens.ids.destroy(id);
}

i32 World::jitter(u32 i) const {
  // Stable per-citizen offset in [-8, +8] ticks (about +-10 game minutes).
  return static_cast<i32>(rng::hash64(static_cast<u64>(i) * 0x9E37u + cfg.seed) % 17) - 8;
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

void World::start_activity(u32 i, Activity a) {
  auto& C = citizens;
  C.rebase_needs(i, now);
  C.activity[i] = a;
  C.next[i] = Activity::None;
  const size_t nb = static_cast<size_t>(i) * kNeedCount;
  auto rate = [&](Need n, i8 r) { C.need_rate[nb + static_cast<size_t>(n)] = r; };
  rate(Need::Food, -6);
  rate(Need::Water, -8);
  rate(Need::Rest, -9);
  rate(Need::Hygiene, -4);
  rate(Need::Religion, -2);
  rate(Need::Entertainment, -3);
  const u32 h = hour_of_day(now);
  Tick end = now + kTicksPerHour;
  const i32 j = jitter(i);
  auto at_hour = [&](u32 hour) { return static_cast<Tick>(static_cast<i64>(next_hour(now, hour)) + j); };
  switch (a) {
    case Activity::Sleep:
      rate(Need::Rest, 40);
      rate(Need::Food, -3);
      rate(Need::Water, -3);
      end = at_hour(5);
      break;
    case Activity::Work: {
      Building& w = buildings[C.work[i]];
      ++w.present;
      rate(Need::Rest, -12);
      rate(Need::Food, -7);
      end = h < 12 ? at_hour(12) : at_hour(17);
      break;
    }
    case Activity::Shop: {
      Household* hh = C.household[i].valid() ? &households[C.household[i]] : nullptr;
      if (hh) {
        hh->food = static_cast<u16>(std::min<u32>(hh->food + hh->members * 6u, 400u));
        hh->shopper = CitizenId{};
      }
      end = now + kTicksPerHour;
      break;
    }
    case Activity::Home: {
      Household* hh = C.household[i].valid() ? &households[C.household[i]] : nullptr;
      if (hh && hh->food > 0 && C.need(i, Need::Food, now) < 200) {
        --hh->food;
        C.set_need(i, Need::Food, 255);
      }
      C.set_need(i, Need::Water, 255);
      if (h < 5 || h >= 22) end = at_hour(5);
      else if (h < 7) end = at_hour(7);
      else if (h < 12) end = at_hour(12);
      else if (h < 13) end = at_hour(13);
      else if (h < 17) end = at_hour(17);
      else end = at_hour(22);
      break;
    }
    default: break;
  }
  if (end <= now) end = now + 1;
  C.activity_end[i] = end;
  schedule_citizen(i, end, CitizenEvent::Decide);
}

void World::decide(u32 i) {
  auto& C = citizens;
  ++stats.decisions;
  // Leave the current activity.
  if (C.activity[i] == Activity::Work && C.work[i].valid() && buildings.alive(C.work[i])) {
    Building& w = buildings[C.work[i]];
    if (w.present > 0) --w.present;
  }
  const u32 h = hour_of_day(now);
  const i32 here = current_tile(i);
  BuildingId home = C.home[i];
  if (!home.valid() || !buildings.alive(home)) {
    C.activity[i] = Activity::Wander;
    schedule_citizen(i, now + kTicksPerHour * 2, CitizenEvent::Decide);
    return;
  }
  BuildingId target = home;
  Activity act = Activity::Home;
  if (h >= 22 || h < 5) {
    act = Activity::Sleep;
  } else if (C.work[i].valid() && buildings.alive(C.work[i]) && h >= 7 && h < 17 && h != 12) {
    target = C.work[i];
    act = Activity::Work;
  } else if (C.household[i].valid() && h >= 13 && h < 20) {
    Household& hh = households[C.household[i]];
    if (hh.food < hh.members * 2 && !hh.shopper.valid()) {
      ServiceId market = defs->service("market");
      if (market.valid() && services.ready(market.v)) {
        BuildingId m = services.provider(market.v, buildings[home].entrance);
        if (m.valid() && buildings.alive(m)) {
          target = m;
          act = Activity::Shop;
          hh.shopper = CitizenId::make(i, C.ids.gens()[i]);
        }
      }
    }
  }
  const i32 to_tile = buildings[target].entrance;
  if (C.loc[i] == target) {
    start_activity(i, act);
    return;
  }
  request_walk(i, here, target, to_tile, act);
}

void World::arrive(u32 i) {
  auto& C = citizens;
  ++stats.arrivals;
  if (C.path[i] != path::kNoHandle) {
    paths.store().unref(C.path[i]);
    C.path[i] = path::kNoHandle;
    if (walking > 0) --walking;
  }
  C.loc[i] = C.dest[i];
  if (!C.loc[i].valid() || !buildings.alive(C.loc[i])) {
    C.loc[i] = BuildingId{};
    C.activity[i] = Activity::Wander;
    schedule_citizen(i, now + 1, CitizenEvent::Decide);
    return;
  }
  start_activity(i, C.next[i]);
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
      if (C.next[i] == Activity::Shop && C.household[i].valid()) households[C.household[i]].shopper = CitizenId{};
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
      case CmdType::PlaceBuilding:
        place_building(BuildingDefId{c.def}, c.x0, c.y0, c.x1, c.y1, (c.flags & 1) != 0);
        break;
      case CmdType::PlaceRoad: place_road_line(c.x0, c.y0, c.x1, c.y1, static_cast<Road>(c.def)); break;
      case CmdType::Demolish: demolish(c.x0, c.y0, c.x1, c.y1); break;
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
      default: break;
    }
  }
}

void World::tick() {
  URBS_ZONE("tick");
  const auto t0 = std::chrono::steady_clock::now();
  core::alloc::TickScope no_alloc;
  stats = TickStats{};
  ++now;
  sched.set_now(now);
  apply_commands();
  if (nav.regions_dirty) nav.rebuild_regions();
  if (providers_dirty_) refresh_service_providers();
  if (nav.regions_dirty || std::any_of(nav.chunk_dirty.begin(), nav.chunk_dirty.end(), [](u8 d) { return d != 0; }))
    services.mark_all_dirty();
  services.step(nav, cfg.service_budget);
  process_events();
  paths.process(now, results_);
  apply_path_results();
  stats.walking = walking;
  stats.tick_ms = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace urbs::sim
