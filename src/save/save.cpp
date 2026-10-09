#include "save/save.hpp"

#include <zstd.h>

#include <fstream>

#include "save/serialize.hpp"

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
  w.tag("HOUS");
  save::write_slots(w, households.ids);
  w.vec(households.h);
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
  r.expect("HOUS");
  save::read_slots(r, households.ids);
  r.vec(households.h);
  r.expect("END.");
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
    if (version != kSaveVersion)
      throw std::runtime_error("incompatible save version " + std::to_string(version) + " (this build reads " +
                               std::to_string(kSaveVersion) + ")");
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
    w.map.mark_all_dirty();
    w.init(cfg, defs);
    w.deserialize(r);
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
  }
  for (const auto& s : w.rng) {
    auto st = s.state();
    h = fnv(h, &st, sizeof(st));
  }
  return h;
}

}  // namespace urbs::save
