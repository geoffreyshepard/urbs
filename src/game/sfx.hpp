#pragma once
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "game/sfx_rules.hpp"
#include "platform/audio.hpp"
#include "render/camera.hpp"

namespace urbs::sim {
struct World;
}

namespace urbs::game {

// Plays the sound effects named in data/defs/sfx.json: interface sounds when asked (play), the city's
// sounds from what changed in the world (SfxWatch), and the ambient bed, whose level follows the people
// on screen. Reads the world, never changes it (docs/11-music.md, "Sound effects").
class SoundDirector {
 public:
  void init(const std::string& data_root, platform::Audio* audio);
  // Once per frame. `w`: the city being played, or nullptr out of a mission (the ambient fades out).
  void update(f32 dt, const sim::World* w, const render::Camera& cam);
  // Plays an event at the centre (interface sounds). Dropped while its cooldown runs.
  void play(std::string_view event) { trigger(sfx_.index(event), 1.0f, 0.0f); }
  // Call after ImGui::Render(): a click when a widget was pressed this frame.
  void ui_frame();
  // A new or loaded city: start watching it afresh (its old letters and alerts make no sound).
  void city_changed() { watching_ = nullptr; }

 private:
  // Plays variant files of event `ev` (index in the manifest) at `scale` x its gain, panned.
  void trigger(int ev, f32 scale, f32 pan);
  // A positional cue at tile (tx, ty): heard only on or near the screen, quieter further off.
  void trigger_at(int ev, f32 tx, f32 ty, const sim::World& w, const render::Camera& cam);
  f32 jitter(f32 spread) { return spread > 0 ? std::uniform_real_distribution<f32>(-spread, spread)(rng_) : 0.0f; }

  platform::Audio* audio_ = nullptr;
  SfxManifest sfx_;
  bool ok_ = false;
  std::vector<std::vector<int>> clips_;  // per event: the clip handle of each variant
  std::vector<f32> last_at_;             // per event: clock of the last play
  std::vector<int> last_variant_;
  int ambient_clip_ = -1;
  std::mt19937 rng_{std::random_device{}()};  // presentation only: the sim never sees it
  f32 clock_ = 0, next_poll_ = 0, next_ambient_ = 0, ambient_ = 0;
  SfxWatch watch_;
  const sim::World* watching_ = nullptr;
};

}  // namespace urbs::game
