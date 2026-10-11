#pragma once
#include <vector>

#include "core/math.hpp"
#include "core/pool.hpp"
#include "core/types.hpp"

namespace urbs::sim {

enum class BState : u8 { Construction, Active, Burning, Ruin };

inline constexpr u32 kNoIndex = 0xFFFFFFFFu;
// Building::flags bits: 0x03 devolve counter, 0x10 sewer, 0x20 piped water, 0x40 cut off from the town.
inline constexpr u8 kBuildingCutOff = 0x40;

struct Building {
  BuildingDefId def;
  BState state = BState::Construction;
  u8 tier = 0;       // housing evolution tier / upgrade level
  u16 token = 0;     // event token
  i16 x = 0, y = 0;  // top-left tile
  u8 w = 1, h = 1;
  i32 entrance = -1;  // walkable tile index citizens path to
  u32 build_progress = 0;
  u32 build_needed = 0;
  // Workforce
  u16 jobs = 0, employed = 0, present = 0;
  // Housing
  u16 capacity = 0, residents = 0;
  u32 first_household = kNoIndex;
  // Services
  u16 visitors = 0, served_today = 0;
  // Hazards
  u8 fire_risk = 0, damage = 0;  // accumulated risk 0..255, structural damage 0..255
  u8 desirability_seen = 128;    // last sampled desirability (128 = neutral)
  u8 flags = 0;
  // Economy: index into econ inventories (kNoIndex if none)
  u32 inv = kNoIndex;
  u32 work_progress = 0;  // production progress in worker-ticks
  i32 money = 0;          // building cash (markets, workplaces)
  u32 extra = 0;          // kind-specific (e.g. god index, patrol counter)
  u32 skill_sum = 0;      // sum of present workers' skill x work trait (productivity)
  u8 burn = 0;            // hours left burning (state Burning)
  u8 plague = 0;          // days of active infection (houses)
  // Orientation, quarter turns clockwise seen from above: the front (door) faces 0 south, 1 west,
  // 2 north, 3 east.  w/h above are already the rotated footprint.  Lives in what was padding up to
  // save v3, so sizeof(Building) is unchanged (save.cpp checks it).
  u8 rot = 0;
};

struct Buildings {
  core::SlotAlloc<BuildingId> ids;
  std::vector<Building> b;

  void init(u32 capacity) {
    ids.init(capacity);
    b.assign(capacity, Building{});
  }
  Building& operator[](BuildingId id) { return b[id.index()]; }
  const Building& operator[](BuildingId id) const { return b[id.index()]; }
  [[nodiscard]] bool alive(BuildingId id) const { return ids.alive(id); }
  template <typename F>
  void for_each(F&& f) {
    for (u32 i = 0; i < ids.high_water(); ++i)
      if (ids.alive_index(i)) f(ids.id_at(i), b[i]);
  }
  template <typename F>
  void for_each(F&& f) const {
    for (u32 i = 0; i < ids.high_water(); ++i)
      if (ids.alive_index(i)) f(ids.id_at(i), b[i]);
  }
};

}  // namespace urbs::sim
