#pragma once
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "game/music_rules.hpp"
#include "platform/audio.hpp"

namespace urbs::sim {
struct World;
}

namespace urbs::game {

enum class MusicContext : u8 { Menu, Map, Setup, Playing };

// Chooses what plays: menu and map themes out of a mission; in a mission, a shuffled city
// playlist with silences, interrupted by tension, battle, disaster and festival music, plus
// stingers for events. Reads the world, never changes it (docs/11-music.md).
class MusicDirector {
 public:
  void init(const std::string& data_root, platform::Audio* audio);
  // Once per frame. `end_screen`: the mission-over panel is showing.
  void update(f32 dt, MusicContext ctx, const sim::World* w, bool end_screen);
  // A new or loaded city starts playing.
  void mission_started(const sim::World& w);
  // The career rank went up: a fanfare plays on the province map.
  void promoted() { promotion_ = true; }

  [[nodiscard]] const char* mood_name() const;
  [[nodiscard]] const std::string& city_track() const { return city_track_; }

 private:
  void meta(std::string_view use);
  void playing(const sim::World& w, bool end_screen);
  void enter(Mood m);
  void scan_alerts(const sim::World& w);
  void stinger(std::string_view event);
  // Starts t (crossfading); for a play-once track, returns whether it has finished.
  bool play_once(const MusicTrack* t, f32 fade);
  [[nodiscard]] std::string file(const std::string& id) const { return root_ + "assets/music/" + id + ".ogg"; }
  f32 uniform(f32 lo, f32 hi) { return std::uniform_real_distribution<f32>(lo, hi)(rng_); }

  platform::Audio* audio_ = nullptr;
  MusicManifest m_;
  std::string root_;
  std::mt19937 rng_{std::random_device{}()};  // presentation only: the sim never sees it
  bool ok_ = false;
  MusicContext ctx_ = MusicContext::Menu;
  bool ctx_seen_ = false;
  f32 clock_ = 0;  // real seconds

  // Out of a mission: the theme plays, then a silence, then again.
  std::string meta_track_;
  bool meta_started_ = false;
  f32 meta_wait_until_ = -1;

  // In a mission.
  MusicSignals sig_;
  f32 next_poll_ = 0;
  Mood mood_ = Mood::City;
  f32 hold_until_ = 0;  // battle and disaster music outlast their cause a little
  f32 disaster_since_ = 0;
  bool aftermath_due_ = false;  // a long disaster ended: the lament plays once
  bool festival_played_ = false;
  bool once_started_ = false;
  std::string city_track_, last_city_;
  f32 silence_until_ = 0;
  Tick last_alert_ = 0;
  u8 last_state_ = 0;  // sim::MissionState
  bool promotion_ = false;
  std::vector<std::pair<std::string, f32>> stinger_at_;  // event -> clock of last play
};

}  // namespace urbs::game
