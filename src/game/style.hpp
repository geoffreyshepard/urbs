#pragma once
#include <imgui.h>

#include <string>

namespace urbs::game {
// Roman look for the UI: travertine panels, terracotta accents, dark ink text.
void apply_roman_style(float scale);

// UI typefaces (assets/fonts, SIL OFL): Alegreya Sans for text, Cinzel (Roman capitals) for titles.
struct UiFonts {
  ImFont* body = nullptr;
  ImFont* bold = nullptr;
  ImFont* heading = nullptr;
};
const UiFonts& ui_fonts();
// Loads the fonts into the ImGui atlas; falls back to the built-in font if a file is missing.
void load_ui_fonts(const std::string& data_root);

// ImGui::Begin with the title bar set in the heading face.
bool begin_window(const char* name, bool* open = nullptr, ImGuiWindowFlags flags = 0);
// A line of text in the heading face, `scale` times the body size.
void heading(const char* text, float scale = 1.35f);

// Colours that read on parchment, shared by the top bar and the panels (text and bars).
inline constexpr ImU32 kColGood = IM_COL32(78, 120, 50, 255);     // a goal met, money gained, a route open
inline constexpr ImU32 kColWarn = IM_COL32(214, 150, 40, 255);    // short of a goal (bars)
inline constexpr ImU32 kColBad = IM_COL32(170, 40, 24, 255);      // money lost, something missing, danger
inline constexpr ImU32 kColPlain = IM_COL32(204, 158, 77, 255);   // a bar with no goal
inline constexpr ImU32 kColTrack = IM_COL32(205, 186, 140, 255);  // the empty part of a bar
inline constexpr ImU32 kColTick = IM_COL32(103, 49, 29, 255);     // a goal marker
inline ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }
}  // namespace urbs::game
