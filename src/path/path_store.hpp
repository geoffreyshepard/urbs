#pragma once
#include <vector>

#include "core/math.hpp"
#include "core/types.hpp"
#include "path/nav.hpp"

namespace urbs::save {
class Writer;
class Reader;
}  // namespace urbs::save

namespace urbs::path {

inline constexpr u32 kNoHandle = 0xFFFFFFFFu;

// Shared, ref-counted tile paths with cumulative travel time, plus a (src,dst) cache.
// Paths are symmetric: a path can be walked in reverse with the same timing.
// Fixed capacity: arena tiles, path records, cache slots. Compacts in place when fragmented.
class PathStore {
 public:
  void init(u32 arena_tiles, u32 max_paths, u32 cache_slots_pow2);

  // Adds a path (tiles s..g). Returns handle, or kNoHandle if out of space even after eviction.
  u32 add(const i32* tiles, u32 n, const NavGrid& nav, Tick now, u64 key);
  void ref(u32 h) { ++recs_[h].refs; }
  void unref(u32 h);

  // Cache lookup for (src,dst). Sets *reversed if the cached path runs dst->src.
  u32 lookup(i32 src, i32 dst, const NavGrid& nav, Tick now, bool* reversed);
  static u64 key(i32 src, i32 dst) { return (static_cast<u64>(static_cast<u32>(src)) << 32) | static_cast<u32>(dst); }

  [[nodiscard]] u32 length(u32 h) const { return recs_[h].len; }
  [[nodiscard]] u32 total_ticks(u32 h) const { return (recs_[h].total + 9) / 10; }
  [[nodiscard]] Vec2i tile(u32 h, u32 i) const {
    u32 p = tiles_[recs_[h].off + i];
    return {static_cast<i32>(p & 0xFFFF), static_cast<i32>(p >> 16)};
  }
  // Position (world tile units, fractional) after `elapsed_tenths` tenths of a tick of walking.
  [[nodiscard]] Vec2 position(u32 h, bool reversed, u32 elapsed_tenths) const;
  // Direction index (0 N, 1 E, 2 S, 3 W) of travel at that time.
  [[nodiscard]] int facing(u32 h, bool reversed, u32 elapsed_tenths) const;

  void serialize(save::Writer& w) const;
  void deserialize(save::Reader& r);

  [[nodiscard]] u32 live_paths() const { return live_; }
  [[nodiscard]] u32 arena_used() const { return arena_top_; }
  [[nodiscard]] u32 cache_size() const { return cache_count_; }
  [[nodiscard]] u64 hits() const { return hits_; }
  [[nodiscard]] u64 misses() const { return misses_; }

 private:
  struct Rec {
    u32 off = 0, len = 0;
    u32 total = 0;  // tenths of ticks
    u32 refs = 0;
    u32 coff = 0;
    u16 ccount = 0;
    u8 alive = 0, cached = 0;
    Tick created = 0, last_used = 0;
    u64 key = 0;
  };
  struct Slot {
    u64 key;
    u32 handle;  // kNoHandle = empty, kTomb = deleted
  };
  static constexpr u32 kTomb = 0xFFFFFFFEu;

  bool valid(const Rec& r, const NavGrid& nav) const;
  void cache_insert(u64 key, u32 h);
  void cache_erase(u64 key);
  void release(u32 h);
  void evict(Tick now);
  void compact();
  [[nodiscard]] u32 find_index(u32 h, u32 elapsed, bool reversed) const;

  std::vector<u32> tiles_;  // packed x | y << 16
  std::vector<u32> cum_;    // cumulative cost at each tile (tenths of ticks)
  std::vector<u16> chunks_;
  std::vector<Rec> recs_;
  std::vector<u32> free_recs_;
  std::vector<u32> order_;  // compaction scratch
  std::vector<Slot> cache_;
  u32 cache_mask_ = 0, cache_count_ = 0, cache_tombs_ = 0;
  u32 arena_top_ = 0, chunk_top_ = 0, live_ = 0;
  u64 hits_ = 0, misses_ = 0;
};

}  // namespace urbs::path
