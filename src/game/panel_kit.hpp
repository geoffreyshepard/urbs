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
// Screen x of the current panel's content edges (set by begin_panel, menu_title and title_band).
float panel_left();
float panel_right();
// Takes the current window's content edges as the panel's (dialogs that start without menu_title).
void content_bounds();
// A dialog's button: centred in the panel, a common width (or `w`), a little taller than a plain button.
bool dialog_button(const char* label, float w = 0);

// How a picture is framed: an icon on a round bronze medallion, a building thumbnail on a square ivory tile, or a
// portrait (gods, advisors) filling a square tile.
enum class Pic : u8 { Icon, Thumb, Portrait };
// Header: the panel's title in Cinzel on a red band across its top, a framed picture hanging from the band (`kind`:
// an icon, a building thumbnail, a portrait), one summary line under the band, and a close button at the band's right
// end. Returns false when the close button was clicked.
bool panel_header(const UiIcons& icons, const std::string& picture, Pic kind, const std::string& title,
                  const std::string& sub, ImU32 sub_col = 0);
// A framed picture; `p` is the top-left corner, `size` the side.
void framed_picture(const UiIcons& icons, const std::string& picture, Pic kind, ImVec2 p, float size);
// A round bronze close button centred on `c`, `d` across. Returns true when clicked. Moves the cursor.
bool close_button(ImVec2 c, float d);
// A window's title on the red band across its top (windows without a picture, such as the build list).
void title_band(const char* title);
// A menu's title (pause, settings, save and load, setup screens): Cinzel on a red tabula ansata, centred.
void menu_title(const char* title);
// A coloured tag ("Plague", "On fire") on the current line.
void tag(const char* label, ImU32 col);
// A thin rule across the panel, then a small-caps label.
void section(const char* label);
void divider();

// An inlaid bar at `p` (top-left), `width` x `h`: `frac` 0..1 filled in `col`, a goal marker at `goal` (0..1) when
// >= 0. Used by the top bar and by bar().
void draw_bar(ImDrawList* dl, ImVec2 p, float width, float h, float frac, ImU32 col, float goal = -1);
// The top bar's bar, larger, on the current line.
void bar(float width, float frac, ImU32 col, float goal = -1);
// One row: label on the left, a bar in the middle, the value right-aligned. `tip` shows on hover.
void meter_row(const char* label, float frac, ImU32 col, const std::string& value, float goal = -1,
               const char* tip = nullptr);
// Text right-aligned at the panel's right edge, on the current line.
void text_right(const std::string& s, ImU32 col = 0, bool bold = true);
// Right-aligned text in the current table cell (numbers in ledgers). `col` 0 keeps the text colour.
void cell_right(const std::string& s, ImU32 col = 0, bool bold = false);
// Small-caps column headings for a table, right-aligned except the first.
void table_headings(const char* const* names, int n);
// A label/value row with no bar.
void value_row(const char* label, const std::string& value, ImU32 col = 0);

// A button that shows a selected state (Pompeian red, cream lettering) when `on`. Returns true when clicked.
bool selectable_button(const char* label, bool on, ImVec2 size = ImVec2(0, 0));
// Compact controls. Each returns true when the value changed.
bool stepper(const char* id, int* v, int lo, int hi, const char* unit = "", int step = 1);
bool choice(const char* id, int* v, const char* const* options, int n, float each_w = 0);
// A button with a coin and a cost after its label; disabled when the city cannot afford it.
bool coin_button(const UiIcons& icons, const char* label, i64 cost, bool afford);
// A tick in a green circle (done) or an empty circle.
void check_mark(bool ok);
// An icon (any cached sprite) or a building thumbnail on the current line, centred on the text line.
void inline_icon(const UiIcons& icons, const std::string& sprite, float size = 0);
void inline_thumb(const UiIcons& icons, const std::string& def_id, float size = 0);
// A goods icon on the current line, and a framed icon + name chip.
void good_icon(const UiIcons& icons, const std::string& good_id, float size = 0);
void good_chip(const UiIcons& icons, const std::string& good_id, const std::string& name);
// Terracotta text that acts as a button.
bool link(const char* label);

// A toast's text: wrapped at `max_w` (points) and cut to `max_lines` lines, the last ending in an ellipsis when the
// text is longer (a tooltip then shows all of it). Returns true when the text was clicked (`clickable`). The text
// is drawn with the current text colour.
bool toast_text(const std::string& text, float max_w, int max_lines, bool clickable);
// The size toast_text() will take, in the current font (to size a toast's window before it is drawn).
ImVec2 toast_size(const std::string& text, float max_w, int max_lines);

std::string with_commas(i64 v);
// "+1,024" / "-72"
std::string signed_commas(i64 v);

}  // namespace urbs::game
