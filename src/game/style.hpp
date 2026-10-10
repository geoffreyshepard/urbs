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
}  // namespace urbs::game
