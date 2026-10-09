#pragma once
#include <memory>
#include <vector>

#include "core/pool.hpp"
#include "path/hpa.hpp"
#include "path/nav.hpp"
#include "path/path_store.hpp"

namespace enki {
class TaskScheduler;
}

namespace urbs::save {
class Writer;
class Reader;
}  // namespace urbs::save

namespace urbs::path {

struct PathRequest {
  u32 requester = 0;  // raw id of the requesting entity
  u16 token = 0;      // requester's token at request time (stale results are dropped by the caller)
  u8 kind = 0;        // requester kind (citizen, cart, ...)
  i32 src = 0, dst = 0;
};

struct PathResult {
  u32 requester = 0;
  u16 token = 0;
  u8 kind = 0;
  bool ok = false;
  bool reversed = false;
  u32 handle = kNoHandle;  // already ref'd for the requester when ok
};

struct PathStats {
  u32 queued = 0, processed = 0, cache_hits = 0, computed = 0, failed = 0, fallbacks = 0;
};

// Asynchronous pathfinding: FIFO request queue, fixed per-tick budget (deterministic), cache
// lookups on the calling thread, misses solved in parallel on enkiTS workers, results merged in
// request order.
class PathSystem {
 public:
  PathSystem();
  ~PathSystem();
  void init(NavGrid& nav, enki::TaskScheduler* ts, u32 queue_capacity, u32 budget_per_tick);
  bool request(const PathRequest& r);
  // Rebuilds dirty nav data, then processes up to `budget` queued requests.
  void process(Tick now, core::FixedVec<PathResult>& out);
  // Synchronous single query (main thread; used by tools/tests). Returns ref'd handle or kNoHandle.
  u32 find_now(i32 src, i32 dst, Tick now, bool* reversed);

  PathStore& store() { return store_; }
  const PathStore& store() const { return store_; }
  Hpa& hpa() { return hpa_; }
  void serialize(save::Writer& w) const;
  void deserialize(save::Reader& r);
  [[nodiscard]] const PathStats& stats() const { return stats_; }
  [[nodiscard]] u32 pending() const { return count_; }
  void set_budget(u32 b) { budget_ = b; }
  void clear_queue() { head_ = tail_ = count_ = 0; }

  struct Job;  // internal (public for the task set)

 private:
  u32 solve(i32 src, i32 dst, PathScratch& sc, bool* fallback);

  NavGrid* nav_ = nullptr;
  enki::TaskScheduler* ts_ = nullptr;
  Hpa hpa_;
  PathStore store_;
  std::vector<PathRequest> ring_;
  u32 head_ = 0, tail_ = 0, count_ = 0;
  u32 budget_ = 256;
  std::vector<PathScratch> scratch_;  // one per thread
  GridSearch fallback_;
  std::vector<PathRequest> batch_;
  std::vector<u32> unique_;     // indices into batch_ of unique misses
  std::vector<i32> job_of_;     // per batch entry: index into unique_ or -1
  std::vector<u32> res_off_, res_len_, res_cost_;
  std::vector<u8> res_fallback_;
  std::vector<std::vector<i32>> thread_out_;
  std::vector<u32> res_thread_;
  std::unique_ptr<Job> job_;
  PathStats stats_;
};

}  // namespace urbs::path
