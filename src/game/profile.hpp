#pragma once
#include <string>
#include <vector>

#include "core/types.hpp"

namespace urbs::game {

struct Settings {
  float ui_scale = 1.0f;
  bool edge_scroll = false;
  bool autosave = true;
  bool fullscreen = false;
  int difficulty = 1;  // 0 Otium, 1 Civitas, 2 Imperium
  bool colorblind = false;          // overlays use a blue-orange scale instead of green-red
  std::vector<std::string> mods;    // enabled mods, by folder name, in load order
  void load(const std::string& dir);
  void save(const std::string& dir) const;
};

// A mod folder: <root>/mods/<name>/ with mod.json and defs/ (and optionally scenarios/).
struct ModInfo {
  std::string name, dir, title, version, description;
};
// Mods shipped next to the game data and in the user data folder.
std::vector<ModInfo> find_mods(const std::string& data_root, const std::string& user_dir);
// Folders of the enabled mods, in the order given by settings.
std::vector<std::string> enabled_mod_dirs(const Settings& s, const std::vector<ModInfo>& all);

struct MissionInfo {
  std::string id, name, rank, region, blurb;
  int salary = 1;
  float x = 0.5f, y = 0.5f;  // province map position (0..1)
  std::vector<std::string> sells;
};

struct CampaignDef {
  std::vector<MissionInfo> missions;
  bool load(const std::string& data_root);
  [[nodiscard]] int index_of(const std::string& id) const;
};

// Persistent career across missions.
struct CampaignProfile {
  std::vector<std::string> completed;
  i64 savings = 0;
  int difficulty = 1;
  void load(const std::string& dir);
  void save(const std::string& dir) const;
  [[nodiscard]] bool done(const std::string& id) const;
  [[nodiscard]] int next_index(const CampaignDef& c) const;  // first not completed
};

}  // namespace urbs::game
