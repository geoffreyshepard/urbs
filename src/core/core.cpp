#include <cstdio>
#include <cstdlib>

#include "core/assert.hpp"
#include "core/log.hpp"

namespace urbs::core {

void assert_fail(const char* expr, const char* file, int line, const char* msg) {
  std::fprintf(stderr, "URBS assertion failed: %s\n  at %s:%d\n  %s\n", expr, file, line, msg);
  std::fflush(stderr);
  std::abort();
}

void init_logging(bool verbose) {
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
  spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
}

}  // namespace urbs::core
