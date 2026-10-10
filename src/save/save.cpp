#include "save/save.hpp"

#include <zstd.h>

#include <fstream>

#include "save/serialize.hpp"
#include "script/lua_host.hpp"

namespace urbs::save {

namespace {
constexpr u32 kMagic = 0x53425255;  // "URBS"

template <typename IdT>
void write_slots(Writer& w, const core::SlotAlloc<IdT>& s) {
  auto& m = const_cast<core::SlotAlloc<IdT>&>(s);
  w.vec(m.gens());
  w.vec(m.alives());
  w.vec(m.free_list());
  w.pod(s.count());
  w.pod(s.high_water());
}
template <typename IdT>
void read_slots(Reader& r, core::SlotAlloc<IdT>& s) {
  r.vec(s.gens());
  r.vec(s.alives());
  r.vec_fixed(s.free_list());
  u32 count = r.pod<u32>(), hw = r.pod<u32>();
  s.set_counts(count, hw);
}

u64 fnv(u64 h, const void* p, size_t n) {
  const auto* b = static_cast<const u8*>(p);
  for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 0x100000001B3ull;
  return h;
}
template <typename T>
u64 fnv_vec(u64 h, const std::vector<T>& v) {
  return fnv(h, v.data(), v.size() * sizeof(T));
}
}  // namespace

}  // namespace urbs::save

namespace urbs::sim {

void World::serialize(save::Writer& w) const {
  w.tag("WRLD");
  w.pod(now);
  w.pod(walking);
  w.pod(static_cast<u8>(providers_dirty_));
  for (const auto& s : rng) w.pod(s.state());
  w.tag("NAVG");
  w.vec(nav.chunk_changed);
  w.tag("PATH");
  paths.serialize(w);
  w.tag("SCHD");
  std::vector<Event> evs;
  sched.collect_all(evs);
  w.vec(evs);
  w.tag("SRVC");
  services.serialize(w);
  w.tag("BLDG");
  save::write_slots(w, buildings.ids);
  w.vec(buildings.b);
  w.tag("CITZ");
  const auto& C = citizens;
  save::write_slots(w, C.ids);
  w.vec(C.token); w.vec(C.activity); w.vec(C.next); w.vec(C.loc); w.vec(C.dest); w.vec(C.dest_tile);
  w.vec(C.path); w.vec(C.path_rev); w.vec(C.walk_start); w.vec(C.activity_end); w.vec(C.home); w.vec(C.work);
  w.vec(C.household); w.vec(C.need_base); w.vec(C.need_rate); w.vec(C.need_t0); w.vec(C.carrying);
  w.vec(C.carry_amount); w.vec(C.task); w.vec(C.cls); w.vec(C.sex); w.vec(C.born); w.vec(C.name0); w.vec(C.name1);
  w.vec(C.name2); w.vec(C.traits); w.vec(C.skills); w.vec(C.happiness); w.vec(C.health); w.vec(C.goal);
  w.vec(C.goal_progress); w.vec(C.spouse); w.vec(C.mother); w.vec(C.father); w.vec(C.memory); w.vec(C.memory_head);
  w.vec(C.flags);
  w.vec(C.reason);
  w.vec(C.bonus);
  w.vec(C.bonus_until);
  w.vec(C.goal_since);
  w.tag("HOUS");
  save::write_slots(w, households.ids);
  w.vec(households.h);
  w.tag("ECON");
  save::write_slots(w, econ.inv_ids);
  w.vec(econ.amount); w.vec(econ.res_in); w.vec(econ.res_out); w.vec(econ.accept); w.vec(econ.target);
  w.vec(econ.capacity); w.vec(econ.total); w.vec(econ.owner);
  save::write_slots(w, econ.task_ids);
  w.vec(econ.tasks);
  w.pod(econ.open_tasks);
  w.pod(econ.treasury);
  w.vec(econ.produced_today); w.vec(econ.consumed_today); w.vec(econ.produced_yday); w.vec(econ.consumed_yday);
  w.vec(econ.city_stock);
  w.pod(labor_hourly_runs);
  w.tag("SOCI");
  w.pod(policy);
  w.vec(alerts);
  w.pod(entry_tile);
  w.pod(immigrants_today); w.pod(emigrants_today); w.pod(immigrants_total); w.pod(emigrants_total);
  w.pod(appeal); w.pod(happy_sum_today); w.pod(happy_count_today); w.pod(avg_happiness);
  w.tag("TRAD");
  w.pod(econ.money);
  w.vec(econ.price);
  w.vec(econ.imported_today); w.vec(econ.exported_today); w.vec(econ.imported_yday); w.vec(econ.exported_yday);
  w.pod(static_cast<u32>(routes.size()));
  for (size_t k = 0; k < routes.size(); ++k) {
    const auto& r = routes[k];
    w.str(defs->partners[k].id);  // v3: routes are matched by partner id on load
    w.pod(r.open); w.pod(r.available); w.pod(r.next_visit); w.pod(r.ship_until); w.pod(r.ship_at); w.pod(r.caravan_traded);
    w.vec(r.bought);
    w.vec(r.sold);
  }
  w.vec(trade_mode);
  w.vec(trade_level);
  w.pod(visitors);
  w.tag("LIFE");
  for (u32 v : {births_today, deaths_today, marriages_today, crimes_today, crimes_yday, freed_today, births_total,
                deaths_total, riots_total})
    w.pod(v);
  w.tag("GODS");
  w.vec(god_mood);
  w.vec(god_available);
  w.vec(god_offerings);
  for (Tick t : effect_until) w.pod(t);
  w.pod(festival_until);
  w.pod(festival_god);
  w.pod(weather);
  w.pod(fires_total);
  w.pod(collapses_total);
  w.pod(plague_houses);
  w.tag("GOVN");
  w.pod(ratings);
  w.pod(favor);
  w.pod(empire);
  w.pod(nile);
  w.vec(requests);
  w.pod(next_request_id);
  w.pod(requests_fulfilled);
  w.pod(requests_failed);
  w.pod(loan);
  w.pod(debt_days);
  w.pod(goals.population); w.pod(goals.culture); w.pod(goals.prosperity); w.pod(goals.security); w.pod(goals.favor);
  w.pod(goals.requests); w.pod(goals.survive_days); w.pod(goals.max_debt_days); w.pod(goals.fail_on_favor);
  w.vec(goals.buildings);
  w.pod(mission);
  w.pod(mission_days);
  w.str(mission_id);
  w.str(region);
  w.str(lost_reason);
  w.vec(unlocked);
  w.pod(mission_number);
  w.pod(difficulty);
  w.pod(static_cast<u32>(messages.size()));
  for (const auto& m : messages) {
    w.pod(m.id); w.pod(m.t); w.pod(m.advisor); w.pod(m.answered);
    w.str(m.title); w.str(m.text); w.str(m.callback);
    w.pod(static_cast<u32>(m.options.size()));
    for (const auto& o : m.options) w.str(o);
  }
  w.pod(next_message_id);
  w.pod(scientia);
  w.vec(tech_known);
  for (u8 f : tech_flags) w.pod(f);
  w.str(script_source);
  w.str(script_name);
  w.str(script ? script->export_state() : std::string());
  w.tag("ARMY");
  save::write_slots(w, army.ids);
  w.vec(army.x); w.vec(army.y); w.vec(army.px); w.vec(army.py); w.vec(army.hp); w.vec(army.unit); w.vec(army.slot);
  w.vec(army.cooldown); w.vec(army.target); w.vec(army.facing); w.vec(army.walked); w.vec(army.attacking);
  for (const auto& u : army.units) w.pod(u);
  w.vec(army.projectiles);
  w.pod(army.deployed);
  w.pod(army.next_number);
  w.pod(soldiers_lost_);
  w.pod(enemies_killed_);
  w.pod(buy_captives_);
  w.pod(crime_cut_);
  w.pod(nav_changed_tick_);
  w.pod(static_cast<u8>(water_dirty));
  w.pod(static_cast<u8>(desir_dirty));
  w.tag("END.");
}

void World::deserialize(save::Reader& r) {
  r.expect("WRLD");
  r.pod(now);
  r.pod(walking);
  providers_dirty_ = r.pod<u8>() != 0;
  for (auto& s : rng) s.set_state(r.pod<rng::Stream::State>());
  r.expect("NAVG");
  r.vec(nav.chunk_changed);
  r.expect("PATH");
  paths.deserialize(r);
  r.expect("SCHD");
  std::vector<Event> evs;
  r.vec(evs);
  sched.restore(now, evs);
  r.expect("SRVC");
  services.deserialize(r);
  r.expect("BLDG");
  save::read_slots(r, buildings.ids);
  r.vec(buildings.b);
  r.expect("CITZ");
  auto& C = citizens;
  save::read_slots(r, C.ids);
  r.vec(C.token); r.vec(C.activity); r.vec(C.next); r.vec(C.loc); r.vec(C.dest); r.vec(C.dest_tile);
  r.vec(C.path); r.vec(C.path_rev); r.vec(C.walk_start); r.vec(C.activity_end); r.vec(C.home); r.vec(C.work);
  r.vec(C.household); r.vec(C.need_base); r.vec(C.need_rate); r.vec(C.need_t0); r.vec(C.carrying);
  r.vec(C.carry_amount); r.vec(C.task); r.vec(C.cls); r.vec(C.sex); r.vec(C.born); r.vec(C.name0); r.vec(C.name1);
  r.vec(C.name2); r.vec(C.traits); r.vec(C.skills); r.vec(C.happiness); r.vec(C.health); r.vec(C.goal);
  r.vec(C.goal_progress); r.vec(C.spouse); r.vec(C.mother); r.vec(C.father); r.vec(C.memory); r.vec(C.memory_head);
  r.vec(C.flags);
  r.vec(C.reason);
  r.vec(C.bonus);
  r.vec(C.bonus_until);
  r.vec(C.goal_since);
  r.expect("HOUS");
  save::read_slots(r, households.ids);
  r.vec(households.h);
  r.expect("ECON");
  save::read_slots(r, econ.inv_ids);
  r.vec(econ.amount); r.vec(econ.res_in); r.vec(econ.res_out); r.vec(econ.accept); r.vec(econ.target);
  r.vec(econ.capacity); r.vec(econ.total); r.vec(econ.owner);
  save::read_slots(r, econ.task_ids);
  r.vec(econ.tasks);
  r.pod(econ.open_tasks);
  r.pod(econ.treasury);
  r.vec(econ.produced_today); r.vec(econ.consumed_today); r.vec(econ.produced_yday); r.vec(econ.consumed_yday);
  r.vec(econ.city_stock);
  r.pod(labor_hourly_runs);
  r.expect("SOCI");
  r.pod(policy);
  r.vec_fixed(alerts);
  r.pod(entry_tile);
  r.pod(immigrants_today); r.pod(emigrants_today); r.pod(immigrants_total); r.pod(emigrants_total);
  r.pod(appeal); r.pod(happy_sum_today); r.pod(happy_count_today); r.pod(avg_happiness);
  r.expect("TRAD");
  if (r.version >= 3) {
    r.pod(econ.money);
  } else {
    // v1-v2 ledgers had no Mint column (index 4): shift the later columns up by one.
    struct OldLedger {
      i64 v[4][econ::kMoneyCount - 1];
    } ol;
    r.pod(ol);
    const auto& old = ol.v;
    i64* dst[4] = {econ.money.today, econ.money.yesterday, econ.money.year, econ.money.last_year};
    for (int a = 0; a < 4; ++a)
      for (int k = 0; k < econ::kMoneyCount - 1; ++k) dst[a][k < 4 ? k : k + 1] = old[a][k];
  }
  r.vec(econ.price);
  r.vec(econ.imported_today); r.vec(econ.exported_today); r.vec(econ.imported_yday); r.vec(econ.exported_yday);
  const u32 nr = r.pod<u32>();
  // The base partner list of v1-v2 saves (Parthia was added in v3); later entries were former cities.
  static const char* kOldBase[] = {"latin_villages", "rome",   "etruria", "carthage", "massalia",
                                   "alexandria",     "hispania", "gallia", "greece",   "germania"};
  u32 former = 0;
  for (u32 k = 0; k < nr; ++k) {
    std::string id;
    if (r.version >= 3) {
      id = r.str();
    } else if (k < 10) {
      id = kOldBase[k];
    }
    // Find the route: by id, or (old saves) the k-th partner after the base list.
    size_t slot = routes.size();
    for (size_t p = 0; p < defs->partners.size() && slot == routes.size(); ++p)
      if (!id.empty() && defs->partners[p].id == id) slot = p;
    if (id.empty()) {
      size_t seen = 0;
      for (size_t p = 0; p < defs->partners.size(); ++p) {
        bool base = defs->partners[p].id == "parthia";
        for (const char* b : kOldBase) base |= defs->partners[p].id == b;
        if (!base && seen++ == former) {
          slot = p;
          break;
        }
      }
      ++former;
    }
    TradeRouteState dummy;
    dummy.bought.assign(econ.goods, 0);
    dummy.sold.assign(econ.goods, 0);
    auto& rt = slot < routes.size() ? routes[slot] : dummy;
    r.pod(rt.open); r.pod(rt.available); r.pod(rt.next_visit); r.pod(rt.ship_until); r.pod(rt.ship_at); r.pod(rt.caravan_traded);
    r.vec(rt.bought);
    r.vec(rt.sold);
  }
  r.vec(trade_mode);
  r.vec(trade_level);
  r.pod(visitors);
  r.expect("LIFE");
  for (u32* v : {&births_today, &deaths_today, &marriages_today, &crimes_today, &crimes_yday, &freed_today,
                 &births_total, &deaths_total, &riots_total})
    r.pod(*v);
  r.expect("GODS");
  r.vec(god_mood);
  r.vec(god_available);
  r.vec(god_offerings);
  for (Tick& t : effect_until) r.pod(t);
  r.pod(festival_until);
  r.pod(festival_god);
  r.pod(weather);
  r.pod(fires_total);
  r.pod(collapses_total);
  r.pod(plague_houses);
  r.expect("GOVN");
  r.pod(ratings);
  r.pod(favor);
  r.pod(empire);
  if (r.version >= 3) r.pod(nile);
  r.vec(requests);
  r.pod(next_request_id);
  r.pod(requests_fulfilled);
  r.pod(requests_failed);
  r.pod(loan);
  r.pod(debt_days);
  r.pod(goals.population); r.pod(goals.culture); r.pod(goals.prosperity); r.pod(goals.security); r.pod(goals.favor);
  r.pod(goals.requests); r.pod(goals.survive_days); r.pod(goals.max_debt_days); r.pod(goals.fail_on_favor);
  r.vec(goals.buildings);
  r.pod(mission);
  r.pod(mission_days);
  mission_id = r.str();
  if (r.version >= 3) region = r.str();
  lost_reason = r.str();
  r.vec(unlocked);
  r.pod(mission_number);
  // Buildings added to the game after the save was written: available if the mission allows them.
  for (size_t d = unlocked.size(); d < defs->buildings.size(); ++d)
    unlocked.push_back(defs->buildings[d].unlock <= mission_number ? 1 : 0);
  r.pod(difficulty);
  {
    const u32 nm = r.pod<u32>();
    messages.clear();
    for (u32 k = 0; k < nm; ++k) {
      Message m;
      r.pod(m.id); r.pod(m.t); r.pod(m.advisor); r.pod(m.answered);
      m.title = r.str(); m.text = r.str(); m.callback = r.str();
      const u32 no = r.pod<u32>();
      for (u32 o = 0; o < no; ++o) m.options.push_back(r.str());
      messages.push_back(std::move(m));
    }
  }
  r.pod(next_message_id);
  r.pod(scientia);
  r.vec(tech_known);
  for (u8& f : tech_flags) r.pod(f);
  script_source = r.str();
  script_name = r.str();
  pending_script_state_ = r.str();
  r.expect("ARMY");
  save::read_slots(r, army.ids);
  r.vec(army.x); r.vec(army.y); r.vec(army.px); r.vec(army.py); r.vec(army.hp); r.vec(army.unit); r.vec(army.slot);
  r.vec(army.cooldown); r.vec(army.target); r.vec(army.facing); r.vec(army.walked); r.vec(army.attacking);
  for (auto& u : army.units) {
    r.pod(u);
    u.field = -1;  // flow fields are recomputed on demand
  }
  r.vec(army.projectiles);
  r.pod(army.deployed);
  r.pod(army.next_number);
  r.pod(soldiers_lost_);
  r.pod(enemies_killed_);
  r.pod(buy_captives_);
  if (r.version >= 2) r.pod(crime_cut_);
  r.pod(nav_changed_tick_);
  water_dirty = r.pod<u8>() != 0;
  desir_dirty = r.pod<u8>() != 0;
  r.expect("END.");
  count_carters();  // derived from buildings
}

}  // namespace urbs::sim

namespace urbs::save {

bool save_world(const sim::World& w, const std::string& path, const std::string& mission, std::string* err) {
  Writer body;
  // Map + config first: needed to initialise the world before the rest can be read.
  body.tag("CONF");
  body.pod(w.cfg);
  body.tag("MAP.");
  body.pod(w.map.w);
  body.pod(w.map.h);
  body.vec(w.map.terrain);
  body.vec(w.map.height);
  body.vec(w.map.groundwater);
  body.vec(w.map.fertility);
  body.vec(w.map.resource);
  body.vec(w.map.road);
  body.vec(w.map.variant);
  body.vec(w.map.building);
  body.vec(w.map.under);
  body.vec(w.map.water);
  body.vec(w.map.desir);
  w.serialize(body);

  const auto& raw = body.data();
  std::vector<u8> comp(ZSTD_compressBound(raw.size()));
  size_t n = ZSTD_compress(comp.data(), comp.size(), raw.data(), raw.size(), 3);
  if (ZSTD_isError(n)) {
    if (err) *err = ZSTD_getErrorName(n);
    return false;
  }
  Writer head;
  head.pod(kMagic);
  head.pod(kSaveVersion);
  head.str(mission);
  head.pod(w.cfg.seed);
  head.pod(w.now);
  head.pod(w.citizens.ids.count());
  head.pod(static_cast<u64>(raw.size()));
  head.pod(static_cast<u64>(n));
  std::string tmp = path + ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary);
    if (!f) {
      if (err) *err = "cannot write " + tmp;
      return false;
    }
    f.write(reinterpret_cast<const char*>(head.data().data()), static_cast<std::streamsize>(head.data().size()));
    f.write(reinterpret_cast<const char*>(comp.data()), static_cast<std::streamsize>(n));
  }
  std::remove(path.c_str());
  if (std::rename(tmp.c_str(), path.c_str()) != 0) {
    if (err) *err = "cannot rename save";
    return false;
  }
  return true;
}

static bool read_file(const std::string& path, std::vector<u8>& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

bool read_info(const std::string& path, SaveInfo& info) {
  std::vector<u8> file;
  if (!read_file(path, file)) return false;
  try {
    Reader r(file.data(), file.size());
    if (r.pod<u32>() != kMagic) return false;
    info.version = r.pod<u32>();
    info.mission = r.str();
    info.seed = r.pod<u64>();
    info.now = r.pod<Tick>();
    info.citizens = r.pod<u32>();
    return true;
  } catch (...) {
    return false;
  }
}

bool load_world(sim::World& w, const data::Defs& defs, const std::string& path, std::string* err) {
  std::vector<u8> file;
  if (!read_file(path, file)) {
    if (err) *err = "cannot read " + path;
    return false;
  }
  try {
    Reader h(file.data(), file.size());
    if (h.pod<u32>() != kMagic) throw std::runtime_error("not an Urbs save");
    u32 version = h.pod<u32>();
    if (version < kOldestSaveVersion || version > kSaveVersion)
      throw std::runtime_error("incompatible save version " + std::to_string(version) + " (this build reads " +
                               std::to_string(kOldestSaveVersion) + "-" + std::to_string(kSaveVersion) + ")");
    h.str();
    h.pod<u64>();
    h.pod<Tick>();
    h.pod<u32>();
    u64 raw_size = h.pod<u64>(), comp_size = h.pod<u64>();
    const size_t head_size = file.size() - comp_size;
    std::vector<u8> raw(raw_size);
    size_t n = ZSTD_decompress(raw.data(), raw.size(), file.data() + head_size, comp_size);
    if (ZSTD_isError(n) || n != raw_size) throw std::runtime_error("corrupt save (decompression failed)");
    Reader r(raw.data(), raw.size());
    r.version = version;
    r.expect("CONF");
    sim::WorldConfig cfg = r.pod<sim::WorldConfig>();
    r.expect("MAP.");
    i32 mw = r.pod<i32>(), mh = r.pod<i32>();
    w.map.resize(mw, mh);
    r.vec(w.map.terrain);
    r.vec(w.map.height);
    r.vec(w.map.groundwater);
    r.vec(w.map.fertility);
    r.vec(w.map.resource);
    r.vec(w.map.road);
    r.vec(w.map.variant);
    r.vec(w.map.building);
    r.vec(w.map.under);
    r.vec(w.map.water);
    r.vec(w.map.desir);
    w.map.mark_all_dirty();
    w.init(cfg, defs);
    w.deserialize(r);
    if (!w.script_source.empty()) {
      auto host = std::make_unique<script::LuaHost>(w, w.script_source, w.script_name);
      if (host->start()) host->import_state(w.pending_script_state_);
      w.script = std::move(host);
    }
    if (!r.done()) throw std::runtime_error("trailing data in save");
  } catch (const std::exception& e) {
    if (err) *err = e.what();
    return false;
  }
  return true;
}

u64 world_hash(const sim::World& w) {
  u64 h = 0xCBF29CE484222325ull;
  h = fnv(h, &w.now, sizeof(w.now));
  const auto& C = w.citizens;
  h = fnv_vec(h, C.token);
  h = fnv_vec(h, C.activity);
  h = fnv_vec(h, C.loc);
  h = fnv_vec(h, C.home);
  h = fnv_vec(h, C.work);
  h = fnv_vec(h, C.need_base);
  h = fnv_vec(h, C.need_t0);
  h = fnv_vec(h, C.walk_start);
  h = fnv_vec(h, C.happiness);
  h = fnv_vec(h, C.health);
  for (u32 i = 0; i < w.buildings.ids.high_water(); ++i) {
    if (!w.buildings.ids.alive_index(i)) continue;
    const auto& b = w.buildings.b[i];
    u32 v[8] = {b.present, b.employed, b.residents, b.visitors, b.work_progress, b.build_progress,
                static_cast<u32>(b.money), b.damage};
    h = fnv(h, v, sizeof(v));
  }
  for (u32 i = 0; i < w.households.ids.high_water(); ++i) {
    if (!w.households.ids.alive_index(i)) continue;
    const auto& hh = w.households.h[i];
    u32 v[4] = {hh.food, hh.members, static_cast<u32>(hh.savings), hh.water};
    h = fnv(h, v, sizeof(v));
    h = fnv(h, hh.fstock, sizeof(hh.fstock));
  }
  h = fnv_vec(h, w.econ.amount);
  h = fnv(h, &w.econ.treasury, sizeof(w.econ.treasury));
  for (const auto& s : w.rng) {
    auto st = s.state();
    h = fnv(h, &st, sizeof(st));
  }
  return h;
}

std::vector<std::pair<std::string, u64>> world_hash_parts(const sim::World& w) {
  std::vector<std::pair<std::string, u64>> out;
  const u64 s0 = 0xCBF29CE484222325ull;
  auto add = [&](const char* name, u64 h) { out.emplace_back(name, h); };
  const auto& C = w.citizens;
  add("time", fnv(s0, &w.now, sizeof(w.now)));
  add("citizens.token", fnv_vec(s0, C.token));
  add("citizens.activity", fnv_vec(s0, C.activity));
  add("citizens.loc", fnv_vec(s0, C.loc));
  add("citizens.home", fnv_vec(s0, C.home));
  add("citizens.work", fnv_vec(s0, C.work));
  add("citizens.needs", fnv_vec(fnv_vec(s0, C.need_base), C.need_t0));
  add("citizens.walk_start", fnv_vec(s0, C.walk_start));
  add("citizens.happiness", fnv_vec(s0, C.happiness));
  add("citizens.health", fnv_vec(s0, C.health));
  add("citizens.task", fnv_vec(s0, C.task));
  add("citizens.reason", fnv_vec(s0, C.reason));
  u64 hb = s0, hp = s0, hw = s0;
  for (u32 i = 0; i < w.buildings.ids.high_water(); ++i) {
    if (!w.buildings.ids.alive_index(i)) continue;
    const auto& b = w.buildings.b[i];
    u32 v[6] = {b.employed, b.residents, b.visitors, b.build_progress, static_cast<u32>(b.money), b.damage};
    hb = fnv(hb, v, sizeof(v));
    hp = fnv(hp, &b.present, sizeof(b.present));
    hw = fnv(hw, &b.work_progress, sizeof(b.work_progress));
  }
  add("buildings", hb);
  add("buildings.present", hp);
  add("buildings.work_progress", hw);
  u64 hh = s0;
  for (u32 i = 0; i < w.households.ids.high_water(); ++i) {
    if (!w.households.ids.alive_index(i)) continue;
    const auto& h = w.households.h[i];
    u32 v[4] = {h.food, h.members, static_cast<u32>(h.savings), h.water};
    hh = fnv(fnv(hh, v, sizeof(v)), h.fstock, sizeof(h.fstock));
  }
  add("households", hh);
  add("econ.amount", fnv_vec(s0, w.econ.amount));
  add("econ.reservations", fnv_vec(fnv_vec(s0, w.econ.res_in), w.econ.res_out));
  add("econ.treasury", fnv(s0, &w.econ.treasury, sizeof(w.econ.treasury)));
  add("econ.price", fnv_vec(s0, w.econ.price));
  u64 ht = s0;
  for (u32 t = 0; t < w.econ.task_ids.high_water(); ++t)
    if (w.econ.task_ids.alive_index(t)) ht = fnv(ht, &w.econ.tasks[t], sizeof(w.econ.tasks[t]));
  add("econ.hauls", ht);
  for (size_t k = 0; k < static_cast<size_t>(rng::StreamId::Count); ++k) {
    auto st = w.rng[k].state();
    add(("rng." + std::to_string(k)).c_str(), fnv(s0, &st, sizeof(st)));
  }
  u64 ha = s0;
  for (const auto& u : w.army.units) ha = fnv(ha, &u, sizeof(u));
  ha = fnv_vec(fnv_vec(fnv_vec(ha, w.army.x), w.army.y), w.army.hp);
  add("army", ha);
  u64 hr = s0;
  for (const auto& r : w.routes) {
    hr = fnv(hr, &r.open, sizeof(r.open));
    hr = fnv(hr, &r.next_visit, sizeof(r.next_visit));
    hr = fnv_vec(fnv_vec(hr, r.bought), r.sold);
  }
  add("trade.routes", hr);
  add("favor", fnv(s0, &w.favor, sizeof(w.favor)));
  add("script", w.script ? fnv(s0, w.script->export_state().data(), w.script->export_state().size()) : 0);
  return out;
}

namespace {
constexpr u32 kMapMagic = 0x4D425255;  // "URBM"
constexpr u32 kMapVersion = 1;
}  // namespace

bool save_map(const sim::TileMap& m, const std::string& path, std::string* err) {
  Writer body;
  body.pod(m.w);
  body.pod(m.h);
  body.vec(m.terrain);
  body.vec(m.height);
  body.vec(m.groundwater);
  body.vec(m.fertility);
  body.vec(m.resource);
  body.vec(m.road);
  body.vec(m.variant);
  const auto& raw = body.data();
  std::vector<u8> comp(ZSTD_compressBound(raw.size()));
  const size_t n = ZSTD_compress(comp.data(), comp.size(), raw.data(), raw.size(), 9);
  if (ZSTD_isError(n)) {
    if (err) *err = ZSTD_getErrorName(n);
    return false;
  }
  std::ofstream f(path, std::ios::binary);
  if (!f) {
    if (err) *err = "cannot write " + path;
    return false;
  }
  Writer head;
  head.pod(kMapMagic);
  head.pod(kMapVersion);
  head.pod(static_cast<u64>(raw.size()));
  f.write(reinterpret_cast<const char*>(head.data().data()), static_cast<std::streamsize>(head.data().size()));
  f.write(reinterpret_cast<const char*>(comp.data()), static_cast<std::streamsize>(n));
  return static_cast<bool>(f);
}

bool load_map(sim::TileMap& m, const std::string& path, std::string* err) {
  std::vector<u8> file;
  if (!read_file(path, file)) {
    if (err) *err = "cannot read " + path;
    return false;
  }
  try {
    Reader h(file.data(), file.size());
    if (h.pod<u32>() != kMapMagic) throw std::runtime_error("not an Urbs map");
    if (h.pod<u32>() > kMapVersion) throw std::runtime_error("map made by a newer version");
    const u64 raw_size = h.pod<u64>();
    const size_t head = 4 + 4 + 8;
    std::vector<u8> raw(raw_size);
    const size_t n = ZSTD_decompress(raw.data(), raw.size(), file.data() + head, file.size() - head);
    if (ZSTD_isError(n) || n != raw_size) throw std::runtime_error("corrupt map");
    Reader r(raw.data(), raw.size());
    const i32 w = r.pod<i32>(), hh = r.pod<i32>();
    m.resize(w, hh);
    r.vec(m.terrain);
    r.vec(m.height);
    r.vec(m.groundwater);
    r.vec(m.fertility);
    r.vec(m.resource);
    r.vec(m.road);
    r.vec(m.variant);
    m.mark_all_dirty();
  } catch (const std::exception& e) {
    if (err) *err = e.what();
    return false;
  }
  return true;
}

}  // namespace urbs::save
