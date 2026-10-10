#include "game/music_rules.hpp"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "sim/world.hpp"

namespace urbs::game {

using json = nlohmann::json;

void MusicManifest::load(const std::string& path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error("cannot open " + path);
  const json j = json::parse(f);
  *this = MusicManifest{};
  auto pair = [](const json& o, const char* key, f32* out) {
    if (o.contains(key) && o[key].is_array() && o[key].size() == 2) {
      out[0] = o[key][0].get<f32>();
      out[1] = o[key][1].get<f32>();
    }
  };
  if (j.contains("silence_s")) {
    pair(j["silence_s"], "city", city_silence);
    pair(j["silence_s"], "meta", meta_silence);
  }
  if (j.contains("crossfade_s")) {
    const json& c = j["crossfade_s"];
    crossfade = c.value("default", crossfade);
    crossfade_battle = c.value("battle", crossfade_battle);
    duck_db = c.value("stinger_duck_db", duck_db);
  }
  for (const json& t : j.at("tracks")) {
    MusicTrack m;
    m.id = t.at("id").get<std::string>();
    m.title = t.value("title", m.id);
    m.kind = t.at("kind").get<std::string>();
    m.use = t.value("use", std::string());
    m.eras = t.value("eras", m.eras);
    m.regions = t.value("regions", m.regions);
    m.missions = t.value("missions", m.missions);
    m.prefer_seasons = t.value("prefer_seasons", m.prefer_seasons);
    m.weight = t.value("weight", m.weight);
    m.loop = t.value("loop", m.loop);
    tracks.push_back(std::move(m));
  }
  for (const json& s : j.value("stingers", json::array()))
    stingers.emplace_back(s.at("event").get<std::string>(), s.at("id").get<std::string>());
}

const MusicTrack* MusicManifest::find(std::string_view id) const {
  for (const auto& t : tracks)
    if (t.id == id) return &t;
  return nullptr;
}

const MusicTrack* MusicManifest::by_use(std::string_view kind, std::string_view use) const {
  for (const auto& t : tracks)
    if (t.kind == kind && t.use == use) return &t;
  return nullptr;
}

std::string MusicManifest::stinger(std::string_view event) const {
  for (const auto& [e, id] : stingers)
    if (e == event) return id;
  return {};
}

CityContext city_context(const sim::World& w) {
  CityContext c;
  c.mission = w.mission_id;
  // The world keeps only "empire"; missions 9-11 are the Late Republic (docs/03).
  c.era = w.empire ? "empire" : w.mission_number >= 9 ? "late_republic" : "republic";
  c.region = w.region.empty() ? "default" : w.region;
  constexpr const char* kSeasons[] = {"spring", "summer", "autumn", "winter"};
  c.season = kSeasons[static_cast<int>(sim::season(w.now))];
  return c;
}

namespace {
bool has(const std::vector<std::string>& v, const std::string& s) {
  return std::find(v.begin(), v.end(), s) != v.end();
}

f32 city_weight(const MusicTrack& t, const CityContext& c) {
  return t.weight * (has(t.prefer_seasons, c.season) ? 2.0f : 1.0f);
}
}  // namespace

bool city_eligible(const MusicTrack& t, const CityContext& c) {
  if (t.kind != "city") return false;
  if (!c.mission.empty() && has(t.missions, c.mission)) return true;
  return has(t.eras, c.era) && has(t.regions, c.region);
}

int pick_city(const MusicManifest& m, const CityContext& c, std::string_view last, f32 r) {
  f32 total = 0;
  int n = 0, only = -1;
  for (size_t i = 0; i < m.tracks.size(); ++i)
    if (city_eligible(m.tracks[i], c)) {
      ++n;
      only = static_cast<int>(i);
    }
  if (n <= 1) return only;
  for (const auto& t : m.tracks)
    if (city_eligible(t, c) && t.id != last) total += city_weight(t, c);
  f32 acc = 0;
  int pick = -1;
  for (size_t i = 0; i < m.tracks.size(); ++i) {
    const auto& t = m.tracks[i];
    if (!city_eligible(t, c) || t.id == last) continue;
    pick = static_cast<int>(i);
    acc += city_weight(t, c);
    if (r * total < acc) break;
  }
  return pick;
}

MusicSignals read_signals(const sim::World& w) {
  MusicSignals s;
  const auto& A = w.army;
  for (const auto& u : A.units) {
    if (!u.alive || u.order == military::Order::Away) continue;
    if (u.side == 1 && u.order != military::Order::Rout) s.enemies += u.count;
    if (u.side == 0) s.defenders += u.count;
    if (u.losses_window > 0 || u.kills_window > 0) s.combat = true;
  }
  if (!s.combat) {
    for (const auto& p : A.projectiles)
      if (p.alive) {
        s.combat = true;
        break;
      }
  }
  if (!s.combat) {
    for (u32 i = 0; i < A.ids.high_water() && !s.combat; ++i)
      if (A.ids.alive_index(i) && A.attacking[i] > 0) s.combat = true;
  }
  if (s.enemies == 0) s.combat = false;  // leftover missiles or a routed band
  w.buildings.for_each([&](BuildingId, const sim::Building& b) {
    if (b.state == sim::BState::Burning) ++s.burning;
  });
  for (auto it = w.alerts.rbegin(); it != w.alerts.rend(); ++it) {
    if (w.now - it->t > kTicksPerDay) continue;  // repeats update old entries in place: no early break
    if (it->kind == sim::AlertKind::Riot || it->kind == sim::AlertKind::Revolt) s.riot = true;
    if (it->kind == sim::AlertKind::Event && it->arg == 1002 && w.now - it->t < kTicksPerHour * 6) s.quake = true;
  }
  s.festival = w.festival_until > w.now;
  s.mission_over = w.mission != sim::MissionState::Running;
  s.empire = w.empire != 0;
  s.favor = w.favor;
  return s;
}

Mood base_mood(const MusicSignals& s) {
  if (s.mission_over) return Mood::MissionOver;
  if (s.combat && s.enemies > 0)
    return s.enemies >= 150 ? Mood::LargeBattle : s.defenders == 0 ? Mood::Siege : Mood::Skirmish;
  if (s.burning >= 5 || s.quake) return Mood::Disaster;
  if (s.enemies > 0) return Mood::RaidNear;
  if (s.riot || s.favor < 20) return Mood::Unrest;
  if (s.festival) return Mood::Festival;
  return Mood::City;
}

std::pair<const char*, const char*> mood_track(Mood m, bool empire) {
  switch (m) {
    case Mood::Festival:
      return {"festival", empire ? "games" : "festival"};
    case Mood::Aftermath:
      return {"lament", "aftermath"};
    case Mood::Unrest:
      return {"tension", "unrest"};
    case Mood::RaidNear:
      return {"tension", "raid"};
    case Mood::Disaster:
      return {"disaster", "disaster"};
    case Mood::Skirmish:
      return {"battle", "skirmish"};
    case Mood::Siege:
      return {"battle", "siege"};
    case Mood::LargeBattle:
      return {"battle", "large"};
    case Mood::MissionOver:
      return {"meta", "briefing"};
    case Mood::City:
      break;
  }
  return {"city", ""};
}

}  // namespace urbs::game
