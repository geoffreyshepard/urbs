#include "sim/scheduler.hpp"

#include <algorithm>

namespace urbs::sim {

void Scheduler::init(u32 capacity, u32 wheel_bits) {
  nodes_.assign(capacity, Node{});
  free_.clear();
  free_.reserve(capacity);
  for (u32 i = capacity; i-- > 0;) free_.push_back(i);
  wheel_bits_ = wheel_bits;
  wheel_size_ = 1u << wheel_bits;
  wheel_mask_ = wheel_size_ - 1;
  head_.assign(wheel_size_, kNil);
  tail_.assign(wheel_size_, kNil);
  over_head_ = kNil;
  live_ = 0;
  dropped_ = 0;
  overflow_checked_ = now_;
}

void Scheduler::link(u32 slot, u32 n, bool overflow) {
  nodes_[n].next = kNil;
  if (overflow) {
    nodes_[n].next = over_head_;
    over_head_ = n;
    return;
  }
  if (tail_[slot] == kNil) {
    head_[slot] = tail_[slot] = n;
  } else {
    nodes_[tail_[slot]].next = n;
    tail_[slot] = n;
  }
}

bool Scheduler::schedule(const Event& ev) {
  if (free_.empty()) {
    ++dropped_;
    return false;
  }
  Event e = ev;
  if (e.when <= now_) e.when = now_ + 1;
  u32 n = free_.back();
  free_.pop_back();
  nodes_[n].e = e;
  ++live_;
  const bool overflow = e.when >= now_ + wheel_size_;
  link(static_cast<u32>(e.when & wheel_mask_), n, overflow);
  return true;
}

void Scheduler::refill_from_overflow() {
  // Move overflow events that now fall inside the wheel window. Order: we rebuild the overflow
  // list preserving relative order of the remaining nodes, and insert moved nodes in list order.
  u32 keep_head = kNil, keep_tail = kNil;
  u32 n = over_head_;
  over_head_ = kNil;
  while (n != kNil) {
    u32 next = nodes_[n].next;
    if (nodes_[n].e.when < now_ + wheel_size_) {
      link(static_cast<u32>(nodes_[n].e.when & wheel_mask_), n, false);
    } else {
      nodes_[n].next = kNil;
      if (keep_tail == kNil) keep_head = n;
      else nodes_[keep_tail].next = n;
      keep_tail = n;
    }
    n = next;
  }
  over_head_ = keep_head;
}

void Scheduler::pop_due(Tick now, u32 max, core::FixedVec<Event>& out) {
  now_ = now;
  if (now_ - overflow_checked_ >= wheel_size_ / 2) {
    refill_from_overflow();
    overflow_checked_ = now_;
  }
  const u32 slot = static_cast<u32>(now & wheel_mask_);
  u32 n = head_[slot];
  head_[slot] = tail_[slot] = kNil;
  const size_t start = out.size();
  while (n != kNil) {
    u32 next = nodes_[n].next;
    if (nodes_[n].e.when > now) {
      link(slot, n, false);  // belongs to a later lap of the wheel
    } else {
      out.push_back(nodes_[n].e);
      free_.push_back(n);
      --live_;
    }
    n = next;
  }
  std::sort(out.begin() + start, out.end(), [](const Event& a, const Event& b) {
    if (a.kind != b.kind) return a.kind < b.kind;
    if (a.type != b.type) return a.type < b.type;
    return a.target < b.target;
  });
  // Decision budget: everything past `max` (in sorted order, so independent of insertion order)
  // spills to the next tick.
  if (out.size() - start > max) {
    for (size_t i = start + max; i < out.size(); ++i) {
      Event e = out[i];
      e.when = now + 1;
      u32 nn = free_.back();
      free_.pop_back();
      nodes_[nn].e = e;
      ++live_;
      link(static_cast<u32>(e.when & wheel_mask_), nn, false);
    }
    out.resize(start + max);
  }
}

void Scheduler::collect_all(std::vector<Event>& out) const {
  out.clear();
  for (u32 s = 0; s < wheel_size_; ++s)
    for (u32 n = head_[s]; n != kNil; n = nodes_[n].next) out.push_back(nodes_[n].e);
  for (u32 n = over_head_; n != kNil; n = nodes_[n].next) out.push_back(nodes_[n].e);
  std::stable_sort(out.begin(), out.end(), [](const Event& a, const Event& b) { return a.when < b.when; });
}

void Scheduler::restore(Tick now, const std::vector<Event>& events) {
  init(static_cast<u32>(nodes_.size()), wheel_bits_);
  now_ = now;
  overflow_checked_ = now;
  for (const Event& e : events) schedule(e);
}

}  // namespace urbs::sim
