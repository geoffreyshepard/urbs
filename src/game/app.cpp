#include "game/app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "core/log.hpp"
#include "core/profile.hpp"
#include "game/iso_draw.hpp"
#include "game/style.hpp"
#include "save/save.hpp"

namespace urbs::game {

namespace {
constexpr int kSpeeds[] = {0, 1, 2, 4};
constexpr f64 kTickSeconds = 1.0 / static_cast<f64>(kTicksPerSecond);
// The menu city's head start, so the streets are busy on the first frame (about 0.15 s in a release
// build; debug builds, ten times slower per tick, take a shorter one).
#ifdef NDEBUG
constexpr int kMenuWarmTicks = 300;
#else
constexpr int kMenuWarmTicks = 30;
#endif
constexpr f32 kMenuHour = 16.5f;  // the menu city's light: a fixed late afternoon, whatever its clock says
}  // namespace

AppOptions parse_args(int argc, char** argv) {
  AppOptions o;
  for (int i = 1; i < argc; ++i) {
    auto arg = [&](const char* name) { return std::strcmp(argv[i], name) == 0 && i + 1 < argc; };
    if (arg("--screenshot")) o.screenshot = argv[++i];
    else if (arg("--frames")) o.frames = std::atoi(argv[++i]);
    else if (arg("--zoom")) o.zoom = std::atoi(argv[++i]);
    else if (arg("--map-size")) o.map_size = std::atoi(argv[++i]);
    else if (arg("--seed")) o.seed = std::strtoull(argv[++i], nullptr, 10);
    else if (arg("--scenario")) o.scenario = argv[++i];
    else if (arg("--screen")) o.screen = argv[++i];
    else if (arg("--speed")) o.speed = std::atoi(argv[++i]);
    else if (arg("--ticks")) o.ticks_before = std::atoi(argv[++i]);
    else if (arg("--inspect")) o.inspect = std::atoi(argv[++i]);
    else if (arg("--overlay")) o.overlay = std::atoi(argv[++i]);
    else if (arg("--build-group")) o.build_group = std::atoi(argv[++i]);
    else if (arg("--show")) o.show = argv[++i];
    else if (arg("--weather")) o.weather = argv[++i];
    else if (arg("--hour")) o.hour = static_cast<float>(std::atof(argv[++i]));
    else if (arg("--career"))
      o.career = std::atoi(argv[++i]);
    else if (std::strcmp(argv[i], "--no-ui") == 0) o.no_ui = true;
    else if (std::strcmp(argv[i], "--autoplay") == 0) o.autoplay = true;
    else if (std::strcmp(argv[i], "--mute") == 0) o.mute = true;
    else if (arg("--cam")) {
      o.cam_x = static_cast<f32>(std::atof(argv[++i]));
      if (i + 1 < argc) o.cam_y = static_cast<f32>(std::atof(argv[++i]));
    }
  }
  return o;
}

// ------------------------------------------------------------------------------------------------
// World lifecycle
// ------------------------------------------------------------------------------------------------

void App::after_world_change() {
  bot_.reset();
  view_.init(renderer_.atlas(), defs_, world_->region);
  icons_.build_thumbs(win_.gpu, renderer_.atlas(), defs_, world_->region);
  renderer_.set_map(&world_->map);
  sel_ = Selection{};
  tools_.cancel();
  alerts_seen_ = world_->now;
  end_dismissed_ = false;
  last_message_ = 0;
  overlay_ = Overlay::None;
  accum_ = 0;
  last_autosave_day_ = static_cast<u16>(sim::day_index(world_->now));
  salary_day_ = world_->mission_days;
  // Start the camera at the heart of the settlement (or the map centre).
  auto& cam = renderer_.camera();
  const i32 t = world_->buildings.ids.count() > 0 ? world_->city_target_tile()
                                                  : (world_->entry_tile >= 0 ? world_->entry_tile : -1);
  if (t >= 0 && world_->buildings.ids.count() == 0 && world_->entry_tile >= 0) {
    // Fresh mission: centre between the entry and the map centre.
    cam.center = tile_center_iso(world_->map, (t % world_->map.w + world_->map.w / 2) / 2,
                                 (t / world_->map.w + world_->map.h / 2) / 2);
  } else if (t >= 0) {
    cam.center = tile_center_iso(world_->map, t % world_->map.w, t / world_->map.w);
  } else {
    cam.center = tile_center_iso(world_->map, world_->map.w / 2, world_->map.h / 2);
  }
  cam.zoom = 3;
}

void App::start_scenario(sim::ScenarioSpec spec) {
  spec_ = std::move(spec);
  auto w = std::make_unique<sim::World>();
  try {
    sim::setup_scenario(*w, defs_, spec_);
  } catch (const std::exception& e) {
    status_ = std::string("Could not start: ") + e.what();
    status_time_ = 8;
    URBS_LOG_ERROR("{}", status_);
    return;
  }
  w->set_difficulty(static_cast<u8>(settings_.difficulty));
  world_ = std::move(w);
  after_world_change();
  screen_ = Screen::Playing;
  speed_ = 1;
  music_.mission_started(*world_);
}

void App::new_world(const std::string& scenario) {
  if (!scenario.empty()) {
    start_scenario(sim::load_scenario(scenario));
    return;
  }
  menu_background();
}

void App::menu_background() {
  // The title screens play over a finished, living city: a scripted scenario frozen from a city the
  // autoplay bot built (data/menu/backdrop, made by tools/menu_backdrop.sh), ticking quietly behind the
  // menus.  If it is missing or will not load (a mod removed one of its buildings), a plain generated
  // landscape stands in.
  const auto t0 = std::chrono::steady_clock::now();
  menu_live_ = false;
  const std::string path = root_ + "data/menu/backdrop/scenario.json";
  if (std::filesystem::exists(path)) {
    try {
      sim::ScenarioSpec spec = sim::load_scenario(path);
      auto w = std::make_unique<sim::World>();
      sim::setup_scenario(*w, defs_, spec);
      // A few game minutes so the streets are already busy on the first frame.
      for (int t = 0; t < kMenuWarmTicks; ++t) w->tick();
      world_ = std::move(w);
      menu_live_ = true;
    } catch (const std::exception& e) {
      URBS_LOG_WARN("menu backdrop: {}", e.what());
    }
  }
  if (!menu_live_) {
    world_ = std::make_unique<sim::World>();
    sim::MapGenParams p;
    p.size = 128;
    p.seed = 77;
    p.coast = 2;
    p.hills = 0.4f;
    sim::generate_map(world_->map, p);
    world_->init(sim::WorldConfig{}, defs_);
    world_->now = kTicksPerHour * 9;
  }
  spec_ = sim::ScenarioSpec{};
  view_.init(renderer_.atlas(), defs_, world_->region);
  renderer_.set_map(&world_->map);
  // The drift circles the middle of the built-up area (the bounding box of the buildings).
  i32 x0 = world_->map.w, y0 = world_->map.h, x1 = 0, y1 = 0;
  world_->buildings.for_each([&](BuildingId, const sim::Building& b) {
    x0 = std::min<i32>(x0, b.x);
    y0 = std::min<i32>(y0, b.y);
    x1 = std::max<i32>(x1, b.x + b.w);
    y1 = std::max<i32>(y1, b.y + b.h);
  });
  if (x1 <= x0) {
    x0 = y0 = world_->map.w / 4;
    x1 = y1 = world_->map.w * 3 / 4;
  }
  menu_focus_ = tile_center_iso(world_->map, (x0 + x1) / 2, (y0 + y1) / 2);
  menu_radius_ = static_cast<f32>(std::max(x1 - x0, y1 - y0)) * 64.0f * 0.16f;  // a tile is 128 iso px wide
  menu_clock_ = 0;
  renderer_.camera().center = menu_focus_;
  renderer_.camera().zoom = 3;
  URBS_LOG_INFO("menu backdrop: {} ({} buildings, {} people) in {:.0f} ms", menu_live_ ? "city" : "landscape",
                world_->buildings.ids.count(), world_->citizens.ids.count(),
                std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count());
}

void App::defs_for_mission(int index) {
  // Former cities become trade partners selling what they were good at.
  defs_ = base_defs_;
  for (int k = 0; k < index; ++k) {
    const auto& prev = campaign_.missions[static_cast<size_t>(k)];
    if (!profile_.done(prev.id) || prev.sells.empty()) continue;
    data::TradePartnerDef t;
    t.id = "former_" + prev.id;
    t.name = prev.name.substr(0, prev.name.find(" - "));
    t.sea = false;
    t.open_cost = 100;
    t.interval_days = 5;
    for (const auto& g : prev.sells) {
      const GoodId gid = defs_.good(g);
      if (gid.valid()) t.sells.push_back({gid, 400});
    }
    for (const char* g : {"pottery", "wine", "olive_oil", "clothing"}) {
      const GoodId gid = defs_.good(g);
      if (gid.valid()) t.buys.push_back({gid, 200});
    }
    defs_.partners.push_back(std::move(t));
  }
}

void App::start_mission(int index) {
  if (index < 0 || index >= static_cast<int>(campaign_.missions.size())) return;
  const auto& mi = campaign_.missions[static_cast<size_t>(index)];
  const std::string path = root_ + "data/scenarios/" + mi.id + "/scenario.json";
  if (!std::filesystem::exists(path)) {
    status_ = "This mission is not available yet.";
    status_time_ = 5;
    return;
  }
  defs_for_mission(index);
  view_.init(renderer_.atlas(), defs_);
  mission_index_ = index;
  start_scenario(sim::load_scenario(path));
  if (world_ && screen_ == Screen::Playing) {
    for (size_t k = base_defs_.partners.size(); k < defs_.partners.size(); ++k) world_->routes[k].available = 1;
  }
}

void App::start_sandbox() {
  defs_ = base_defs_;
  view_.init(renderer_.atlas(), defs_);
  mission_index_ = -1;
  std::string j = R"({"name":"Sandbox","id":"sandbox","mission":16,"difficulty":0,"start_hour":7,)";
  j += R"("map":{"size":)" + std::to_string(gen_.size) + R"(,"seed":)" + std::to_string(gen_.seed) +
       R"(,"coast":)" + std::to_string(gen_.coast) + R"(,"river":)" + (gen_.river ? "true" : "false") +
       R"(,"hills":)" + std::to_string(gen_.hills) + R"(,"forest":)" + std::to_string(gen_.forest) +
       R"(,"arid":)" + std::to_string(gen_.arid) + R"(},)";
  {
    const char* styles[] = {"", "north", "east", "egypt"};
    j += R"("region":")" + std::string(styles[std::clamp(sandbox_region_, 0, 3)]) + R"(",)";
    if (sandbox_nile_) j += R"("nile":true,)";
  }
  j += R"("world":{"max_citizens":16000,"max_buildings":4096,"max_households":5000},)";
  // A cart of timber and grain at the end of the road, as in the first campaign mission: without any
  // store the first market and granary wait on wood that nobody has hauled anywhere yet.
  j += R"("generator":"scripted","entry_road":3,"start":{"treasury":15000},"start_stock":{"wood":80,"wheat":60},)";
  j += R"("gods":["jupiter","juno","mars","venus","ceres","neptune","mercury","minerva","vulcan","vesta"],)";
  j += R"("trade":{"partners":["latin_villages","rome","etruria","carthage","massalia","gallia","greece"]}})";
  start_scenario(sim::parse_scenario(j, root_ + "data/scenarios/sandbox.json"));
}

std::string App::save_path(const std::string& name) const { return user_dir_ + "saves/" + name + ".urbs"; }

void App::save_slot(const std::string& name) {
  std::filesystem::create_directories(user_dir_ + "saves");
  std::string err;
  const std::string label = world_->mission_id.empty() ? std::string("sandbox") : world_->mission_id;
  if (save::save_world(*world_, save_path(name), label, &err)) {
    status_ = "Saved (" + name + ").";
  } else {
    status_ = "Save failed: " + err;
  }
  status_time_ = 4;
}

bool App::load_slot(const std::string& name) {
  auto w = std::make_unique<sim::World>();
  std::string err;
  save::SaveInfo info;
  defs_ = base_defs_;
  if (save::read_info(save_path(name), info)) defs_for_mission(campaign_.index_of(info.mission));
  if (!save::load_world(*w, defs_, save_path(name), &err)) {
    status_ = "Load failed: " + err;
    status_time_ = 6;
    return false;
  }
  view_.init(renderer_.atlas(), defs_);
  world_ = std::move(w);
  mission_index_ = campaign_.index_of(world_->mission_id);
  spec_ = sim::ScenarioSpec{};
  after_world_change();
  screen_ = Screen::Playing;
  speed_ = 0;
  music_.mission_started(*world_);
  status_ = "Loaded (" + name + ").";
  status_time_ = 4;
  return true;
}

bool App::init(const AppOptions& opt) {
  opt_ = opt;
  user_dir_ = platform::user_data_dir();
  settings_.load(user_dir_);
  if (!platform::create_window(win_, "Urbs", 1600, 900)) return false;
  if (settings_.fullscreen) SDL_SetWindowFullscreen(win_.window, true);
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  apply_roman_style(settings_.ui_scale);
  ImGui_ImplSDL3_InitForSDLGPU(win_.window);
  ImGui_ImplSDLGPU3_InitInfo ii{};
  ii.Device = win_.gpu;
  ii.ColorTargetFormat = win_.swapchain_format;
  ii.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  ImGui_ImplSDLGPU3_Init(&ii);

  root_ = platform::data_root();
  load_ui_fonts(root_);
  if (skin_.init(win_.gpu, root_)) {
    set_ui_skin(&skin_);
    apply_roman_style(settings_.ui_scale);
  }
  if (!renderer_.init(win_, root_)) return false;
  if (opt.screenshot.empty() && !opt.mute && audio_.init()) audio_.set_volume(settings_.music_volume);
  music_.init(root_, &audio_);
  set_overlay_colorblind(settings_.colorblind);
  mods_ = find_mods(root_, user_dir_);
  {
    std::string err;
    if (!reload_defs(&err)) {
      // A broken mod must not lock the player out: start without mods and say why.
      URBS_LOG_ERROR("defs with mods: {}", err);
      settings_.mods.clear();
      if (!reload_defs(&err)) {
        URBS_LOG_ERROR("defs: {}", err);
        return false;
      }
      status_ = "Mods disabled: " + err;
      status_time_ = 12;
    }
  }
  campaign_.load(root_);
  profile_.load(user_dir_);
  if (opt.career >= 0) {  // screenshots of a career in progress; run() does not save it
    profile_ = CampaignProfile{};
    for (int k = 0; k < std::min<int>(opt.career, static_cast<int>(campaign_.missions.size())); ++k) {
      profile_.completed.push_back(campaign_.missions[static_cast<size_t>(k)].id);
      profile_.savings += 1200;
    }
  }
  selected_mission_ = profile_.next_index(campaign_);
  view_.init(renderer_.atlas(), defs_);
  icons_.build_thumbs(win_.gpu, renderer_.atlas(), defs_, std::string());
  icons_.init(win_.gpu, renderer_.atlas(), defs_);
  gen_.size = 192;
  gen_.seed = opt.seed;
  gen_.coast = 2;

  if (!opt.scenario.empty()) {
    new_world(opt.scenario);
    if (!world_) return false;
    if (opt.autoplay) {
      bot_ = std::make_unique<sim::Autoplay>();
      bot_->init(*world_);
    }
    for (int t = 0; t < opt.ticks_before; ++t) {
      sim::push_due_orders(*world_, spec_);
      if (bot_) bot_->step(*world_);
      world_->tick();
    }
  } else {
    menu_background();
    screen_ = opt.screen == "campaign" ? Screen::Campaign
              : opt.screen == "mods"   ? Screen::Mods
              : opt.screen == "custom" ? Screen::Custom
                                       : Screen::Menu;
    if (opt.screen == "editor") editor_new();
  }
  auto& cam = renderer_.camera();
  if (opt.cam_x >= 0) cam.center = render::iso::from_tile(opt.cam_x, opt.cam_y);
  if (opt.zoom >= 0) cam.zoom = std::min(opt.zoom, render::kZoomCount - 1);
  speed_ = std::clamp(opt.speed, 0, 3);
  overlay_ = static_cast<Overlay>(std::clamp(opt.overlay, 0, static_cast<int>(Overlay::Count) - 1));
  overlay_age_ = 1 << 20;
  build_group_ = opt.build_group;
  // Screenshots: open panels and dialogs by name (--show finance,nomsg,noend,bld:temple).
  const char* kPanelNames[] = {"rome",     "advisors", "finance",  "goods",  "trade", "labor",
                               "religion", "military", "research", "alerts", "help"};
  const std::pair<const char*, bool*> kShow[] = {
      {"pause", &paused_menu_}, {"settings", &show_settings_}, {"load", &show_saveload_}};
  for (size_t a = 0; a < opt.show.size();) {
    size_t b = opt.show.find(',', a);
    if (b == std::string::npos) b = opt.show.size();
    const std::string p = opt.show.substr(a, b - a);
    a = b + 1;
    for (const auto& [name, flag] : kShow)
      if (p == name) *flag = true;
    for (int k = 0; k < kPanelCount; ++k)
      if (p == kPanelNames[k]) panel_ = static_cast<Panel>(k);
    if (p == "save") show_saveload_ = saveload_saving_ = true;
    if (p == "nobuild") show_build_ = false;
    if (p == "noend") end_dismissed_ = true;  // hide the won/lost dialog
    if (p == "nomsg") last_message_ = 0xffff;  // the mission briefing and other messages stay unread and hidden
    if (p.rfind("advisor", 0) == 0 && p.size() == 8 && p[7] >= '1' && p[7] <= '5') {  // advisor1..advisor5
      panel_ = Panel::Advisors;
      advisor_ = std::atoi(p.c_str() + 7);
    }
    if (p.rfind("bld:", 0) == 0)  // select the first building of this def
      world_->buildings.for_each([&](BuildingId id, const sim::Building& bd) {
        if (!sel_.building.valid() && world_->defs->b(bd.def).id == p.substr(4)) sel_.building = id;
      });
  }
  build_fly_y_ = 100 + 50.0f * static_cast<float>(std::max(0, opt.build_group));
  if (opt.inspect >= 0 && world_->citizens.ids.alive_index(static_cast<u32>(opt.inspect))) {
    sel_.citizen = world_->citizens.ids.id_at(static_cast<u32>(opt.inspect));
    sel_.follow = true;
    sel_.building = world_->citizens.home[static_cast<u32>(opt.inspect)];
  }
  last_counter_ = SDL_GetPerformanceCounter();
  return true;
}

// ------------------------------------------------------------------------------------------------
// Input
// ------------------------------------------------------------------------------------------------

void App::handle_event(const SDL_Event& e) {
  ImGui_ImplSDL3_ProcessEvent(&e);
  if (e.type == SDL_EVENT_QUIT) {
    running_ = false;
    return;
  }
  if (screen_ == Screen::Editor) {
    editor_event(e);
    return;
  }
  if (screen_ != Screen::Playing) return;
  auto& cam = renderer_.camera();
  const bool ui_mouse = ImGui::GetIO().WantCaptureMouse;
  switch (e.type) {
    case SDL_EVENT_MOUSE_WHEEL:
      if (!ui_mouse) {
        float mx, my;
        SDL_GetMouseState(&mx, &my);
        Vec2 before = cam.screen_to_world({mx, my});
        if (e.wheel.y > 0 && cam.zoom < render::kZoomCount - 1) ++cam.zoom;
        if (e.wheel.y < 0 && cam.zoom > 0) --cam.zoom;
        Vec2 after = cam.screen_to_world({mx, my});
        cam.center = cam.center + (before - after);
      }
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      if (paused_menu_) break;
      if (!ui_mouse && (e.button.button == SDL_BUTTON_MIDDLE || e.button.button == SDL_BUTTON_RIGHT)) {
        dragging_ = true;
        right_drag_ = 0;
      }
      if (!ui_mouse && e.button.button == SDL_BUTTON_LEFT) {
        build_group_ = -1;  // the build list gets out of the way once the player works on the map
        if (!tools_.mouse_down(mouse_tile())) pick_at_mouse();
      }
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (e.button.button == SDL_BUTTON_MIDDLE || e.button.button == SDL_BUTTON_RIGHT) {
        // A right click (not a drag): order the selected unit there, or cancel the current tool.
        if (e.button.button == SDL_BUTTON_RIGHT && dragging_ && right_drag_ < 5 && !ui_mouse) {
          if (sel_.unit < military::kMaxUnits && world_->army.units[sel_.unit].alive && world_->army.units[sel_.unit].side == 0) {
            const Vec2i t = mouse_tile();
            sim::Command c;
            c.type = sim::CmdType::UnitOrder;
            c.def = sel_.unit;
            c.value = static_cast<i32>(military::Order::Move);
            c.x0 = t.x;
            c.y0 = t.y;
            world_->push(c);
          } else {
            tools_.cancel();
            build_group_ = -1;
          }
        }
        dragging_ = false;
      }
      if (e.button.button == SDL_BUTTON_LEFT) tools_.mouse_up(mouse_tile(), *world_);
      break;
    case SDL_EVENT_MOUSE_MOTION:
      tools_.mouse_move(mouse_tile());
      if (dragging_) {
        right_drag_ += std::fabs(e.motion.xrel) + std::fabs(e.motion.yrel);
        f32 k = cam.density / cam.scale();
        cam.center = cam.center - Vec2{e.motion.xrel * k, e.motion.yrel * k};
      }
      break;
    case SDL_EVENT_KEY_DOWN:
      if (ImGui::GetIO().WantCaptureKeyboard) break;
      if (e.key.key == SDLK_ESCAPE) {
        if (tools_.kind != ToolKind::Inspect || build_group_ >= 0) {
          tools_.cancel();
          build_group_ = -1;
        } else if (sel_.building.valid() || sel_.citizen.valid() || sel_.unit != 0xFFFF)
          sel_ = Selection{};
        else paused_menu_ = !paused_menu_;
      }
      if (paused_menu_) break;
      if (e.key.key == SDLK_R) tools_.select_road(sim::Road::Dirt);
      // Rotate the building in hand (R is the road tool): E clockwise, Q counter-clockwise.
      if (e.key.key == SDLK_E) tools_.rotate(*world_, (e.key.mod & SDL_KMOD_SHIFT) ? -1 : 1);
      if (e.key.key == SDLK_Q) tools_.rotate(*world_, -1);
      if (e.key.key == SDLK_X) tools_.select_demolish();
      if (e.key.key == SDLK_B) show_build_ = !show_build_;
      if (e.key.key == SDLK_L) toggle_panel(Panel::Goods);
      if (e.key.key == SDLK_J) toggle_panel(Panel::Labor);
      if (e.key.key == SDLK_F) sel_.follow = !sel_.follow;
      if (e.key.key == SDLK_H || e.key.key == SDLK_F1) toggle_panel(Panel::Help);
      if (e.key.key == SDLK_F12) show_debug_ = !show_debug_;
      if (e.key.key == SDLK_F5) save_slot("quicksave");
      if (e.key.key == SDLK_F9) load_slot("quicksave");
      if (e.key.key == SDLK_O) {
        overlay_ = static_cast<Overlay>((static_cast<int>(overlay_) + 1) % static_cast<int>(Overlay::Count));
        overlay_age_ = 1 << 20;
      }
      if (e.key.key == SDLK_SPACE) speed_ = speed_ == 0 ? 1 : 0;
      if (e.key.key == SDLK_1) speed_ = 1;
      if (e.key.key == SDLK_2) speed_ = 2;
      if (e.key.key == SDLK_3) speed_ = 3;
      break;
    default: break;
  }
}

void App::update_camera(f32 dt) {
  auto& cam = renderer_.camera();
  if (screen_ != Screen::Playing && screen_ != Screen::Editor) {
    // A slow drift behind the menus: a wide ellipse around the middle of the city, one lap in about
    // seven minutes, with the city pushed right of centre so the menu column on the left stays calm.
    menu_clock_ += dt;
    const f32 a = menu_clock_ * 0.015f + 0.8f;
    const f32 shift = cam.screen.x * 0.14f / cam.scale();
    cam.center =
        Vec2{menu_focus_.x - shift + std::cos(a) * menu_radius_, menu_focus_.y + std::sin(a) * menu_radius_ * 0.5f};
    return;
  }
  if (ImGui::GetIO().WantCaptureKeyboard) return;
  const bool* keys = SDL_GetKeyboardState(nullptr);
  f32 speed = 900.0f * dt / render::kZoomLevels[cam.zoom];
  if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) cam.center.y -= speed;
  if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) cam.center.y += speed;
  if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) cam.center.x -= speed;
  if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) cam.center.x += speed;
  if (settings_.edge_scroll && SDL_GetWindowFlags(win_.window) & SDL_WINDOW_INPUT_FOCUS) {
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    int ww, wh;
    SDL_GetWindowSize(win_.window, &ww, &wh);
    if (mx < 6) cam.center.x -= speed;
    if (my < 6) cam.center.y -= speed;
    if (mx > static_cast<f32>(ww - 6)) cam.center.x += speed;
    if (my > static_cast<f32>(wh - 6)) cam.center.y += speed;
  }
  cam.center = clamp_to_map(world_->map, cam.center);
}

void App::step_sim(f64 dt) {
  if (menu_live_ && screen_ != Screen::Playing && screen_ != Screen::Editor) {
    // The menu city lives at normal speed (no bot, no saves, no salary).
    accum_ += std::min(dt, 0.1);
    for (int n = 0; accum_ >= kTickSeconds && n < 4; ++n) {
      world_->tick();
      accum_ -= kTickSeconds;
    }
    accum_ = std::min(accum_, kTickSeconds);
    alpha_ = static_cast<f32>(accum_ / kTickSeconds);
    return;
  }
  if (screen_ != Screen::Playing || paused_menu_) {
    alpha_ = 0;
    return;
  }
  const int mult = kSpeeds[speed_];
  if (mult == 0) {
    alpha_ = 0;
    return;
  }
  accum_ += std::min(dt, 0.1) * mult;
  int n = 0;
  const auto t0 = std::chrono::steady_clock::now();
  while (accum_ >= kTickSeconds && n < 8) {
    sim::push_due_orders(*world_, spec_);
    if (bot_) bot_->step(*world_);
    world_->tick();
    accum_ -= kTickSeconds;
    ++n;
  }
  if (n == 8) accum_ = 0;  // behind: slow down instead of spiralling
  if (n > 0) sim_ms_ = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
  alpha_ = static_cast<f32>(accum_ / kTickSeconds);
  // Career salary accrues to personal savings; autosave every four days.
  if (mission_index_ >= 0 && world_->mission_days > salary_day_) {
    profile_.savings += campaign_.missions[static_cast<size_t>(mission_index_)].salary * (world_->mission_days - salary_day_);
    salary_day_ = world_->mission_days;
  }
  const u16 day = static_cast<u16>(sim::day_index(world_->now));
  // Bot demos and screenshot runs never touch the player's saves.
  if (settings_.autosave && !bot_ && opt_.screenshot.empty() && day >= last_autosave_day_ + 4) {
    last_autosave_day_ = day;
    save_slot("autosave");
    profile_.save(user_dir_);
  }
}

Vec2i App::mouse_tile() const {
  float mx, my;
  SDL_GetMouseState(&mx, &my);
  const Vec2 ip = const_cast<render::Renderer&>(renderer_).camera().screen_to_world({mx, my});
  i32 tx = -1, ty = -1;
  if (render::iso::pick_tile(world_->map, ip, tx, ty)) return {tx, ty};
  // Off the map: the tile on the ground plane (callers bounds-check).
  const Vec2 t = render::iso::to_tile(ip);
  return {static_cast<i32>(std::floor(t.x)), static_cast<i32>(std::floor(t.y))};
}

void App::pick_at_mouse() {
  float mx, my;
  SDL_GetMouseState(&mx, &my);
  const Vec2 wp = renderer_.camera().screen_to_world({mx, my});  // iso px
  namespace iso = render::iso;
  // Soldiers first (to select units), then walking citizens, then buildings.  Distances are to the
  // middle of the figure, in iso px.
  constexpr f32 kPickR = 28.0f, kBody = 18.0f;
  const auto& A = world_->army;
  f32 best = kPickR * kPickR;
  for (u32 i = 0; i < A.ids.high_water(); ++i) {
    if (!A.ids.alive_index(i)) continue;
    const Vec2 f =
        iso::from_tile(A.x[i], A.y[i], iso::level_at(world_->map, static_cast<i32>(A.x[i]), static_cast<i32>(A.y[i])));
    const f32 dx = f.x - wp.x, dy = f.y - kBody - wp.y;
    if (dx * dx + dy * dy < best) {
      best = dx * dx + dy * dy;
      sel_.unit = A.unit[i];
    }
  }
  if (best < kPickR * kPickR) {
    panel_ = Panel::Military;
    return;
  }
  const auto& C = world_->citizens;
  CitizenId pick;
  for (u32 i = 0; i < C.ids.high_water(); ++i) {
    if (!C.ids.alive_index(i) || C.activity[i] != sim::Activity::Walk) continue;
    Vec2 p;
    if (!world_->citizen_position(i, alpha_, p, nullptr)) continue;
    const Vec2 f = iso::project(p, iso::level_at_world(world_->map, p.x, p.y));
    const f32 dx = f.x - wp.x, dy = f.y - kBody - wp.y;
    if (dx * dx + dy * dy < best) {
      best = dx * dx + dy * dy;
      pick = C.ids.id_at(i);
    }
  }
  if (pick.valid()) {
    sel_.citizen = pick;
    return;
  }
  const Vec2i t = mouse_tile();
  if (world_->map.in_bounds(t.x, t.y)) {
    BuildingId b = world_->map.building[static_cast<size_t>(world_->map.idx(t.x, t.y))];
    sel_.building = b;
    sel_.unit = 0xFFFF;
    if (!b.valid()) sel_.citizen = CitizenId{};
  }
}

void App::run() {
  while (running_) {
    URBS_FRAME_MARK;
    u64 now = SDL_GetPerformanceCounter();
    f64 dt = static_cast<f64>(now - last_counter_) / static_cast<f64>(SDL_GetPerformanceFrequency());
    last_counter_ = now;
    frame_ms_ = frame_ms_ * 0.95f + static_cast<f32>(dt) * 1000.0f * 0.05f;
    SDL_Event e;
    while (SDL_PollEvent(&e)) handle_event(e);
    update_camera(static_cast<f32>(dt));
    step_sim(dt);
    {
      const MusicContext ctx = screen_ == Screen::Playing    ? MusicContext::Playing
                               : screen_ == Screen::Menu     ? MusicContext::Menu
                               : screen_ == Screen::Campaign ? MusicContext::Map
                                                             : MusicContext::Setup;
      const bool end_screen = world_->mission != sim::MissionState::Running && !end_dismissed_;
      music_.update(static_cast<f32>(dt), ctx, screen_ == Screen::Playing ? world_.get() : nullptr, end_screen);
      audio_.pump();
    }
    renderer_.time += static_cast<f32>(dt);
    if (status_time_ > 0) status_time_ -= static_cast<f32>(dt);

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    draw_ui();
    ImGui::Render();
    skin_.process(ImGui::GetDrawData());

    if (screen_ == Screen::Playing) {
      if (sel_.follow && sel_.citizen.valid() && world_->citizens.alive(sel_.citizen)) {
        Vec2 p;
        const u32 ci = sel_.citizen.index();
        if (world_->citizen_position(ci, alpha_, p, nullptr)) {
          renderer_.camera().center = render::iso::project(p, render::iso::level_at_world(world_->map, p.x, p.y));
        } else if (world_->citizens.loc[ci].valid() && world_->buildings.alive(world_->citizens.loc[ci])) {
          const auto& lb = world_->buildings[world_->citizens.loc[ci]];
          renderer_.camera().center = render::iso::from_tile(static_cast<f32>(lb.x) + static_cast<f32>(lb.w) / 2,
                                                             static_cast<f32>(lb.y) + static_cast<f32>(lb.h) / 2,
                                                             render::iso::level_at(world_->map, lb.x, lb.y));
        }
      }
      draw_selection(*world_, sel_, renderer_, alpha_);
      if (overlay_ == Overlay::None) {
        renderer_.set_overlay(nullptr);
      } else if (++overlay_age_ > 30) {
        build_overlay(*world_, overlay_, overlay_service_, overlay_px_);
        renderer_.set_overlay(overlay_px_.data());
        overlay_age_ = 0;
      }
    } else {
      renderer_.set_overlay(nullptr);
    }
    sim::Weather weather = world_->weather;
    for (int k = 0; k < static_cast<int>(sim::Weather::Count); ++k)
      if (opt_.weather == sim::weather_name(static_cast<sim::Weather>(k))) weather = static_cast<sim::Weather>(k);
    const bool menu_city = menu_live_ && screen_ != Screen::Playing && screen_ != Screen::Editor;
    if (menu_city && opt_.weather.empty()) weather = sim::Weather::Clear;
    lighting_.update(*world_, weather, alpha_, static_cast<f32>(dt), menu_city ? kMenuHour : opt_.hour);
    renderer_.grade = lighting_.grade;
    renderer_.shadows = lighting_.shadows;
    for (int k = 0; k < 4; ++k) renderer_.ground_season[k] = lighting_.ground[k];
    view_.draw(*world_, renderer_, alpha_);
    if (weather == sim::Weather::Rain || weather == sim::Weather::Storm || weather == sim::Weather::Snow) {
      // Rain streaks / snowflakes over the visible area.
      const Recti vis = renderer_.camera().visible_world(0);
      const bool snow = weather == sim::Weather::Snow;
      const int n = weather == sim::Weather::Storm ? 500 : 280;
      for (int k = 0; k < n; ++k) {
        const u32 hsh = static_cast<u32>(k) * 2654435761u;
        const f32 fx = static_cast<f32>(hsh % 10007) / 10007.0f, fy0 = static_cast<f32>((hsh >> 12) % 9973) / 9973.0f;
        const f32 speed = snow ? 0.08f : 0.9f;
        const f32 fy = std::fmod(fy0 + renderer_.time * speed, 1.0f);
        const f32 x = static_cast<f32>(vis.x) + fx * static_cast<f32>(vis.w) +
                      (snow ? std::sin(renderer_.time + static_cast<f32>(k)) * 4.0f * kIsoK : 0.0f);
        const f32 y = static_cast<f32>(vis.y) + fy * static_cast<f32>(vis.h);
        renderer_.push_rect(x, y, kIsoK, (snow ? 1.0f : 5.0f) * kIsoK, 0.04f,
                            snow ? render::rgba(250, 250, 255, 220) : render::rgba(180, 200, 230, 120));
      }
    }
    if (screen_ == Screen::Playing) tools_.draw(*world_, view_, renderer_);
    ++frame_;
    if (!opt_.screenshot.empty() && frame_ == opt_.frames) renderer_.request_screenshot(opt_.screenshot, !opt_.no_ui);
    renderer_.render(ImGui::GetDrawData());
    if (!opt_.screenshot.empty() && frame_ >= opt_.frames) running_ = false;
  }
  // Screenshot runs leave the player's career and settings alone.
  if (opt_.screenshot.empty()) {
    profile_.save(user_dir_);
    settings_.save(user_dir_);
  }
}

void App::shutdown() {
  audio_.shutdown();
  icons_.shutdown(win_.gpu);
  set_ui_skin(nullptr);
  skin_.shutdown(win_.gpu);
  province_map_.release(win_.gpu);
  renderer_.shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();
  platform::destroy_window(win_);
}

}  // namespace urbs::game
