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
          renderer_.camera().center = {static_cast<f32>(jb.x * sim::kTilePx), static_cast<f32>(jb.y * sim::kTilePx)};
          sel_.building = jump;
        }
      }
      if (show_debug_) draw_debug_ui();
      draw_messages(*world_, icons_, &last_message_);
      if (world_->mission != sim::MissionState::Running && !end_dismissed_) {
        center_next(420, 0);
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);
        begin_window("##end", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
        if (world_->mission == sim::MissionState::Won) {
          menu_title("Victoria!");
          ImGui::TextWrapped("The Senate is pleased with your work. You have earned promotion and a greater charge.");
          ImGui::Text("Personal savings: %s denarii", with_commas(profile_.savings).c_str());
          if (mission_index_ >= 0) {
            if (big_button("To the province map")) {
              const auto& id = campaign_.missions[static_cast<size_t>(mission_index_)].id;
              if (!profile_.done(id)) profile_.completed.push_back(id);
              profile_.save(user_dir_);
              selected_mission_ = profile_.next_index(campaign_);
              menu_background();
              screen_ = Screen::Campaign;
            }
          }
          if (big_button("Keep building this city")) end_dismissed_ = true;
        } else {
          menu_title("The mission has failed");
          ImGui::TextWrapped("%s", world_->lost_reason.c_str());
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

void App::draw_main_menu() {
  center_next(360, 540);
  begin_window("##menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
  ImGui::SetCursorPosX((360 - 300) * 0.5f);
  if (!icons_.picture("ui/logo", ImVec2(300, 90))) {
    heading("U R B S", 2.4f);
  }
  const char* sub = "Build Rome one family at a time";
  ImGui::SetCursorPosX((360 - ImGui::CalcTextSize(sub).x) * 0.5f);
  ImGui::TextDisabled("%s", sub);
  ImGui::Dummy(ImVec2(0, 14));
  const float x = (360 - 260) * 0.5f;
  ImGui::SetCursorPosX(x);
  if (big_button("Campaign")) screen_ = Screen::Campaign;
  ImGui::SetCursorPosX(x);
  if (big_button("Sandbox")) screen_ = Screen::Sandbox;
  ImGui::SetCursorPosX(x);
  if (big_button("Custom scenarios")) screen_ = Screen::Custom;
  ImGui::SetCursorPosX(x);
  if (big_button("Scenario editor")) editor_new();
  ImGui::SetCursorPosX(x);
  if (big_button("Mods")) screen_ = Screen::Mods;
  ImGui::SetCursorPosX(x);
  if (big_button("Load game")) {
    show_saveload_ = true;
    saveload_saving_ = false;
  }
  ImGui::SetCursorPosX(x);
  if (big_button("Settings")) show_settings_ = true;
  ImGui::SetCursorPosX(x);
  if (big_button("Quit")) running_ = false;
  ImGui::Dummy(ImVec2(0, 10));
  ImGui::TextDisabled("v%s  -  noncommercial, source available", URBS_VERSION);
  ImGui::End();
}

void App::draw_campaign() {
  const ImVec2 ds = display();
  ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(ds.x - 40, ds.y - 40), ImGuiCond_Always);
  begin_window(
      "##campaign", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
  menu_title("The Province of the Career");
  float map_w = (ds.x - 40) * 0.64f, map_h = ds.y - 160;
  map_h = std::min(map_h, (map_w - 16) * 288.0f / 512.0f + 16);
  // Stylised Mediterranean: sea, land masses as soft blobs, the road of the career between missions.
  ImGui::BeginChild("map", ImVec2(map_w, map_h), true);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetCursorScreenPos();
  const float W = map_w - 16, H = map_h - 16;
  ImVec2 uv0, uv1;
  if (icons_.big_uv("ui/province_map", uv0, uv1)) {
    dl->AddImage(icons_.big_texture(), o, ImVec2(o.x + W, o.y + H), uv0, uv1);
  } else {
    dl->AddRectFilled(o, ImVec2(o.x + W, o.y + H), IM_COL32(64, 104, 140, 255));
  }
  // Path of the career.
  const auto& M = campaign_.missions;
  const int next = profile_.next_index(campaign_);
  for (size_t k = 1; k < M.size() && static_cast<int>(k) <= next; ++k)
    dl->AddLine(ImVec2(o.x + M[k - 1].x * W, o.y + M[k - 1].y * H), ImVec2(o.x + M[k].x * W, o.y + M[k].y * H),
                IM_COL32(150, 60, 35, 200), 2.0f);
  for (size_t k = 0; k < M.size(); ++k) {
    const ImVec2 p(o.x + M[k].x * W, o.y + M[k].y * H);
    const bool done = profile_.done(M[k].id), open = static_cast<int>(k) <= next;
    const ImU32 col = done ? IM_COL32(220, 180, 60, 255) : open ? IM_COL32(190, 70, 50, 255) : IM_COL32(110, 100, 90, 255);
    dl->AddCircleFilled(p, static_cast<int>(k) == selected_mission_ ? 9.0f : 6.0f, col);
    dl->AddCircle(p, static_cast<int>(k) == selected_mission_ ? 9.0f : 6.0f, IM_COL32(40, 25, 15, 255), 0, 2);
    ImGui::SetCursorScreenPos(ImVec2(p.x - 9, p.y - 9));
    ImGui::PushID(static_cast<int>(k));
    if (ImGui::InvisibleButton("m", ImVec2(18, 18)) && open) selected_mission_ = static_cast<int>(k);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%zu. %s", k + 1, M[k].name.c_str());
    ImGui::PopID();
  }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("brief", ImVec2(0, map_h), true);
  if (selected_mission_ >= 0 && selected_mission_ < static_cast<int>(M.size())) {
    const auto& m = M[static_cast<size_t>(selected_mission_)];
    heading((std::to_string(selected_mission_ + 1) + ". " + m.name).c_str(), 1.5f);
    ImGui::TextDisabled("%s  -  rank: %s  -  salary %d/day", m.region.c_str(), m.rank.c_str(), m.salary);
    ImGui::Separator();
    ImGui::TextWrapped("%s", m.blurb.c_str());
    ImGui::Spacing();
    if (profile_.done(m.id)) ImGui::TextColored(ImVec4(0.55f, 0.4f, 0.05f, 1), "Completed");
    const bool available = std::filesystem::exists(root_ + "data/scenarios/" + m.id + "/scenario.json");
    ImGui::Spacing();
    ImGui::SetNextItemWidth(220);
    ImGui::Combo("Difficulty", &settings_.difficulty, kDifficulty, 3);
    if (available) {
      if (big_button("Begin the mission")) start_mission(selected_mission_);
    } else {
      ImGui::TextDisabled("This mission is still being written.");
    }
  }
  ImGui::Separator();
  ImGui::Text("Career: %zu of %zu missions", profile_.completed.size(), campaign_.missions.size());
  ImGui::Text("Personal savings: %s denarii", with_commas(profile_.savings).c_str());
  ImGui::TextWrapped("Cities you have built trade with you in later missions.");
  ImGui::EndChild();
  if (big_button("Back", 160)) screen_ = Screen::Menu;
  ImGui::SameLine();
  if (big_button("Reset career", 160)) {
    profile_ = CampaignProfile{};
    profile_.save(user_dir_);
    selected_mission_ = 0;
  }
  ImGui::End();
}

void App::draw_sandbox_setup() {
  center_next(420, 480);
  begin_window(
      "##sandbox", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
  menu_title("Sandbox");
  ImGui::TextWrapped("A free city with every building unlocked and 15,000 denarii.");
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
  for (const char* s : slots) {
    if (saveload_saving_ && (std::string(s) == "autosave")) continue;
    save::SaveInfo info;
    const bool exists = save::read_info(save_path(s), info);
    std::string label = std::string(s) + ": ";
    if (exists) {
      label += (info.mission.empty() ? std::string("city") : info.mission) + ", year " +
               std::to_string(sim::year_index(info.now) + 1) + " day " + std::to_string(sim::day_of_year(info.now) + 1) +
               ", " + std::to_string(info.citizens) + " people";
    } else {
      label += "(empty)";
    }
    ImGui::PushID(s);
    const bool can = saveload_saving_ || exists;
    if (!can) ImGui::BeginDisabled();
    if (ImGui::Button(label.c_str(), ImVec2(420, 30))) {
      if (saveload_saving_) {
        save_slot(s);
      } else {
        load_slot(s);
        paused_menu_ = false;
      }
      show_saveload_ = false;
    }
    if (!can) ImGui::EndDisabled();
    ImGui::PopID();
  }
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
      "Building: open Build (B), pick a building, left-click to place. Farms and gardens: drag an area. "
      "Roads (R): drag a line, diagonals allowed. Demolish (X): drag a rectangle. Right-click or Esc cancels.\n"
      "Inspect: left-click a building or a walking citizen. F follows the selected citizen.\n"
      "Soldiers: click a soldier to select the unit, right-click the map to march there.\n"
      "Overlays (O): desirability, water, pipes, any service's coverage, housing level and more.\n"
      "Saving: F5 quicksave, F9 quickload, or the menu (Esc).\n\n"
      "Houses grow when their residents have what they need: water, food variety, worship, markets, baths and "
      "more. Click a house to see what it needs next. Workers come from houses, so keep people housed, fed and employed. "
      "Porters from warehouses, granaries and markets carry goods; farms and workshops need them nearby.\n\n"
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
  const auto& st = renderer_.stats();
  ImGui::Text("zoom %.2fx  chunks %u  inst %u  dyn %u", static_cast<double>(render::kZoomLevels[cam.zoom]),
              st.chunks_drawn, st.chunk_instances, st.dynamic_instances);
  float mx, my;
  SDL_GetMouseState(&mx, &my);
  Vec2 wp = cam.screen_to_world({mx, my});
  i32 tx = static_cast<i32>(wp.x) / sim::kTilePx, ty = static_cast<i32>(wp.y) / sim::kTilePx;
  if (w.map.in_bounds(tx, ty)) {
    size_t i = static_cast<size_t>(w.map.idx(tx, ty));
    ImGui::Text("tile %d,%d %s h%u gw%u fert%u", tx, ty, sim::terrain_name(w.map.terrain[i]), w.map.height[i],
                w.map.groundwater[i], w.map.fertility[i]);
  }
  if (ImGui::Button("Threats panel")) show_threats_ = !show_threats_;
  ImGui::End();
}

}  // namespace urbs::game
