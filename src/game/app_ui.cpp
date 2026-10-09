// Screens and HUD: main menu, campaign map and briefing, sandbox setup, in-game HUD, pause,
// save slots, settings, research, build menu, goods, help, debug.
#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <string>

#include "game/app.hpp"
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
std::string with_commas(i64 v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
const char* kDifficulty[] = {"Otium (relaxed)", "Civitas (normal)", "Imperium (hard)"};
}  // namespace

void App::draw_ui() {
  switch (screen_) {
    case Screen::Menu: draw_main_menu(); break;
    case Screen::Campaign: draw_campaign(); break;
    case Screen::Sandbox: draw_sandbox_setup(); break;
    case Screen::Playing:
      draw_hud();
      draw_build_menu();
      draw_stock_window();
      draw_citizen_panel(*world_, sel_);
      draw_building_panel(*world_, sel_);
      draw_labor_panel(*world_, &show_labor_);
      draw_finance_panel(*world_, &show_finance_);
      draw_trade_panel(*world_, &show_trade_);
      draw_religion_panel(*world_, &show_religion_);
      draw_threats_panel(*world_, sel_, &show_threats_);
      draw_military_panel(*world_, sel_, &show_military_);
      draw_rome_panel(*world_, &show_rome_);
      draw_advisors_panel(*world_, icons_, &advisor_, &show_advisors_);
      draw_research();
      draw_help();
      if (show_debug_) draw_debug_ui();
      if (BuildingId jump = draw_alerts_panel(*world_, &show_alerts_); jump.valid() && world_->buildings.alive(jump)) {
        const auto& jb = world_->buildings[jump];
        renderer_.camera().center = {static_cast<f32>(jb.x * sim::kTilePx), static_cast<f32>(jb.y * sim::kTilePx)};
        sel_.building = jump;
      }
      draw_messages(*world_, icons_, &last_message_);
      if (world_->mission != sim::MissionState::Running && !end_dismissed_) {
        center_next(420, 0);
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);
        ImGui::Begin("##end", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
        if (world_->mission == sim::MissionState::Won) {
          ImGui::Text("VICTORIA!");
          ImGui::Separator();
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
          ImGui::Text("The mission has failed");
          ImGui::Separator();
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
    ImGui::Begin("##status", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs);
    ImGui::TextUnformatted(status_.c_str());
    ImGui::End();
  }
}

void App::draw_main_menu() {
  center_next(360, 350);
  ImGui::Begin("##menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
  ImGui::SetWindowFontScale(2.4f);
  const float tw = ImGui::CalcTextSize("U R B S").x;
  ImGui::SetCursorPosX((360 - tw) * 0.5f);
  ImGui::TextUnformatted("U R B S");
  ImGui::SetWindowFontScale(1.0f);
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
  if (big_button("Load game")) {
    show_saveload_ = true;
    saveload_saving_ = false;
  }
  ImGui::SetCursorPosX(x);
  if (big_button("Settings")) show_settings_ = true;
  ImGui::SetCursorPosX(x);
  if (big_button("Quit")) running_ = false;
  ImGui::Dummy(ImVec2(0, 10));
  ImGui::TextDisabled("v0.1  -  noncommercial, source available");
  ImGui::End();
}

void App::draw_campaign() {
  const ImVec2 ds = display();
  ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(ds.x - 40, ds.y - 40), ImGuiCond_Always);
  ImGui::Begin("The Province of the Career", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
  const float map_w = (ds.x - 40) * 0.62f, map_h = ds.y - 120;
  // Stylised Mediterranean: sea, land masses as soft blobs, the road of the career between missions.
  ImGui::BeginChild("map", ImVec2(map_w, map_h), true);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetCursorScreenPos();
  const float W = map_w - 16, H = map_h - 16;
  dl->AddRectFilled(o, ImVec2(o.x + W, o.y + H), IM_COL32(64, 104, 140, 255));
  struct Blob {
    float x, y, rx, ry;
  };
  // Rough land masses: Hispania, Gallia, Italia, Germania, Britannia, Graecia, Asia, Africa, Aegyptus.
  const Blob land[] = {{0.17f, 0.48f, 0.12f, 0.11f}, {0.34f, 0.3f, 0.12f, 0.12f}, {0.47f, 0.37f, 0.035f, 0.1f},
                       {0.52f, 0.45f, 0.03f, 0.06f}, {0.45f, 0.14f, 0.16f, 0.08f}, {0.3f, 0.09f, 0.05f, 0.06f},
                       {0.67f, 0.47f, 0.05f, 0.07f}, {0.88f, 0.48f, 0.12f, 0.1f},  {0.5f, 0.79f, 0.5f, 0.12f},
                       {0.82f, 0.7f, 0.08f, 0.06f},  {0.5f, 0.54f, 0.03f, 0.02f},  {0.62f, 0.3f, 0.12f, 0.08f}};
  for (const Blob& b : land)
    dl->AddEllipseFilled(ImVec2(o.x + b.x * W, o.y + b.y * H), ImVec2(b.rx * W, b.ry * H), IM_COL32(196, 170, 112, 255), 0, 32);
  // Path of the career.
  const auto& M = campaign_.missions;
  for (size_t k = 1; k < M.size(); ++k)
    dl->AddLine(ImVec2(o.x + M[k - 1].x * W, o.y + M[k - 1].y * H), ImVec2(o.x + M[k].x * W, o.y + M[k].y * H),
                IM_COL32(120, 60, 40, 120), 2.0f);
  const int next = profile_.next_index(campaign_);
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
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextWrapped("%d. %s", selected_mission_ + 1, m.name.c_str());
    ImGui::SetWindowFontScale(1.0f);
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
  center_next(420, 360);
  ImGui::Begin("Sandbox", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
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
  ImGui::Spacing();
  if (big_button("Found the city")) start_sandbox();
  if (big_button("Back")) screen_ = Screen::Menu;
  ImGui::End();
}

void App::draw_hud() {
  const sim::World& w = *world_;
  const ImVec2 ds = display();
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(ds.x, 0), ImGuiCond_Always);
  ImGui::Begin("##hud", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
  icons_.image("goods/denarii_16", 18);
  ImGui::SameLine();
  ImGui::Text("%s", with_commas(w.econ.treasury).c_str());
  ImGui::SameLine(0, 20);
  ImGui::Text("Pop %u", w.population());
  ImGui::SameLine(0, 20);
  ImGui::Text("Day %llu (%s), %02u:00", static_cast<unsigned long long>(sim::day_of_year(w.now) + 1),
              sim::season_name(sim::season(w.now)), sim::hour_of_day(w.now));
  ImGui::SameLine(0, 20);
  ImGui::Text("Year %llu", static_cast<unsigned long long>(sim::year_index(w.now) + 1));
  ImGui::SameLine(0, 20);
  const char* speeds[] = {"||", "1x", "2x", "4x"};
  for (int i = 0; i < 4; ++i) {
    if (i) ImGui::SameLine();
    const bool on = speed_ == i;
    if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::SmallButton(speeds[i])) speed_ = i;
    if (on) ImGui::PopStyleColor();
  }
  ImGui::SameLine(0, 20);
  ImGui::Text("Mood %u  Cul %u  Pros %u  Sec %u  Favor %d", w.avg_happiness, w.ratings.culture, w.ratings.prosperity,
              w.ratings.security, w.favor);
  // Panel buttons on the second row.
  struct Btn {
    const char* label;
    bool* flag;
  };
  Btn btns[] = {{"Build", &show_build_},     {"Rome", &show_rome_},         {"Advisors", &show_advisors_},
                {"Finance", &show_finance_}, {"Goods", &show_stock_},       {"Trade", &show_trade_},
                {"Labor", &show_labor_},     {"Religion", &show_religion_}, {"Military", &show_military_},
                {"Research", &show_research_}, {"Alerts", &show_alerts_},   {"Help", &show_help_}};
  for (size_t k = 0; k < sizeof(btns) / sizeof(btns[0]); ++k) {
    if (k) ImGui::SameLine();
    const bool on = *btns[k].flag;
    if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(btns[k].label)) *btns[k].flag = !*btns[k].flag;
    if (on) ImGui::PopStyleColor();
  }
  ImGui::SameLine(0, 20);
  int ov = static_cast<int>(overlay_);
  ImGui::SetNextItemWidth(150);
  const char* names[static_cast<int>(Overlay::Count)];
  for (int k = 0; k < static_cast<int>(Overlay::Count); ++k) names[k] = overlay_name(static_cast<Overlay>(k));
  if (ImGui::Combo("##ov", &ov, names, static_cast<int>(Overlay::Count))) {
    overlay_ = static_cast<Overlay>(ov);
    overlay_age_ = 1 << 20;
  }
  if (overlay_ == Overlay::Service) {
    ImGui::SameLine();
    int sv = overlay_service_.valid() ? overlay_service_.v : 0;
    ImGui::SetNextItemWidth(150);
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
  ImGui::SameLine(0, 20);
  if (ImGui::Button("Menu (Esc)")) paused_menu_ = true;
  // Latest alert as a ticker.
  if (!w.alerts.empty()) {
    ImGui::SameLine(0, 20);
    const auto& a = w.alerts.back();
    if (w.now - a.t < kTicksPerDay) ImGui::TextColored(ImVec4(0.55f, 0.15f, 0.08f, 1), "%s", alert_text(w, a).c_str());
  }
  ImGui::End();
}

void App::draw_pause() {
  center_next(320, 330);
  ImGui::Begin("Paused", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
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
  center_next(380, 300);
  ImGui::Begin("Settings", &show_settings_, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
  if (ImGui::SliderFloat("UI scale", &settings_.ui_scale, 0.75f, 2.0f, "%.2f")) apply_roman_style(settings_.ui_scale);
  ImGui::Checkbox("Scroll at screen edges", &settings_.edge_scroll);
  ImGui::Checkbox("Autosave every four days", &settings_.autosave);
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
  center_next(460, 420);
  ImGui::Begin(saveload_saving_ ? "Save game" : "Load game", &show_saveload_, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
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
  ImGui::End();
}

void App::draw_research() {
  if (!show_research_) return;
  ImGui::SetNextWindowPos({700, 140}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({420, 380}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Civic research", &show_research_);
  ImGui::TextWrapped("Scholars in schools, libraries and academies produce scientia. Spend it on techniques for this city.");
  ImGui::Text("Scientia: %u", world_->scientia);
  ImGui::Separator();
  for (u16 t = 0; t < defs_.techniques.size(); ++t) {
    const auto& tech = defs_.techniques[t];
    ImGui::PushID(t);
    if (world_->tech_known[t]) {
      ImGui::TextColored(ImVec4(0.3f, 0.45f, 0.1f, 1), "[known] %s", tech.name.c_str());
    } else {
      const bool can = world_->scientia >= tech.cost;
      if (!can) ImGui::BeginDisabled();
      if (ImGui::Button(("Study (" + std::to_string(tech.cost) + ")").c_str())) {
        sim::Command c;
        c.type = sim::CmdType::Research;
        c.def = t;
        world_->push(c);
      }
      if (!can) ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::TextUnformatted(tech.name.c_str());
    }
    ImGui::TextDisabled("   %s", tech.desc.c_str());
    ImGui::PopID();
  }
  if (mission_index_ >= 0) {
    ImGui::Separator();
    ImGui::Text("Personal savings: %s", with_commas(profile_.savings).c_str());
    if (profile_.savings >= 500 && ImGui::Button("Donate 500 to the city treasury")) {
      profile_.savings -= 500;
      sim::Command c;
      c.type = sim::CmdType::Grant;
      c.value = 500;
      world_->push(c);
    }
  }
  ImGui::End();
}

void App::draw_help() {
  if (!show_help_) return;
  ImGui::SetNextWindowPos({300, 140}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({460, 420}, ImGuiCond_FirstUseEver);
  ImGui::Begin("How to play", &show_help_);
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
      "Porters from warehouses, granaries and markets carry goods; farms and workshops need them nearby.");
  ImGui::End();
}

void App::draw_build_menu() {
  if (!show_build_) return;
  ImGui::SetNextWindowPos({10, 70}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({280, 560}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Build", &show_build_);
  if (ImGui::Button("Dirt road (R)")) tools_.select_road(sim::Road::Dirt);
  ImGui::SameLine();
  if (world_->is_unlocked(defs_.building("plaza")) && ImGui::Button("Paved road")) tools_.select_road(sim::Road::Paved);
  if (ImGui::Button("Demolish (X)")) tools_.select_demolish();
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) tools_.cancel();
  if (tools_.kind != ToolKind::Inspect) {
    const i64 cost = tools_.cost(*world_);
    ImGui::Text("Cost: %lld", static_cast<long long>(cost));
  }
  static const char* kCats[] = {"housing", "farm", "food",      "industry", "storage", "water",    "health",
                                "religion", "entertainment", "education", "safety",   "civic",   "trade",
                                "infrastructure", "decor", "military", "monument"};
  static const char* kCatNames[] = {"Housing", "Farms", "Food", "Industry", "Storage & markets", "Water", "Health",
                                    "Religion", "Entertainment", "Education", "Safety", "Government", "Trade",
                                    "Infrastructure", "Decoration", "Military", "Monuments"};
  for (size_t ci = 0; ci < 17; ++ci) {
    bool any = false;
    for (u16 i = 0; i < defs_.buildings.size(); ++i)
      if (defs_.buildings[i].category == kCats[ci] && world_->is_unlocked(BuildingDefId{i})) any = true;
    if (!any || !ImGui::CollapsingHeader(kCatNames[ci])) continue;
    for (u16 i = 0; i < defs_.buildings.size(); ++i) {
      const auto& d = defs_.buildings[i];
      if (d.category != kCats[ci] || !world_->is_unlocked(BuildingDefId{i})) continue;
      const bool sel = tools_.kind == ToolKind::Build && tools_.def.v == i;
      std::string label = d.name + "  (" + std::to_string(d.cost_denarii) + ")";
      if (ImGui::Selectable(label.c_str(), sel)) tools_.select_building(BuildingDefId{i});
      if (ImGui::IsItemHovered()) {
        std::string tip = std::to_string(d.w) + "x" + std::to_string(d.h) + (d.painted ? " (drag to paint an area)" : "");
        for (const auto& c : d.cost_goods) tip += "\n" + std::to_string(c.amount) + " " + defs_.goods[c.good.v].name;
        if (d.workers) tip += "\nWorkers: " + std::to_string(d.workers);
        if (d.recipe.valid()) {
          const auto& r = defs_.recipes[d.recipe.v];
          tip += "\nMakes:";
          for (const auto& o : r.outputs) tip += " " + defs_.goods[o.good.v].name;
          if (!r.inputs.empty()) {
            tip += "\nFrom:";
            for (const auto& in : r.inputs) tip += " " + defs_.goods[in.good.v].name;
          }
        }
        if (d.resource) {
          static const char* kRes[] = {"", "clay", "stone", "iron ore", "silver ore", "marble", "limestone", "pozzolana", "fish"};
          tip += std::string("\nMust be built on ") + kRes[d.resource] + " deposits";
        }
        if (d.needs_water_adjacent) tip += "\nMust be next to water";
        ImGui::SetTooltip("%s", tip.c_str());
      }
      if (sel && d.service.valid() && defs_.services[d.service.v].need == data::NeedKind::Religion && d.id != "shrine") {
        ImGui::TextDisabled("  Dedicate to:");
        for (u8 g = 0; g < defs_.gods.size(); ++g) {
          if (!world_->god_available[g]) continue;
          ImGui::SameLine();
          if (ImGui::RadioButton(defs_.gods[g].name.c_str(), tools_.god == g)) tools_.god = g;
        }
      }
    }
  }
  ImGui::End();
}

void App::draw_stock_window() {
  if (!show_stock_) return;
  ImGui::SetNextWindowPos({1200, 70}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({380, 420}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Goods", &show_stock_);
  const auto& e = world_->econ;
  if (ImGui::BeginTable("goods", 6, ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Good");
    ImGui::TableSetupColumn("Stock");
    ImGui::TableSetupColumn("Made");
    ImGui::TableSetupColumn("Used");
    ImGui::TableSetupColumn("Trade");
    ImGui::TableSetupColumn("Price");
    ImGui::TableHeadersRow();
    for (u32 g = 0; g < e.goods; ++g) {
      if (!e.city_stock[g] && !e.produced_yday[g] && !e.consumed_yday[g] && !e.imported_yday[g] && !e.exported_yday[g]) continue;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      icons_.image("goods/" + defs_.goods[g].id + "_16", 14);
      ImGui::SameLine();
      ImGui::TextUnformatted(defs_.goods[g].name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%u", e.city_stock[g]);
      ImGui::TableNextColumn();
      ImGui::Text("%u", e.produced_yday[g]);
      ImGui::TableNextColumn();
      ImGui::Text("%u", e.consumed_yday[g]);
      ImGui::TableNextColumn();
      if (e.imported_yday[g] || e.exported_yday[g]) ImGui::Text("+%u/-%u", e.imported_yday[g], e.exported_yday[g]);
      ImGui::TableNextColumn();
      ImGui::Text("%.1f", e.price[g] / 10.0);
    }
    ImGui::EndTable();
  }
  ImGui::TextDisabled("Made, used and traded are yesterday's figures.");
  ImGui::End();
}

void App::draw_debug_ui() {
  auto& cam = renderer_.camera();
  const sim::World& w = *world_;
  ImGui::SetNextWindowPos({10, 640}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Debug (F12)", &show_debug_);
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
