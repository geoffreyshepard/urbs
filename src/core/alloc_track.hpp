#pragma once
#include "core/types.hpp"

// Allocation tracking for the "no heap allocation inside tick()" rule.
// The hook (global operator new/delete) lives in alloc_hook.cpp, which is linked only into
// executables (tests, bench, headless, game). While a TickScope is alive on any thread, every
// operator new is counted as a violation.
namespace urbs::core::alloc {

bool hook_installed();
u64 violations();
u64 total_allocations();
void reset_violations();
void set_tick_active(bool active);
bool tick_active();

struct TickScope {
  TickScope() { set_tick_active(true); }
  ~TickScope() { set_tick_active(false); }
  TickScope(const TickScope&) = delete;
  TickScope& operator=(const TickScope&) = delete;
};

// Temporarily allows allocation (e.g. rare, justified growth paths). Must be commented at use.
struct AllowScope {
  AllowScope() : was_(tick_active()) { set_tick_active(false); }
  ~AllowScope() { set_tick_active(was_); }
  AllowScope(const AllowScope&) = delete;
  AllowScope& operator=(const AllowScope&) = delete;

 private:
  bool was_;
};

}  // namespace urbs::core::alloc
