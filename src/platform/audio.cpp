#include "platform/audio.hpp"

#include <algorithm>
#include <cmath>

#include "core/log.hpp"

#define STB_VORBIS_HEADER_ONLY
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif
#include <stb_vorbis.c>  // NOLINT(bugprone-suspicious-include): single-file library
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace urbs::platform {

namespace {
constexpr int kRate = 44100;
constexpr int kChunk = 1024;             // frames mixed per step
constexpr int kQueueFrames = kRate / 4;  // keep ~250 ms queued: survives a slow frame
constexpr f32 kDuckRamp = 1.0f / (0.15f * kRate);
}  // namespace

bool Audio::init() {
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    URBS_LOG_WARN("audio disabled: {}", SDL_GetError());
    return false;
  }
  const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, kRate};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (!stream_) {
    URBS_LOG_WARN("audio disabled: {}", SDL_GetError());
    return false;
  }
  mix_.assign(static_cast<size_t>(kChunk) * 2, 0.0f);
  dec_.assign(static_cast<size_t>(kChunk) * 2, 0.0f);
  SDL_ResumeAudioStreamDevice(stream_);
  return true;
}

void Audio::shutdown() {
  close(music_[0]);
  close(music_[1]);
  close(stinger_);
  if (stream_) SDL_DestroyAudioStream(stream_);
  stream_ = nullptr;
}

bool Audio::open(Voice& v, const std::string& path, bool loop) {
  close(v);
  int err = 0;
  v.dec = stb_vorbis_open_filename(path.c_str(), &err, nullptr);
  if (!v.dec) {
    URBS_LOG_WARN("cannot open music {} (stb_vorbis error {})", path, err);
    return false;
  }
  const stb_vorbis_info info = stb_vorbis_get_info(v.dec);
  if (info.sample_rate != static_cast<unsigned>(kRate) || info.channels < 1 || info.channels > 2) {
    URBS_LOG_WARN("music {}: {} Hz, {} channels; expected {} Hz mono or stereo", path, info.sample_rate, info.channels,
                  kRate);
    if (info.channels < 1 || info.channels > 2) {
      close(v);
      return false;
    }
  }
  v.channels = info.channels;
  v.path = path;
  v.loop = loop;
  return true;
}

void Audio::close(Voice& v) {
  if (v.dec) stb_vorbis_close(v.dec);
  v = Voice{};
}

void Audio::play_music(const std::string& path, bool loop, f32 fade_s) {
  // Asking again for the track that is sounding does nothing; a finished one starts over.
  if (!stream_ || (path == current_ && (path.empty() || music_playing()))) return;
  const f32 step = 1.0f / (std::max(fade_s, 0.01f) * kRate);
  for (Voice& v : music_) {
    v.target = 0;
    v.step = step;
  }
  current_ = path;
  if (path.empty()) return;
  // Use the quieter voice; whatever it still holds is cut (it was already fading out).
  const int k = music_[0].dec == nullptr   ? 0
                : music_[1].dec == nullptr ? 1
                                           : (music_[0].gain <= music_[1].gain ? 0 : 1);
  if (!open(music_[k], path, loop)) return;
  music_[k].gain = 0;
  music_[k].target = 1;
  music_[k].step = step;
  active_ = k;
}

void Audio::play_stinger(const std::string& path, f32 duck_db) {
  if (!stream_ || !open(stinger_, path, false)) return;
  stinger_.gain = stinger_.target = 1;
  duck_target_ = std::pow(10.0f, duck_db / 20.0f);
}

bool Audio::music_playing() const {
  const Voice& v = music_[active_];
  return v.dec != nullptr && v.target > 0 && !current_.empty();
}

void Audio::render(Voice& v, int n, f32 scale) {
  int done = 0, empty = 0;
  while (done < n && v.dec) {
    const int want = n - done;
    int got;
    if (v.channels == 2) {
      got = stb_vorbis_get_samples_float_interleaved(v.dec, 2, dec_.data(), want * 2);
    } else {
      got = stb_vorbis_get_samples_float_interleaved(v.dec, 1, dec_.data(), want);
      for (int i = got - 1; i >= 0; --i)
        dec_[static_cast<size_t>(2 * i)] = dec_[static_cast<size_t>(2 * i + 1)] = dec_[static_cast<size_t>(i)];
    }
    for (int i = 0; i < got; ++i) {
      if (v.gain < v.target)
        v.gain = std::min(v.target, v.gain + v.step);
      else if (v.gain > v.target)
        v.gain = std::max(v.target, v.gain - v.step);
      const f32 g = v.gain * scale;
      const size_t o = static_cast<size_t>(2 * (done + i));
      mix_[o] += dec_[static_cast<size_t>(2 * i)] * g;
      mix_[o + 1] += dec_[static_cast<size_t>(2 * i + 1)] * g;
    }
    done += got;
    if (got < want) {
      if (v.loop && (got > 0 || ++empty < 2)) {  // an empty loop would spin forever
        stb_vorbis_seek_start(v.dec);
      } else {
        close(v);
        break;
      }
    }
    if (v.target == 0 && v.gain == 0) {
      close(v);
      break;
    }
  }
}

void Audio::pump() {
  if (!stream_) return;
  int need = (kQueueFrames * 8 - SDL_GetAudioStreamQueued(stream_)) / 8;
  while (need > 0) {
    const int n = std::min(need, kChunk);
    std::fill(mix_.begin(), mix_.begin() + 2 * n, 0.0f);
    // Music ducks under a stinger; the duck ramps in a mix step's worth at a time.
    if (!stinger_.dec) duck_target_ = 1;
    duck_ = duck_ < duck_target_ ? std::min(duck_target_, duck_ + kDuckRamp * static_cast<f32>(n))
                                 : std::max(duck_target_, duck_ - kDuckRamp * static_cast<f32>(n));
    for (Voice& v : music_) render(v, n, volume_ * duck_);
    render(stinger_, n, volume_);
    SDL_PutAudioStreamData(stream_, mix_.data(), n * 8);
    need -= n;
  }
}

}  // namespace urbs::platform
