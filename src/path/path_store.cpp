#include "path/path_store.hpp"

#include <algorithm>

#include "core/rng.hpp"

namespace urbs::path {

void PathStore::init(u32 arena_tiles, u32 max_paths, u32 cache_slots_pow2) {
  tiles_.assign(arena_tiles, 0);
  cum_.assign(arena_tiles, 0);
  chunks_.assign(arena_tiles / 4 + 1024, 0);
  recs_.assign(max_paths, Rec{});
  free_recs_.clear();
  free_recs_.reserve(max_paths);
  for (u32 i = max_paths; i-- > 0;) free_recs_.push_back(i);
  order_.assign(max_paths, 0);
  cache_.assign(cache_slots_pow2, Slot{0, kNoHandle});
  cache_mask_ = cache_slots_pow2 - 1;
  cache_count_ = cache_tombs_ = 0;
  arena_top_ = chunk_top_ = live_ = 0;
  hits_ = misses_ = 0;
}

bool PathStore::valid(const Rec& r, const NavGrid& nav) const {
  for (u32 i = 0; i < r.ccount; ++i)
    if (nav.chunk_changed[chunks_[r.coff + i]] >= r.created) return false;
  return true;
}

void PathStore::cache_insert(u64 key, u32 h) {
  if ((cache_count_ + cache_tombs_ + 1) * 4 > (cache_mask_ + 1) * 3) {
    // Rebuild without tombstones (in place: clear and re-insert live cached paths).
    for (auto& s : cache_) s = Slot{0, kNoHandle};
    cache_count_ = cache_tombs_ = 0;
    for (u32 i = 0; i < recs_.size(); ++i) {
      if (!recs_[i].alive || !recs_[i].cached) continue;
      u32 p = static_cast<u32>(rng::hash64(recs_[i].key)) & cache_mask_;
      while (cache_[p].handle != kNoHandle) p = (p + 1) & cache_mask_;
      cache_[p] = {recs_[i].key, i};
      ++cache_count_;
    }
    if ((cache_count_ + 1) * 4 > (cache_mask_ + 1) * 3) {
      recs_[h].cached = 0;  // table full of live entries: don't cache this one
      return;
    }
  }
  u32 p = static_cast<u32>(rng::hash64(key)) & cache_mask_;
  while (cache_[p].handle != kNoHandle && cache_[p].handle != kTomb) p = (p + 1) & cache_mask_;
  if (cache_[p].handle == kTomb) --cache_tombs_;
  cache_[p] = {key, h};
  ++cache_count_;
}

void PathStore::cache_erase(u64 key) {
  u32 p = static_cast<u32>(rng::hash64(key)) & cache_mask_;
  while (cache_[p].handle != kNoHandle) {
    if (cache_[p].handle != kTomb && cache_[p].key == key) {
      u32 h = cache_[p].handle;
      cache_[p].handle = kTomb;
      --cache_count_;
      ++cache_tombs_;
      recs_[h].cached = 0;
      if (recs_[h].refs == 0) release(h);
      return;
    }
    p = (p + 1) & cache_mask_;
  }
}

void PathStore::release(u32 h) {
  recs_[h].alive = 0;
  free_recs_.push_back(h);
  --live_;
}

void PathStore::unref(u32 h) {
  Rec& r = recs_[h];
  if (r.refs > 0) --r.refs;
  if (r.refs == 0 && !r.cached && r.alive) release(h);
}

u32 PathStore::lookup(i32 src, i32 dst, const NavGrid& nav, Tick now, bool* reversed) {
  for (int pass = 0; pass < 2; ++pass) {
    u64 k = pass == 0 ? key(src, dst) : key(dst, src);
    u32 p = static_cast<u32>(rng::hash64(k)) & cache_mask_;
    while (cache_[p].handle != kNoHandle) {
      if (cache_[p].handle != kTomb && cache_[p].key == k) {
        u32 h = cache_[p].handle;
        if (!valid(recs_[h], nav)) {
          cache_erase(k);
          break;
        }
        recs_[h].last_used = now;
        if (reversed) *reversed = pass == 1;
        ++hits_;
        return h;
      }
      p = (p + 1) & cache_mask_;
    }
  }
  ++misses_;
  return kNoHandle;
}

void PathStore::evict(Tick now) {
  for (u32 i = 0; i < recs_.size(); ++i) {
    Rec& r = recs_[i];
    if (r.alive && r.cached && r.refs == 0 && r.last_used + kTicksPerDay < now) cache_erase(r.key);
  }
  if (live_ * 10 > recs_.size() * 8 || arena_top_ * 10 > tiles_.size() * 8) {
    for (u32 i = 0; i < recs_.size(); ++i) {
      Rec& r = recs_[i];
      if (r.alive && r.cached && r.refs == 0) cache_erase(r.key);
    }
  }
}

void PathStore::compact() {
  u32 n = 0;
  for (u32 i = 0; i < recs_.size(); ++i)
    if (recs_[i].alive) order_[n++] = i;
  std::sort(order_.begin(), order_.begin() + n, [&](u32 a, u32 b) { return recs_[a].off < recs_[b].off; });
  u32 top = 0;
  for (u32 k = 0; k < n; ++k) {
    Rec& r = recs_[order_[k]];
    if (r.off != top) {
      std::copy(tiles_.begin() + r.off, tiles_.begin() + r.off + r.len, tiles_.begin() + top);
      std::copy(cum_.begin() + r.off, cum_.begin() + r.off + r.len, cum_.begin() + top);
      r.off = top;
    }
    top += r.len;
  }
  arena_top_ = top;
  std::sort(order_.begin(), order_.begin() + n, [&](u32 a, u32 b) { return recs_[a].coff < recs_[b].coff; });
  u32 ctop = 0;
  for (u32 k = 0; k < n; ++k) {
    Rec& r = recs_[order_[k]];
    if (r.coff != ctop) {
      std::copy(chunks_.begin() + r.coff, chunks_.begin() + r.coff + r.ccount, chunks_.begin() + ctop);
      r.coff = ctop;
    }
    ctop += r.ccount;
  }
  chunk_top_ = ctop;
}

u32 PathStore::add(const i32* tiles, u32 n, const NavGrid& nav, Tick now, u64 k) {
  if (n == 0) return kNoHandle;
  auto fits = [&]() {
    return !free_recs_.empty() && arena_top_ + n <= tiles_.size() && chunk_top_ + n <= chunks_.size();
  };
  if (!fits()) {
    evict(now);
    compact();
    if (!fits()) return kNoHandle;
  }
  u32 h = free_recs_.back();
  free_recs_.pop_back();
  Rec& r = recs_[h];
  r = Rec{};
  r.off = arena_top_;
  r.len = n;
  r.coff = chunk_top_;
  r.alive = 1;
  r.created = now;
  r.last_used = now;
  r.key = k;
  u32 total = 0;
  i32 last_chunk = -1;
  for (u32 i = 0; i < n; ++i) {
    i32 t = tiles[i];
    i32 x = t % nav.w, y = t / nav.w;
    tiles_[r.off + i] = static_cast<u32>(x) | (static_cast<u32>(y) << 16);
    if (i > 0) {
      i32 pt = tiles[i - 1];
      i32 dx = x - pt % nav.w, dy = y - pt / nav.w;
      for (int d = 0; d < 8; ++d)
        if (kDir8X[d] == dx && kDir8Y[d] == dy) {
          u32 sc = nav.step_cost(pt % nav.w, pt / nav.w, d);
          total += sc ? sc : 50;  // blocked since computation: still advance time
          break;
        }
    }
    cum_[r.off + i] = total;
    i32 c = nav.chunk_of(x, y);
    if (c != last_chunk) {
      chunks_[chunk_top_ + r.ccount++] = static_cast<u16>(c);
      last_chunk = c;
    }
  }
  r.total = total;
  arena_top_ += n;
  chunk_top_ += r.ccount;
  ++live_;
  r.cached = 1;
  cache_insert(k, h);
  return h;
}

u32 PathStore::find_index(u32 h, u32 e, bool) const {
  const Rec& r = recs_[h];
  const u32* c = cum_.data() + r.off;
  // Last index with cum <= e.
  u32 lo = 0, hi = r.len - 1;
  while (lo < hi) {
    u32 mid = (lo + hi + 1) / 2;
    if (c[mid] <= e) lo = mid;
    else hi = mid - 1;
  }
  return lo;
}

Vec2 PathStore::position(u32 h, bool reversed, u32 elapsed) const {
  const Rec& r = recs_[h];
  if (elapsed >= r.total) {
    Vec2i t = tile(h, reversed ? 0 : r.len - 1);
    return {static_cast<f32>(t.x) + 0.5f, static_cast<f32>(t.y) + 0.5f};
  }
  u32 e = reversed ? r.total - elapsed : elapsed;
  u32 i = find_index(h, e, reversed);
  const u32* c = cum_.data() + r.off;
  Vec2i a = tile(h, i);
  if (i + 1 >= r.len) return {static_cast<f32>(a.x) + 0.5f, static_cast<f32>(a.y) + 0.5f};
  Vec2i b = tile(h, i + 1);
  f32 seg = static_cast<f32>(c[i + 1] - c[i]);
  f32 f = seg > 0 ? static_cast<f32>(e - c[i]) / seg : 0.0f;
  return {static_cast<f32>(a.x) + 0.5f + static_cast<f32>(b.x - a.x) * f,
          static_cast<f32>(a.y) + 0.5f + static_cast<f32>(b.y - a.y) * f};
}

int PathStore::facing(u32 h, bool reversed, u32 elapsed) const {
  const Rec& r = recs_[h];
  if (r.len < 2) return 2;
  u32 e = reversed ? (elapsed >= r.total ? 0 : r.total - elapsed) : std::min(elapsed, r.total);
  u32 i = find_index(h, e, reversed);
  if (i + 1 >= r.len) i = r.len - 2;
  Vec2i a = tile(h, i), b = tile(h, i + 1);
  i32 dx = b.x - a.x, dy = b.y - a.y;
  if (reversed) {
    dx = -dx;
    dy = -dy;
  }
  if (dx > 0 && dx >= (dy < 0 ? -dy : dy)) return 1;
  if (dx < 0 && -dx >= (dy < 0 ? -dy : dy)) return 3;
  return dy < 0 ? 0 : 2;
}

}  // namespace urbs::path

#include "save/serialize.hpp"

namespace urbs::path {

void PathStore::serialize(save::Writer& w) const {
  w.vec(tiles_);
  w.vec(cum_);
  w.vec(chunks_);
  w.vec(recs_);
  w.vec(free_recs_);
  w.vec(cache_);
  w.pod(cache_mask_);
  w.pod(cache_count_);
  w.pod(cache_tombs_);
  w.pod(arena_top_);
  w.pod(chunk_top_);
  w.pod(live_);
}

void PathStore::deserialize(save::Reader& r) {
  r.vec(tiles_);
  r.vec(cum_);
  r.vec(chunks_);
  r.vec(recs_);
  r.vec_fixed(free_recs_);
  r.vec(cache_);
  r.pod(cache_mask_);
  r.pod(cache_count_);
  r.pod(cache_tombs_);
  r.pod(arena_top_);
  r.pod(chunk_top_);
  r.pod(live_);
  order_.assign(recs_.size(), 0);
}

}  // namespace urbs::path
