#include "game/profile.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace urbs::game {

using json = nlohmann::json;

namespace {
json read_json(const std::string& path) {
  std::ifstream f(path);
  if (!f) return json::object();
  try {
    return json::parse(f);
  } catch (...) {
    return json::object();
  }
}
void write_json(const std::string& path, const json& j) {
  std::ofstream f(path);
  f << j.dump(2);
}
}  // namespace

void Settings::load(const std::string& dir) {
  json j = read_json(dir + "settings.json");
  ui_scale = j.value("ui_scale", ui_scale);
  edge_scroll = j.value("edge_scroll", edge_scroll);
  autosave = j.value("autosave", autosave);
  fullscreen = j.value("fullscreen", fullscreen);
  difficulty = j.value("difficulty", difficulty);
  colorblind = j.value("colorblind", colorblind);
  music_volume = std::clamp(j.value("music_volume", music_volume), 0.0f, 1.0f);
  sfx_volume = std::clamp(j.value("sfx_volume", sfx_volume), 0.0f, 1.0f);  // absent before v1.1: the default
  mods = j.value("mods", mods);
}

void Settings::save(const std::string& dir) const {
  write_json(dir + "settings.json", json{{"ui_scale", ui_scale},
                                         {"edge_scroll", edge_scroll},
                                         {"autosave", autosave},
                                         {"fullscreen", fullscreen},
                                         {"difficulty", difficulty},
                                         {"colorblind", colorblind},
                                         {"music_volume", music_volume},
                                         {"sfx_volume", sfx_volume},
                                         {"mods", mods}});
}

std::vector<ModInfo> find_mods(const std::string& data_root, const std::string& user_dir) {
  namespace fs = std::filesystem;
  std::vector<ModInfo> out;
  for (const std::string& base : {data_root + "mods", user_dir + "mods"}) {
    std::error_code ec;
    if (!fs::is_directory(base, ec)) continue;
    std::vector<fs::path> dirs;
    for (const auto& e : fs::directory_iterator(base, ec))
      if (e.is_directory()) dirs.push_back(e.path());
    std::sort(dirs.begin(), dirs.end());
    for (const auto& d : dirs) {
      ModInfo m;
      m.name = d.filename().string();
      m.dir = d.string();
      json j = read_json((d / "mod.json").string());
      m.title = j.value("name", m.name);
      m.version = j.value("version", std::string());
      m.description = j.value("description", std::string());
      bool dup = false;
      for (const auto& o : out) dup |= o.name == m.name;  // a user copy never shadows a shipped mod silently
      if (!dup) out.push_back(m);
    }
  }
  return out;
}

std::vector<std::string> enabled_mod_dirs(const Settings& s, const std::vector<ModInfo>& all) {
  std::vector<std::string> out;
  for (const auto& name : s.mods)
    for (const auto& m : all)
      if (m.name == name) out.push_back(m.dir);
  return out;
}

bool CampaignDef::load(const std::string& root) {
  json j = read_json(root + "data/campaign.json");
  if (!j.contains("missions")) return false;
  missions.clear();
  for (auto& m : j["missions"]) {
    MissionInfo mi;
    mi.id = m.value("id", "");
    mi.name = m.value("name", mi.id);
    mi.rank = m.value("rank", "");
    mi.region = m.value("region", "");
    mi.blurb = m.value("blurb", "");
    mi.letter = m.value("letter", "");
    mi.victory = m.value("victory", "");
    mi.epilogue = m.value("epilogue", "");
    mi.salary = m.value("salary", 1);
    mi.x = m.value("x", 0.5f);
    mi.y = m.value("y", 0.5f);
    mi.sells = m.value("sells", std::vector<std::string>{});
    missions.push_back(std::move(mi));
  }
  return true;
}

int CampaignDef::index_of(const std::string& id) const {
  for (size_t k = 0; k < missions.size(); ++k)
    if (missions[k].id == id) return static_cast<int>(k);
  return -1;
}

void CampaignProfile::load(const std::string& dir) {
  json j = read_json(dir + "campaign.json");
  completed = j.value("completed", std::vector<std::string>{});
  savings = j.value("savings", static_cast<i64>(0));
  difficulty = j.value("difficulty", 1);
}

void CampaignProfile::save(const std::string& dir) const {
  write_json(dir + "campaign.json", json{{"completed", completed}, {"savings", savings}, {"difficulty", difficulty}});
}

bool CampaignProfile::done(const std::string& id) const {
  return std::find(completed.begin(), completed.end(), id) != completed.end();
}

int CampaignProfile::next_index(const CampaignDef& c) const {
  for (size_t k = 0; k < c.missions.size(); ++k)
    if (!done(c.missions[k].id)) return static_cast<int>(k);
  return static_cast<int>(c.missions.size()) - 1;
}

}  // namespace urbs::game
