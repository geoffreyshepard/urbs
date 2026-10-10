// The stb_vorbis implementation, compiled once. Other files include it with STB_VORBIS_HEADER_ONLY.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <stb_vorbis.c>  // NOLINT(bugprone-suspicious-include): single-file library
