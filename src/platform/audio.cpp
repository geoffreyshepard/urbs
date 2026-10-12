#include "platform/audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

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
constexpr f32 kEdgeFrames = 32;  // decoded one-shots ramp in and out over this many frames: never a click
// Music and effects together can pass full scale: above kKnee the master bends smoothly toward 1.
constexpr f32 kKnee = 0.9f;
inline f32 soft_clip(f32 x) {
  const f32 a = std::fabs(x);
  if (a <= kKnee) return x;
  return std::copysign(kKnee + (1.0f - kKnee) * std::tanh((a - kKnee) / (1.0f - kKnee)), x);
}
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
  clips_.clear();
  for (SfxVoice& v : sfx_) v = SfxVoice{};
  ambient_ = AmbientVoice{};
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

int Audio::load_clip(const std::string& path) {
  if (!stream_) return -1;
  for (size_t k = 0; k < clips_.size(); ++k)
    if (clips_[k].path == path) return static_cast<int>(k);
  int channels = 0, rate = 0;
  short* pcm = nullptr;
  const int frames = stb_vorbis_decode_filename(path.c_str(), &channels, &rate, &pcm);
  if (frames <= 0 || !pcm || channels < 1 || channels > 2) {
    URBS_LOG_WARN("cannot load sound {}", path);
    free(pcm);  // NOLINT(cppcoreguidelines-no-malloc): stb_vorbis allocates with malloc
    return -1;
  }
  if (rate != kRate) URBS_LOG_WARN("sound {}: {} Hz, expected {} Hz", path, rate, kRate);
  Clip c;
  c.path = path;
  c.channels = channels;
  c.frames = static_cast<u32>(frames);
  c.data.assign(pcm, pcm + static_cast<size_t>(frames) * static_cast<size_t>(channels));
  free(pcm);  // NOLINT(cppcoreguidelines-no-malloc)
  clips_.push_back(std::move(c));
  return static_cast<int>(clips_.size()) - 1;
}

void Audio::play_sfx(int clip, f32 gain, f32 pan, f32 pitch) {
  if (!stream_ || clip < 0 || clip >= static_cast<int>(clips_.size()) || gain <= 0) return;
  // A free voice, else the one closest to its end (the least of it is lost).
  SfxVoice* v = nullptr;
  f32 best = -1;
  for (SfxVoice& s : sfx_) {
    if (s.clip < 0) {
      v = &s;
      break;
    }
    const f32 done = static_cast<f32>(s.pos) / static_cast<f32>(clips_[static_cast<size_t>(s.clip)].frames);
    if (done > best) {
      best = done;
      v = &s;
    }
  }
  // Equal-power pan, with the centre at unity on both sides.
  const f32 th = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * 0.785398f;
  v->clip = clip;
  v->pos = 0;
  v->rate = std::clamp(pitch, 0.5f, 2.0f);
  v->left = gain * std::cos(th) * 1.41421f;
  v->right = gain * std::sin(th) * 1.41421f;
}

int Audio::sfx_playing(int clip) const {
  int n = 0;
  for (const SfxVoice& v : sfx_) n += v.clip == clip && clip >= 0;
  return n;
}

void Audio::set_ambient(int clip, f32 gain, f32 fade_s) {
  if (!stream_) return;
  const f32 step = 1.0f / (std::max(fade_s, 0.01f) * kRate);
  if (clip < 0 || clip >= static_cast<int>(clips_.size()) || gain <= 0) {
    ambient_.target = 0;  // fade out whatever plays; render_sfx frees the voice once silent
    ambient_.step = step;
    return;
  }
  if (clip != ambient_.clip) {
    ambient_ = AmbientVoice{};
    ambient_.clip = clip;
  }
  ambient_.target = gain;
  ambient_.step = step;
}

void Audio::render_sfx(int n) {
  const f32 vol = sfx_volume_ / 32768.0f;  // clips hold 16-bit samples
  for (SfxVoice& v : sfx_) {
    if (v.clip < 0) continue;
    const Clip& c = clips_[static_cast<size_t>(v.clip)];
    const i16* src = c.data.data();
    const auto d = [src](u32 i) { return static_cast<f32>(src[i]); };
    const u32 last = c.frames - 1;
    for (int i = 0; i < n; ++i) {
      const u32 k = static_cast<u32>(v.pos);
      if (k >= last) {
        v.clip = -1;
        break;
      }
      const f32 frac = static_cast<f32>(v.pos - static_cast<f64>(k));
      // Ramps at both ends of the clip, whatever the encoder left there.
      const f32 edge = std::min({1.0f, static_cast<f32>(k) / kEdgeFrames, static_cast<f32>(last - k) / kEdgeFrames});
      f32 l, r;
      if (c.channels == 2) {
        l = d(2 * k) + (d(2 * k + 2) - d(2 * k)) * frac;
        r = d(2 * k + 1) + (d(2 * k + 3) - d(2 * k + 1)) * frac;
      } else {
        l = r = d(k) + (d(k + 1) - d(k)) * frac;
      }
      const size_t o = static_cast<size_t>(2 * i);
      mix_[o] += l * v.left * edge * vol;
      mix_[o + 1] += r * v.right * edge * vol;
      v.pos += v.rate;
    }
  }
  AmbientVoice& a = ambient_;
  if (a.clip < 0) return;
  const Clip& c = clips_[static_cast<size_t>(a.clip)];
  for (int i = 0; i < n; ++i) {
    if (a.gain < a.target)
      a.gain = std::min(a.target, a.gain + a.step);
    else if (a.gain > a.target)
      a.gain = std::max(a.target, a.gain - a.step);
    const size_t o = static_cast<size_t>(2 * i);
    const f32 g = a.gain * vol;
    if (c.channels == 2) {
      mix_[o] += static_cast<f32>(c.data[2 * static_cast<size_t>(a.pos)]) * g;
      mix_[o + 1] += static_cast<f32>(c.data[2 * static_cast<size_t>(a.pos) + 1]) * g;
    } else {
      mix_[o] += static_cast<f32>(c.data[a.pos]) * g;
      mix_[o + 1] += static_cast<f32>(c.data[a.pos]) * g;
    }
    if (++a.pos >= c.frames) a.pos = 0;  // the file is a seamless loop
  }
  if (a.target == 0 && a.gain == 0) a.clip = -1;
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
    render_sfx(n);
    for (int i = 0; i < 2 * n; ++i) mix_[static_cast<size_t>(i)] = soft_clip(mix_[static_cast<size_t>(i)]);
    SDL_PutAudioStreamData(stream_, mix_.data(), n * 8);
    need -= n;
  }
}

}  // namespace urbs::platform
