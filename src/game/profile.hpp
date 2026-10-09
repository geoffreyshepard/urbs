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
  void load(const std::string& dir);
  void save(const std::string& dir) const;
};

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
