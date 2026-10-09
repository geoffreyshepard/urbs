#pragma once

namespace urbs::core {
[[noreturn]] void assert_fail(const char* expr, const char* file, int line, const char* msg);
}

// URBS_ASSERT is active in all builds except Release (NDEBUG). URBS_VERIFY is always active.
#ifdef NDEBUG
#define URBS_ASSERT(expr, ...) ((void)0)
#else
#define URBS_ASSERT(expr, ...) \
  ((expr) ? (void)0 : ::urbs::core::assert_fail(#expr, __FILE__, __LINE__, "" __VA_ARGS__))
#endif
#define URBS_VERIFY(expr, ...) \
  ((expr) ? (void)0 : ::urbs::core::assert_fail(#expr, __FILE__, __LINE__, "" __VA_ARGS__))
