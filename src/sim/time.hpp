#pragma once
#include "core/types.hpp"

namespace urbs::sim {

enum class DayPhase : u8 { Night, Dawn, Morning, Midday, Afternoon, Evening };
enum class Season : u8 { Spring, Summer, Autumn, Winter };

inline constexpr Tick kTicksPerYear = kTicksPerDay * kDaysPerYear;

inline constexpr u32 tick_of_day(Tick t) { return static_cast<u32>(t % kTicksPerDay); }
inline constexpr u32 hour_of_day(Tick t) { return tick_of_day(t) / static_cast<u32>(kTicksPerHour); }
inline constexpr u64 day_index(Tick t) { return t / kTicksPerDay; }
inline constexpr u32 day_of_year(Tick t) { return static_cast<u32>(day_index(t) % kDaysPerYear); }
inline constexpr u64 year_index(Tick t) { return t / kTicksPerYear; }
inline constexpr Season season(Tick t) { return static_cast<Season>(day_of_year(t) / kDaysPerSeason); }
// Start tick of the given hour today (or tomorrow if already past).
inline constexpr Tick next_hour(Tick now, u32 hour) {
  Tick day_start = now - tick_of_day(now);
  Tick t = day_start + hour * kTicksPerHour;
  return t > now ? t : t + kTicksPerDay;
}

inline constexpr DayPhase phase_of(Tick t) {
  u32 h = hour_of_day(t);
  if (h < 5 || h >= 22) return DayPhase::Night;
  if (h < 7) return DayPhase::Dawn;
  if (h < 12) return DayPhase::Morning;
  if (h < 13) return DayPhase::Midday;
  if (h < 17) return DayPhase::Afternoon;
  return DayPhase::Evening;
}

inline constexpr const char* season_name(Season s) {
  constexpr const char* n[] = {"Spring", "Summer", "Autumn", "Winter"};
  return n[static_cast<int>(s)];
}
inline constexpr const char* phase_name(DayPhase p) {
  constexpr const char* n[] = {"Night", "Dawn", "Morning", "Midday", "Afternoon", "Evening"};
  return n[static_cast<int>(p)];
}

}  // namespace urbs::sim
