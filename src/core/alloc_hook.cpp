// Global operator new/delete replacement that counts allocations made while a tick is active.
// Linked only into executables (see src/CMakeLists.txt: urbs_alloc_hook).
#include <atomic>
#include <cstdlib>
#include <new>

#include "core/types.hpp"

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#include <sanitizer/common_interface_defs.h>
#endif
#endif

namespace urbs::core::alloc::detail {
extern std::atomic<bool> g_tick_active;
extern std::atomic<urbs::u64> g_violations;
extern std::atomic<urbs::u64> g_total;
extern std::atomic<bool> g_hook_installed;

namespace {
struct Installer {
  Installer() { g_hook_installed.store(true); }
} g_installer;

void* counted_alloc(std::size_t n, std::size_t align) {
  g_total.fetch_add(1, std::memory_order_relaxed);
  if (g_tick_active.load(std::memory_order_relaxed)) {
    if (g_violations.fetch_add(1, std::memory_order_relaxed) == 0 && std::getenv("URBS_ALLOC_TRAP")) {
      // Debugging aid: print where the first allocation inside tick() happened.
      g_tick_active.store(false);
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
      __sanitizer_print_stack_trace();
#endif
#endif
      std::abort();
    }
  }
  if (n == 0) n = 1;
  void* p = nullptr;
  if (align <= alignof(std::max_align_t)) {
    p = std::malloc(n);
  } else {
    std::size_t sz = (n + align - 1) / align * align;
    p = std::aligned_alloc(align, sz);
  }
  return p;
}
}  // namespace
}  // namespace urbs::core::alloc::detail

using urbs::core::alloc::detail::counted_alloc;

void* operator new(std::size_t n) {
  if (void* p = counted_alloc(n, 0)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) {
  if (void* p = counted_alloc(n, 0)) return p;
  throw std::bad_alloc();
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return counted_alloc(n, 0); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return counted_alloc(n, 0); }
void* operator new(std::size_t n, std::align_val_t a) {
  if (void* p = counted_alloc(n, static_cast<std::size_t>(a))) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n, std::align_val_t a) {
  if (void* p = counted_alloc(n, static_cast<std::size_t>(a))) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
