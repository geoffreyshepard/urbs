#pragma once
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "core/types.hpp"

namespace urbs::save {

// Little binary writer/reader for POD data. All saved data uses fixed-size types.
class Writer {
 public:
  template <typename T>
  void pod(const T& v) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* p = reinterpret_cast<const u8*>(&v);
    buf_.insert(buf_.end(), p, p + sizeof(T));
  }
  template <typename T>
  void vec(const std::vector<T>& v) {
    static_assert(std::is_trivially_copyable_v<T>);
    pod(static_cast<u64>(v.size()));
    const auto* p = reinterpret_cast<const u8*>(v.data());
    buf_.insert(buf_.end(), p, p + v.size() * sizeof(T));
  }
  void str(const std::string& s) {
    pod(static_cast<u32>(s.size()));
    buf_.insert(buf_.end(), s.begin(), s.end());
  }
  void tag(const char (&t)[5]) { pod(*reinterpret_cast<const u32*>(t)); }
  std::vector<u8>& data() { return buf_; }

 private:
  std::vector<u8> buf_;
};

class Reader {
 public:
  Reader(const u8* p, size_t n) : p_(p), end_(p + n) {}
  template <typename T>
  T pod() {
    static_assert(std::is_trivially_copyable_v<T>);
    need(sizeof(T));
    T v;
    std::memcpy(&v, p_, sizeof(T));
    p_ += sizeof(T);
    return v;
  }
  template <typename T>
  void pod(T& v) {
    v = pod<T>();
  }
  template <typename T>
  void vec(std::vector<T>& v) {
    u64 n = pod<u64>();
    need(n * sizeof(T));
    v.resize(n);
    std::memcpy(v.data(), p_, n * sizeof(T));
    p_ += n * sizeof(T);
  }
  // Reads into an existing vector that must already have exactly the saved size (keeps capacity).
  template <typename T>
  void vec_fixed(std::vector<T>& v) {
    u64 n = pod<u64>();
    if (n > v.capacity()) v.reserve(n);
    need(n * sizeof(T));
    v.resize(n);
    std::memcpy(v.data(), p_, n * sizeof(T));
    p_ += n * sizeof(T);
  }
  std::string str() {
    u32 n = pod<u32>();
    need(n);
    std::string s(reinterpret_cast<const char*>(p_), n);
    p_ += n;
    return s;
  }
  void expect(const char (&t)[5]) {
    u32 v = pod<u32>();
    if (v != *reinterpret_cast<const u32*>(t)) throw std::runtime_error(std::string("save: bad section, expected ") + t);
  }
  [[nodiscard]] bool done() const { return p_ == end_; }

 private:
  void need(size_t n) const {
    if (static_cast<size_t>(end_ - p_) < n) throw std::runtime_error("save: truncated");
  }
  const u8* p_;
  const u8* end_;
};

}  // namespace urbs::save
