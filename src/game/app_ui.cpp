// Screens and HUD: main menu, campaign map and briefing, sandbox setup, in-game HUD, pause,
// save slots, settings, research, build menu, goods, help, debug.
#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "game/app.hpp"
#include "game/panel_kit.hpp"
#include "game/style.hpp"
#include "render/iso.hpp"
#include "save/save.hpp"

namespace urbs::game {

namespace {
ImVec2 display() { return ImGui::GetIO().DisplaySize; }
void center_next(float w, float h) {
  ImGui::SetNextWindowPos(ImVec2(display().x * 0.5f, display().y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
}
bool big_button(const char* label, float w = 260) { return ImGui::Button(label, ImVec2(w, 34)); }
const char* kDifficulty[] = {"Otium (relaxed)", "Civitas (normal)", "Imperium (hard)"};
}  // namespace

void App::draw_ui() {
  switch (screen_) {
    case Screen::Menu: draw_main_menu(); break;
    case Screen::Campaign: draw_campaign(); break;
    case Screen::Sandbox: draw_sandbox_setup(); break;
    case Screen::Mods: draw_mods(); break;
    case Screen::Custom: draw_custom(); break;
    case Screen::Editor: draw_editor(); break;
    case Screen::Playing:
      draw_hud();
      draw_build_menu();
      {
        Dock d;
        d.pos = ImVec2(rail_r_ + 6, hud_h_ + 8);
        d.width = std::round(ImGui::GetFontSize() * 25);
        d.max_h = display().y - d.pos.y - 8;
        draw_inspector(*world_, sel_, icons_, d);
      }
      draw_threats_panel(*world_, sel_, &show_threats_);
      if (panel_ != Panel::None) {
        const Dock pd = panel_dock(panel_);
        bool open = true;
        BuildingId jump;
        switch (panel_) {
          case Panel::Rome:
            draw_rome_panel(*world_, icons_, pd, &open);
            break;
          case Panel::Advisors:
            draw_advisors_panel(*world_, icons_, &advisor_, pd, &open);
            break;
          case Panel::Finance:
            draw_finance_panel(*world_, icons_, pd, &open);
            break;
          case Panel::Goods:
            draw_stock_window(pd, &open);
            break;
          case Panel::Trade:
            draw_trade_panel(*world_, icons_, pd, &open);
            break;
          case Panel::Labor:
            draw_labor_panel(*world_, icons_, pd, &open);
            break;
          case Panel::Religion:
            draw_religion_panel(*world_, icons_, pd, &open);
            break;
          case Panel::Military:
            draw_military_panel(*world_, sel_, icons_, pd, &open);
            break;
          case Panel::Research:
            draw_research(pd, &open);
            break;
          case Panel::Alerts:
            jump = draw_alerts_panel(*world_, icons_, pd, &open);
            break;
          case Panel::Help:
            draw_help(pd, &open);
            break;
          case Panel::None:
            break;
        }
        if (!open) panel_ = Panel::None;
        if (jump.valid() && world_->buildings.alive(jump)) {
          const auto& jb = world_->buildings[jump];
          renderer_.camera().center =
              render::iso::from_tile(static_cast<f32>(jb.x) + static_cast<f32>(jb.w) * 0.5f,
                                     static_cast<f32>(jb.y) + static_cast<f32>(jb.h) * 0.5f,
                                     render::iso::level_at(world_->map, jb.x, jb.y));
          sel_.building = jump;
        }
      }
      if (show_debug_) draw_debug_ui();
      draw_messages(*world_, icons_, &last_message_);
      if (world_->mission != sim::MissionState::Running && !end_dismissed_) {
        const MissionInfo* mi =
            mission_index_ >= 0 && mission_index_ < static_cast<int>(campaign_.missions.size())
                ? &campaign_.missions[static_cast<size_t>(mission_index_)]
                : nullptr;
        const bool finale = mi && world_->mission == sim::MissionState::Won && !mi->epilogue.empty();
        const float end_w = finale ? 600.0f : 460.0f;
        center_next(end_w, 0);
        ImGui::SetNextWindowSize(ImVec2(end_w, 0), ImGuiCond_Always);
        begin_window("##end", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
        if (world_->mission == sim::MissionState::Won) {
          menu_title("Victoria!");
          ImGui::TextWrapped("%s", mi && !mi->victory.empty()
                                       ? mi->victory.c_str()
                                       : "The city has met every goal. Rome takes note of the name of its builder.");
          if (finale) {
            ImGui::Spacing();
            ImGui::BeginChild("epilogue", ImVec2(0, std::min(360.0f, display().y * 0.45f)), true);
            ImGui::TextWrapped("%s", mi->epilogue.c_str());
            ImGui::EndChild();
            ImGui::TextDisabled("The campaign is complete.");
          }
          ImGui::Spacing();
          ImGui::Text("Personal savings: %s denarii", with_commas(profile_.savings).c_str());
          if (mission_index_ >= 0) {
            if (big_button("To the province map")) {
              const auto& id = campaign_.missions[static_cast<size_t>(mission_index_)].id;
              if (!profile_.done(id)) {
                profile_.completed.push_back(id);
                music_.promoted();
              }
              profile_.save(user_dir_);
              selected_mission_ = profile_.next_index(campaign_);
              menu_background();
              screen_ = Screen::Campaign;
            }
          }
          if (big_button("Keep building this city")) end_dismissed_ = true;
        } else {
          menu_title("Recalled");
          ImGui::TextWrapped("%s", world_->lost_reason.c_str());
          ImGui::Spacing();
          ImGui::TextWrapped(mi ? "A letter recalls you to Rome, and the post goes to someone else. The city is not "
                                  "finished with you, though: load a save or begin again."
                                : "The city is lost. Load a save or begin again.");
          if (big_button("Load a saved game")) {
            show_saveload_ = true;
            saveload_saving_ = false;
            end_dismissed_ = true;
          }
          if (mission_index_ >= 0 && big_button("Restart the mission")) start_mission(mission_index_);
          if (big_button("Return to the main menu")) {
            menu_background();
            screen_ = Screen::Menu;
          }
        }
        ImGui::End();
      }
      if (paused_menu_) draw_pause();
      break;
  }
  if (show_settings_) draw_settings();
  if (show_saveload_) draw_saveload();
  if (status_time_ > 0 && !status_.empty()) {
    ImGui::SetNextWindowPos(ImVec2(display().x * 0.5f, display().y - 60), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.9f);
    begin_window("##status", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs);
    ImGui::TextUnformatted(status_.c_str());
    ImGui::End();
  }
}

void dim_backdrop(int alpha) {
  ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, IM_COL32(18, 12, 8, alpha));
}

namespace {

std::string roman(int n) {
  static const std::pair<int, const char*> kNum[] = {{50, "L"}, {40, "XL"}, {10, "X"}, {9, "IX"},
                                                     {5, "V"},  {4, "IV"},  {1, "I"}};
  std::string out;
  for (const auto& [v, s] : kNum)
    while (n >= v) {
      out += s;
      n -= v;
    }
  return out;
}

// "Capua - Campania" -> {"Capua", "Campania"}.
std::pair<std::string, std::string> split_name(const std::string& name) {
  const size_t k = name.find(" - ");
  if (k == std::string::npos) return {name, std::string()};
  return {name.substr(0, k), name.substr(k + 3)};
}

// Title-screen colours: light text on the dark column over the city.
constexpr ImU32 kMenuInk = IM_COL32(16, 11, 7, 228);
constexpr ImU32 kMenuText = IM_COL32(236, 224, 198, 255);
constexpr ImU32 kMenuMuted = IM_COL32(186, 168, 136, 255);
constexpr ImU32 kMenuGold = IM_COL32(232, 192, 112, 255);
constexpr ImU32 kTerracotta = IM_COL32(170, 66, 40, 255), kTerracottaHi = IM_COL32(196, 84, 52, 255),
                kTerracottaLo = IM_COL32(136, 50, 30, 255);

// A big call-to-action button in terracotta (the main menu's Campaign, the dossier's Begin).
bool primary_button(const char* label, ImVec2 size) {
  ImGui::PushStyleColor(ImGuiCol_Button, kTerracotta);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kTerracottaHi);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, kTerracottaLo);
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 244, 222, 255));
  ImGui::PushFont(ui_fonts().heading, std::round(ImGui::GetStyle().FontSizeBase * 1.3f));
  const bool r = ImGui::Button(label, size);
  ImGui::PopFont();
  ImGui::PopStyleColor(4);
  return r;
}

// A title-screen entry: Cinzel text on the dark column, a gold bar and a soft band when hovered, and an
// optional caption below it.
bool menu_entry(const char* label, const std::string& caption, float width, bool enabled = true) {
  const UiFonts& f = ui_fonts();
  const float fs = ImGui::GetStyle().FontSizeBase, ts = std::round(fs * 1.18f);
  const float h = std::round(caption.empty() ? fs * 2.0f : fs * 2.9f);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::PushID(label);
  const bool clicked = ImGui::InvisibleButton("##entry", ImVec2(width, h)) && enabled;
  ImGui::PopID();
  const bool hot = enabled && ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (hot) {
    dl->AddRectFilledMultiColor(p, ImVec2(p.x + width, p.y + h), IM_COL32(232, 192, 112, 46),
                                IM_COL32(232, 192, 112, 0), IM_COL32(232, 192, 112, 0), IM_COL32(232, 192, 112, 46));
    dl->AddRectFilled(p, ImVec2(p.x + 3, p.y + h), kMenuGold);
  }
  const float ty = caption.empty() ? p.y + (h - ts) * 0.5f : p.y + fs * 0.35f;
  dl->AddText(f.heading, ts, ImVec2(p.x + fs * 0.9f, ty),
              !enabled ? IM_COL32(120, 108, 90, 255)
              : hot    ? IM_COL32(255, 232, 178, 255)
                       : kMenuText,
              label);
  if (!caption.empty())
    dl->AddText(f.body, std::round(fs * 0.86f), ImVec2(p.x + fs * 0.9f, ty + ts + fs * 0.12f), kMenuMuted,
                caption.c_str());
  return clicked;
}

}  // namespace

void App::draw_main_menu() {
  const ImVec2 ds = display();
  const float fs = ImGui::GetStyle().FontSizeBase;
  const UiFonts& f = ui_fonts();
  // Shade the city: a dark column on the left that fades into it, and darker top and bottom edges.
  ImDrawList* bg = ImGui::GetBackgroundDrawList();
  const float col_w = std::round(std::clamp(ds.x * 0.30f, fs * 21.0f, fs * 30.0f));
  const ImU32 clear = IM_COL32(16, 11, 7, 0);
  bg->AddRectFilled(ImVec2(0, 0), ImVec2(col_w, ds.y), kMenuInk);
  bg->AddRectFilledMultiColor(ImVec2(col_w, 0), ImVec2(col_w + ds.x * 0.22f, ds.y), kMenuInk, clear, clear, kMenuInk);
  bg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(ds.x, ds.y * 0.16f), IM_COL32(16, 11, 7, 120),
                              IM_COL32(16, 11, 7, 120), clear, clear);
  bg->AddRectFilledMultiColor(ImVec2(0, ds.y * 0.78f), ImVec2(ds.x, ds.y), clear, clear, IM_COL32(16, 11, 7, 150),
                              IM_COL32(16, 11, 7, 150));
  bg->AddLine(ImVec2(col_w - 1, 0), ImVec2(col_w - 1, ds.y), IM_COL32(232, 192, 112, 0));

  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(col_w, ds.y), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
  ImGui::Begin("##menu", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar();
  const float pad = std::round(fs * 2.2f), inner = col_w - 2 * pad;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const bool dialog = show_saveload_ || show_settings_;  // the column waits behind an open dialog
  if (dialog) ImGui::BeginDisabled();
  float y = std::round(std::max(fs * 2.0f, ds.y * 0.07f));
  // The logo at its own aspect (the atlas knows its size), then the motto.
  {
    const render::Sprite logo = renderer_.atlas().find("ui/logo");
    const float aspect = logo.valid && logo.h > 0 ? static_cast<float>(logo.w) / static_cast<float>(logo.h) : 3.33f;
    const float lw = std::min(inner, fs * 20.0f), lh = std::round(lw / aspect);
    ImGui::SetCursorPos(ImVec2(pad - fs * 0.4f, y));
    if (!icons_.picture("ui/logo", ImVec2(lw, lh))) {
      ImGui::PushFont(f.heading, std::round(fs * 4.0f));
      ImGui::PushStyleColor(ImGuiCol_Text, kMenuText);
      ImGui::TextUnformatted("URBS");
      ImGui::PopStyleColor();
      ImGui::PopFont();
    }
    y = ImGui::GetCursorPosY() + fs * 0.3f;
    const char* motto = "Build Rome one family at a time";
    dl->AddText(f.body, std::round(fs * 1.08f), ImVec2(ImGui::GetWindowPos().x + pad, ImGui::GetWindowPos().y + y),
                kMenuMuted, motto);
    y += fs * 1.2f;
    const float rx = ImGui::GetWindowPos().x + pad, ry = ImGui::GetWindowPos().y + y + fs * 0.9f;
    dl->AddLine(ImVec2(rx, ry), ImVec2(rx + inner, ry), IM_COL32(232, 192, 112, 90), 1.0f);
    dl->AddQuadFilled(ImVec2(rx + inner * 0.5f, ry - 4), ImVec2(rx + inner * 0.5f + 4, ry),
                      ImVec2(rx + inner * 0.5f, ry + 4), ImVec2(rx + inner * 0.5f - 4, ry), kMenuGold);
    y += fs * 2.4f;
  }

  // The campaign is the main call to action; its caption says where the career stands.
  ImGui::SetCursorPos(ImVec2(pad, y));
  if (primary_button("Campaign", ImVec2(inner, std::round(fs * 2.9f)))) screen_ = Screen::Campaign;
  {
    const int next = profile_.next_index(campaign_);
    std::string cap;
    if (next >= 0 && next < static_cast<int>(campaign_.missions.size())) {
      const auto& m = campaign_.missions[static_cast<size_t>(next)];
      cap = (profile_.completed.empty() ? "Begins with " : "Next: ") + roman(next + 1) + ". " +
            split_name(m.name).first + "  \xC2\xB7  " + m.rank;
    } else if (!campaign_.missions.empty()) {
      cap = "Every mission won. The career is complete.";
    }
    ImGui::SetCursorPosX(pad);
    ImGui::PushStyleColor(ImGuiCol_Text, kMenuMuted);
    ImGui::TextUnformatted(cap.c_str());
    ImGui::PopStyleColor();
  }
  ImGui::Dummy(ImVec2(0, fs * 0.6f));

  // Continue: the most recent save, if there is one.
  {
    static const char* kSlots[] = {"autosave", "quicksave", "slot1", "slot2", "slot3", "slot4", "slot5", "slot6"};
    std::string best;
    std::filesystem::file_time_type best_t{};
    for (const char* sl : kSlots) {
      std::error_code ec;
      const auto t = std::filesystem::last_write_time(save_path(sl), ec);
      if (!ec && (best.empty() || t > best_t)) {
        best = sl;
        best_t = t;
      }
    }
    save::SaveInfo info;
    if (!best.empty() && save::read_info(save_path(best), info)) {
      const int mi = campaign_.index_of(info.mission);
      const std::string where = mi >= 0 ? split_name(campaign_.missions[static_cast<size_t>(mi)].name).first
                                        : (info.mission.empty() ? std::string("Sandbox") : info.mission);
      const std::string cap = where + ", year " + std::to_string(sim::year_index(info.now) + 1) + "  \xC2\xB7  " +
                              with_commas(info.citizens) + " people  \xC2\xB7  " + best;
      ImGui::SetCursorPosX(pad - fs * 0.9f);
      if (menu_entry("Continue", cap, inner + fs * 0.9f)) load_slot(best);
    }
  }
  struct Entry {
    const char* label;
    int action;
  };
  const Entry entries[] = {{"Sandbox", 0},   {"Custom scenarios", 1}, {"Scenario editor", 2}, {"Mods", 3},
                           {"Load game", 4}, {nullptr, -1},           {"Settings", 5},        {"Quit", 6}};
  for (const Entry& e : entries) {
    if (!e.label) {
      const ImVec2 p(ImGui::GetWindowPos().x + pad, ImGui::GetCursorScreenPos().y);
      dl->AddLine(ImVec2(p.x, p.y + fs * 0.5f), ImVec2(p.x + inner * 0.35f, p.y + fs * 0.5f),
                  IM_COL32(232, 192, 112, 70));
      ImGui::Dummy(ImVec2(0, fs));
      continue;
    }
    ImGui::SetCursorPosX(pad - fs * 0.9f);
    if (!menu_entry(e.label, std::string(), inner + fs * 0.9f)) continue;
    switch (e.action) {
      case 0:
        screen_ = Screen::Sandbox;
        break;
      case 1:
        screen_ = Screen::Custom;
        break;
      case 2:
        editor_new();
        break;
      case 3:
        screen_ = Screen::Mods;
        break;
      case 4:
        show_saveload_ = true;
        saveload_saving_ = false;
        break;
      case 5:
        show_settings_ = true;
        break;
      case 6:
        running_ = false;
        break;
      default:
        break;
    }
  }
  // Version and licence, small, at the foot of the column.
  {
    char line[96];
    std::snprintf(line, sizeof line, "v%s  \xC2\xB7  noncommercial, source available", URBS_VERSION);
    const float ls = std::round(fs * 0.82f);
    dl->AddText(f.body, ls, ImVec2(ImGui::GetWindowPos().x + pad, ImGui::GetWindowPos().y + ds.y - pad * 0.6f - ls),
                IM_COL32(150, 134, 108, 255), line);
  }
  if (dialog) {
    ImGui::EndDisabled();
    dim_backdrop(150);
    dl->AddRectFilled(ImGui::GetWindowPos(), ImVec2(ImGui::GetWindowPos().x + col_w, ImGui::GetWindowPos().y + ds.y),
                      IM_COL32(18, 12, 8, 150));
  }
  ImGui::End();
}

// The province map with the career's missions as medallions, and the selected mission's dossier:
// title, region, rank and salary, the letter of appointment as a scroll, difficulty and Begin.
void App::draw_campaign() {
  const ImVec2 ds = display();
  const float fs = ImGui::GetStyle().FontSizeBase;
  const UiFonts& f = ui_fonts();
  dim_backdrop(214);
  if (!province_map_.valid() && !province_map_tried_) {
    province_map_tried_ = true;
    province_map_.load(win_.gpu, root_ + "assets/ui/province_map.png");
  }
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ds, ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
  ImGui::Begin("##campaign", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const auto& M = campaign_.missions;
  const int next = profile_.next_index(campaign_);
  const float m = std::round(fs * 1.3f);

  // Header: the title on the left, the career on the right, a thin gold rule below.
  const float head_h = std::round(fs * 3.4f);
  {
    const float ts = std::round(fs * 2.1f);
    dl->AddText(f.heading, ts, ImVec2(m, (head_h - ts) * 0.5f + fs * 0.2f), kMenuText, "Cursus honorum");
    char career[160];
    std::snprintf(career, sizeof career, "Career  %zu of %zu missions    \xC2\xB7    Personal savings  %s denarii",
                  profile_.completed.size(), M.size(), with_commas(profile_.savings).c_str());
    const float cs = std::round(fs * 1.0f);
    const ImVec2 sz = f.body->CalcTextSizeA(cs, FLT_MAX, 0, career);
    dl->AddText(f.body, cs, ImVec2(ds.x - m - sz.x, (head_h - cs) * 0.5f + fs * 0.25f), kMenuMuted, career);
    dl->AddLine(ImVec2(m, head_h), ImVec2(ds.x - m, head_h), IM_COL32(232, 192, 112, 90), 1.0f);
  }
  const float foot_h = std::round(fs * 3.6f);
  const float body_y0 = head_h + m * 0.8f, body_y1 = ds.y - foot_h;
  const float dossier_w = std::round(std::clamp(ds.x * 0.30f, fs * 22.0f, fs * 30.0f));
  // The map takes the width left of the dossier at its own aspect; the career's timeline sits below it.
  // When the window is too short for both, the map gives up some of its far north and south (never
  // its missions), and only then some width.
  const float time_h = std::round(fs * 4.4f);
  constexpr float kAspect = 16.0f / 9.0f;
  float mw = ds.x - dossier_w - 3 * m, mh = mw / kAspect;
  const float box_h = body_y1 - body_y0 - time_h;
  float u0 = 0, u1 = 1, v0 = 0, v1 = 1;
  if (mh > box_h) {
    const float fy = std::max(0.78f, box_h / mh);
    v0 = std::clamp(0.47f - fy * 0.5f, 0.0f, 1.0f - fy);
    v1 = v0 + fy;
    mh *= fy;
    if (mh > box_h) {
      mw *= box_h / mh;
      mh = box_h;
    }
  }
  const float mx0 = m, my0 = body_y0;
  const ImVec2 mp0(mx0, my0), mp1(std::round(mx0 + mw), std::round(my0 + mh));
  // Frame: a soft drop shadow, a dark bronze border and a thin gold inner line.
  for (int k = 1; k <= 4; ++k) {
    const float o = static_cast<float>(k);
    dl->AddRect(ImVec2(mp0.x - o, mp0.y - o + 2), ImVec2(mp1.x + o, mp1.y + o + 2), IM_COL32(0, 0, 0, 60 - k * 12), 0,
                0, 2.0f);
  }
  if (province_map_.valid()) {
    dl->AddImage(province_map_.texture(), mp0, mp1, ImVec2(u0, v0), ImVec2(u1, v1));
  } else {
    ImVec2 a, b;
    if (icons_.big_uv("ui/province_map", a, b)) {
      dl->AddImage(icons_.big_texture(), mp0, mp1, ImVec2(a.x + (b.x - a.x) * u0, a.y + (b.y - a.y) * v0),
                   ImVec2(a.x + (b.x - a.x) * u1, a.y + (b.y - a.y) * v1));
    } else {
      dl->AddRectFilled(mp0, mp1, IM_COL32(52, 98, 136, 255));
    }
  }
  dl->AddRect(ImVec2(mp0.x - 2, mp0.y - 2), ImVec2(mp1.x + 2, mp1.y + 2), IM_COL32(92, 62, 34, 255), 0, 0, 3.0f);
  dl->AddRect(ImVec2(mp0.x + 3, mp0.y + 3), ImVec2(mp1.x - 3, mp1.y - 3), IM_COL32(232, 200, 130, 110), 0, 0, 1.0f);

  // Mission medallions: at their places, nudged apart where they crowd (Latium and Campania), with a
  // short leader line back to the true spot.
  const float r = std::round(fs * 0.78f);
  const size_t n = M.size();
  std::vector<ImVec2> at(n), pos(n);
  for (size_t k = 0; k < n; ++k) {
    at[k] = ImVec2(mp0.x + (M[k].x - u0) / (u1 - u0) * (mp1.x - mp0.x),
                   mp0.y + (M[k].y - v0) / (v1 - v0) * (mp1.y - mp0.y));
    pos[k] = at[k];
  }
  const float dmin = 2 * r + fs * 0.3f;
  for (int it = 0; it < 60; ++it)
    for (size_t a = 0; a < n; ++a)
      for (size_t b = a + 1; b < n; ++b) {
        float dx = pos[b].x - pos[a].x, dy = pos[b].y - pos[a].y;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d >= dmin) continue;
        if (d < 1e-3f) {  // same spot: separate along a fixed direction
          dx = 1;
          dy = 0.3f;
          d = std::sqrt(dx * dx + dy * dy);
        }
        const float push = (dmin - d) * 0.5f;
        pos[a].x -= dx / d * push;
        pos[a].y -= dy / d * push;
        pos[b].x += dx / d * push;
        pos[b].y += dy / d * push;
      }
  // The road of the career so far: a dashed line through the missions in order.
  for (size_t k = 1; k < n && static_cast<int>(k) <= next; ++k) {
    const ImVec2 a = pos[k - 1], b = pos[k];
    const float len = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
    const int dashes = std::max(1, static_cast<int>(len / (fs * 0.55f)));
    for (int d = 0; d < dashes; d += 2) {
      const float t0 = static_cast<float>(d) / static_cast<float>(dashes),
                  t1 = static_cast<float>(d + 1) / static_cast<float>(dashes);
      dl->AddLine(ImVec2(a.x + (b.x - a.x) * t0, a.y + (b.y - a.y) * t0),
                  ImVec2(a.x + (b.x - a.x) * t1, a.y + (b.y - a.y) * t1), IM_COL32(120, 40, 24, 210), 2.0f);
    }
  }
  int hovered = -1;
  const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f);
  for (size_t k = 0; k < n; ++k) {
    const ImVec2 p = pos[k];
    const bool done = profile_.done(M[k].id), open = static_cast<int>(k) <= next,
               sel = static_cast<int>(k) == selected_mission_;
    const float dx = p.x - at[k].x, dy = p.y - at[k].y;
    if (dx * dx + dy * dy > 9) {
      dl->AddLine(at[k], p, IM_COL32(40, 26, 14, 200), 1.5f);
      dl->AddCircleFilled(at[k], 2.5f, IM_COL32(40, 26, 14, 230));
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x - r, p.y - r));
    ImGui::PushID(static_cast<int>(k));
    if (ImGui::InvisibleButton("m", ImVec2(2 * r, 2 * r)) && open) selected_mission_ = static_cast<int>(k);
    if (ImGui::IsItemHovered()) hovered = static_cast<int>(k);
    ImGui::PopID();
    const float rr = open ? r : r * 0.82f;
    dl->AddCircleFilled(ImVec2(p.x + 1.5f, p.y + 2.5f), rr + 1, IM_COL32(0, 0, 0, 90));
    ImU32 fill, ring, ink;
    if (done) {
      fill = IM_COL32(204, 158, 70, 255);
      ring = IM_COL32(110, 72, 26, 255);
      ink = IM_COL32(70, 40, 12, 255);
    } else if (open) {
      fill = kTerracotta;
      ring = IM_COL32(236, 196, 110, 255);
      ink = IM_COL32(255, 242, 220, 255);
      dl->AddCircle(p, rr + 3 + pulse * 4, IM_COL32(236, 196, 110, static_cast<int>(200 * (1 - pulse))), 0, 2.0f);
    } else {
      fill = IM_COL32(170, 162, 146, 220);
      ring = IM_COL32(104, 94, 80, 230);
      ink = IM_COL32(84, 74, 62, 255);
    }
    dl->AddCircleFilled(p, rr, fill);
    if (done) dl->AddCircle(p, rr - 3, IM_COL32(246, 214, 140, 255), 0, 1.0f);
    dl->AddCircle(p, rr, ring, 0, 2.0f);
    if (sel) dl->AddCircle(p, rr + 3.5f, IM_COL32(255, 244, 214, 255), 0, 2.5f);
    const std::string num = roman(static_cast<int>(k) + 1);
    const float ns = std::round(fs * (num.size() > 3 ? 0.62f : 0.74f));
    const ImVec2 nsz = f.bold->CalcTextSizeA(ns, FLT_MAX, 0, num.c_str());
    dl->AddText(f.bold, ns, ImVec2(std::round(p.x - nsz.x * 0.5f), std::round(p.y - nsz.y * 0.5f)), ink, num.c_str());
  }
  // The cursus honorum itself: every mission in order under its rank, on one line below the map.
  {
    const float tx0 = mp0.x + fs * 2.6f, tx1 = mp1.x - fs * 3.2f;
    const float ty = std::round(mp1.y + std::max(time_h, body_y1 - mp1.y) * 0.5f + fs * 0.7f);
    const float tr = std::round(fs * 0.6f);
    int groups = 0;
    for (size_t k = 0; k < n; ++k)
      if (k == 0 || M[k].rank != M[k - 1].rank) ++groups;
    // Missions are spaced evenly with an extra half step between ranks.
    const float steps = static_cast<float>(n - 1) + 0.6f * static_cast<float>(std::max(0, groups - 1));
    const float step = n > 1 ? (tx1 - tx0) / steps : 0.0f;
    std::vector<float> tx(n);
    float x = tx0;
    for (size_t k = 0; k < n; ++k) {
      if (k > 0) x += step * (M[k].rank != M[k - 1].rank ? 1.6f : 1.0f);
      tx[k] = x;
    }
    for (size_t k = 1; k < n; ++k)
      dl->AddLine(ImVec2(tx[k - 1], ty), ImVec2(tx[k], ty),
                  static_cast<int>(k) <= next ? IM_COL32(232, 192, 112, 220) : IM_COL32(150, 134, 108, 120),
                  static_cast<int>(k) <= next ? 2.5f : 1.5f);
    const float rs = std::round(fs * 0.78f);
    for (size_t a = 0, b; a < n; a = b) {  // rank names over their groups
      for (b = a + 1; b < n && M[b].rank == M[a].rank;) ++b;
      const float cx = (tx[a] + tx[b - 1]) * 0.5f;
      // Narrow windows: a rank name longer than its stretch of the line gets a smaller face.
      const float room = tx[b - 1] - tx[a] + step * 1.5f;
      float rsz = rs;
      ImVec2 sz = f.heading->CalcTextSizeA(rsz, FLT_MAX, 0, M[a].rank.c_str());
      if (sz.x > room) {
        rsz = std::max(std::round(fs * 0.55f), std::floor(rs * room / sz.x));
        sz = f.heading->CalcTextSizeA(rsz, FLT_MAX, 0, M[a].rank.c_str());
      }
      const bool reached = static_cast<int>(a) <= next;
      dl->AddText(f.heading, rsz, ImVec2(std::round(cx - sz.x * 0.5f), ty - tr - fs * 0.5f - sz.y),
                  reached ? kMenuGold : IM_COL32(150, 134, 108, 255), M[a].rank.c_str());
    }
    for (size_t k = 0; k < n; ++k) {
      const ImVec2 p(tx[k], ty);
      const bool done = profile_.done(M[k].id), open = static_cast<int>(k) <= next,
                 sel = static_cast<int>(k) == selected_mission_;
      ImGui::SetCursorScreenPos(ImVec2(p.x - tr, p.y - tr));
      ImGui::PushID(1000 + static_cast<int>(k));
      if (ImGui::InvisibleButton("t", ImVec2(2 * tr, 2 * tr)) && open) selected_mission_ = static_cast<int>(k);
      if (ImGui::IsItemHovered()) hovered = static_cast<int>(k);
      ImGui::PopID();
      const ImU32 fill = done ? IM_COL32(204, 158, 70, 255) : open ? kTerracotta : IM_COL32(64, 54, 44, 255);
      dl->AddCircleFilled(p, tr, fill);
      dl->AddCircle(p, tr,
                    done   ? IM_COL32(246, 214, 140, 255)
                    : open ? IM_COL32(236, 196, 110, 255)
                           : IM_COL32(150, 134, 108, 200),
                    0, 1.5f);
      if (sel) dl->AddCircle(p, tr + 3.5f, IM_COL32(255, 244, 214, 255), 0, 2.0f);
      const std::string num = roman(static_cast<int>(k) + 1);
      const float ns = std::round(fs * (num.size() > 3 ? 0.5f : 0.6f));
      const ImVec2 nsz = f.bold->CalcTextSizeA(ns, FLT_MAX, 0, num.c_str());
      dl->AddText(f.bold, ns, ImVec2(std::round(p.x - nsz.x * 0.5f), std::round(p.y - nsz.y * 0.5f)),
                  done   ? IM_COL32(70, 40, 12, 255)
                  : open ? IM_COL32(255, 242, 220, 255)
                         : IM_COL32(170, 154, 128, 255),
                  num.c_str());
    }
  }

  // Name plaques: the selected mission and the one under the mouse.
  auto plaque = [&](int k) {
    if (k < 0 || k >= static_cast<int>(n)) return;
    const std::string label = split_name(M[static_cast<size_t>(k)].name).first;
    const float ls = std::round(fs * 0.95f);
    const ImVec2 sz = f.bold->CalcTextSizeA(ls, FLT_MAX, 0, label.c_str());
    const ImVec2 p = pos[static_cast<size_t>(k)];
    const float w = sz.x + fs * 1.0f, h = sz.y + fs * 0.4f;
    float x = p.x + r + fs * 0.5f;
    if (x + w > mp1.x - 4) x = p.x - r - fs * 0.5f - w;
    const float y = p.y - h * 0.5f;
    dl->AddRectFilled(ImVec2(x + 1, y + 2), ImVec2(x + w + 1, y + h + 2), IM_COL32(0, 0, 0, 70), 3);
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(244, 232, 204, 240), 3);
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(92, 62, 34, 255), 3, 0, 1.5f);
    dl->AddText(f.bold, ls, ImVec2(x + fs * 0.5f, y + fs * 0.2f), IM_COL32(52, 32, 18, 255), label.c_str());
  };
  plaque(selected_mission_);
  if (hovered != selected_mission_) plaque(hovered);
  if (hovered >= 0) {
    const auto& hm = M[static_cast<size_t>(hovered)];
    const bool done = profile_.done(hm.id), open = hovered <= next;
    ImGui::SetTooltip("%s. %s\n%s", roman(hovered + 1).c_str(), hm.name.c_str(),
                      done   ? "Completed: click to read the letter again or replay it"
                      : open ? "The next post of your career"
                             : "Not yet: finish the missions before it");
  }

  // The dossier: a parchment card beside the map, the full height of the screen body.
  const ImVec2 dp0(std::round(ds.x - m - dossier_w), body_y0);
  const ImVec2 dsz(dossier_w, body_y1 - body_y0);
  ImGui::SetCursorScreenPos(dp0);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(std::round(fs * 1.2f), std::round(fs * 1.0f)));
  ImGui::BeginChild("dossier", dsz, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar);
  if (selected_mission_ >= 0 && selected_mission_ < static_cast<int>(n)) {
    const auto& mi = M[static_cast<size_t>(selected_mission_)];
    const auto [place, subtitle] = split_name(mi.name);
    const bool done = profile_.done(mi.id);
    ImDrawList* cl = ImGui::GetWindowDrawList();
    const float cw = ImGui::GetContentRegionAvail().x;
    // Mission number and status.
    {
      const std::string kicker = "Mission " + roman(selected_mission_ + 1) + " of " + roman(static_cast<int>(n));
      ImGui::PushFont(f.bold, std::round(fs * 0.85f));
      ImGui::TextDisabled("%s", kicker.c_str());
      ImGui::PopFont();
      const char* st = done ? "Completed" : selected_mission_ == next ? "Your next post" : "";
      if (*st) {
        ImGui::SameLine();
        const ImU32 sc = done ? IM_COL32(150, 106, 30, 255) : kTerracotta;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cw - ImGui::GetCursorPosX() -
                             f.bold->CalcTextSizeA(std::round(fs * 0.85f), FLT_MAX, 0, st).x +
                             ImGui::GetStyle().WindowPadding.x);
        ImGui::PushFont(f.bold, std::round(fs * 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Text, sc);
        ImGui::TextUnformatted(st);
        ImGui::PopStyleColor();
        ImGui::PopFont();
      }
    }
    ImGui::PushFont(f.heading, std::round(fs * 2.0f));
    ImGui::TextUnformatted(place.c_str());
    ImGui::PopFont();
    if (!subtitle.empty()) {
      ImGui::PushFont(f.body, std::round(fs * 1.15f));
      ImGui::TextDisabled("%s", subtitle.c_str());
      ImGui::PopFont();
    }
    divider();
    // Region, rank and salary in three labelled cells.
    {
      const std::string salary = std::to_string(mi.salary) + " denarii a day";
      const char* labels[] = {"Province", "Rank", "Salary"};
      const std::string values[] = {mi.region, mi.rank, salary};
      const float x0 = ImGui::GetCursorPosX(), cell = cw / 3.0f;
      const float y0 = ImGui::GetCursorPosY();
      for (int c = 0; c < 3; ++c) {
        ImGui::SetCursorPos(ImVec2(x0 + cell * static_cast<float>(c), y0));
        ImGui::BeginGroup();
        ImGui::PushFont(f.bold, std::round(fs * 0.75f));
        ImGui::TextDisabled("%s", labels[c]);
        ImGui::PopFont();
        ImGui::PushFont(f.bold, 0.0f);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cell - fs * 0.4f);
        ImGui::TextWrapped("%s", values[c].c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ImGui::EndGroup();
      }
    }
    ImGui::Dummy(ImVec2(0, fs * 0.3f));
    ImGui::TextWrapped("%s", mi.blurb.c_str());
    ImGui::Dummy(ImVec2(0, fs * 0.4f));
    // The letter of appointment as a scroll: a lighter sheet between two rods, filling the room left
    // above the difficulty and the Begin button.
    const float controls_h = std::round(fs * 6.2f);
    const float letter_h = ImGui::GetContentRegionAvail().y - controls_h;
    if (!mi.letter.empty() && letter_h > fs * 4) {
      const ImVec2 sp0 = ImGui::GetCursorScreenPos();
      const float rod = std::round(fs * 0.55f), ov = std::round(fs * 0.45f);
      const ImVec2 s0(sp0.x, sp0.y + rod * 0.5f), s1(sp0.x + cw, sp0.y + letter_h - rod * 0.5f);
      cl->AddRectFilled(ImVec2(s0.x + 2, s0.y + 3), ImVec2(s1.x + 2, s1.y + 3), IM_COL32(60, 40, 20, 40));
      cl->AddRectFilledMultiColor(s0, s1, IM_COL32(250, 242, 222, 255), IM_COL32(250, 242, 222, 255),
                                  IM_COL32(238, 224, 192, 255), IM_COL32(238, 224, 192, 255));
      cl->AddLine(ImVec2(s0.x, s0.y), ImVec2(s0.x, s1.y), IM_COL32(190, 166, 120, 255));
      cl->AddLine(ImVec2(s1.x, s0.y), ImVec2(s1.x, s1.y), IM_COL32(190, 166, 120, 255));
      for (const float ry : {sp0.y, sp0.y + letter_h - rod}) {
        const ImVec2 r0(s0.x - ov, ry), r1(s1.x + ov, ry + rod);
        cl->AddRectFilledMultiColor(r0, r1, IM_COL32(150, 98, 52, 255), IM_COL32(150, 98, 52, 255),
                                    IM_COL32(92, 56, 26, 255), IM_COL32(92, 56, 26, 255));
        cl->AddLine(ImVec2(r0.x + 2, r0.y + 2), ImVec2(r1.x - 2, r0.y + 2), IM_COL32(206, 150, 90, 200));
        cl->AddCircleFilled(ImVec2(r0.x, ry + rod * 0.5f), rod * 0.62f, IM_COL32(110, 70, 34, 255));
        cl->AddCircleFilled(ImVec2(r1.x, ry + rod * 0.5f), rod * 0.62f, IM_COL32(110, 70, 34, 255));
      }
      ImGui::SetCursorScreenPos(ImVec2(s0.x + fs * 0.9f, s0.y + rod * 0.5f + fs * 0.5f));
      ImGui::BeginChild("letter", ImVec2(cw - fs * 1.4f, letter_h - 2 * rod - fs * 0.8f), ImGuiChildFlags_None,
                        ImGuiWindowFlags_NoBackground);
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(52, 34, 20, 255));
      ImGui::TextWrapped("%s", mi.letter.c_str());
      ImGui::PopStyleColor();
      ImGui::EndChild();
      ImGui::SetCursorScreenPos(ImVec2(sp0.x, sp0.y + letter_h));
    }
    // Difficulty and Begin, at the foot of the card.
    ImGui::SetCursorPosY(
        std::max(ImGui::GetCursorPosY() + fs * 0.4f,
                 ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y - controls_h + fs * 0.6f));
    const bool available = std::filesystem::exists(root_ + "data/scenarios/" + mi.id + "/scenario.json");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Difficulty");
    ImGui::SameLine(fs * 6.0f + ImGui::GetStyle().WindowPadding.x);
    ImGui::SetNextItemWidth(cw - fs * 6.0f);
    ImGui::Combo("##difficulty", &settings_.difficulty, kDifficulty, 3);
    ImGui::Dummy(ImVec2(0, fs * 0.3f));
    if (available) {
      if (primary_button(done ? "Play the mission again" : "Begin the mission", ImVec2(cw, std::round(fs * 2.8f))))
        start_mission(selected_mission_);
    } else {
      ImGui::TextDisabled("This mission is still being written.");
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();

  // Footer: Back and Reset career on the left, a reminder of how missions connect on the right.
  ImGui::SetCursorScreenPos(ImVec2(m, body_y1 + (foot_h - fs * 2.2f) * 0.5f));
  if (ImGui::Button("Back", ImVec2(std::round(fs * 9), std::round(fs * 2.2f)))) screen_ = Screen::Menu;
  ImGui::SameLine(0, fs * 0.8f);
  if (ImGui::Button("Reset career", ImVec2(std::round(fs * 9), std::round(fs * 2.2f)))) ImGui::OpenPopup("##reset");
  {
    const char* hint = "The cities you have built trade with you in later missions.";
    const float hs = std::round(fs * 0.92f);
    const ImVec2 sz = f.body->CalcTextSizeA(hs, FLT_MAX, 0, hint);
    dl->AddText(f.body, hs, ImVec2(ds.x - m - sz.x, body_y1 + (foot_h - sz.y) * 0.5f), kMenuMuted, hint);
  }
  ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal("##reset", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize)) {
    heading("Reset the career?", 1.3f);
    ImGui::TextUnformatted("Every mission won and all personal savings are forgotten.");
    ImGui::Spacing();
    if (ImGui::Button("Reset", ImVec2(fs * 8, 0))) {
      profile_ = CampaignProfile{};
      profile_.save(user_dir_);
      selected_mission_ = 0;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Keep it", ImVec2(fs * 8, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
  ImGui::End();
}

void App::draw_sandbox_setup() {
  dim_backdrop();
  center_next(420, 480);
  begin_window(
      "##sandbox", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
  menu_title("Sandbox");
  ImGui::TextWrapped("A free city with every building unlocked, 15,000 denarii and a cart of timber and grain.");
  int size = gen_.size, seed = static_cast<int>(gen_.seed);
  const char* sizes[] = {"Small (128)", "Medium (192)", "Large (256)", "Huge (384)"};
  const int size_vals[] = {128, 192, 256, 384};
  int si = 1;
  for (int k = 0; k < 4; ++k)
    if (size_vals[k] == size) si = k;
  if (ImGui::Combo("Map size", &si, sizes, 4)) gen_.size = size_vals[si];
  if (ImGui::InputInt("Seed", &seed)) gen_.seed = static_cast<u64>(std::max(0, seed));
  const char* coasts[] = {"No coast", "North", "East", "South", "West"};
  int coast = gen_.coast + 1;
  if (ImGui::Combo("Coast", &coast, coasts, 5)) gen_.coast = coast - 1;
  ImGui::Checkbox("River", &gen_.river);
  ImGui::SliderFloat("Hills", &gen_.hills, 0.0f, 1.0f);
  ImGui::SliderFloat("Forest", &gen_.forest, 0.0f, 1.0f);
  ImGui::SliderFloat("Desert", &gen_.arid, 0.0f, 1.0f);
  ImGui::Checkbox("Yearly Nile flood", &sandbox_nile_);
  const char* styles[] = {"Italy", "North", "East", "Egypt"};
  ImGui::Combo("Building style", &sandbox_region_, styles, 4);
  if (ImGui::Button("Random map")) {
    gen_.seed = static_cast<u64>(SDL_GetTicksNS() % 1000000);
    gen_.coast = static_cast<i32>(gen_.seed % 5) - 1;
    gen_.hills = static_cast<f32>((gen_.seed / 5) % 10) / 10.0f;
    gen_.forest = static_cast<f32>((gen_.seed / 50) % 10) / 10.0f;
    gen_.river = (gen_.seed / 500) % 4 != 0;
  }
  ImGui::Spacing();
  if (big_button("Found the city")) start_sandbox();
  if (big_button("Back")) screen_ = Screen::Menu;
  ImGui::End();
}

// The top bar, in three zones: the city's numbers on the left (each rating with a small bar and its mission goal),
// date and speed in the centre, and the panels as picture buttons on the right. The latest news shows below it.
void App::draw_hud() {
  const sim::World& w = *world_;
  const ImVec2 ds = display();
  const float fs = ImGui::GetFontSize();
  const UiFonts& fonts = ui_fonts();
  const float icon = std::round(fs * 1.25f), pad = std::round(fs * 0.32f), btn = icon + 2 * pad,
              gap = std::round(fs * 1.1f);
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(ds.x, 0), ImGuiCond_Always);
  begin_window("##hud", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(pad, pad));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(std::round(fs * 0.25f), 0));
  const float top = ImGui::GetCursorPosY();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  bool first = true;
  auto next = [&](float space) {
    if (!first) ImGui::SameLine(0, space);
    first = false;
  };
  auto separator = [&]() {
    next(gap * 0.6f);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddLine(ImVec2(p.x, p.y + btn * 0.12f), ImVec2(p.x, p.y + btn * 0.88f), ImGui::GetColorU32(ImGuiCol_Separator),
                1.5f);
    ImGui::Dummy(ImVec2(1, btn));
  };
  // A number with its icon and, below it, either a small caption or a bar (0..1) with an optional goal tick.
  auto stat = [&](const char* icon_name, const std::string& value, const std::string& sub, ImU32 sub_col, float bar,
                  float goal, ImU32 bar_col, const std::string& tip, float space) {
    next(space);
    ImGui::BeginGroup();
    ImGui::SetCursorPosY(top + (btn - icon) * 0.5f);
    icons_.image(icon_name, icon);
    ImGui::SameLine(0, std::round(fs * 0.3f));
    const float tx = ImGui::GetCursorPosX();
    const bool two = !sub.empty() || bar >= 0;
    ImGui::SetCursorPosY(top + (two ? btn * 0.5f - fs * 0.95f : (btn - fs) * 0.5f));
    ImGui::PushFont(fonts.bold, 0.0f);
    ImGui::TextUnformatted(value.c_str());
    ImGui::PopFont();
    if (!sub.empty()) {
      ImGui::SetCursorPos(ImVec2(tx, top + btn * 0.5f + fs * 0.02f));
      ImGui::PushFont(fonts.body, fs * 0.78f);
      ImGui::PushStyleColor(ImGuiCol_Text, sub_col);
      ImGui::TextUnformatted(sub.c_str());
      ImGui::PopStyleColor();
      ImGui::PopFont();
    } else if (bar >= 0) {
      const float bw = std::round(fs * 2.6f), bh = std::max(3.0f, std::round(fs * 0.22f));
      const ImVec2 p0(ImGui::GetWindowPos().x + tx, ImGui::GetWindowPos().y + top + btn * 0.5f + fs * 0.35f);
      dl->AddRectFilled(p0, ImVec2(p0.x + bw, p0.y + bh), kColTrack, bh);
      dl->AddRectFilled(p0, ImVec2(p0.x + bw * std::clamp(bar, 0.0f, 1.0f), p0.y + bh), bar_col, bh);
      if (goal > 0) {
        const float gx = std::round(p0.x + bw * goal);
        dl->AddLine(ImVec2(gx, p0.y - bh), ImVec2(gx, p0.y + bh * 2), kColTick, 2.0f);
      }
      ImGui::SetCursorPos(ImVec2(tx, top + btn * 0.5f + fs * 0.35f));
      ImGui::Dummy(ImVec2(bw, bh));
    }
    ImGui::EndGroup();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip.c_str());
  };
  const ImU32 kGreen = kColGood, kAmber = kColWarn, kOchre = kColPlain, kRed = kColBad,
              kInk2 = ImGui::GetColorU32(ImGuiCol_TextDisabled);

  // Left: money, people, the ratings.
  {
    const auto& L = w.econ.money;
    i64 net = 0;
    for (int k = 0; k < econ::kMoneyCount; ++k)
      net += (econ::money_is_income(static_cast<econ::Money>(k)) ? 1 : -1) * L.yesterday[k];
    stat("goods/denarii_16", with_commas(w.econ.treasury), (net >= 0 ? "+" : "") + with_commas(net) + " yesterday",
         net >= 0 ? kGreen : kRed, -1, 0, 0, "Treasury, and yesterday's income minus spending", gap);
    const u32 pop = w.population();
    stat("ui/icon_population", with_commas(pop),
         w.goals.population ? "goal " + with_commas(w.goals.population) : std::string(), kInk2, -1, 0, 0,
         w.goals.population ? "Population (mission goal " + with_commas(w.goals.population) + ")"
                            : std::string("Population"),
         gap);
    separator();
    auto rating = [&](const char* icon_name, int v, int goal, const char* name) {
      const ImU32 col = goal ? (v >= goal ? kGreen : kAmber) : kOchre;
      std::string tip = std::string(name) + ": " + std::to_string(v) + " of 100";
      if (goal)
        tip +=
            v >= goal ? "  (goal " + std::to_string(goal) + " met)" : "  (mission goal " + std::to_string(goal) + ")";
      stat(icon_name, std::to_string(v), std::string(), 0, static_cast<float>(v) / 100.0f,
           static_cast<float>(goal) / 100.0f, col, tip, gap * 0.6f);
    };
    const int mood = w.avg_happiness;
    stat("ui/icon_happiness", std::to_string(mood), std::string(), 0, static_cast<float>(mood) / 255.0f,
         128.0f / 255.0f, mood >= 128 ? kGreen : kAmber, "Mood of the people (128 = content)", gap);
    rating("ui/icon_culture", w.ratings.culture, w.goals.culture, "Culture");
    rating("ui/icon_prosperity", w.ratings.prosperity, w.goals.prosperity, "Prosperity");
    rating("ui/icon_security", w.ratings.security, w.goals.security, "Security");
    rating("ui/icon_favor", w.favor, w.goals.favor, "Favor with Rome");
  }

  // Centre: date and speed, centred on the screen when there is room.
  const std::string date1 =
      std::string(sim::season_name(sim::season(w.now))) + ", day " + std::to_string(sim::day_of_year(w.now) + 1);
  char date2[48];
  std::snprintf(date2, sizeof date2, "Year %llu  \xC2\xB7  %02u:00",
                static_cast<unsigned long long>(sim::year_index(w.now) + 1), sim::hour_of_day(w.now));
  const float date_w = std::max(fonts.bold->CalcTextSizeA(fs, FLT_MAX, 0, date1.c_str()).x,
                                fonts.body->CalcTextSizeA(fs * 0.78f, FLT_MAX, 0, date2).x);
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float centre_w = date_w + gap + 4 * btn + 3 * spacing;
  {
    ImGui::SameLine(0, 0);
    const float want = std::round((ds.x - centre_w) * 0.5f);
    const float x = std::max(ImGui::GetCursorPosX() + gap * 1.5f, want);
    ImGui::SameLine(x);
    ImGui::BeginGroup();
    ImGui::SetCursorPosY(top + btn * 0.5f - fs * 0.95f);
    ImGui::PushFont(fonts.bold, 0.0f);
    ImGui::TextUnformatted(date1.c_str());
    ImGui::PopFont();
    ImGui::SetCursorPosY(top + btn * 0.5f + fs * 0.02f);
    ImGui::PushFont(fonts.body, fs * 0.78f);
    ImGui::TextDisabled("%s", date2);
    ImGui::PopFont();
    ImGui::EndGroup();
    ImGui::SameLine(0, gap);
    ImGui::SetCursorPosY(top);
    static const char* kSpeedIcons[] = {"ui/icon_pause", "ui/icon_play", "ui/icon_fast", "ui/icon_faster"};
    static const char* kSpeedTips[] = {"Pause (Space)", "Normal speed (1)", "Fast (2)", "Fastest (3)"};
    for (int i = 0; i < 4; ++i) {
      if (i) ImGui::SameLine();
      ImGui::PushID(i);
      if (icons_.icon_button("##speed", kSpeedIcons[i], icon, speed_ == i)) speed_ = i;
      ImGui::PopID();
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kSpeedTips[i]);
    }
  }

  // Right: overlay, panels, alerts, help, menu.
  struct Btn {
    const char* icon;
    Panel panel;
    const char* tip;
  };
  const Btn panels[] = {{"ui/icon_rome", Panel::Rome, "Rome\nFavor, the Senate and the Emperor's requests"},
                        {"ui/icon_advisors", Panel::Advisors, "Advisors\nReports from your five advisors"},
                        {"ui/icon_finance", Panel::Finance, "Finance\nTaxes, wages and the budget"},
                        {"ui/icon_goods", Panel::Goods, "Goods (L)\nWhat the city holds, makes and uses"},
                        {"ui/icon_trade", Panel::Trade, "Trade\nRoutes, imports and exports"},
                        {"ui/icon_labor", Panel::Labor, "Labor (J)\nWorkers, jobs and priorities"},
                        {"ui/icon_religion", Panel::Religion, "Religion\nThe gods' moods and festivals"},
                        {"ui/icon_military", Panel::Military, "Military\nLegions, walls and threats"},
                        {"ui/icon_research", Panel::Research, "Research\nCivic research"}};
  // Where each panel hangs from: the centre of its button.
  auto mark = [&](Panel p) {
    panel_x_[static_cast<int>(p)] = std::round((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f);
  };
  constexpr int kPanels = static_cast<int>(sizeof(panels) / sizeof(panels[0]));
  const int right_buttons = 1 + kPanels + 3;  // overlay, panels, alerts + help + menu
  const float right_w = static_cast<float>(right_buttons) * (btn + spacing) + 2 * gap;
  {
    ImGui::SameLine(0, 0);
    const float x = std::max(ImGui::GetCursorPosX() + gap * 1.5f, ds.x - right_w - ImGui::GetStyle().WindowPadding.x);
    ImGui::SameLine(x);
    ImGui::SetCursorPosY(top);
    // Map overlays: a picture button that opens the list.
    if (icons_.icon_button("##overlay", "ui/icon_overlay", icon, overlay_ != Overlay::None))
      ImGui::OpenPopup("##overlays");
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Map overlay (O)\nNow: %s", overlay_ == Overlay::None ? "none" : overlay_name(overlay_));
    if (ImGui::BeginPopup("##overlays")) {
      ImGui::PopStyleVar(2);
      for (int k = 0; k < static_cast<int>(Overlay::Count); ++k)
        if (ImGui::Selectable(overlay_name(static_cast<Overlay>(k)), static_cast<int>(overlay_) == k)) {
          overlay_ = static_cast<Overlay>(k);
          overlay_age_ = 1 << 20;
        }
      if (overlay_ == Overlay::Service) {
        ImGui::Separator();
        int sv = overlay_service_.valid() ? overlay_service_.v : 0;
        ImGui::SetNextItemWidth(fs * 9);
        if (ImGui::BeginCombo("##svc", defs_.services[static_cast<size_t>(sv)].name.c_str())) {
          for (u16 k = 0; k < defs_.services.size(); ++k)
            if (ImGui::Selectable(defs_.services[k].name.c_str(), k == sv)) {
              overlay_service_ = ServiceId{k};
              overlay_age_ = 1 << 20;
            }
          ImGui::EndCombo();
        }
        if (!overlay_service_.valid()) overlay_service_ = ServiceId{0};
      }
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(pad, pad));
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(std::round(fs * 0.25f), 0));
      ImGui::EndPopup();
    }
    ImGui::SameLine(0, gap);
    for (int k = 0; k < kPanels; ++k) {
      if (k) ImGui::SameLine();
      ImGui::PushID(k);
      if (icons_.icon_button("##panel", panels[k].icon, icon, panel_ == panels[k].panel)) toggle_panel(panels[k].panel);
      ImGui::PopID();
      mark(panels[k].panel);
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", panels[k].tip);
    }
    ImGui::SameLine(0, gap);
    // Alerts, with a count of the ones that came in since the panel was last open.
    if (panel_ == Panel::Alerts) alerts_seen_ = w.now;
    int unread = 0;
    for (const auto& a : w.alerts)
      if (a.t > alerts_seen_) ++unread;
    if (icons_.icon_button("##alerts", "ui/icon_alerts", icon, panel_ == Panel::Alerts)) toggle_panel(Panel::Alerts);
    mark(Panel::Alerts);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(unread ? "Alerts\n%d new" : "Alerts\nNothing new", unread);
    if (unread) {
      const ImVec2 mx = ImGui::GetItemRectMax(), mn = ImGui::GetItemRectMin();
      const std::string n = unread > 99 ? std::string("99") : std::to_string(unread);
      const float r = fs * 0.5f;
      const ImVec2 c(mx.x - r * 0.4f, mn.y + r * 0.4f);
      dl->AddCircleFilled(c, r, IM_COL32(176, 40, 28, 255));
      dl->AddCircle(c, r, IM_COL32(60, 20, 10, 255), 0, 1.5f);
      const ImVec2 ts = fonts.bold->CalcTextSizeA(fs * 0.7f, FLT_MAX, 0, n.c_str());
      dl->AddText(fonts.bold, fs * 0.7f, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), IM_COL32(255, 246, 228, 255),
                  n.c_str());
    }
    ImGui::SameLine();
    if (icons_.icon_button("##help", "ui/icon_help", icon, panel_ == Panel::Help)) toggle_panel(Panel::Help);
    mark(Panel::Help);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("How to play (H)");
    ImGui::SameLine();
    if (icons_.icon_button("##menu", "ui/icon_menu", icon, paused_menu_)) paused_menu_ = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Menu (Esc)\nSave, load, settings");
  }
  ImGui::PopStyleVar(2);
  hud_h_ = ImGui::GetWindowHeight();
  ImGui::End();

  // The latest news, for a day, under the right end of the bar; a click opens the alerts.
  if (!w.alerts.empty() && w.now - w.alerts.back().t < kTicksPerDay) {
    // Left of an open panel, so it does not cover it.
    const float right = panel_ == Panel::None ? ds.x - 8 : ds.x - 16 - panel_dock(panel_).width;
    ImGui::SetNextWindowPos(ImVec2(right, hud_h_ + 6), ImGuiCond_Always, ImVec2(1, 0));
    begin_window("##news", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.15f, 0.08f, 1));
    if (ImGui::Selectable(alert_text(w, w.alerts.back()).c_str(), false)) panel_ = Panel::Alerts;
    ImGui::PopStyleColor();
    ImGui::End();
  }
}

Dock App::panel_dock(Panel p) const {
  // Widths in font sizes, in top-bar order (Rome .. Help).
  static constexpr float kWidth[kPanelCount] = {27, 30, 28, 27, 32, 27, 28, 28, 28, 30, 30};
  const float fs = ImGui::GetFontSize();
  Dock d;
  d.right = true;
  d.width = std::round(fs * kWidth[static_cast<int>(p)]);
  d.pos = ImVec2(display().x - 8, hud_h_ + std::round(fs * 0.8f));
  d.max_h = display().y - d.pos.y - 8;
  d.notch_x = panel_x_[static_cast<int>(p)];
  return d;
}

void App::draw_pause() {
  center_next(320, 300);
  begin_window(
      "##paused", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
  menu_title("Paused");
  const float x = (320 - 260) * 0.5f;
  ImGui::SetCursorPosX(x);
  if (big_button("Resume")) paused_menu_ = false;
  ImGui::SetCursorPosX(x);
  if (big_button("Save game")) {
    show_saveload_ = true;
    saveload_saving_ = true;
  }
  ImGui::SetCursorPosX(x);
  if (big_button("Load game")) {
    show_saveload_ = true;
    saveload_saving_ = false;
  }
  ImGui::SetCursorPosX(x);
  if (big_button("Settings")) show_settings_ = true;
  ImGui::SetCursorPosX(x);
  if (big_button("Exit to main menu")) {
    profile_.save(user_dir_);
    paused_menu_ = false;
    menu_background();
    screen_ = Screen::Menu;
  }
  ImGui::End();
}

void App::draw_settings() {
  center_next(400, 360);
  begin_window(
      "##settings", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
  menu_title("Settings");
  if (ImGui::SliderFloat("UI scale", &settings_.ui_scale, 0.75f, 2.0f, "%.2f")) apply_roman_style(settings_.ui_scale);
  ImGui::Checkbox("Scroll at screen edges", &settings_.edge_scroll);
  ImGui::Checkbox("Autosave every four days", &settings_.autosave);
  if (ImGui::Checkbox("Colour-blind friendly overlays", &settings_.colorblind)) set_overlay_colorblind(settings_.colorblind);
  if (ImGui::Checkbox("Fullscreen", &settings_.fullscreen)) SDL_SetWindowFullscreen(win_.window, settings_.fullscreen);
  if (ImGui::SliderFloat("Music volume", &settings_.music_volume, 0.0f, 1.0f, "%.2f"))
    audio_.set_volume(settings_.music_volume);
  ImGui::Combo("Difficulty", &settings_.difficulty, kDifficulty, 3);
  ImGui::TextWrapped("Difficulty applies to newly started missions.");
  if (big_button("Done")) {
    settings_.save(user_dir_);
    show_settings_ = false;
  }
  ImGui::End();
}

void App::draw_saveload() {
  center_next(460, 470);
  begin_window(
      "##saveload", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
  menu_title(saveload_saving_ ? "Save game" : "Load game");
  const char* slots[] = {"autosave", "quicksave", "slot1", "slot2", "slot3", "slot4", "slot5", "slot6"};
  const char* names[] = {"Autosave", "Quicksave", "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Slot 6"};
  const float fs = ImGui::GetFontSize();
  ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
  for (int k = 0; k < 8; ++k) {
    const char* s = slots[k];
    if (saveload_saving_ && k == 0) continue;
    save::SaveInfo info;
    const bool exists = save::read_info(save_path(s), info);
    std::string detail = "empty";
    if (exists) {
      const int mi = campaign_.index_of(info.mission);
      const std::string where = mi >= 0 ? campaign_.missions[static_cast<size_t>(mi)].name
                                        : (info.mission.empty() ? std::string("City") : info.mission);
      detail = where + "  \xC2\xB7  year " + std::to_string(sim::year_index(info.now) + 1) + ", day " +
               std::to_string(sim::day_of_year(info.now) + 1) + "  \xC2\xB7  " + with_commas(info.citizens) + " people";
    }
    ImGui::PushID(s);
    const bool can = saveload_saving_ || exists;
    if (!can) ImGui::BeginDisabled();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (ImGui::Button("##slot", ImVec2(420, std::round(fs * 1.9f)))) {
      if (saveload_saving_) {
        save_slot(s);
      } else {
        load_slot(s);
        paused_menu_ = false;
      }
      show_saveload_ = false;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float ty = p.y + (std::round(fs * 1.9f) - fs) * 0.5f;
    dl->AddText(ui_fonts().bold, fs, ImVec2(p.x + fs * 0.6f, ty), ImGui::GetColorU32(ImGuiCol_Text), names[k]);
    dl->AddText(ui_fonts().body, fs, ImVec2(p.x + fs * 5.6f, ty),
                ImGui::GetColorU32(exists ? ImGuiCol_Text : ImGuiCol_TextDisabled), detail.c_str());
    if (!can) ImGui::EndDisabled();
    ImGui::PopID();
  }
  ImGui::PopStyleVar();
  ImGui::Spacing();
  if (big_button("Back", 160)) show_saveload_ = false;
  ImGui::End();
}

void App::draw_research(const Dock& dock, bool* open) {
  if (!begin_panel("##research", dock)) {
    end_panel();
    return;
  }
  if (!panel_header(icons_, "ui/icon_research", false, "Research",
                    with_commas(world_->scientia) + " scientia to spend"))
    *open = false;
  ImGui::TextDisabled("Scholars in schools, libraries and academies produce scientia.");
  const float fs = ImGui::GetFontSize();
  section("Techniques");
  for (u16 t = 0; t < defs_.techniques.size(); ++t) {
    const auto& tech = defs_.techniques[t];
    const bool known = world_->tech_known[t] != 0;
    ImGui::PushID(t);
    const float y0 = ImGui::GetCursorScreenPos().y;
    check_mark(known);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushFont(ui_fonts().bold, 0.0f);
    ImGui::TextUnformatted(tech.name.c_str());
    ImGui::PopFont();
    ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - fs * 6.5f);
    ImGui::TextDisabled("%s", tech.desc.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    const float y1 = ImGui::GetCursorScreenPos().y;
    ImGui::SameLine(0, 0);
    const float bw = std::round(fs * 5.6f);
    ImGui::SetCursorScreenPos(
        ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - bw, y0 + 2));
    if (known) {
      ImGui::Dummy(ImVec2(bw, 1));
    } else {
      const bool can = world_->scientia >= tech.cost;
      if (!can) ImGui::BeginDisabled();
      if (ImGui::Button(("Study  " + std::to_string(tech.cost)).c_str(), ImVec2(bw, 0))) {
        sim::Command c;
        c.type = sim::CmdType::Research;
        c.def = t;
        world_->push(c);
      }
      if (!can) ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(can ? "Costs %u scientia" : "Needs %u scientia", tech.cost);
    }
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x, y1));
    ImGui::Dummy(ImVec2(0, 0));
    ImGui::PopID();
  }
  if (mission_index_ >= 0) {
    section("Your own purse");
    value_row("Personal savings", with_commas(profile_.savings) + " denarii");
    if (coin_button(icons_, "Give the city", 500, profile_.savings >= 500)) {
      profile_.savings -= 500;
      sim::Command c;
      c.type = sim::CmdType::Grant;
      c.value = 500;
      world_->push(c);
    }
  }
  end_panel();
}

void App::draw_help(const Dock& dock, bool* open) {
  if (!begin_panel("##help", dock)) {
    end_panel();
    return;
  }
  if (!panel_header(icons_, "ui/icon_help", false, "How to play", "Keys, and how the city works")) *open = false;
  ImGui::TextWrapped(
      "Camera: WASD or arrow keys to pan, right- or middle-drag to pan, mouse wheel to zoom.\n"
      "Time: Space pauses, 1/2/3 set speed.\n"
      "Building: open Build (B), pick a building, left-click to place; E and Q turn it (its door faces the "
      "front). Farms and gardens: drag an area. "
      "Roads (R): drag a line, diagonals allowed. Demolish (X): drag a rectangle. Right-click or Esc cancels.\n"
      "Inspect: left-click a building or a walking citizen. F follows the selected citizen.\n"
      "Soldiers: click a soldier to select the unit, right-click the map to march there.\n"
      "Overlays (O): desirability, water, pipes, any service's coverage, housing level and more.\n"
      "Saving: F5 quicksave, F9 quickload, or the menu (Esc).\n\n"
      "Houses grow when their residents have what they need: water, food variety, worship, markets, baths and "
      "more. Click a house to see what it needs next. Workers come from houses, so keep people housed, fed and "
      "employed. Porters from warehouses, granaries and markets carry goods; farms and workshops need them nearby.\n\n"
      "Money: the city pays wages and gets them back through taxes and market sales; its real profit is exports. "
      "Build a trade post, open routes (Trade) and export surpluses. Food nobody eats is wages spent for nothing.\n"
      "Rome's requests: set the good to Stockpile in the Trade panel so markets leave it in storage, then Send it "
      "from the Rome panel.");
  end_panel();
  ;
}

namespace {

// Build-menu groups: the rail shows one picture button per group, each holding one or more building categories
// (the `category` field in data/defs/buildings.json). Small categories share a button so the rail stays short.
struct BuildGroup {
  const char* id;  // icon: ui/cat_<id>
  const char* name;
  const char* blurb;
  const char* cats[3];
};
constexpr BuildGroup kGroups[] = {
    {"roads", "Roads & works", "Roads, bridges, engineers, demolish", {"infrastructure"}},
    {"housing", "Housing", "Plots where families settle", {"housing"}},
    {"farms", "Farms", "Grain, vegetables, livestock, groves, vines", {"farm"}},
    {"food", "Food", "Fishing, mills, bakeries, presses, wineries", {"food"}},
    {"industry", "Industry", "Raw materials and workshops", {"industry"}},
    {"storage", "Storage & trade", "Warehouses, granaries, markets, harbors", {"storage", "trade"}},
    {"water", "Water", "Wells, fountains, aqueducts, sewers", {"water"}},
    {"health", "Health & safety", "Physicians, baths, vigiles, guards", {"health", "safety"}},
    {"religion", "Religion", "Shrines and temples", {"religion"}},
    {"culture", "Culture", "Taverns, theaters, schools, libraries", {"entertainment", "education"}},
    {"civic", "Government & decor", "Forum, palace, basilica, gardens, statues", {"civic", "decor"}},
    {"military", "Military", "Walls, barracks, forts", {"military"}},
    {"monument", "Monuments", "Great works that win favor", {"monument"}},
};
constexpr int kGroupCount = static_cast<int>(sizeof(kGroups) / sizeof(kGroups[0]));
constexpr int kOtherGroup = kGroupCount;  // categories no group claims (e.g. from mods)

int group_of(const std::string& cat) {
  for (int g = 0; g < kGroupCount; ++g)
    for (const char* c : kGroups[g].cats)
      if (c && cat == c) return g;
  return kOtherGroup;
}

const char* kResourceNames[] = {"",       "clay",      "stone",     "iron ore", "silver ore",
                                "marble", "limestone", "pozzolana", "fish"};

// A good's name as it reads mid-sentence ("Cut stone" -> "cut stone").
std::string lower_first(std::string s) {
  if (!s.empty() && s[0] >= 'A' && s[0] <= 'Z') s[0] = static_cast<char>(s[0] - 'A' + 'a');
  return s;
}

// One short line under a building's name: its size and the one fact that matters most when choosing it.
std::string build_hint(const data::Defs& defs, const data::BuildingDef& d) {
  std::vector<std::string> parts;
  if (d.painted)
    parts.emplace_back(d.max_w == 1 || d.max_h == 1 ? "drag to lay" : "drag to paint");
  else
    parts.push_back(std::to_string(d.w) + "\xC3\x97" + std::to_string(d.h));
  std::string made;
  if (d.recipe.valid()) {
    const auto& r = defs.recipes[d.recipe.v];
    std::string m = "makes";
    for (size_t k = 0; k < r.outputs.size() && k < 2; ++k)
      m += (k ? ", " : " ") + lower_first(defs.goods[r.outputs[k].good.v].name);
    if (!r.outputs.empty()) made = lower_first(defs.goods[r.outputs[0].good.v].name);
    if (!r.inputs.empty()) m += " from " + lower_first(defs.goods[r.inputs[0].good.v].name);
    parts.push_back(m);
  } else if (d.water_radius) {
    parts.push_back("water within " + std::to_string(d.water_radius) + " tiles");
  } else if (d.service.valid()) {
    static const char* kNeed[] = {"food", "water", "rest", "hygiene", "faith", "entertainment", "health", "education"};
    const auto& sv = defs.services[d.service.v];
    if (sv.patrol)
      parts.emplace_back("patrols the streets");
    else if (sv.need != data::NeedKind::None)
      parts.push_back(std::string(kNeed[static_cast<int>(sv.need)]) + " for homes nearby");
  } else if (d.housing) {
    parts.push_back("homes for " + std::to_string(d.housing));
  } else if ((d.kind == data::BuildingKind::Storage || d.kind == data::BuildingKind::Market) && d.storage) {
    parts.push_back("holds " + std::to_string(d.storage) + " goods");
  } else if (d.kind == data::BuildingKind::Decor && d.desirability > 0) {
    parts.emplace_back("makes the area more desirable");
  } else if (d.crime_cut) {
    parts.emplace_back("cuts crime city-wide");
  } else if (d.favor_days) {
    parts.emplace_back("wins favor with Rome");
  } else if (d.workers) {
    parts.push_back(std::to_string(d.workers) + " workers");
  }
  if (d.needs_pipe) parts.emplace_back("needs a pipe");
  if (d.resource && d.resource < sizeof(kResourceNames) / sizeof(kResourceNames[0]) &&
      made != kResourceNames[d.resource])
    parts.push_back(std::string("on ") + kResourceNames[d.resource]);
  if (d.needs_water_adjacent) parts.emplace_back("beside water");
  std::string s;
  for (const auto& p : parts) s += (s.empty() ? "" : " \xC2\xB7 ") + p;
  return s;
}

void building_tooltip(const data::Defs& defs, const data::BuildingDef& d) {
  std::string tip =
      d.name + "\n" + std::to_string(d.w) + "x" + std::to_string(d.h) + (d.painted ? " (drag to paint an area)" : "");
  for (const auto& c : d.cost_goods) tip += "\n" + std::to_string(c.amount) + " " + defs.goods[c.good.v].name;
  if (d.workers) tip += "\nWorkers: " + std::to_string(d.workers);
  if (d.recipe.valid()) {
    const auto& r = defs.recipes[d.recipe.v];
    tip += "\nMakes:";
    for (const auto& o : r.outputs) tip += " " + defs.goods[o.good.v].name;
    if (!r.inputs.empty()) {
      tip += "\nFrom:";
      for (const auto& in : r.inputs) tip += " " + defs.goods[in.good.v].name;
    }
  }
  if (d.resource && d.resource < sizeof(kResourceNames) / sizeof(kResourceNames[0]))
    tip += std::string("\nMust be built on ") + kResourceNames[d.resource] + " deposits";
  if (d.needs_water_adjacent) tip += "\nMust be next to water";
  ImGui::SetTooltip("%s", tip.c_str());
}

}  // namespace

// The build menu: a rail of picture buttons down the left edge; clicking one pops out its buildings as a list of
// rows (picture, name, hint, cost). The list closes on a click on the map, on Esc, or on its button again.
void App::draw_build_menu() {
  const ImVec2 ds = display();
  const sim::World& w = *world_;
  // Which defs are unlocked, per group.
  std::vector<std::vector<u16>> members(kGroupCount + 1);
  for (u16 i = 0; i < defs_.buildings.size(); ++i)
    if (w.is_unlocked(BuildingDefId{i}))
      members[static_cast<size_t>(group_of(defs_.buildings[i].category))].push_back(i);
  int active = -1;  // the group of the tool in hand
  if (tools_.kind == ToolKind::Road || tools_.kind == ToolKind::Demolish) active = 0;
  if (tools_.kind == ToolKind::Build && tools_.def.valid()) active = group_of(defs_.buildings[tools_.def.v].category);
  if (build_group_ > kGroupCount || (build_group_ == kOtherGroup && members[kOtherGroup].empty())) build_group_ = -1;
  bool opened = false;  // a list popped out this frame: raise it above other windows

  if (show_build_) {
    const int buttons = kGroupCount + (members[kOtherGroup].empty() ? 0 : 1);
    const ImGuiStyle& st = ImGui::GetStyle();
    const float per = st.FramePadding.y * 2 + st.ItemSpacing.y;
    const float avail = ds.y - hud_h_ - 16 - st.WindowPadding.y * 2;
    const float icon = std::clamp(avail / static_cast<float>(buttons) - per, 24.0f, 48.0f);
    ImGui::SetNextWindowPos(ImVec2(8, hud_h_ + 8), ImGuiCond_Always);
    begin_window("##buildrail", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoSavedSettings);
    for (int g = 0; g < buttons; ++g) {
      const bool other = g == kOtherGroup;
      const bool roads = g == 0;
      const bool any = roads || !members[static_cast<size_t>(g)].empty();
      const float y = ImGui::GetCursorScreenPos().y;
      if (!any) ImGui::BeginDisabled();
      const std::string icon_name = other ? std::string("ui/icon_build") : std::string("ui/cat_") + kGroups[g].id;
      ImGui::PushID(g);
      if (icons_.icon_button("##grp", icon_name, icon, build_group_ == g || active == g)) {
        build_group_ = build_group_ == g ? -1 : g;
        build_fly_y_ = y;
        opened = true;
        sel_.building = BuildingId{};  // the list opens where the inspector sits
        sel_.citizen = CitizenId{};
      }
      ImGui::PopID();
      if (!any) ImGui::EndDisabled();
      if (build_group_ != g && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::PushFont(ui_fonts().bold, 0.0f);
        ImGui::TextUnformatted(other ? "Other" : kGroups[g].name);
        ImGui::PopFont();
        ImGui::TextDisabled("%s", !any    ? "Nothing here is unlocked yet"
                                  : other ? "Buildings added by mods"
                                          : kGroups[g].blurb);
        ImGui::EndTooltip();
      }
    }
    rail_r_ = ImGui::GetWindowPos().x + ImGui::GetWindowWidth();
    ImGui::End();
  } else {
    // The rail is hidden (B): one small button brings it back.
    build_group_ = -1;
    ImGui::SetNextWindowPos(ImVec2(8, hud_h_ + 8), ImGuiCond_Always);
    begin_window("##buildshow", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    if (icons_.icon_button("##showbuild", "ui/icon_build", std::round(ImGui::GetFontSize() * 1.25f), false))
      show_build_ = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Build (B)");
    rail_r_ = ImGui::GetWindowPos().x + ImGui::GetWindowWidth();
    ImGui::End();
  }

  if (build_group_ >= 0) {
    const int g = build_group_;
    const float y = std::clamp(build_fly_y_ - 10, hud_h_ + 8, std::max(hud_h_ + 8, ds.y - build_fly_h_ - 8));
    ImGui::SetNextWindowPos(ImVec2(rail_r_ + 6, y), ImGuiCond_Always);
    if (opened) ImGui::SetNextWindowFocus();
    begin_window("##buildfly", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoSavedSettings);
    heading(g == kOtherGroup ? "Other" : kGroups[g].name, 1.25f);
    ImGui::Separator();

    const UiFonts& f = ui_fonts();
    const float fs = ImGui::GetFontSize();
    const float rh = std::round(fs * 2.7f), thumb = rh - 4, row_w = std::round(fs * 19);
    // One row: picture, name, hint, cost (red when the treasury is short). Returns true when clicked.
    auto row = [&](const std::string& key, const std::string& name, const std::string& hint, i64 cost, bool sel) {
      const ImVec2 p = ImGui::GetCursorScreenPos();
      ImGui::PushID(key.c_str());
      const bool clicked = ImGui::Selectable("##row", sel, 0, ImVec2(row_w, rh));
      ImGui::PopID();
      ImDrawList* dl = ImGui::GetWindowDrawList();
      dl->PushClipRect(p, ImVec2(p.x + row_w, p.y + rh), true);
      dl->AddRectFilled(ImVec2(p.x + 1, p.y + 2), ImVec2(p.x + 1 + thumb, p.y + 2 + thumb),
                        ImGui::GetColorU32(ImGuiCol_FrameBg), 3);
      icons_.draw_thumb(key, ImVec2(p.x + 1, p.y + 2), thumb);
      const float tx = p.x + thumb + 10;
      float cost_w = 0;
      if (cost > 0) {
        const std::string c = std::to_string(cost);
        cost_w = f.body->CalcTextSizeA(fs, FLT_MAX, 0, c.c_str()).x + fs + 6;
        const ImU32 col = w.econ.treasury < cost ? IM_COL32(170, 40, 24, 255) : ImGui::GetColorU32(ImGuiCol_Text);
        icons_.draw_icon("goods/denarii_16", ImVec2(p.x + row_w - cost_w, p.y + rh * 0.28f - fs * 0.5f + 2), fs);
        dl->AddText(f.body, fs, ImVec2(p.x + row_w - cost_w + fs + 4, p.y + rh * 0.28f - fs * 0.5f), col, c.c_str());
      }
      dl->AddText(f.bold, fs, ImVec2(tx, p.y + rh * 0.28f - fs * 0.5f), ImGui::GetColorU32(ImGuiCol_Text),
                  name.c_str());
      dl->AddText(f.body, fs * 0.86f, ImVec2(tx, p.y + rh * 0.70f - fs * 0.43f),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), hint.c_str());
      dl->PopClipRect();
      return clicked;
    };

    if (g == 0) {  // tools before the buildings
      if (row("tool:dirt", "Dirt road  (R)", "drag to lay", 0,
              tools_.kind == ToolKind::Road && tools_.road == sim::Road::Dirt))
        tools_.select_road(sim::Road::Dirt);
      if (w.is_unlocked(defs_.building("plaza")) &&
          row("tool:paved", "Paved road", "drag to lay \xC2\xB7 stone paving", 0,
              tools_.kind == ToolKind::Road && tools_.road == sim::Road::Paved))
        tools_.select_road(sim::Road::Paved);
      if (row("tool:demolish", "Demolish  (X)", "click or drag to clear", 0, tools_.kind == ToolKind::Demolish))
        tools_.select_demolish();
    }
    // Big groups split into raw materials and workshops (inputs or none), then into columns as tall as the screen
    // allows.
    const auto& mem = members[static_cast<size_t>(g)];
    std::vector<std::pair<const char*, std::vector<u16>>> sections;
    const float row_step = rh + ImGui::GetStyle().ItemSpacing.y;
    const size_t kRows = static_cast<size_t>(std::max(6.0f, std::floor((ds.y - hud_h_ - fs * 6) / row_step)));
    if (mem.size() > 12) {
      std::vector<u16> raw, made;
      for (u16 i : mem) {
        const auto& d = defs_.buildings[i];
        (d.recipe.valid() && !defs_.recipes[d.recipe.v].inputs.empty() ? made : raw).push_back(i);
      }
      if (!raw.empty()) sections.emplace_back("Raw materials", std::move(raw));
      if (!made.empty()) sections.emplace_back("Workshops", std::move(made));
    } else {
      sections.emplace_back(nullptr, mem);
    }
    bool first_col = true;
    for (const auto& [title, ids] : sections) {
      const size_t cols = (ids.size() + kRows - 1) / kRows, per = (ids.size() + cols - 1) / std::max<size_t>(1, cols);
      for (size_t c0 = 0; c0 < ids.size(); c0 += per) {
        if (!first_col) ImGui::SameLine(0, 12);
        first_col = false;
        ImGui::BeginGroup();
        if (sections.size() > 1) ImGui::TextDisabled("%s", c0 == 0 ? title : " ");
        for (size_t k = c0; k < ids.size() && k < c0 + per; ++k) {
          const u16 i = ids[k];
          const auto& d = defs_.buildings[i];
          const bool sel = tools_.kind == ToolKind::Build && tools_.def.v == i;
          if (row(d.id, d.name, build_hint(defs_, d), d.cost_denarii, sel)) tools_.select_building(BuildingDefId{i});
          if (ImGui::IsItemHovered()) building_tooltip(defs_, d);
        }
        ImGui::EndGroup();
      }
    }
    // Temples: which god the new temple is dedicated to.
    if (tools_.kind == ToolKind::Build && tools_.def.valid() && group_of(defs_.buildings[tools_.def.v].category) == g) {
      const auto& d = defs_.buildings[tools_.def.v];
      if (d.service.valid() && defs_.services[d.service.v].need == data::NeedKind::Religion && d.id != "shrine") {
        ImGui::Separator();
        ImGui::TextDisabled("Dedicate to:");
        for (u8 gd = 0; gd < defs_.gods.size(); ++gd) {
          if (!w.god_available[gd]) continue;
          ImGui::SameLine();
          if (ImGui::RadioButton(defs_.gods[gd].name.c_str(), tools_.god == gd)) tools_.god = gd;
        }
      }
    }
    build_fly_h_ = ImGui::GetWindowHeight();
    ImGui::End();
  }

  // The tool in hand, its cost, and how to put it down.
  if (tools_.kind != ToolKind::Inspect) {
    std::string what = tools_.kind == ToolKind::Demolish ? std::string("Demolish")
                       : tools_.kind == ToolKind::Road
                           ? std::string(tools_.road == sim::Road::Paved ? "Paved road" : "Dirt road")
                       : tools_.def.valid() ? defs_.buildings[tools_.def.v].name
                                            : std::string();
    ImGui::SetNextWindowPos(ImVec2(rail_r_ + 6, ds.y - 8), ImGuiCond_Always, ImVec2(0, 1));
    begin_window("##toolchip", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::PushFont(ui_fonts().bold, 0.0f);
    ImGui::TextUnformatted(what.c_str());
    ImGui::PopFont();
    const i64 cost = tools_.cost(*world_);
    if (cost > 0) {
      ImGui::SameLine(0, 12);
      icons_.image("goods/denarii_16", ImGui::GetFontSize());
      ImGui::SameLine(0, 4);
      ImGui::Text("%lld", static_cast<long long>(cost));
    }
    if (tools_.can_rotate(*world_)) {
      ImGui::SameLine(0, 16);
      ImGui::TextDisabled("E / Q rotate");
    }
    ImGui::SameLine(0, 16);
    ImGui::TextDisabled("Right-click or Esc to stop");
    ImGui::End();
  }
}

void App::draw_stock_window(const Dock& dock, bool* open) {
  if (!begin_panel("##goods", dock)) {
    end_panel();
    return;
  }
  if (!panel_header(icons_, "ui/icon_goods", false, "Goods", "What the city holds, made and used yesterday"))
    *open = false;
  const auto& e = world_->econ;
  const float fs = ImGui::GetFontSize();
  if (ImGui::BeginTable("##goods", 6, ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
    for (int k = 0; k < 5; ++k) ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, std::round(fs * 3.6f));
    const char* heads[] = {"Good", "Stock", "Made", "Used", "Change", "Price"};
    table_headings(heads, 6);
    for (u32 g = 0; g < e.goods; ++g) {
      if (!e.city_stock[g] && !e.produced_yday[g] && !e.consumed_yday[g] && !e.imported_yday[g] && !e.exported_yday[g])
        continue;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      good_icon(icons_, defs_.goods[g].id);
      ImGui::SameLine();
      ImGui::TextUnformatted(defs_.goods[g].name.c_str());
      ImGui::TableNextColumn();
      // Nothing left of something people use: red.
      cell_right(with_commas(e.city_stock[g]), e.city_stock[g] == 0 && e.consumed_yday[g] ? kColBad : 0, true);
      ImGui::TableNextColumn();
      cell_right(with_commas(e.produced_yday[g]));
      ImGui::TableNextColumn();
      cell_right(with_commas(e.consumed_yday[g]));
      ImGui::TableNextColumn();
      const i64 change =
          static_cast<i64>(e.produced_yday[g]) + e.imported_yday[g] - e.consumed_yday[g] - e.exported_yday[g];
      cell_right(change ? signed_commas(change) : std::string("0"), change > 0 ? kColGood : change < 0 ? kColBad : 0);
      if (ImGui::IsItemHovered() && (e.imported_yday[g] || e.exported_yday[g]))
        ImGui::SetTooltip("Imported %u, exported %u", e.imported_yday[g], e.exported_yday[g]);
      ImGui::TableNextColumn();
      char price[16];
      std::snprintf(price, sizeof price, "%.1f", e.price[g] / 10.0);
      cell_right(price);
    }
    ImGui::EndTable();
  }
  ImGui::TextDisabled("Yesterday's figures. Change counts trade in and out.");
  end_panel();
}

void App::draw_debug_ui() {
  auto& cam = renderer_.camera();
  const sim::World& w = *world_;
  ImGui::SetNextWindowPos({10, 640}, ImGuiCond_FirstUseEver);
  begin_window("Debug (F12)", &show_debug_);
  ImGui::Text("%.1f FPS  (%.2f ms)  sim %.2f ms", static_cast<double>(ImGui::GetIO().Framerate),
              static_cast<double>(frame_ms_), sim_ms_);
  ImGui::Text("citizens %u  walking %u  buildings %u  soldiers %u", w.citizens.ids.count(), w.walking,
              w.buildings.ids.count(), w.army.deployed);
  ImGui::Text("appeal %d", w.appeal);
  {
    const std::string& track = audio_.current_music();
    ImGui::Text("music: %s  %s", music_.mood_name(),
                track.empty() ? "(silence)" : track.substr(track.rfind('/') + 1).c_str());
  }
  const auto& st = renderer_.stats();
  ImGui::Text("zoom %.2fx  chunks %u  inst %u  dyn %u", static_cast<double>(render::kZoomLevels[cam.zoom]),
              st.chunks_drawn, st.chunk_instances, st.dynamic_instances);
  float mx, my;
  SDL_GetMouseState(&mx, &my);
  i32 tx = -1, ty = -1;
  if (render::iso::pick_tile(w.map, cam.screen_to_world({mx, my}), tx, ty)) {
    size_t i = static_cast<size_t>(w.map.idx(tx, ty));
    ImGui::Text("tile %d,%d %s h%u gw%u fert%u", tx, ty, sim::terrain_name(w.map.terrain[i]), w.map.height[i],
                w.map.groundwater[i], w.map.fertility[i]);
  }
  if (ImGui::Button("Threats panel")) show_threats_ = !show_threats_;
  ImGui::End();
}

}  // namespace urbs::game
