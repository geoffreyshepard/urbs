#pragma once
#include "core/types.hpp"

namespace urbs::rng {

// SplitMix64: used to derive independent stream seeds.
constexpr u64 splitmix64(u64& x) {
  u64 z = (x += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

constexpr u64 hash64(u64 x) {
  u64 s = x;
  return splitmix64(s);
}

// Deterministic per-system random stream (xoshiro256**). The only source of randomness in the sim.
class Stream {
 public:
  Stream() : Stream(0, 0) {}
  Stream(u64 world_seed, u64 stream_id) { seed(world_seed, stream_id); }

  void seed(u64 world_seed, u64 stream_id) {
    u64 x = world_seed ^ (stream_id * 0xD1B54A32D192ED03ull);
    for (auto& v : s_) v = splitmix64(x);
  }

  u64 next_u64() {
    const u64 result = rotl(s_[1] * 5, 7) * 9;
    const u64 t = s_[1] << 17;
    s_[2] ^= s_[0];
    s_[3] ^= s_[1];
    s_[1] ^= s_[2];
    s_[0] ^= s_[3];
    s_[2] ^= t;
    s_[3] = rotl(s_[3], 45);
    return result;
  }
  u32 next_u32() { return static_cast<u32>(next_u64() >> 32); }

  // Uniform in [0, n). n must be > 0.
  u32 below(u32 n) { return static_cast<u32>((static_cast<u64>(next_u32()) * n) >> 32); }
  // Uniform in [lo, hi].
  i32 range(i32 lo, i32 hi) { return lo + static_cast<i32>(below(static_cast<u32>(hi - lo + 1))); }
  // Uniform in [0, 1).
  f32 unit() { return static_cast<f32>(next_u32() >> 8) * (1.0f / 16777216.0f); }
  bool chance(f32 p) { return unit() < p; }

  struct State {
    u64 s[4];
  };
  [[nodiscard]] State state() const { return {{s_[0], s_[1], s_[2], s_[3]}}; }
  void set_state(const State& st) {
    for (int i = 0; i < 4; ++i) s_[i] = st.s[i];
  }

 private:
  static constexpr u64 rotl(u64 x, int k) { return (x << k) | (x >> (64 - k)); }
  u64 s_[4]{};
};

// Stable stream ids per system. Never reorder; append only (saved state depends on them).
enum class StreamId : u64 {
  World = 1,
  Terrain,
  Citizens,
  Decisions,
  Immigration,
  Lifecycle,
  Economy,
  Trade,
  Threats,
  Religion,
  Military,
  Events,
  Names,
  Count
};

}  // namespace urbs::rng
