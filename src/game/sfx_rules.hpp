#pragma once
// Sound-effect rules: the manifest (data/defs/sfx.json), where a sound in the city is heard from the
// camera, and which sounds the city's state calls for between two looks at it. No SDL here, so the unit
// tests can link it (tests/test_sfx.cpp). See docs/11-music.md, "Sound effects".
#include <string>
#include <string_view>
#include <vector>

#include "core/types.hpp"
#include "econ/money.hpp"

namespace urbs::sim {
struct World;
}

namespace urbs::game {

struct SfxEvent {
  std::string id;
  std::vector<std::string> files;  // variants
  f32 gain = 0.5f, cooldown = 0.1f, pitch_var = 0, gain_var = 0;
  int max_playing = 2;
  bool positional = false;
};

struct SfxPositional {
  f32 full = 0.8f, max = 2.0f, far_gain = 0.3f, pan = 0.6f;
};

struct SfxAmbient {
  std::string file;
  f32 gain = 0.4f, people_min = 20, people_full = 900, fade_s = 2;
};

struct SfxManifest {
  std::vector<SfxEvent> events;
  SfxPositional positional;
  SfxAmbient ambient;
  i64 coin_min = 25;  // denarii of trade, grants or payments since the last look that clink

  // Throws on malformed JSON (load time only).
  void load(const std::string& path);
  [[nodiscard]] int index(std::string_view id) const;  // -1 when absent
};

// Where a sound at (dx, dy) half-screens from the centre of the view is heard: gain 0 = not at all.
struct SfxPlacement {
  f32 gain = 0, pan = 0;
};
SfxPlacement place_sound(const SfxPositional& p, f32 dx, f32 dy);

// The ambient bed's level (0..1) for this many people in the buildings on screen.
f32 ambient_level(const SfxAmbient& a, f32 people);

// A sound the city calls for: an event id from sfx.json, at a tile when `at` (positional events).
struct SfxCue {
  const char* event = nullptr;
  bool at = false;
  f32 x = 0, y = 0;  // tile coordinates
};

// Watches the world between frames and turns what changed into cues: new fires and collapses (at the
// building), fighting (at the soldier nearest the view), letters, important alerts, raids, cohorts
// marching off or coming home, festivals, trade income and payments, and a won mission. Reads the world,
// never changes it.
class SfxWatch {
 public:
  static constexpr int kMaxCues = 8;
  // Starts from w as it is now: what is already there makes no sound.
  void reset(const sim::World& w);
  // Compares w with the last look; writes up to kMaxCues cues and returns how many. `fx, fy`: the tile at
  // the centre of the view (fighting is heard from the nearest soldier).
  int poll(const sim::World& w, i64 coin_min, f32 fx, f32 fy, SfxCue* out);

 private:
  Tick last_now_ = 0;
  u16 last_message_ = 0;
  u8 mission_ = 0;
  bool festival_ = false;
  u32 away_ = 0;  // city units leaving on or away on campaign
  i64 money_[econ::kMoneyCount] = {};
};

}  // namespace urbs::game
