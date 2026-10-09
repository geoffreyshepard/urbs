#include "core/alloc_track.hpp"

#include <atomic>

namespace urbs::core::alloc {

namespace detail {
std::atomic<bool> g_tick_active{false};
std::atomic<u64> g_violations{0};
std::atomic<u64> g_total{0};
std::atomic<bool> g_hook_installed{false};
}  // namespace detail

bool hook_installed() { return detail::g_hook_installed.load(std::memory_order_relaxed); }
u64 violations() { return detail::g_violations.load(std::memory_order_relaxed); }
u64 total_allocations() { return detail::g_total.load(std::memory_order_relaxed); }
void reset_violations() { detail::g_violations.store(0, std::memory_order_relaxed); }
void set_tick_active(bool active) { detail::g_tick_active.store(active, std::memory_order_relaxed); }
bool tick_active() { return detail::g_tick_active.load(std::memory_order_relaxed); }

}  // namespace urbs::core::alloc
