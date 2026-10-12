#include "game/sfx.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <exception>

#include "core/log.hpp"
#include "render/iso.hpp"
#include "sim/world.hpp"

namespace urbs::game {

namespace {
constexpr f32 kPollSeconds = 0.25f;    // how often the world is compared with the last look
constexpr f32 kAmbientSeconds = 0.5f;  // how often the people on screen are counted
}  // namespace

void SoundDirector::init(const std::string& data_root, platform::Audio* audio) {
  audio_ = audio;
  if (!audio_ || !audio_->enabled()) return;  // no device: stay silent, load nothing
  try {
    sfx_.load(data_root + "data/defs/sfx.json");
  } catch (const std::exception& e) {
    URBS_LOG_WARN("sound effects disabled: {}", e.what());
    return;
  }
  const auto file = [&](const std::string& id) { return data_root + "assets/sfx/" + id + ".ogg"; };
  clips_.resize(sfx_.events.size());
  for (size_t k = 0; k < sfx_.events.size(); ++k)
    for (const std::string& f : sfx_.events[k].files) {
      const int c = audio_->load_clip(file(f));
      if (c >= 0) clips_[k].push_back(c);
    }
  if (!sfx_.ambient.file.empty()) ambient_clip_ = audio_->load_clip(file(sfx_.ambient.file));
  last_at_.assign(sfx_.events.size(), -1e9f);
  last_variant_.assign(sfx_.events.size(), -1);
  ok_ = true;
}

void SoundDirector::trigger(int ev, f32 scale, f32 pan) {
  if (!ok_ || ev < 0) return;
  const size_t k = static_cast<size_t>(ev);
  const SfxEvent& e = sfx_.events[k];
  const std::vector<int>& v = clips_[k];
  if (v.empty() || clock_ - last_at_[k] < e.cooldown) return;
  int playing = 0;
  for (int c : v) playing += audio_->sfx_playing(c);
  if (playing >= e.max_playing) return;
  // A random variant, never the same one twice in a row.
  const int last = last_variant_[k], count = static_cast<int>(v.size());
  int pick = std::uniform_int_distribution<int>(0, count - (count > 1 && last >= 0 ? 2 : 1))(rng_);
  if (count > 1 && last >= 0 && pick >= last) ++pick;
  last_variant_[k] = pick;
  last_at_[k] = clock_;
  const f32 gain = e.gain * scale * (1.0f + jitter(e.gain_var));
  const f32 pitch = 1.0f + jitter(e.pitch_var);  // a playback-rate spread: 0.05 = +-5% (under a semitone)
  audio_->play_sfx(v[static_cast<size_t>(pick)], gain, pan, pitch);
}

void SoundDirector::trigger_at(int ev, f32 tx, f32 ty, const sim::World& w, const render::Camera& cam) {
  namespace iso = render::iso;
  const Vec2 p = iso::from_tile(tx, ty, iso::level_at(w.map, static_cast<i32>(tx), static_cast<i32>(ty)));
  const f32 s = cam.scale();
  const f32 half_w = cam.screen.x / (2 * s), half_h = cam.screen.y / (2 * s);  // iso px
  const SfxPlacement at = place_sound(sfx_.positional, (p.x - cam.center.x) / half_w, (p.y - cam.center.y) / half_h);
  if (at.gain > 0) trigger(ev, at.gain, at.pan);
}

void SoundDirector::update(f32 dt, const sim::World* w, const render::Camera& cam) {
  if (!ok_) return;
  clock_ += dt;
  if (!w) {
    watching_ = nullptr;
    ambient_ = 0;
    audio_->set_ambient(ambient_clip_, 0, sfx_.ambient.fade_s);
    return;
  }
  if (w != watching_) {  // a new city, or back from the menus: what is already there makes no sound
    watch_.reset(*w);
    watching_ = w;
    next_poll_ = clock_ + kPollSeconds;
  }
  const Vec2 focus = render::iso::to_tile(cam.center);
  if (clock_ >= next_poll_) {
    next_poll_ = clock_ + kPollSeconds;
    SfxCue cues[SfxWatch::kMaxCues];
    const int n = watch_.poll(*w, sfx_.coin_min, focus.x, focus.y, cues);
    for (int i = 0; i < n; ++i) {
      const int ev = sfx_.index(cues[i].event);
      if (ev < 0) continue;
      if (std::string_view(cues[i].event) != "battle")
        URBS_LOG_INFO("sfx cue: {}", cues[i].event);  // fights: too often
      if (cues[i].at && sfx_.events[static_cast<size_t>(ev)].positional)
        trigger_at(ev, cues[i].x, cues[i].y, *w, cam);
      else
        trigger(ev, 1.0f, 0.0f);
    }
  }
  if (ambient_clip_ >= 0 && clock_ >= next_ambient_) {
    next_ambient_ = clock_ + kAmbientSeconds;
    // People living and working in the buildings on screen (a little margin around it).
    const f32 s = cam.scale();
    const f32 half_w = cam.screen.x / (2 * s) * 1.2f, half_h = cam.screen.y / (2 * s) * 1.2f;
    f32 people = 0;
    w->buildings.for_each([&](BuildingId, const sim::Building& b) {
      if (b.residents == 0 && b.present == 0) return;
      const Vec2 p = render::iso::from_tile(static_cast<f32>(b.x) + static_cast<f32>(b.w) * 0.5f,
                                            static_cast<f32>(b.y) + static_cast<f32>(b.h) * 0.5f);
      if (std::fabs(p.x - cam.center.x) <= half_w && std::fabs(p.y - cam.center.y) <= half_h)
        people += static_cast<f32>(b.residents + b.present);
    });
    ambient_ = ambient_level(sfx_.ambient, people);
    audio_->set_ambient(ambient_clip_, sfx_.ambient.gain * ambient_, sfx_.ambient.fade_s);
  }
}

void SoundDirector::ui_frame() {
  if (!ok_) return;
  // A widget became active this frame (pressed with the mouse or the keyboard). Pressing a window's
  // empty space or its scrollbar also activates an id: those stay quiet.
  const ImGuiContext& g = *ImGui::GetCurrentContext();
  if (g.LastActiveId == 0 || g.LastActiveIdTimer != 0.0f) return;
  if (ImGuiWindow* hw = g.HoveredWindow) {
    if (g.LastActiveId == hw->MoveId || g.LastActiveId == ImGui::GetWindowScrollbarID(hw, ImGuiAxis_X) ||
        g.LastActiveId == ImGui::GetWindowScrollbarID(hw, ImGuiAxis_Y))
      return;
  }
  play("ui_click");
}

}  // namespace urbs::game
