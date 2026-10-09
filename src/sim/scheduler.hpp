#pragma once
#include <vector>

#include "core/pool.hpp"
#include "core/types.hpp"

namespace urbs::sim {

// What an event refers to. The target is a raw id of that kind.
enum class EventKind : u8 { Citizen, Building, System, Unit };

struct Event {
  Tick when = 0;
  u32 target = 0;
  u16 type = 0;
  u16 token = 0;     // must match the target's current token, otherwise the event is stale (lazy cancel)
  EventKind kind = EventKind::Citizen;
};

// Timing wheel with an overflow list. Events live in a fixed pool of intrusive list nodes; no
// allocation after init. Events due in the same tick are returned sorted by (kind, type, target).
class Scheduler {
 public:
  void init(u32 capacity, u32 wheel_bits = 12);
  // Schedules at max(when, now + 1). Returns false if the pool is full (event dropped).
  bool schedule(const Event& e);
  // Pops up to `max` due events for tick `now`; leftovers spill to the next tick.
  // `out` must have room for every due event (size it to the scheduler capacity).
  void pop_due(Tick now, u32 max, core::FixedVec<Event>& out);
  void set_now(Tick now) { now_ = now; }
  [[nodiscard]] Tick now() const { return now_; }
  [[nodiscard]] u32 size() const { return live_; }
  [[nodiscard]] u32 capacity() const { return static_cast<u32>(nodes_.size()); }
  [[nodiscard]] u64 dropped() const { return dropped_; }

  // Serialization: all live events, in a stable order.
  void collect_all(std::vector<Event>& out) const;
  void restore(Tick now, const std::vector<Event>& events);

 private:
  static constexpr u32 kNil = 0xFFFFFFFFu;
  struct Node {
    Event e;
    u32 next;
  };
  void link(u32 slot_head_index, u32 node, bool overflow);
  void refill_from_overflow();

  std::vector<Node> nodes_;
  std::vector<u32> free_;
  std::vector<u32> head_, tail_;  // per wheel slot
  u32 over_head_ = kNil;
  u32 wheel_bits_ = 12, wheel_size_ = 0, wheel_mask_ = 0;
  Tick now_ = 0;
  Tick overflow_checked_ = 0;
  u32 live_ = 0;
  u64 dropped_ = 0;
};

}  // namespace urbs::sim
