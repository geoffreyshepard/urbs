#pragma once
#include <vector>

#include "core/math.hpp"
#include "core/types.hpp"
#include "path/nav.hpp"

namespace urbs::path {

// Indexed binary min-heap keyed by u32 priority over node ids [0, capacity). No allocation after init.
class IndexedHeap {
 public:
  void init(u32 capacity) {
    heap_.assign(capacity, 0);
    pos_.assign(capacity, kNone);
    key_.assign(capacity, 0);
    size_ = 0;
  }
  void clear() {
    for (u32 i = 0; i < size_; ++i) pos_[heap_[i]] = kNone;
    size_ = 0;
  }
  [[nodiscard]] bool empty() const { return size_ == 0; }
  void push_or_decrease(u32 id, u32 key) {
    if (pos_[id] == kNone) {
      heap_[size_] = id;
      pos_[id] = size_;
      key_[id] = key;
      up(size_++);
    } else if (key < key_[id]) {
      key_[id] = key;
      up(pos_[id]);
    }
  }
  u32 pop() {
    u32 top = heap_[0];
    pos_[top] = kNone;
    if (--size_ > 0) {
      heap_[0] = heap_[size_];
      pos_[heap_[0]] = 0;
      down(0);
    }
    return top;
  }
  [[nodiscard]] u32 capacity() const { return static_cast<u32>(pos_.size()); }

 private:
  static constexpr u32 kNone = 0xFFFFFFFFu;
  // Ties broken by id so results never depend on insertion history.
  bool less(u32 a, u32 b) const { return key_[a] < key_[b] || (key_[a] == key_[b] && a < b); }
  void up(u32 i) {
    while (i > 0) {
      u32 p = (i - 1) / 2;
      if (!less(heap_[i], heap_[p])) break;
      swap(i, p);
      i = p;
    }
  }
  void down(u32 i) {
    for (;;) {
      u32 l = 2 * i + 1, r = l + 1, m = i;
      if (l < size_ && less(heap_[l], heap_[m])) m = l;
      if (r < size_ && less(heap_[r], heap_[m])) m = r;
      if (m == i) break;
      swap(i, m);
      i = m;
    }
  }
  void swap(u32 a, u32 b) {
    u32 t = heap_[a];
    heap_[a] = heap_[b];
    heap_[b] = t;
    pos_[heap_[a]] = a;
    pos_[heap_[b]] = b;
  }
  std::vector<u32> heap_, pos_, key_;
  u32 size_ = 0;
};

// A*/Dijkstra on the tile grid restricted to a rectangle. Scratch sized for a max rect area.
class GridSearch {
 public:
  void init(u32 max_area);
  // A* from s to g (global tile indices) inside rect. Appends tiles s..g (inclusive) to out.
  // Returns path cost or 0xFFFFFFFF if unreachable.
  u32 astar(const NavGrid& nav, Recti rect, i32 s, i32 g, std::vector<i32>* out);
  // Dijkstra from s inside rect; afterwards cost_to(tile) answers for any tile in rect.
  void dijkstra(const NavGrid& nav, Recti rect, i32 s, const i32* stop_tiles = nullptr, u32 stop_count = 0);
  [[nodiscard]] u32 cost_to(i32 tile) const;

 private:
  [[nodiscard]] bool local(i32 tile, u32& li) const;
  std::vector<u32> g_;
  std::vector<i32> parent_;
  std::vector<u32> stamp_;
  u32 gen_ = 0;
  IndexedHeap heap_;
  Recti rect_{};
  i32 map_w_ = 0;
};

}  // namespace urbs::path
