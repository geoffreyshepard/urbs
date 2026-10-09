#pragma once
#include <vector>

#include "sim/world.hpp"

namespace urbs::game {

enum class Overlay : u8 { None, Desirability, Water, Pipes, Service, Housing, Elevation, Fertility, Groundwater, Count };
inline constexpr const char* overlay_name(Overlay o) {
  constexpr const char* n[] = {"None", "Desirability", "Water access", "Pipes & sewers", "Service coverage",
                               "Housing level", "Elevation", "Fertility", "Groundwater"};
  return n[static_cast<int>(o)];
}

// Computes the per-tile RGBA overlay for the current mode. `service` selects the service map.
void build_overlay(const sim::World& w, Overlay mode, ServiceId service, std::vector<u32>& out);

}  // namespace urbs::game
