#pragma once
#include <spdlog/spdlog.h>

namespace urbs::core {
void init_logging(bool verbose);
}

#define URBS_LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define URBS_LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define URBS_LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define URBS_LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define URBS_LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
