#pragma once
#include <imgui.h>

#include <cmath>
#include <string>

#include "game/ui_skin.hpp"

namespace urbs::game {
// Roman look for the UI: carved travertine frames with bronze trim, parchment panels, Pompeian-red title plaques,
// dark ink text (docs/07-art-pipeline.md, "UI style"). The chrome comes from UiSkin; without it the same colours
// fall back to flat fills.
void apply_roman_style(float scale);
// The UI scale setting last applied.
float ui_scale();
// True when the chrome pictures are loaded (otherwise everything draws flat).
bool skinned();

// UI typefaces (assets/fonts, SIL OFL): Alegreya Sans for text, Cinzel (Roman capitals) for titles.
struct UiFonts {
  ImFont* body = nullptr;
  ImFont* bold = nullptr;
  ImFont* heading = nullptr;
};
const UiFonts& ui_fonts();
// Loads the fonts into the ImGui atlas; falls back to the built-in font if a file is missing.
void load_ui_fonts(const std::string& data_root);

// ImGui::Begin with the title bar set in the heading face, the window drawn as `skin` (Panel: the carved frame;
// Card: a slim bronze-edged card for toasts; Cornice / Column: the top bar and the build rail; None: no background
// of its own) and the padding that frame needs.
bool begin_window(const char* name, bool* open = nullptr, ImGuiWindowFlags flags = 0, Skin skin = Skin::Panel);
// Inside a dialog's window (a letter, the pause menu): keep it above the side panels, even one opened
// after it.
void keep_in_front();
// The padding a window drawn as `skin` gets.
ImVec2 window_padding(Skin skin);
// A line of text in the heading face, `scale` times the body size.
void heading(const char* text, float scale = 1.35f);

// Colours that read on parchment, shared by the top bar and the panels (text and bars).
inline constexpr ImU32 kColInk = IM_COL32(43, 28, 18, 255);       // text
inline constexpr ImU32 kColInk2 = IM_COL32(104, 84, 62, 255);     // secondary text, captions
inline constexpr ImU32 kColCream = IM_COL32(250, 241, 220, 255);  // text on Pompeian red
inline constexpr ImU32 kColRed = IM_COL32(142, 51, 36, 255);      // Pompeian red: titles, the selected choice
inline constexpr ImU32 kColBronze = IM_COL32(125, 82, 38, 255);   // strokes, rules
inline constexpr ImU32 kColSection = IM_COL32(122, 46, 30, 255);  // section labels (Cinzel small caps)
inline constexpr ImU32 kColLink = IM_COL32(148, 52, 30, 255);     // text that acts as a button
inline constexpr ImU32 kColGood = IM_COL32(78, 120, 50, 255);     // a goal met, money gained, a route open
inline constexpr ImU32 kColWarn = IM_COL32(214, 150, 40, 255);    // short of a goal (bars)
inline constexpr ImU32 kColBad = IM_COL32(170, 40, 24, 255);      // money lost, something missing, danger
inline constexpr ImU32 kColPlain = IM_COL32(204, 158, 77, 255);   // a bar with no goal
inline constexpr ImU32 kColTrack = IM_COL32(205, 186, 140, 255);  // the empty part of a bar
inline constexpr ImU32 kColTick = IM_COL32(103, 49, 29, 255);     // a goal marker
inline ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }
}  // namespace urbs::game
