#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

namespace urbs {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;

// Simulation tick count. 20 ticks per second at 1x.
using Tick = u64;
inline constexpr Tick kTicksPerSecond = 20;
inline constexpr Tick kTicksPerDay = 60 * kTicksPerSecond;  // 1 game day = 60 s at 1x
inline constexpr Tick kTicksPerHour = kTicksPerDay / 24;    // 50 ticks
inline constexpr u32 kDaysPerSeason = 4;
inline constexpr u32 kDaysPerYear = 16;

// Strong, generational ID over a single u32: low 24 bits index, high 8 bits generation.
template <typename Tag>
struct Id {
  static constexpr u32 kIndexBits = 24;
  static constexpr u32 kIndexMask = (1u << kIndexBits) - 1;
  static constexpr u32 kInvalid = 0xFFFFFFFFu;

  u32 raw = kInvalid;

  constexpr Id() = default;
  constexpr explicit Id(u32 r) : raw(r) {}
  static constexpr Id make(u32 index, u32 gen) {
    return Id{(index & kIndexMask) | ((gen & 0xFFu) << kIndexBits)};
  }
  [[nodiscard]] constexpr u32 index() const { return raw & kIndexMask; }
  [[nodiscard]] constexpr u32 gen() const { return raw >> kIndexBits; }
  [[nodiscard]] constexpr bool valid() const { return raw != kInvalid; }
  constexpr explicit operator bool() const { return valid(); }
  friend constexpr bool operator==(Id a, Id b) { return a.raw == b.raw; }
  friend constexpr bool operator!=(Id a, Id b) { return a.raw != b.raw; }
  friend constexpr bool operator<(Id a, Id b) { return a.raw < b.raw; }
};

using CitizenId = Id<struct CitizenTag>;
using HouseholdId = Id<struct HouseholdTag>;
using BuildingId = Id<struct BuildingTag>;
using HaulTaskId = Id<struct HaulTaskTag>;
using UnitId = Id<struct UnitTag>;
using SoldierId = Id<struct SoldierTag>;

// Index into a compiled definitions table (goods, buildings, ...). Not generational.
template <typename Tag>
struct DefId {
  static constexpr u16 kInvalid = 0xFFFF;
  u16 v = kInvalid;
  constexpr DefId() = default;
  constexpr explicit DefId(u16 x) : v(x) {}
  [[nodiscard]] constexpr bool valid() const { return v != kInvalid; }
  constexpr explicit operator bool() const { return valid(); }
  friend constexpr bool operator==(DefId a, DefId b) { return a.v == b.v; }
  friend constexpr bool operator!=(DefId a, DefId b) { return a.v != b.v; }
  friend constexpr bool operator<(DefId a, DefId b) { return a.v < b.v; }
};

using GoodId = DefId<struct GoodTag>;
using BuildingDefId = DefId<struct BuildingDefTag>;
using RecipeId = DefId<struct RecipeTag>;
using ClassId = DefId<struct ClassTag>;
using GodId = DefId<struct GodTag>;
using UnitDefId = DefId<struct UnitDefTag>;
using ServiceId = DefId<struct ServiceTag>;
using TraitId = DefId<struct TraitTag>;

}  // namespace urbs

template <typename Tag>
struct std::hash<urbs::Id<Tag>> {
  size_t operator()(urbs::Id<Tag> id) const noexcept { return std::hash<urbs::u32>{}(id.raw); }
};
