#include "sim/citizens.hpp"

#include "sim/time.hpp"

namespace urbs::sim {

void Citizens::init(u32 cap) {
  ids.init(cap);
  token.assign(cap, 0);
  activity.assign(cap, Activity::None);
  next.assign(cap, Activity::None);
  loc.assign(cap, BuildingId{});
  dest.assign(cap, BuildingId{});
  dest_tile.assign(cap, -1);
  path.assign(cap, path::kNoHandle);
  path_rev.assign(cap, 0);
  walk_start.assign(cap, 0);
  activity_end.assign(cap, 0);
  home.assign(cap, BuildingId{});
  work.assign(cap, BuildingId{});
  household.assign(cap, HouseholdId{});
  need_base.assign(static_cast<size_t>(cap) * kNeedCount, 200);
  need_rate.assign(static_cast<size_t>(cap) * kNeedCount, 0);
  need_t0.assign(cap, 0);
  carrying.assign(cap, 0);
  carry_amount.assign(cap, 0);
  task.assign(cap, 0xFFFFFFFFu);
  reason.assign(cap, Reason::None);
  cls.assign(cap, CitizenClass::Pleb);
  sex.assign(cap, 0);
  born.assign(cap, 0);
  name0.assign(cap, 0);
  name1.assign(cap, 0);
  name2.assign(cap, 0);
  traits.assign(static_cast<size_t>(cap) * kMaxTraits, 0xFF);
  skills.assign(static_cast<size_t>(cap) * kSkillCount, 0);
  happiness.assign(cap, 128);
  health.assign(cap, 255);
  goal.assign(cap, 0);
  goal_progress.assign(cap, 0);
  spouse.assign(cap, CitizenId{});
  mother.assign(cap, CitizenId{});
  father.assign(cap, CitizenId{});
  memory.assign(static_cast<size_t>(cap) * kMemoryEntries, MemoryEntry{});
  memory_head.assign(cap, 0);
  flags.assign(cap, 0);
}

u32 Citizens::age_years(u32 i, Tick now) const {
  const i64 d = static_cast<i64>(now) - born[i];
  return d > 0 ? static_cast<u32>(d / static_cast<i64>(kTicksPerYear)) : 0;
}

void Citizens::remember(u32 i, u32 day, u8 kind, u16 arg) {
  u8& head = memory_head[i];
  memory[static_cast<size_t>(i) * kMemoryEntries + head] = MemoryEntry{day, kind, 0, arg};
  head = static_cast<u8>((head + 1) % kMemoryEntries);
}

}  // namespace urbs::sim
