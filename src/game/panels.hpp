#pragma once
#include <string>

#include "render/renderer.hpp"
#include "sim/world.hpp"

namespace urbs::game {

class WorldView;

// Inspection and management panels (debug-UI toolkit for now; reskinned in M10/M11).
struct Selection {
  CitizenId citizen;
  BuildingId building;
  bool follow = false;
};

std::string requirement_text(const data::Defs& defs, const data::HousingReq& r);
std::string alert_text(const sim::World& w, const sim::Alert& a);

void draw_citizen_panel(sim::World& w, Selection& sel);
void draw_building_panel(sim::World& w, Selection& sel);
void draw_labor_panel(sim::World& w, bool* open);
// Returns the building to jump to if an alert was clicked.
BuildingId draw_alerts_panel(const sim::World& w, bool* open);
void draw_selection(const sim::World& w, const Selection& sel, render::Renderer& r, f32 alpha);

}  // namespace urbs::game
