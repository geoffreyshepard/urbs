#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "core/types.hpp"

struct stb_vorbis;

namespace urbs::platform {

// Music output: two crossfading music voices and one stinger voice that ducks the music.
// Decoding (stb_vorbis) and mixing run on the main thread in pump(), which keeps a short buffer
// queued on an SDL audio stream, so no state is shared with the audio thread.
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

  SDL_AudioStream* stream_ = nullptr;
  Voice music_[2], stinger_;
  int active_ = 0;  // index of the current music voice
  std::string current_;
  f32 volume_ = 0.7f;
  f32 duck_ = 1, duck_target_ = 1;
  std::vector<f32> mix_, dec_;  // scratch buffers, sized once in init()
};

}  // namespace urbs::platform
