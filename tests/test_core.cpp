#include <doctest/doctest.h>

#include <vector>

#include "core/alloc_track.hpp"
#include "core/rng.hpp"
#include "core/types.hpp"

using namespace urbs;

TEST_CASE("Id packs index and generation") {
  auto id = CitizenId::make(123456, 7);
  CHECK(id.index() == 123456);
  CHECK(id.gen() == 7);
  CHECK(id.valid());
  CHECK_FALSE(CitizenId{}.valid());
}

TEST_CASE("RNG streams are deterministic and independent") {
  rng::Stream a(42, 1), b(42, 1), c(42, 2);
  for (int i = 0; i < 100; ++i) {
    auto x = a.next_u64();
    CHECK(x == b.next_u64());
  }
  CHECK(a.next_u64() != c.next_u64());
  rng::Stream r(1, 1);
  for (int i = 0; i < 1000; ++i) {
    auto v = r.below(10);
    CHECK(v < 10);
    auto f = r.unit();
    CHECK(f >= 0.0f);
    CHECK(f < 1.0f);
  }
}

TEST_CASE("RNG state round-trips") {
  rng::Stream a(9, 9);
  a.next_u64();
  auto st = a.state();
  auto x = a.next_u64();
  rng::Stream b;
  b.set_state(st);
  CHECK(b.next_u64() == x);
}

#ifndef _MSC_VER
TEST_CASE("Allocation hook counts allocations during a tick scope") {
  REQUIRE(core::alloc::hook_installed());
  core::alloc::reset_violations();
  {
    core::alloc::TickScope s;
    // A direct operator new call through a volatile pointer: optimised builds may elide a plain
    // new/delete pair, which would leave nothing for the hook to count.
    void* volatile p = ::operator new(16);
    ::operator delete(p);
  }
  CHECK(core::alloc::violations() == 1);
  core::alloc::reset_violations();
  std::vector<int> v(100);
  CHECK(core::alloc::violations() == 0);
}
#endif
