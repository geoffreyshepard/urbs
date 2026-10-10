// Mods screen, custom scenario list and the scenario editor (paint terrain, set the rules, save, play).
#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "core/log.hpp"
#include "game/app.hpp"
#include "save/save.hpp"

namespace urbs::game {

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
ImVec2 display() { return ImGui::GetIO().DisplaySize; }
void center_next(float w, float h) {
  const ImVec2 ds = display();
  ImGui::SetNextWindowPos(ImVec2((ds.x - w) * 0.5f, (ds.y - h) * 0.5f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
}
bool big_button(const char* label, float w = 260) { return ImGui::Button(label, ImVec2(w, 34)); }

constexpr const char* kTerrains[] = {"Grass", "Fertile", "Sand", "Rock", "Forest", "Marsh", "Shallow water", "Deep water"};
constexpr const char* kResources[] = {"(none)", "Clay", "Stone", "Iron ore", "Silver ore", "Marble", "Limestone", "Pozzolana", "Fish"};
constexpr const char* kBrushes[] = {"Terrain", "Raise ground", "Lower ground", "Resource", "Clear resource", "Road", "Erase road"};
constexpr const char* kEras[] = {"republic", "late_republic", "empire"};
constexpr const char* kRegions[] = {"", "north", "east", "egypt"};
constexpr const char* kRegionNames[] = {"Italy", "North (Gaul, Germania, Britannia)", "East (Greece, Syria)", "Egypt"};
constexpr const char* kSides[] = {"North", "East", "South", "West"};

std::string slug(const std::string& s) {
  std::string out;
  for (char c : s) {
    if (std::isalnum(static_cast<unsigned char>(c))) out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    else if (!out.empty() && out.back() != '_') out += '_';
  }
  while (!out.empty() && out.back() == '_') out.pop_back();
  return out.empty() ? "scenario" : out;
}

bool input_string(const char* label, std::string& s, size_t cap = 64) {
  char buf[128];
  std::snprintf(buf, sizeof buf, "%s", s.c_str());
  if (ImGui::InputText(label, buf, std::min(cap, sizeof buf))) {
    s = buf;
    return true;
  }
  return false;
}
}  // namespace

// ------------------------------------------------------------------------------------------------
// Mods
// ------------------------------------------------------------------------------------------------

bool App::reload_defs(std::string* err) {
  try {
    data::Defs d = data::load_defs(root_ + "data", enabled_mod_dirs(settings_, mods_));
    base_defs_ = std::move(d);
    defs_ = base_defs_;
    view_.init(renderer_.atlas(), defs_, world_ ? world_->region : std::string());
    return true;
  } catch (const std::exception& e) {
    if (err) *err = e.what();
    return false;
  }
}

void App::draw_mods() {
  center_next(640, 520);
  ImGui::Begin("Mods", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
  ImGui::TextWrapped("Mods add or change goods, buildings, housing, gods, units, trade partners and scenarios. "
                     "Enabled mods load in this order after the base game. See docs/10-modding.md.");
  ImGui::TextDisabled("Folders: %smods  and  %smods", root_.c_str(), user_dir_.c_str());
  ImGui::Separator();
  if (mods_.empty()) ImGui::TextDisabled("No mods found.");
  ImGui::BeginChild("list", ImVec2(0, 330), true);
  for (size_t k = 0; k < mods_.size(); ++k) {
    const auto& m = mods_[k];
    ImGui::PushID(static_cast<int>(k));
    auto it = std::find(settings_.mods.begin(), settings_.mods.end(), m.name);
    bool on = it != settings_.mods.end();
    if (ImGui::Checkbox("##on", &on)) {
      if (on) settings_.mods.push_back(m.name);
      else settings_.mods.erase(std::find(settings_.mods.begin(), settings_.mods.end(), m.name));
    }
    ImGui::SameLine();
    ImGui::Text("%s %s", m.title.c_str(), m.version.c_str());
    if (on) {
      const size_t pos = static_cast<size_t>(std::find(settings_.mods.begin(), settings_.mods.end(), m.name) - settings_.mods.begin());
      ImGui::SameLine();
      ImGui::TextDisabled("(load order %zu)", pos + 1);
      ImGui::SameLine();
      if (pos > 0 && ImGui::SmallButton("up")) std::swap(settings_.mods[pos], settings_.mods[pos - 1]);
      ImGui::SameLine();
      if (pos + 1 < settings_.mods.size() && ImGui::SmallButton("down")) std::swap(settings_.mods[pos], settings_.mods[pos + 1]);
    }
    if (!m.description.empty()) ImGui::TextWrapped("    %s", m.description.c_str());
    ImGui::PopID();
  }
  ImGui::EndChild();
  if (big_button("Apply", 180)) {
    std::string err;
    if (reload_defs(&err)) {
      status_ = settings_.mods.empty() ? "Playing without mods." : "Mods loaded.";
      menu_background();
    } else {
      status_ = "Mod error: " + err;
      settings_.mods.clear();
      reload_defs(nullptr);
      menu_background();
    }
    status_time_ = 8;
    settings_.save(user_dir_);
  }
  ImGui::SameLine();
  if (big_button("Rescan folders", 180)) mods_ = find_mods(root_, user_dir_);
  ImGui::SameLine();
  if (big_button("Back", 180)) screen_ = Screen::Menu;
  ImGui::End();
}

// ------------------------------------------------------------------------------------------------
// Custom scenarios
// ------------------------------------------------------------------------------------------------

std::vector<std::pair<std::string, std::string>> App::custom_scenarios() const {
  std::vector<std::pair<std::string, std::string>> out;
  std::vector<std::string> bases = {user_dir_ + "scenarios"};
  for (const auto& d : enabled_mod_dirs(settings_, mods_)) bases.push_back(d + "/scenarios");
  for (const auto& base : bases) {
    std::error_code ec;
    if (!fs::is_directory(base, ec)) continue;
    std::vector<fs::path> dirs;
    for (const auto& e : fs::directory_iterator(base, ec))
      if (e.is_directory() && fs::exists(e.path() / "scenario.json")) dirs.push_back(e.path());
    std::sort(dirs.begin(), dirs.end());
    for (const auto& d : dirs) {
      std::string title = d.filename().string();
      std::ifstream f(d / "scenario.json");
      try {
        json j = json::parse(f, nullptr, true, true);
        title = j.value("name", title);
      } catch (...) {
      }
      out.emplace_back(title, d.string());
    }
  }
  return out;
}

void App::draw_custom() {
  center_next(560, 480);
  ImGui::Begin("Custom scenarios", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
  ImGui::TextWrapped("Scenarios made in the editor (saved in %sscenarios) and scenarios from enabled mods.", user_dir_.c_str());
  ImGui::Separator();
  const auto list = custom_scenarios();
  ImGui::BeginChild("list", ImVec2(0, 340), true);
  if (list.empty()) ImGui::TextDisabled("None yet. Make one in the Scenario editor.");
  for (size_t k = 0; k < list.size(); ++k) {
    ImGui::PushID(static_cast<int>(k));
    ImGui::TextUnformatted(list[k].first.c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 170);
    if (ImGui::SmallButton("Play")) {
      try {
        mission_index_ = -1;
        defs_ = base_defs_;
        start_scenario(sim::load_scenario(list[k].second + "/scenario.json"));
      } catch (const std::exception& e) {
        status_ = std::string("Could not start: ") + e.what();
        status_time_ = 8;
      }
    }
    if (list[k].second.rfind(user_dir_, 0) == 0) {
      ImGui::SameLine();
      if (ImGui::SmallButton("Edit")) editor_open(list[k].second);
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
  if (big_button("Back")) screen_ = Screen::Menu;
  ImGui::End();
}

// ------------------------------------------------------------------------------------------------
// Scenario editor
// ------------------------------------------------------------------------------------------------

void App::editor_new() {
  ed_ = EditorState{};
  ed_.gods.assign(defs_.gods.size(), 0);
  for (const char* g : {"jupiter", "juno", "ceres", "vesta", "mercury"}) {
    const u8 k = defs_.god(g);
    if (k < ed_.gods.size()) ed_.gods[k] = 1;
  }
  ed_.partners.assign(defs_.partners.size(), 0);
  if (!ed_.partners.empty()) ed_.partners[0] = 1;
  json j = {{"name", "Editor"}, {"generator", "empty"},
            {"map", {{"size", gen_.size}, {"seed", gen_.seed}, {"coast", gen_.coast}, {"river", gen_.river},
                     {"hills", gen_.hills}, {"forest", gen_.forest}, {"marsh", gen_.marsh}}}};
  try {
    auto w = std::make_unique<sim::World>();
    sim::ScenarioSpec spec = sim::parse_scenario(j.dump(), "");
    sim::setup_scenario(*w, defs_, spec);
    w->now = kTicksPerHour * 12;  // paint in daylight
    world_ = std::move(w);
  } catch (const std::exception& e) {
    status_ = std::string("Editor: ") + e.what();
    status_time_ = 8;
    return;
  }
  after_world_change();
  renderer_.camera().center = {static_cast<f32>(world_->map.w * sim::kTilePx) / 2, static_cast<f32>(world_->map.h * sim::kTilePx) / 2};
  screen_ = Screen::Editor;
  speed_ = 0;
}

void App::editor_open(const std::string& dir) {
  editor_new();
  if (screen_ != Screen::Editor) return;
  try {
    std::ifstream f(dir + "/scenario.json");
    json j = json::parse(f, nullptr, true, true);
    ed_.path = dir;
    ed_.id = j.value("id", fs::path(dir).filename().string());
    ed_.name = j.value("name", ed_.id);
    ed_.mission = j.value("mission", 16);
    const std::string era = j.value("era", std::string("republic"));
    for (int k = 0; k < 3; ++k)
      if (era == kEras[k]) ed_.era = k;
    const std::string region = j.value("region", std::string());
    for (int k = 0; k < 4; ++k)
      if (region == kRegions[k]) ed_.region = k;
    ed_.entry = j.value("entry_road", 2);
    ed_.start_hour = j.value("start_hour", 7);
    if (j.contains("start")) ed_.treasury = j["start"].value("treasury", ed_.treasury);
    if (j.contains("start_stock")) {
      ed_.wood = j["start_stock"].value("wood", 0);
      ed_.wheat = j["start_stock"].value("wheat", 0);
    }
    std::fill(ed_.gods.begin(), ed_.gods.end(), u8{0});
    if (j.contains("gods"))
      for (auto& g : j["gods"]) {
        const u8 k = defs_.god(g.get<std::string>());
        if (k < ed_.gods.size()) ed_.gods[k] = 1;
      }
    std::fill(ed_.partners.begin(), ed_.partners.end(), u8{0});
    if (j.contains("trade") && j["trade"].contains("partners"))
      for (auto& p : j["trade"]["partners"])
        for (size_t k = 0; k < defs_.partners.size(); ++k)
          if (defs_.partners[k].id == p.get<std::string>()) ed_.partners[k] = 1;
    if (j.contains("goals")) {
      auto& g = j["goals"];
      ed_.population = g.value("population", 0);
      ed_.culture = g.value("culture", 0);
      ed_.prosperity = g.value("prosperity", 0);
      ed_.security = g.value("security", 0);
    }
    if (j.contains("map_file")) {
      std::string err;
      sim::TileMap m;
      if (!save::load_map(m, dir + "/" + j["map_file"].get<std::string>(), &err)) throw std::runtime_error(err);
      // Rebuild the editor world on the painted map.
      auto w = std::make_unique<sim::World>();
      w->map = std::move(m);
      w->init(sim::WorldConfig{}, defs_);
      w->now = kTicksPerHour * 12;
      world_ = std::move(w);
      after_world_change();
    }
    ed_.status = "Opened " + ed_.name;
  } catch (const std::exception& e) {
    ed_.status = std::string("Could not open: ") + e.what();
  }
  screen_ = Screen::Editor;
}

bool App::editor_save() {
  ed_.id = slug(ed_.id);
  const std::string dir = user_dir_ + "scenarios/" + ed_.id;
  std::error_code ec;
  fs::create_directories(dir, ec);
  std::string err;
  if (!save::save_map(world_->map, dir + "/map.urbsmap", &err)) {
    ed_.status = "Save failed: " + err;
    return false;
  }
  json gods = json::array(), partners = json::array();
  for (size_t k = 0; k < ed_.gods.size(); ++k)
    if (ed_.gods[k]) gods.push_back(defs_.gods[k].id);
  for (size_t k = 0; k < ed_.partners.size(); ++k)
    if (ed_.partners[k]) partners.push_back(defs_.partners[k].id);
  const i32 area = world_->map.w * world_->map.h;
  json goals = json::object();
  if (ed_.population) goals["population"] = ed_.population;
  if (ed_.culture) goals["culture"] = ed_.culture;
  if (ed_.prosperity) goals["prosperity"] = ed_.prosperity;
  if (ed_.security) goals["security"] = ed_.security;
  json j = {{"id", ed_.id},
            {"name", ed_.name},
            {"mission", ed_.mission},
            {"era", kEras[ed_.era]},
            {"region", kRegions[ed_.region]},
            {"start_hour", ed_.start_hour},
            {"map_file", "map.urbsmap"},
            {"world", {{"max_citizens", std::clamp(area / 4, 4000, 24000)}, {"max_buildings", std::clamp(area / 10, 1500, 9000)},
                       {"max_households", std::clamp(area / 9, 1600, 9000)}, {"max_soldiers", 3000}}},
            {"generator", "scripted"},
            {"entry_road", ed_.entry},
            {"start", {{"treasury", ed_.treasury}}},
            {"start_stock", {{"wood", ed_.wood}, {"wheat", ed_.wheat}}},
            {"gods", gods},
            {"trade", {{"partners", partners}}},
            {"goals", goals},
            {"script", "script.lua"}};
  {
    std::ofstream f(dir + "/scenario.json");
    f << j.dump(2) << "\n";
  }
  if (!fs::exists(dir + "/script.lua")) {
    std::ofstream f(dir + "/script.lua");
    f << "-- " << ed_.name << ": events for this scenario. See docs/10-modding.md for the urbs.* API.\n"
         "local S = urbs.state\n\n"
         "function on_start()\n"
         "  urbs.message(\"" << ed_.name << "\", \"Build a city worthy of Rome.\", 5)\n"
         "end\n\n"
         "function on_day(day)\nend\n";
  }
  ed_.path = dir;
  ed_.status = "Saved to " + dir;
  return true;
}

void App::editor_paint(Vec2i t) {
  auto& m = world_->map;
  const i32 r = ed_.size;
  for (i32 dy = -r; dy <= r; ++dy)
    for (i32 dx = -r; dx <= r; ++dx) {
      if (dx * dx + dy * dy > r * r + r) continue;
      const i32 x = t.x + dx, y = t.y + dy;
      if (!m.in_bounds(x, y)) continue;
      const size_t i = static_cast<size_t>(m.idx(x, y));
      switch (ed_.brush) {
        case 0: {
          const auto ter = static_cast<sim::Terrain>(ed_.terrain);
          const bool was_water = sim::terrain_water(m.terrain[i]);
          m.terrain[i] = ter;
          if (sim::terrain_water(ter)) {
            m.height[i] = 0;
            if (m.road[i] != sim::Road::Bridge) m.road[i] = sim::Road::None;
          } else if (was_water) {
            m.height[i] = 2;
          }
          m.fertility[i] = ter == sim::Terrain::Fertile ? 210 : ter == sim::Terrain::Sand || ter == sim::Terrain::Rock ? 30 : 128;
          break;
        }
        case 1:
          if (!sim::terrain_water(m.terrain[i])) m.height[i] = static_cast<u8>(std::min(7, m.height[i] + 1));
          break;
        case 2:
          if (!sim::terrain_water(m.terrain[i])) m.height[i] = static_cast<u8>(std::max(1, m.height[i] - 1));
          break;
        case 3: m.resource[i] = static_cast<sim::Resource>(ed_.resource); break;
        case 4: m.resource[i] = sim::Resource::None; break;
        case 5: m.road[i] = sim::terrain_water(m.terrain[i]) ? sim::Road::Bridge : sim::Road::Dirt; break;
        case 6: m.road[i] = sim::Road::None; break;
        default: break;
      }
      world_->nav.update_tile(m, x, y, world_->now);
      m.mark_dirty(x, y);
    }
  ++m.revision;
}

void App::editor_event(const SDL_Event& e) {
  auto& cam = renderer_.camera();
  const bool ui_mouse = ImGui::GetIO().WantCaptureMouse;
  switch (e.type) {
    case SDL_EVENT_MOUSE_WHEEL:
      if (!ui_mouse) {
        if (e.wheel.y > 0 && cam.zoom < render::kZoomCount - 1) ++cam.zoom;
        if (e.wheel.y < 0 && cam.zoom > 0) --cam.zoom;
      }
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      if (ui_mouse) break;
      if (e.button.button == SDL_BUTTON_LEFT) {
        ed_.painting = true;
        editor_paint(mouse_tile());
      } else {
        dragging_ = true;
      }
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (e.button.button == SDL_BUTTON_LEFT) ed_.painting = false;
      else dragging_ = false;
      break;
    case SDL_EVENT_MOUSE_MOTION:
      if (ed_.painting && !ui_mouse) editor_paint(mouse_tile());
      if (dragging_) {
        const f32 k = cam.density / cam.scale();
        cam.center = cam.center - Vec2{e.motion.xrel * k, e.motion.yrel * k};
      }
      break;
    default: break;
  }
}

void App::draw_editor() {
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(400, display().y - 20), ImGuiCond_FirstUseEver);
  ImGui::Begin("Scenario editor", nullptr, ImGuiWindowFlags_NoCollapse);
  if (ImGui::CollapsingHeader("Paint", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Combo("Brush", &ed_.brush, kBrushes, 7);
    if (ed_.brush == 0) ImGui::Combo("Terrain", &ed_.terrain, kTerrains, 8);
    if (ed_.brush == 3) ImGui::Combo("Resource", &ed_.resource, kResources, 9);
    ImGui::SliderInt("Brush size", &ed_.size, 0, 8);
    ImGui::TextDisabled("Left-drag paints, right-drag pans, wheel zooms, WASD moves.");
  }
  if (ImGui::CollapsingHeader("New map")) {
    const char* sizes[] = {"Small (128)", "Medium (192)", "Large (256)"};
    const int vals[] = {128, 192, 256};
    int si = 1;
    for (int k = 0; k < 3; ++k)
      if (vals[k] == gen_.size) si = k;
    if (ImGui::Combo("Size", &si, sizes, 3)) gen_.size = vals[si];
    int seed = static_cast<int>(gen_.seed);
    if (ImGui::InputInt("Seed", &seed)) gen_.seed = static_cast<u64>(std::max(0, seed));
    const char* coasts[] = {"No coast", "North", "East", "South", "West"};
    int coast = gen_.coast + 1;
    if (ImGui::Combo("Coast", &coast, coasts, 5)) gen_.coast = coast - 1;
    ImGui::Checkbox("River", &gen_.river);
    ImGui::SliderFloat("Hills", &gen_.hills, 0, 1);
    ImGui::SliderFloat("Forest", &gen_.forest, 0, 1);
    if (ImGui::Button("Generate (discards painting)")) {
      const EditorState keep = ed_;
      editor_new();
      ed_ = keep;
    }
  }
  if (ImGui::CollapsingHeader("Scenario", ImGuiTreeNodeFlags_DefaultOpen)) {
    input_string("Name", ed_.name);
    input_string("Folder id", ed_.id);
    ImGui::SliderInt("Unlock level", &ed_.mission, 1, 16);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Buildings unlocked as in that campaign mission.");
    ImGui::Combo("Era", &ed_.era, kEras, 3);
    ImGui::Combo("Art style", &ed_.region, kRegionNames, 4);
    ImGui::Combo("Road enters from", &ed_.entry, kSides, 4);
    ImGui::InputInt("Treasury", &ed_.treasury, 1000);
    ImGui::InputInt("Start wood", &ed_.wood, 50);
    ImGui::InputInt("Start wheat", &ed_.wheat, 50);
  }
  if (ImGui::CollapsingHeader("Gods and trade")) {
    for (size_t k = 0; k < defs_.gods.size() && k < ed_.gods.size(); ++k) {
      bool on = ed_.gods[k] != 0;
      if (ImGui::Checkbox(defs_.gods[k].name.c_str(), &on)) ed_.gods[k] = on ? 1 : 0;
      if (k % 3 != 2) ImGui::SameLine();
    }
    ImGui::NewLine();
    for (size_t k = 0; k < defs_.partners.size() && k < ed_.partners.size(); ++k) {
      bool on = ed_.partners[k] != 0;
      if (ImGui::Checkbox(defs_.partners[k].name.c_str(), &on)) ed_.partners[k] = on ? 1 : 0;
    }
  }
  if (ImGui::CollapsingHeader("Goals", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::InputInt("Population", &ed_.population, 100);
    ImGui::SliderInt("Culture", &ed_.culture, 0, 100);
    ImGui::SliderInt("Prosperity", &ed_.prosperity, 0, 100);
    ImGui::SliderInt("Security", &ed_.security, 0, 100);
    ImGui::TextDisabled("Events and requests: edit script.lua in the scenario folder.");
  }
  ImGui::Separator();
  if (ImGui::Button("Save", ImVec2(110, 30))) editor_save();
  ImGui::SameLine();
  if (ImGui::Button("Save and play", ImVec2(130, 30)) && editor_save()) {
    try {
      mission_index_ = -1;
      start_scenario(sim::load_scenario(ed_.path + "/scenario.json"));
    } catch (const std::exception& e) {
      ed_.status = std::string("Could not start: ") + e.what();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Menu", ImVec2(110, 30))) {
    menu_background();
    screen_ = Screen::Menu;
  }
  if (!ed_.status.empty()) ImGui::TextWrapped("%s", ed_.status.c_str());
  ImGui::End();
}

}  // namespace urbs::game
