#pragma once
#include <imgui.h>

#include <string>

#include "core/types.hpp"

namespace urbs::game {

class UiIcons;

// The pieces every panel is built from, in the look of the top bar and the build rail: a parchment panel docked to
// the screen (no title bar), a header with a picture, small-caps section labels, the top bar's bars, and compact
// controls. Sizes follow the font size, so the UI scale setting applies.

// Where a panel sits. Report panels hang from their top-bar button at the right edge (`right`, with a notch under
// the button at `notch_x`); the inspector sits beside the build rail.
struct Dock {
  ImVec2 pos;  // top-left corner, or top-right when `right`
  bool right = false;
  float width = 0;     // outer width
  float max_h = 0;     // taller content scrolls
  float notch_x = -1;  // screen x of the button the panel hangs from (< 0: none)
};

// Begins a docked panel window; always pair with end_panel(). Returns false when it is collapsed or clipped.
bool begin_panel(const char* id, const Dock& d);
void end_panel();

// Header: a framed picture (an icon from UiIcons, or a building thumbnail when `thumb` is set), the title in Cinzel,
// one summary line under it, and a close button. Returns false when the close button was clicked.
bool panel_header(const UiIcons& icons, const std::string& picture, bool thumb, const std::string& title,
                  const std::string& sub, ImU32 sub_col = 0);
// A coloured tag ("Plague", "On fire") on the current line.
void tag(const char* label, ImU32 col);
// A thin rule across the panel, then a small-caps label.
void section(const char* label);
void divider();

// The top bar's bar, larger: `frac` 0..1 filled in `col`, with a goal marker at `goal` (0..1) when >= 0.
void bar(float width, float frac, ImU32 col, float goal = -1);
// One row: label on the left, a bar in the middle, the value right-aligned. `tip` shows on hover.
void meter_row(const char* label, float frac, ImU32 col, const std::string& value, float goal = -1,
               const char* tip = nullptr);
// Text right-aligned at the panel's right edge, on the current line.
void text_right(const std::string& s, ImU32 col = 0, bool bold = true);
// A label/value row with no bar.
void value_row(const char* label, const std::string& value, ImU32 col = 0);

// Compact controls. Each returns true when the value changed.
bool stepper(const char* id, int* v, int lo, int hi, const char* unit = "");
bool choice(const char* id, int* v, const char* const* options, int n, float each_w = 0);
// A button with a coin and a cost after its label; disabled when the city cannot afford it.
bool coin_button(const UiIcons& icons, const char* label, i64 cost, bool afford);
// A tick in a green circle (done) or an empty circle.
void check_mark(bool ok);
// A goods icon on the current line, and a framed icon + name chip.
void good_icon(const UiIcons& icons, const std::string& good_id, float size = 0);
void good_chip(const UiIcons& icons, const std::string& good_id, const std::string& name);
// Terracotta text that acts as a button.
bool link(const char* label);

std::string with_commas(i64 v);
// "+1,024" / "-72"
std::string signed_commas(i64 v);

}  // namespace urbs::game
