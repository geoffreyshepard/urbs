#pragma once
// Music selection rules: the manifest (data/defs/music.json), which city tracks fit a mission,
// and what the city's state calls for. No SDL here, so the unit tests can link it.
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/types.hpp"

namespace urbs::sim {
struct World;
}

namespace urbs::game {

struct MusicTrack {
  std::string id, title, kind, use;
  std::vector<std::string> eras, regions, missions, prefer_seasons;
  f32 weight = 1;
  bool loop = false;
};

struct MusicManifest {
  std::vector<MusicTrack> tracks;
  std::vector<std::pair<std::string, std::string>> stingers;  // (event, id)
  f32 city_silence[2] = {45, 150}, meta_silence[2] = {8, 20};
  f32 crossfade = 2.5f, crossfade_battle = 1.0f, duck_db = -10;

  // Throws on malformed JSON (load time only).
  void load(const std::string& path);
  [[nodiscard]] const MusicTrack* find(std::string_view id) const;
  [[nodiscard]] const MusicTrack* by_use(std::string_view kind, std::string_view use) const;
  [[nodiscard]] std::string stinger(std::string_view event) const;  // track id, or empty
};

// What a city track is matched against.
struct CityContext {
  std::string mission, era, region, season;
};
CityContext city_context(const sim::World& w);

// A track fits when its missions list names the mission, or when both its eras and regions match.
bool city_eligible(const MusicTrack& t, const CityContext& c);
// Weighted pick among eligible city tracks with r in [0,1); avoids repeating `last` when there is
// a choice. Returns -1 when no city track fits.
int pick_city(const MusicManifest& m, const CityContext& c, std::string_view last, f32 r);

// Music moods, lowest priority first.
enum class Mood : u8 {
  City,
  Festival,
  Aftermath,
  Unrest,
  RaidNear,
  Disaster,
  Skirmish,
  Siege,
  LargeBattle,
  MissionOver
};

// The parts of the city's state the music reacts to.
struct MusicSignals {
  u32 enemies = 0, defenders = 0;  // soldiers in the field
  bool combat = false;             // blows or missiles exchanged recently
  u32 burning = 0;                 // buildings on fire
  bool quake = false, riot = false, festival = false, mission_over = false, empire = false;
  i32 favor = 50;
};
MusicSignals read_signals(const sim::World& w);

// The mood the signals call for, before hysteresis. Battles outrank disasters, which outrank a
// raid on its way, unrest and festivals.
Mood base_mood(const MusicSignals& s);
// Manifest (kind, use) of the track for a non-city mood.
std::pair<const char*, const char*> mood_track(Mood m, bool empire);

}  // namespace urbs::game
