#include "path/path_system.hpp"

#include <TaskScheduler.h>

#include <algorithm>

#include "core/alloc_track.hpp"
#include "core/profile.hpp"

namespace urbs::path {

struct PathSystem::Job : enki::ITaskSet {
  PathSystem* ps = nullptr;
  void ExecuteRange(enki::TaskSetPartition range, uint32_t thread) override {
    PathSystem& s = *ps;
    PathScratch& sc = s.scratch_[thread];
    std::vector<i32>& out = s.thread_out_[thread];
    for (uint32_t u = range.start; u < range.end; ++u) {
      const PathRequest& r = s.batch_[s.unique_[u]];
      bool fb = false;
      u32 cost = s.hpa_.find(*s.nav_, r.src, r.dst, sc, &fb);
      s.res_thread_[u] = thread;
      s.res_cost_[u] = cost;
      s.res_fallback_[u] = fb ? 1 : 0;
      if (cost == kNoPath || out.size() + sc.tiles.size() > out.capacity()) {
        s.res_len_[u] = 0;
        if (cost != kNoPath) s.res_fallback_[u] = 1;  // output buffer full: solve on main thread
        continue;
      }
      s.res_off_[u] = static_cast<u32>(out.size());
      s.res_len_[u] = static_cast<u32>(sc.tiles.size());
      out.insert(out.end(), sc.tiles.begin(), sc.tiles.end());
    }
  }
};

PathSystem::PathSystem() = default;
PathSystem::~PathSystem() = default;

void PathSystem::init(NavGrid& nav, enki::TaskScheduler* ts, u32 queue_capacity, u32 budget) {
  nav_ = &nav;
  ts_ = ts;
  budget_ = budget;
  hpa_.init(nav);
  hpa_.rebuild_dirty(nav);
  const u32 tiles = static_cast<u32>(nav.w * nav.h);
  store_.init(std::max<u32>(1u << 20, tiles * 6), 1u << 17, 1u << 18);
  ring_.assign(queue_capacity, PathRequest{});
  head_ = tail_ = count_ = 0;
  const u32 threads = ts ? ts->GetNumTaskThreads() : 1;
  scratch_.resize(threads);
  thread_out_.resize(threads);
  for (u32 t = 0; t < threads; ++t) {
    scratch_[t].init(hpa_.total_nodes());
    thread_out_[t].clear();
    thread_out_[t].reserve(1u << 18);
  }
  fallback_.init(tiles);
  batch_.reserve(queue_capacity);
  unique_.reserve(queue_capacity);
  job_of_.reserve(queue_capacity);
  res_off_.assign(queue_capacity, 0);
  res_len_.assign(queue_capacity, 0);
  res_cost_.assign(queue_capacity, 0);
  res_fallback_.assign(queue_capacity, 0);
  res_thread_.assign(queue_capacity, 0);
  job_ = std::make_unique<Job>();
  job_->ps = this;
}

bool PathSystem::request(const PathRequest& r) {
  if (count_ == ring_.size()) return false;
  ring_[tail_] = r;
  tail_ = (tail_ + 1) % static_cast<u32>(ring_.size());
  ++count_;
  ++stats_.queued;
  return true;
}

u32 PathSystem::solve(i32 src, i32 dst, PathScratch& sc, bool* fallback) {
  u32 cost = hpa_.find(*nav_, src, dst, sc, fallback);
  if (cost == kNoPath && fallback && *fallback) {
    sc.tiles.clear();
    cost = fallback_.astar(*nav_, Recti{0, 0, nav_->w, nav_->h}, src, dst, &sc.tiles);
  }
  return cost;
}

u32 PathSystem::find_now(i32 src, i32 dst, Tick now, bool* reversed) {
  if (nav_->regions_dirty) nav_->rebuild_regions();
  hpa_.rebuild_dirty(*nav_);
  u32 h = store_.lookup(src, dst, *nav_, now, reversed);
  if (h != kNoHandle) {
    store_.ref(h);
    return h;
  }
  if (reversed) *reversed = false;
  bool fb = false;
  u32 cost = solve(src, dst, scratch_[0], &fb);
  if (cost == kNoPath) return kNoHandle;
  h = store_.add(scratch_[0].tiles.data(), static_cast<u32>(scratch_[0].tiles.size()), *nav_, now,
                 PathStore::key(src, dst));
  if (h != kNoHandle) store_.ref(h);
  return h;
}

void PathSystem::process(Tick now, core::FixedVec<PathResult>& out) {
  URBS_ZONE("paths");
  stats_.processed = stats_.cache_hits = stats_.computed = stats_.failed = stats_.fallbacks = 0;
  if (nav_->regions_dirty) nav_->rebuild_regions();
  hpa_.rebuild_dirty(*nav_);
  if (count_ == 0) return;

  // 1. Take the batch (FIFO), resolve cache hits and trivially-failing requests on this thread.
  batch_.clear();
  job_of_.clear();
  unique_.clear();
  const u32 n = std::min(count_, budget_);
  for (u32 i = 0; i < n; ++i) {
    batch_.push_back(ring_[head_]);
    head_ = (head_ + 1) % static_cast<u32>(ring_.size());
  }
  count_ -= n;
  stats_.processed = n;
  for (u32 i = 0; i < n; ++i) {
    const PathRequest& r = batch_[i];
    if (!nav_->connected(r.src, r.dst)) {
      job_of_.push_back(-2);
      continue;
    }
    bool rev = false;
    u32 h = store_.lookup(r.src, r.dst, *nav_, now, &rev);
    if (h != kNoHandle) {
      store_.ref(h);
      out.push_back({r.requester, r.token, r.kind, true, rev, h});
      ++stats_.cache_hits;
      job_of_.push_back(-1);
      continue;
    }
    // Dedupe identical misses within the batch (either direction).
    i32 found = -3;
    for (u32 k = 0; k < unique_.size(); ++k) {
      const PathRequest& o = batch_[unique_[k]];
      if ((o.src == r.src && o.dst == r.dst) || (o.src == r.dst && o.dst == r.src)) {
        found = static_cast<i32>(k);
        break;
      }
    }
    if (found == -3) {
      found = static_cast<i32>(unique_.size());
      unique_.push_back(i);
    }
    job_of_.push_back(found);
  }

  // 2. Solve unique misses in parallel.
  for (auto& o : thread_out_) o.clear();
  const u32 m = static_cast<u32>(unique_.size());
  if (m > 0) {
    if (ts_) {
      job_->m_SetSize = m;
      job_->m_MinRange = 4;
      ts_->AddTaskSetToPipe(job_.get());
      ts_->WaitforTask(job_.get());
    } else {
      enki::TaskSetPartition all{0, m};
      job_->ExecuteRange(all, 0);
    }
  }

  // 3. Merge in request order.
  std::vector<u32>& handles = res_off_;  // reuse: after reading offsets we store handles in res_cost_
  (void)handles;
  for (u32 u = 0; u < m; ++u) {
    const PathRequest& r = batch_[unique_[u]];
    u32 h = kNoHandle;
    if (res_len_[u] > 0) {
      const std::vector<i32>& src = thread_out_[res_thread_[u]];
      h = store_.add(src.data() + res_off_[u], res_len_[u], *nav_, now, PathStore::key(r.src, r.dst));
    } else if (res_fallback_[u]) {
      ++stats_.fallbacks;
      bool fb = false;
      u32 cost = solve(r.src, r.dst, scratch_[0], &fb);
      if (cost != kNoPath)
        h = store_.add(scratch_[0].tiles.data(), static_cast<u32>(scratch_[0].tiles.size()), *nav_, now,
                       PathStore::key(r.src, r.dst));
    }
    res_cost_[u] = h;
    ++stats_.computed;
  }
  for (u32 i = 0; i < n; ++i) {
    const PathRequest& r = batch_[i];
    i32 j = job_of_[i];
    if (j == -1) continue;
    if (j == -2) {
      out.push_back({r.requester, r.token, r.kind, false, false, kNoHandle});
      ++stats_.failed;
      continue;
    }
    u32 h = res_cost_[static_cast<size_t>(j)];
    if (h == kNoHandle) {
      out.push_back({r.requester, r.token, r.kind, false, false, kNoHandle});
      ++stats_.failed;
      continue;
    }
    const PathRequest& owner = batch_[unique_[static_cast<size_t>(j)]];
    const bool rev = owner.src != r.src;
    store_.ref(h);
    out.push_back({r.requester, r.token, r.kind, true, rev, h});
  }
}

}  // namespace urbs::path

#include "save/serialize.hpp"

namespace urbs::path {

void PathSystem::serialize(save::Writer& w) const {
  store_.serialize(w);
  w.vec(ring_);
  w.pod(head_);
  w.pod(tail_);
  w.pod(count_);
}

void PathSystem::deserialize(save::Reader& r) {
  store_.deserialize(r);
  r.vec(ring_);
  r.pod(head_);
  r.pod(tail_);
  r.pod(count_);
}

}  // namespace urbs::path
