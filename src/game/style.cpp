#include "game/style.hpp"

#include <imgui.h>

#include <filesystem>

namespace urbs::game {

namespace {
UiFonts g_fonts;
constexpr float kBodyPx = 17.0f;
}  // namespace

const UiFonts& ui_fonts() { return g_fonts; }

void load_ui_fonts(const std::string& data_root) {
  ImGuiIO& io = ImGui::GetIO();
  auto load = [&](const char* file) -> ImFont* {
    const std::string path = data_root + "assets/fonts/" + file;
    if (!std::filesystem::exists(path)) return nullptr;
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    return io.Fonts->AddFontFromFileTTF(path.c_str(), kBodyPx, &cfg);
  };
  g_fonts.body = load("AlegreyaSans-Regular.ttf");  // first font added is the default
  g_fonts.bold = load("AlegreyaSans-Bold.ttf");
  g_fonts.heading = load("Cinzel.ttf");
  if (!g_fonts.body) g_fonts.body = io.Fonts->AddFontDefault();
  if (!g_fonts.bold) g_fonts.bold = g_fonts.body;
  if (!g_fonts.heading) g_fonts.heading = g_fonts.bold;
}

bool begin_window(const char* name, bool* open, ImGuiWindowFlags flags) {
  ImGui::PushFont(g_fonts.heading, 0.0f);
  const bool r = ImGui::Begin(name, open, flags);
  ImGui::PopFont();
  return r;
}

void heading(const char* text, float scale) {
  ImGui::PushFont(g_fonts.heading, ImGui::GetStyle().FontSizeBase * scale);
  ImGui::TextUnformatted(text);
  ImGui::PopFont();
}

void apply_roman_style(float scale) {
  ImGuiStyle& s = ImGui::GetStyle();
  s = ImGuiStyle();
  s.WindowRounding = 2;
  s.FrameRounding = 2;
  s.GrabRounding = 2;
  s.ScrollbarRounding = 2;
  s.WindowBorderSize = 2;
  s.FrameBorderSize = 1;
  s.WindowPadding = ImVec2(10, 8);
  s.FramePadding = ImVec2(6, 3);
  s.ItemSpacing = ImVec2(6, 5);
  ImVec4* c = s.Colors;
  const ImVec4 ink(0.17f, 0.11f, 0.07f, 1), parchment(0.91f, 0.85f, 0.71f, 0.97f), parchment2(0.85f, 0.77f, 0.60f, 1),
      terracotta(0.72f, 0.33f, 0.20f, 1), terracotta_hi(0.82f, 0.42f, 0.26f, 1), terracotta_lo(0.58f, 0.25f, 0.15f, 1),
      ochre(0.80f, 0.62f, 0.30f, 1), border(0.45f, 0.30f, 0.18f, 1);
  c[ImGuiCol_Text] = ink;
  c[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.34f, 0.25f, 1);
  c[ImGuiCol_WindowBg] = parchment;
  c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_PopupBg] = parchment;
  c[ImGuiCol_Border] = border;
  c[ImGuiCol_FrameBg] = parchment2;
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.93f, 0.80f, 0.58f, 1);
  c[ImGuiCol_FrameBgActive] = ochre;
  c[ImGuiCol_TitleBg] = terracotta_lo;
  c[ImGuiCol_TitleBgActive] = terracotta;
  c[ImGuiCol_TitleBgCollapsed] = terracotta_lo;
  c[ImGuiCol_MenuBarBg] = parchment2;
  c[ImGuiCol_ScrollbarBg] = parchment2;
  c[ImGuiCol_ScrollbarGrab] = ochre;
  c[ImGuiCol_ScrollbarGrabHovered] = terracotta_hi;
  c[ImGuiCol_ScrollbarGrabActive] = terracotta;
  c[ImGuiCol_CheckMark] = terracotta_lo;
  c[ImGuiCol_SliderGrab] = terracotta;
  c[ImGuiCol_SliderGrabActive] = terracotta_lo;
  c[ImGuiCol_Button] = ochre;
  c[ImGuiCol_ButtonHovered] = terracotta_hi;
  c[ImGuiCol_ButtonActive] = terracotta;
  c[ImGuiCol_Header] = ImVec4(0.80f, 0.62f, 0.30f, 0.6f);
  c[ImGuiCol_HeaderHovered] = ImVec4(0.82f, 0.42f, 0.26f, 0.6f);
  c[ImGuiCol_HeaderActive] = terracotta;
  c[ImGuiCol_Separator] = border;
  c[ImGuiCol_Tab] = ochre;
  c[ImGuiCol_TabHovered] = terracotta_hi;
  c[ImGuiCol_TabSelected] = terracotta;
  c[ImGuiCol_PlotHistogram] = terracotta;
  c[ImGuiCol_TableHeaderBg] = parchment2;
  c[ImGuiCol_TableRowBgAlt] = ImVec4(0.85f, 0.77f, 0.60f, 0.4f);
  c[ImGuiCol_ResizeGrip] = ochre;
  c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.1f, 0.06f, 0.03f, 0.5f);
  s.ScaleAllSizes(scale);
  ImGui::GetIO().FontGlobalScale = scale;
}

}  // namespace urbs::game
