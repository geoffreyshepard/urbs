#include "sim/chaos.hpp"

#include <algorithm>

#include "sim/world.hpp"

namespace urbs::sim {

u64 Chaos::next() {
  // splitmix64: the chaos player has its own stream, so it never perturbs the simulation's RNG.
  u64 z = (s_ += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

void Chaos::step(World& w, u32 per_hour) {
  if (per_hour == 0) return;
  // Spread the actions over the hour.
  const Tick period = std::max<Tick>(1, kTicksPerHour / per_hour);
  if (w.now % period == 0) act(w);
}

void Chaos::act(World& w) {
  ++actions;
  Command c;
  const i32 W = w.map.w, H = w.map.h;
  // Most actions land near the heart of the city, where they hurt.
  const i32 heart = w.city_target_tile();
  const i32 hx = heart >= 0 ? heart % W : W / 2, hy = heart >= 0 ? heart / W : H / 2;
  auto near_x = [&] { return std::clamp(hx + below(61) - 30, 0, W - 1); };
  auto near_y = [&] { return std::clamp(hy + below(61) - 30, 0, H - 1); };
  const auto& D = *w.defs;
  const i32 roll = below(100);
  if (roll < 3) {
    // Garbage: any command type with fields out of range.
    c.type = static_cast<CmdType>(below(static_cast<i32>(CmdType::Debug)));
    if (c.type == CmdType::Grant || c.type == CmdType::Trigger) c.type = CmdType::Demolish;  // keep the run meaningful
    c.def = static_cast<u16>(next());
    c.x0 = static_cast<i32>(next() % 2000) - 1000;
    c.y0 = static_cast<i32>(next() % 2000) - 1000;
    c.x1 = static_cast<i32>(next() % 2000) - 1000;
    c.y1 = static_cast<i32>(next() % 2000) - 1000;
    c.value = static_cast<i32>(next());
    c.flags = static_cast<u8>(next() & 0xFE);  // never "instant"
  } else if (roll < 33) {
    // Build something somewhere.
    const u16 d = static_cast<u16>(below(static_cast<i32>(D.buildings.size())));
    const auto& bd = D.buildings[d];
    c.type = CmdType::PlaceBuilding;
    c.def = d;
    c.x0 = near_x();
    c.y0 = near_y();
    c.x1 = bd.painted ? bd.min_w + below(std::max(1, bd.max_w - bd.min_w + 1)) : bd.w;
    c.y1 = bd.painted ? bd.min_h + below(std::max(1, bd.max_h - bd.min_h + 1)) : bd.h;
    c.value = below(4);
  } else if (roll < 45) {
    c.type = CmdType::PlaceRoad;
    c.def = static_cast<u16>(1 + below(3));
    c.x0 = near_x();
    c.y0 = near_y();
    const bool horiz = below(2) == 0;
    c.x1 = horiz ? std::clamp(c.x0 + below(31) - 15, 0, W - 1) : c.x0;
    c.y1 = horiz ? c.y0 : std::clamp(c.y0 + below(31) - 15, 0, H - 1);
  } else if (roll < 60) {
    // Demolish a small area: houses, roads, workshops, monuments under construction, anything.
    c.type = CmdType::Demolish;
    c.x0 = near_x();
    c.y0 = near_y();
    c.x1 = std::min(W - 1, c.x0 + below(4));
    c.y1 = std::min(H - 1, c.y0 + below(4));
  } else if (roll < 68) {
    c.type = CmdType::SetPolicy;
    c.def = static_cast<u16>(below(static_cast<i32>(PolicyId::Count)));
    c.value = below(10) == 0 ? static_cast<i32>(next() % 100000) - 50000 : below(120);
  } else if (roll < 74) {
    c.type = CmdType::SetTrade;
    c.def = static_cast<u16>(below(static_cast<i32>(w.econ.goods)));
    c.value = below(4);
    c.x0 = below(1000);
  } else if (roll < 76) {
    c.type = CmdType::OpenRoute;
    c.def = static_cast<u16>(below(static_cast<i32>(w.routes.size()) + 1));
  } else if (roll < 78) {
    c.type = CmdType::PlaceUnder;
    c.def = static_cast<u16>(1 + below(2));
    c.x0 = near_x();
    c.y0 = near_y();
    c.x1 = c.x0 + below(11) - 5;
    c.y1 = c.y0;
    c.value = 1;
  } else if (roll < 84) {
    // Armies: raise at a random building, order, reshape, send away, disband.
    const i32 kind = below(5);
    const u16 unit = static_cast<u16>(below(military::kMaxUnits));
    if (kind == 0) {
      c.type = CmdType::RaiseUnit;
      c.def = static_cast<u16>(below(static_cast<i32>(D.units.size())));
      const u32 bi = static_cast<u32>(below(static_cast<i32>(std::max<u32>(1, w.buildings.ids.high_water()))));
      c.value = w.buildings.ids.alive_index(bi) ? static_cast<i32>(w.buildings.ids.id_at(bi).raw) : 0;
    } else if (kind == 1) {
      c.type = CmdType::UnitOrder;
      c.def = unit;
      c.value = below(6);
      c.x0 = near_x();
      c.y0 = near_y();
    } else if (kind == 2) {
      c.type = below(2) ? CmdType::UnitFormation : CmdType::UnitStance;
      c.def = unit;
      c.value = below(8);
    } else if (kind == 3) {
      c.type = CmdType::Campaign;
      c.def = unit;
      c.value = 1 + below(3);
    } else {
      c.type = CmdType::DisbandUnit;
      c.def = unit;
    }
  } else if (roll < 88) {
    // Disasters on purpose (fire, collapse, plague, riot, raid, ...).
    c.type = CmdType::Trigger;
    c.def = static_cast<u16>(below(static_cast<i32>(TriggerId::Count)));
    c.x0 = near_x();
    c.y0 = near_y();
    c.value = below(4);
  } else if (roll < 91) {
    c.type = CmdType::BuySlaves;
    c.value = below(60);
  } else if (roll < 93) {
    c.type = CmdType::Festival;
    c.def = static_cast<u16>(below(static_cast<i32>(D.gods.size()) + 1));
    c.value = below(3);
  } else if (roll < 95) {
    c.type = CmdType::TakeLoan;
    c.value = below(20000);
  } else if (roll < 97) {
    c.type = CmdType::Answer;
    c.def = w.messages.empty() ? 0 : w.messages[static_cast<size_t>(below(static_cast<i32>(w.messages.size())))].id;
    c.value = below(5);
  } else if (roll < 99) {
    c.type = CmdType::FulfillRequest;
    c.def = static_cast<u16>(below(static_cast<i32>(w.next_request_id) + 2));
  } else {
    c.type = CmdType::Research;
    c.def = static_cast<u16>(below(static_cast<i32>(D.techniques.size()) + 1));
  }
  w.push(c);
}

}  // namespace urbs::sim
