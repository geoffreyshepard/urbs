#include "game/style.hpp"

#include <imgui.h>

#include <filesystem>

#include "game/ui_skin.hpp"

namespace urbs::game {

namespace {
UiFonts g_fonts;
float g_scale = 1.0f;
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

bool skinned() { return ui_skin() && ui_skin()->ready(); }

ImVec2 window_padding(Skin skin) {
  const float k = g_scale;
  switch (skin) {
    case Skin::Panel:
      return ImVec2(std::round(22 * k), std::round(20 * k));
    case Skin::Card:
      return ImVec2(std::round(12 * k), std::round(8 * k));
    case Skin::Column:
      return ImVec2(std::round(6 * k), std::round(14 * k));
    default:
      return ImGui::GetStyle().WindowPadding;
  }
}

bool begin_window(const char* name, bool* open, ImGuiWindowFlags flags, Skin skin) {
  int colors = 0, vars = 0;
  if (skin == Skin::None) {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    colors = 1;
  } else if (skinned()) {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, skin_col4(skin));
    colors = 1;
    if (skin == Skin::Panel || skin == Skin::Card || skin == Skin::Column) {
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, window_padding(skin));
      vars = 1;
    }
    if (skin == Skin::Panel) {
      // An invisible border as wide as the stone keeps scrolled content and the scrollbar inside the frame.
      const float b = std::round(ui_skin()->border(Skin::Panel));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, b);
      ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, ImGui::GetStyle().ScrollbarSize + std::round(b * 0.5f));
      ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, skin_col4(Skin::PScrollTrack));
      ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, skin_col4(Skin::PScrollGrab));
      ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, skin_col4(Skin::PScrollGrabHov));
      ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, skin_col4(Skin::PScrollGrabHov));
      vars += 2;
      colors += 5;
    }
  }
  ImGui::PushFont(g_fonts.heading, 0.0f);
  const bool r = ImGui::Begin(name, open, flags);
  ImGui::PopFont();
  ImGui::PopStyleVar(vars);
  ImGui::PopStyleColor(colors);
  return r;
}

void heading(const char* text, float scale) {
  ImGui::PushFont(g_fonts.heading, ImGui::GetStyle().FontSizeBase * scale);
  ImGui::TextUnformatted(text);
  ImGui::PopFont();
}

float ui_scale() { return g_scale; }

void apply_roman_style(float scale) {
  g_scale = scale;
  ImGuiStyle& s = ImGui::GetStyle();
  s = ImGuiStyle();
  const bool sk = skinned();
  // The chrome draws every frame, so ImGui's own rounding and borders are off: a skinned rectangle must stay a
  // plain rectangle for UiSkin::process to find it.
  s.WindowRounding = s.ChildRounding = s.PopupRounding = s.FrameRounding = s.GrabRounding = s.ScrollbarRounding =
      s.TabRounding = sk ? 0.0f : 2.0f;
  s.WindowBorderSize = sk ? 0.0f : 2.0f;
  s.ChildBorderSize = s.PopupBorderSize = s.FrameBorderSize = sk ? 0.0f : 1.0f;
  s.WindowPadding = ImVec2(12, 8);
  s.FramePadding = ImVec2(8, 4);
  s.ItemSpacing = ImVec2(8, 6);
  s.ItemInnerSpacing = ImVec2(6, 4);
  s.ScrollbarSize = 13;
  s.GrabMinSize = 12;
  s.WindowMinSize = ImVec2(24, 24);
  s.SeparatorTextBorderSize = 1;
  ImVec4* c = s.Colors;
  const ImVec4 ink = col4(kColInk), parchment(0.95f, 0.91f, 0.80f, 0.97f), parchment2(0.85f, 0.77f, 0.60f, 1),
               red = col4(kColRed), red_hi(0.70f, 0.29f, 0.20f, 1), red_lo(0.40f, 0.13f, 0.09f, 1),
               ochre(0.80f, 0.62f, 0.30f, 1), bronze = col4(kColBronze);
  c[ImGuiCol_Text] = ink;
  c[ImGuiCol_TextDisabled] = col4(kColInk2);
  c[ImGuiCol_Border] = bronze;
  c[ImGuiCol_Separator] = bronze;
  c[ImGuiCol_SeparatorHovered] = red_hi;
  c[ImGuiCol_SeparatorActive] = red;
  c[ImGuiCol_TitleBg] = red_lo;
  c[ImGuiCol_TitleBgActive] = red;
  c[ImGuiCol_TitleBgCollapsed] = red_lo;
  c[ImGuiCol_MenuBarBg] = parchment2;
  c[ImGuiCol_CheckMark] = red;
  c[ImGuiCol_Tab] = ochre;
  c[ImGuiCol_TabHovered] = red_hi;
  c[ImGuiCol_TabSelected] = red;
  c[ImGuiCol_PlotHistogram] = red;
  c[ImGuiCol_TableHeaderBg] = ImVec4(0.85f, 0.77f, 0.60f, 0.5f);
  c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_TableRowBgAlt] = ImVec4(0.55f, 0.40f, 0.18f, 0.07f);
  c[ImGuiCol_TableBorderLight] = ImVec4(0.55f, 0.40f, 0.18f, 0.25f);
  c[ImGuiCol_TableBorderStrong] = ImVec4(0.55f, 0.40f, 0.18f, 0.5f);
  c[ImGuiCol_TextSelectedBg] = ImVec4(0.85f, 0.62f, 0.28f, 0.35f);
  c[ImGuiCol_TextLink] = col4(kColLink);
  c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ResizeGripHovered] = ochre;
  c[ImGuiCol_ResizeGripActive] = red;
  c[ImGuiCol_NavCursor] = red_hi;
  c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.10f, 0.06f, 0.03f, 0.45f);
  c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_InputTextCursor] = ink;
  c[ImGuiCol_TabSelectedOverline] = red;
  c[ImGuiCol_TabDimmed] = ochre;
  c[ImGuiCol_TabDimmedSelected] = red_lo;
  c[ImGuiCol_TabDimmedSelectedOverline] = red;
  c[ImGuiCol_PlotLines] = bronze;
  c[ImGuiCol_PlotLinesHovered] = red;
  c[ImGuiCol_PlotHistogramHovered] = red_hi;
  c[ImGuiCol_TreeLines] = bronze;
  c[ImGuiCol_DragDropTarget] = red;
  c[ImGuiCol_DragDropTargetBg] = ImVec4(0.56f, 0.20f, 0.14f, 0.15f);
  c[ImGuiCol_UnsavedMarker] = red;
  c[ImGuiCol_NavWindowingHighlight] = ochre;
  c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.10f, 0.06f, 0.03f, 0.35f);
  if (sk) {
    c[ImGuiCol_WindowBg] = skin_col4(Skin::Panel);
    c[ImGuiCol_PopupBg] = skin_col4(Skin::Card);
    c[ImGuiCol_ChildBg] = skin_col4(Skin::Inset);
    c[ImGuiCol_FrameBg] = skin_col4(Skin::Frame);
    c[ImGuiCol_FrameBgHovered] = skin_col4(Skin::FrameHov);
    c[ImGuiCol_FrameBgActive] = skin_col4(Skin::FrameAct);
    c[ImGuiCol_CheckboxSelectedBg] = skin_col4(Skin::Frame);
    c[ImGuiCol_Button] = skin_col4(Skin::Button);
    c[ImGuiCol_ButtonHovered] = skin_col4(Skin::ButtonHov);
    c[ImGuiCol_ButtonActive] = skin_col4(Skin::ButtonAct);
    c[ImGuiCol_Header] = skin_col4(Skin::HiliteSel);
    c[ImGuiCol_HeaderHovered] = skin_col4(Skin::Hilite);
    c[ImGuiCol_HeaderActive] = skin_col4(Skin::HiliteSel);
    c[ImGuiCol_SliderGrab] = skin_col4(Skin::Grab);
    c[ImGuiCol_SliderGrabActive] = skin_col4(Skin::GrabAct);
    c[ImGuiCol_ScrollbarBg] = skin_col4(Skin::ScrollTrack);
    c[ImGuiCol_ScrollbarGrab] = skin_col4(Skin::ScrollGrab);
    c[ImGuiCol_ScrollbarGrabHovered] = skin_col4(Skin::ScrollGrabHov);
    c[ImGuiCol_ScrollbarGrabActive] = skin_col4(Skin::ScrollGrabHov);
  } else {
    c[ImGuiCol_WindowBg] = parchment;
    c[ImGuiCol_PopupBg] = parchment;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = parchment2;
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.93f, 0.80f, 0.58f, 1);
    c[ImGuiCol_FrameBgActive] = ochre;
    c[ImGuiCol_CheckboxSelectedBg] = parchment2;
    c[ImGuiCol_Button] = ochre;
    c[ImGuiCol_ButtonHovered] = red_hi;
    c[ImGuiCol_ButtonActive] = red;
    c[ImGuiCol_Header] = ImVec4(0.80f, 0.62f, 0.30f, 0.6f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.82f, 0.42f, 0.26f, 0.6f);
    c[ImGuiCol_HeaderActive] = red;
    c[ImGuiCol_SliderGrab] = red;
    c[ImGuiCol_SliderGrabActive] = red_lo;
    c[ImGuiCol_ScrollbarBg] = parchment2;
    c[ImGuiCol_ScrollbarGrab] = ochre;
    c[ImGuiCol_ScrollbarGrabHovered] = red_hi;
    c[ImGuiCol_ScrollbarGrabActive] = red;
  }
  s.ScaleAllSizes(scale);
  ImGui::GetIO().FontGlobalScale = scale;
  if (ui_skin()) ui_skin()->set_scale(scale);
}

}  // namespace urbs::game
