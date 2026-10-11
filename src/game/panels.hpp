#pragma once
#include <string>

#include "render/renderer.hpp"
#include "sim/world.hpp"

namespace urbs::game {

class WorldView;

// Inspection and management panels, built from the pieces in game/panel_kit.hpp.
struct Selection {
  CitizenId citizen;
  BuildingId building;
  u16 unit = 0xFFFF;
  bool follow = false;
};

std::string requirement_text(const data::Defs& defs, const data::HousingReq& r);
std::string alert_text(const sim::World& w, const sim::Alert& a);

struct Dock;
class UiIcons;
// The selected citizen, or else the selected building, in a panel beside the build rail.
void draw_inspector(sim::World& w, Selection& sel, const UiIcons& icons, const Dock& dock);
void draw_labor_panel(sim::World& w, const UiIcons& icons, const Dock& dock, bool* open);
void draw_finance_panel(sim::World& w, const UiIcons& icons, const Dock& dock, bool* open);
void draw_trade_panel(sim::World& w, const UiIcons& icons, const Dock& dock, bool* open);
void draw_religion_panel(sim::World& w, const UiIcons& icons, const Dock& dock, bool* open);
void draw_military_panel(sim::World& w, Selection& sel, const UiIcons& icons, const Dock& dock, bool* open);
void draw_rome_panel(sim::World& w, const UiIcons& icons, const Dock& dock, bool* open);
void draw_advisors_panel(const sim::World& w, const UiIcons& icons, int* advisor, const Dock& dock, bool* open);
// New messages and choices (shown until read/answered). Returns true if a mission-end window is open.
void draw_messages(sim::World& w, const UiIcons& icons, u16* last_seen);
void draw_mission_end(const sim::World& w, bool* dismissed);
void draw_threats_panel(sim::World& w, const Selection& sel, bool* open);
// Returns the building to jump to if an alert was clicked.
BuildingId draw_alerts_panel(const sim::World& w, const UiIcons& icons, const Dock& dock, bool* open);
void draw_selection(const sim::World& w, const Selection& sel, render::Renderer& r, f32 alpha);

}  // namespace urbs::game
