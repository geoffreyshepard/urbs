#pragma once
#include <SDL3/SDL.h>

#include <array>
#include <string>
#include <vector>

#include "core/types.hpp"

struct stb_vorbis;

namespace urbs::platform {

// Audio output: two crossfading music voices and one stinger voice that ducks the music, plus sound
// effects: short clips decoded into memory once (load_clip) and played on a fixed pool of voices, and
// one looping ambient voice. Decoding and mixing run on the main thread in pump(), which keeps a short
// buffer queued on an SDL audio stream, so no state is shared with the audio thread.
class Audio {
 public:
  // Opens the default playback device. Returns false (and stays silent) when there is none.
  bool init();
  void shutdown();
  // Call once per frame.
  void pump();

  // Crossfades to `path` (an .ogg; empty = fade to silence) over `fade_s` seconds. A loop
  // repeats seamlessly. Asking for the track that is still playing does nothing.
  void play_music(const std::string& path, bool loop, f32 fade_s);
  // One-shot over the music, which ducks by `duck_db` while it plays.
  void play_stinger(const std::string& path, f32 duck_db);
  void set_volume(f32 v) { volume_ = v; }

  // Decodes a short .ogg into memory (load time; the same path twice returns the same clip). Returns a
  // clip handle, or -1 when there is no device or the file cannot be read.
  int load_clip(const std::string& path);
  // Plays a clip once: gain 0..1, pan -1 (left)..1 (right), pitch as a playback rate (1 = as recorded).
  // When every voice is busy, the one nearest its end is cut. Allocates nothing.
  void play_sfx(int clip, f32 gain, f32 pan, f32 pitch);
  // Voices sounding this clip now.
  [[nodiscard]] int sfx_playing(int clip) const;
  // Loops `clip` on the ambient voice and ramps its gain to `gain` over `fade_s` (gain 0 fades it out).
  void set_ambient(int clip, f32 gain, f32 fade_s);
  void set_sfx_volume(f32 v) { sfx_volume_ = v; }

  [[nodiscard]] bool enabled() const { return stream_ != nullptr; }
  // The current music track is still sounding (not finished, not fading out).
  [[nodiscard]] bool music_playing() const;
  [[nodiscard]] bool stinger_playing() const { return stinger_.dec != nullptr; }
  [[nodiscard]] const std::string& current_music() const { return current_; }

 private:
  struct Voice {
    stb_vorbis* dec = nullptr;
    std::string path;
    bool loop = false;
    int channels = 2;
    f32 gain = 0, target = 0, step = 0;  // per-frame linear ramp toward target
  };
  bool open(Voice& v, const std::string& path, bool loop);
  static void close(Voice& v);
  // Adds n frames of v (times its gain ramp and `scale`) into mix_.
  void render(Voice& v, int n, f32 scale);
  void render_sfx(int n);

  struct Clip {
    std::string path;
    std::vector<i16> data;  // interleaved; 16-bit halves the memory of every effect held decoded
    int channels = 1;
    u32 frames = 0;
  };
  struct SfxVoice {
    int clip = -1;  // -1 = free
    f64 pos = 0;    // frames (fractional when pitched)
    f32 rate = 1, left = 0, right = 0;
  };
  struct AmbientVoice {
    int clip = -1;
    u32 pos = 0;
    f32 gain = 0, target = 0, step = 0;
  };
  static constexpr int kSfxVoices = 16;

  SDL_AudioStream* stream_ = nullptr;
  Voice music_[2], stinger_;
  int active_ = 0;  // index of the current music voice
  std::string current_;
  f32 volume_ = 0.7f, sfx_volume_ = 0.7f;
  std::vector<Clip> clips_;
  std::array<SfxVoice, kSfxVoices> sfx_{};
  AmbientVoice ambient_;
  f32 duck_ = 1, duck_target_ = 1;
  std::vector<f32> mix_, dec_;  // scratch buffers, sized once in init()
};

}  // namespace urbs::platform
