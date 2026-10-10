#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "game/music_rules.hpp"

using namespace urbs;
using namespace urbs::game;

namespace {
const std::string kRoot = std::string(URBS_SOURCE_DIR) + "/";

MusicManifest manifest() {
  MusicManifest m;
  m.load(kRoot + "data/defs/music.json");
  return m;
}
}  // namespace

TEST_CASE("Music manifest: every track and stinger has a rendered file") {
  const MusicManifest m = manifest();
  CHECK(m.tracks.size() >= 20);
  CHECK(m.stingers.size() >= 10);
  for (const auto& t : m.tracks) {
    INFO(t.id);
    CHECK(std::filesystem::exists(kRoot + "assets/music/" + t.id + ".ogg"));
  }
  for (const auto& [event, id] : m.stingers) {
    INFO(event);
    CHECK(std::filesystem::exists(kRoot + "assets/music/" + id + ".ogg"));
  }
}

TEST_CASE("Music manifest: every mood the director asks for has a track") {
  const MusicManifest m = manifest();
  for (Mood mood : {Mood::Festival, Mood::Aftermath, Mood::Unrest, Mood::RaidNear, Mood::Disaster, Mood::Skirmish,
                    Mood::Siege, Mood::LargeBattle, Mood::MissionOver}) {
    for (bool empire : {false, true}) {
      const auto [kind, use] = mood_track(mood, empire);
      INFO(kind << "/" << use);
      CHECK(m.by_use(kind, use) != nullptr);
    }
  }
  for (const char* use : {"menu", "map", "briefing"}) CHECK(m.by_use("meta", use) != nullptr);
  for (const char* ev : {"mission_start", "mission_won", "mission_failed", "promotion", "god_blessing", "god_wrath",
                         "raid_sighted", "request_from_rome", "monument_complete", "festival_begins"})
    CHECK_FALSE(m.stinger(ev).empty());
}

TEST_CASE("Music: every campaign mission has at least three city tracks") {
  const MusicManifest m = manifest();
  for (const auto& e : std::filesystem::directory_iterator(kRoot + "data/scenarios")) {
    const auto path = e.path() / "scenario.json";
    if (!std::filesystem::exists(path) || e.path().filename().string().rfind('m', 0) != 0) continue;
    std::ifstream f(path);
    const auto j = nlohmann::json::parse(f);
    CityContext c;
    c.mission = e.path().filename().string();
    c.era = j.value("era", std::string("republic"));
    c.region = j.value("region", std::string());
    if (c.region.empty()) c.region = "default";
    c.season = "spring";
    int n = 0;
    for (const auto& t : m.tracks) n += city_eligible(t, c) ? 1 : 0;
    INFO(c.mission << " era " << c.era << " region " << c.region);
    CHECK(n >= 3);
  }
}

TEST_CASE("Music: the playlist never repeats a track back to back") {
  const MusicManifest m = manifest();
  const CityContext c{"m03_beneventum", "republic", "default", "summer"};
  std::string last;
  for (int k = 0; k < 200; ++k) {
    const int i = pick_city(m, c, last, static_cast<f32>(k % 97) / 97.0f);
    REQUIRE(i >= 0);
    const auto& t = m.tracks[static_cast<size_t>(i)];
    CHECK(city_eligible(t, c));
    CHECK(t.id != last);
    last = t.id;
  }
  // Mission-specific music: Rome's finale is reserved for Rome.
  CHECK_FALSE(city_eligible(*m.find("caput_mundi"), c));
  CHECK(city_eligible(*m.find("caput_mundi"), CityContext{"m16_roma", "empire", "default", "spring"}));
}

TEST_CASE("Music moods: battles outrank disasters, raids, unrest and festivals") {
  MusicSignals s;
  CHECK(base_mood(s) == Mood::City);
  s.festival = true;
  CHECK(base_mood(s) == Mood::Festival);
  s.favor = 10;
  CHECK(base_mood(s) == Mood::Unrest);
  s.enemies = 40;
  CHECK(base_mood(s) == Mood::RaidNear);
  s.burning = 6;
  CHECK(base_mood(s) == Mood::Disaster);
  s.combat = true;
  s.defenders = 30;
  CHECK(base_mood(s) == Mood::Skirmish);
  s.defenders = 0;
  CHECK(base_mood(s) == Mood::Siege);
  s.enemies = 300;
  CHECK(base_mood(s) == Mood::LargeBattle);
  s.mission_over = true;
  CHECK(base_mood(s) == Mood::MissionOver);
  // Combat with no enemy left (a routed band, a stray missile) is not a battle.
  MusicSignals t;
  t.combat = true;
  CHECK(base_mood(t) == Mood::City);
}
