#pragma once
#include <vector>

#include "core/assert.hpp"
#include "core/types.hpp"

namespace urbs::core {

// Generational slot allocator for fixed-capacity pools. Data lives in parallel arrays owned by
// the user (struct-of-arrays); this class only hands out and validates indices.
// Capacity is fixed at init; create() never allocates.
template <typename IdT>
class SlotAlloc {
 public:
  void init(u32 capacity) {
    gen_.assign(capacity, 0);
    alive_.assign(capacity, 0);
    free_.clear();
    free_.reserve(capacity);
    for (u32 i = capacity; i-- > 0;) free_.push_back(i);  // lowest index handed out first
    count_ = 0;
    high_water_ = 0;
  }

  [[nodiscard]] IdT create() {
    if (free_.empty()) return IdT{};
    u32 i = free_.back();
    free_.pop_back();
    alive_[i] = 1;
    ++count_;
    if (i + 1 > high_water_) high_water_ = i + 1;
    return IdT::make(i, gen_[i]);
  }

  void destroy(IdT id) {
    URBS_ASSERT(alive(id));
    u32 i = id.index();
    alive_[i] = 0;
    gen_[i] = static_cast<u8>(gen_[i] + 1);
    free_.push_back(i);
    --count_;
  }

  [[nodiscard]] bool alive(IdT id) const {
    return id.valid() && id.index() < gen_.size() && alive_[id.index()] && gen_[id.index()] == static_cast<u8>(id.gen());
  }
  [[nodiscard]] bool alive_index(u32 i) const { return i < alive_.size() && alive_[i]; }
  [[nodiscard]] IdT id_at(u32 i) const { return IdT::make(i, gen_[i]); }
  [[nodiscard]] u32 count() const { return count_; }
  [[nodiscard]] u32 capacity() const { return static_cast<u32>(gen_.size()); }
  // One past the highest index ever handed out: iterate [0, high_water) and check alive_index.
  [[nodiscard]] u32 high_water() const { return high_water_; }

  // Raw access for serialization.
  std::vector<u8>& gens() { return gen_; }
  std::vector<u8>& alives() { return alive_; }
  std::vector<u32>& free_list() { return free_; }
  void set_counts(u32 count, u32 high_water) {
    count_ = count;
    high_water_ = high_water;
  }

 private:
  std::vector<u8> gen_;
  std::vector<u8> alive_;
  std::vector<u32> free_;
  u32 count_ = 0;
  u32 high_water_ = 0;
};

// Fixed-capacity vector: reserve once, push_back never reallocates (asserts instead).
template <typename T>
class FixedVec {
 public:
  void init(size_t cap) {
    v_.clear();
    v_.reserve(cap);
  }
  void push_back(const T& x) {
    URBS_ASSERT(v_.size() < v_.capacity(), "FixedVec overflow");
    if (v_.size() < v_.capacity()) v_.push_back(x);
  }
  [[nodiscard]] bool full() const { return v_.size() >= v_.capacity(); }
  void clear() { v_.clear(); }
  [[nodiscard]] size_t size() const { return v_.size(); }
  [[nodiscard]] bool empty() const { return v_.empty(); }
  T& operator[](size_t i) { return v_[i]; }
  const T& operator[](size_t i) const { return v_[i]; }
  T* begin() { return v_.data(); }
  T* end() { return v_.data() + v_.size(); }
  const T* begin() const { return v_.data(); }
  const T* end() const { return v_.data() + v_.size(); }
  T* data() { return v_.data(); }
  void resize(size_t n) {
    URBS_ASSERT(n <= v_.capacity());
    v_.resize(n);
  }
  std::vector<T>& raw() { return v_; }

 private:
  std::vector<T> v_;
};

}  // namespace urbs::core
