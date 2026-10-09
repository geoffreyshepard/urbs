#include "game/app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <chrono>
#include <cmath>
#include <string>
#include <cstdlib>
#include <cstring>

#include "core/log.hpp"
#include "core/profile.hpp"
#include "sim/scenario.hpp"

namespace urbs::game {

namespace {
constexpr int kSpeeds[] = {0, 1, 2, 4};
constexpr f64 kTickSeconds = 1.0 / static_cast<f64>(kTicksPerSecond);
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
    else if (arg("--speed")) o.speed = std::atoi(argv[++i]);
    else if (arg("--ticks")) o.ticks_before = std::atoi(argv[++i]);
    else if (arg("--inspect")) o.inspect = std::atoi(argv[++i]);
    else if (std::strcmp(argv[i], "--no-ui") == 0) o.no_ui = true;
    else if (arg("--overlay")) o.overlay = std::atoi(argv[++i]);
    else if (arg("--cam")) {
      o.cam_x = static_cast<f32>(std::atof(argv[++i]));
      if (i + 1 < argc) o.cam_y = static_cast<f32>(std::atof(argv[++i]));
    }
  }
  return o;
}

void App::new_world(const std::string& scenario) {
  world_ = std::make_unique<sim::World>();
  if (!scenario.empty()) {
    spec_ = sim::load_scenario(scenario);
    sim::setup_scenario(*world_, defs_, spec_);
  } else {
    sim::generate_map(world_->map, gen_);
    world_->init(sim::WorldConfig{}, defs_);
  }
  renderer_.set_map(&world_->map);
  auto& cam = renderer_.camera();
  cam.center = {static_cast<f32>(world_->map.w * sim::kTilePx) / 2, static_cast<f32>(world_->map.h * sim::kTilePx) / 2};
}

bool App::init(const AppOptions& opt) {
  opt_ = opt;
  if (!platform::create_window(win_, "Urbs", 1600, 900)) return false;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForSDLGPU(win_.window);
  ImGui_ImplSDLGPU3_InitInfo ii{};
  ii.Device = win_.gpu;
  ii.ColorTargetFormat = win_.swapchain_format;
  ii.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  ImGui_ImplSDLGPU3_Init(&ii);

  const std::string root = platform::data_root();
  if (!renderer_.init(win_, root)) return false;
  try {
    defs_ = data::load_defs(root + "data");
  } catch (const std::exception& e) {
    URBS_LOG_ERROR("defs: {}", e.what());
    return false;
  }
  view_.init(renderer_.atlas(), defs_);
  gen_.size = opt.map_size;
  gen_.seed = opt.seed;
  gen_.coast = 2;
  new_world(opt.scenario);
  for (int t = 0; t < opt.ticks_before; ++t) {
    sim::push_due_orders(*world_, spec_);
    world_->tick();
  }
  auto& cam = renderer_.camera();
  if (opt.cam_x >= 0) cam.center = {opt.cam_x * sim::kTilePx, opt.cam_y * sim::kTilePx};
  if (opt.zoom >= 0) cam.zoom = std::min(opt.zoom, render::kZoomCount - 1);
  speed_ = std::clamp(opt.speed, 0, 3);
  overlay_ = static_cast<Overlay>(std::clamp(opt.overlay, 0, static_cast<int>(Overlay::Count) - 1));
  overlay_age_ = 1 << 20;
  if (opt.inspect >= 0 && world_->citizens.ids.alive_index(static_cast<u32>(opt.inspect))) {
    sel_.citizen = world_->citizens.ids.id_at(static_cast<u32>(opt.inspect));
    sel_.follow = true;
    sel_.building = world_->citizens.home[static_cast<u32>(opt.inspect)];
  }
  last_counter_ = SDL_GetPerformanceCounter();
  return true;
}

void App::handle_event(const SDL_Event& e) {
  ImGui_ImplSDL3_ProcessEvent(&e);
  auto& cam = renderer_.camera();
  const bool ui_mouse = ImGui::GetIO().WantCaptureMouse;
  switch (e.type) {
    case SDL_EVENT_QUIT: running_ = false; break;
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
      if (!ui_mouse && (e.button.button == SDL_BUTTON_MIDDLE || e.button.button == SDL_BUTTON_RIGHT)) dragging_ = true;
      if (!ui_mouse && e.button.button == SDL_BUTTON_LEFT && !tools_.mouse_down(mouse_tile())) pick_at_mouse();
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (e.button.button == SDL_BUTTON_MIDDLE || e.button.button == SDL_BUTTON_RIGHT) dragging_ = false;
      if (e.button.button == SDL_BUTTON_LEFT) tools_.mouse_up(mouse_tile(), *world_);
      break;
    case SDL_EVENT_MOUSE_MOTION:
      tools_.mouse_move(mouse_tile());
      if (dragging_) {
        f32 k = cam.density / cam.scale();
        cam.center = cam.center - Vec2{e.motion.xrel * k, e.motion.yrel * k};
      }
      break;
    case SDL_EVENT_KEY_DOWN:
      if (ImGui::GetIO().WantCaptureKeyboard) break;
      if (e.key.key == SDLK_ESCAPE) {
        if (tools_.kind != ToolKind::Inspect) tools_.cancel();
        else running_ = false;
      }
      if (e.key.key == SDLK_R) tools_.select_road(sim::Road::Dirt);
      if (e.key.key == SDLK_X) tools_.select_demolish();
      if (e.key.key == SDLK_B) show_build_ = !show_build_;
      if (e.key.key == SDLK_L) show_stock_ = !show_stock_;
      if (e.key.key == SDLK_J) show_labor_ = !show_labor_;
      if (e.key.key == SDLK_F) sel_.follow = !sel_.follow;
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
  if (ImGui::GetIO().WantCaptureKeyboard) return;
  auto& cam = renderer_.camera();
  const bool* keys = SDL_GetKeyboardState(nullptr);
  f32 speed = 900.0f * dt / render::kZoomLevels[cam.zoom];
  if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) cam.center.y -= speed;
  if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) cam.center.y += speed;
  if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) cam.center.x -= speed;
  if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) cam.center.x += speed;
  cam.center.x = clamp(cam.center.x, 0.0f, static_cast<f32>(world_->map.w * sim::kTilePx));
  cam.center.y = clamp(cam.center.y, 0.0f, static_cast<f32>(world_->map.h * sim::kTilePx));
}

void App::step_sim(f64 dt) {
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
    world_->tick();
    accum_ -= kTickSeconds;
    ++n;
  }
  if (n == 8) accum_ = 0;  // behind: slow down instead of spiralling
  if (n > 0) sim_ms_ = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
  alpha_ = static_cast<f32>(accum_ / kTickSeconds);
}

Vec2i App::mouse_tile() const {
  float mx, my;
  SDL_GetMouseState(&mx, &my);
  Vec2 wp = const_cast<render::Renderer&>(renderer_).camera().screen_to_world({mx, my});
  return {static_cast<i32>(std::floor(wp.x / static_cast<f32>(sim::kTilePx))),
          static_cast<i32>(std::floor(wp.y / static_cast<f32>(sim::kTilePx)))};
}

void App::pick_at_mouse() {
  float mx, my;
  SDL_GetMouseState(&mx, &my);
  const Vec2 wp = renderer_.camera().screen_to_world({mx, my});
  const auto& C = world_->citizens;
  f32 best = 7.0f * 7.0f;
  CitizenId pick;
  for (u32 i = 0; i < C.ids.high_water(); ++i) {
    if (!C.ids.alive_index(i) || C.activity[i] != sim::Activity::Walk) continue;
    Vec2 p;
    if (!world_->citizen_position(i, alpha_, p, nullptr)) continue;
    const f32 dx = p.x - wp.x, dy = (p.y - 4) - wp.y;
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
    if (!b.valid()) sel_.citizen = CitizenId{};
  }
}

void App::draw_build_menu() {
  if (!show_build_) return;
  ImGui::SetNextWindowPos({10, 220}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({260, 480}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Build (B)", &show_build_);
  ImGui::Text("Treasury: %lld denarii", static_cast<long long>(world_->econ.treasury));
  if (ImGui::Button("Dirt road (R)")) tools_.select_road(sim::Road::Dirt);
  ImGui::SameLine();
  if (ImGui::Button("Paved road")) tools_.select_road(sim::Road::Paved);
  if (ImGui::Button("Demolish (X)")) tools_.select_demolish();
  ImGui::SameLine();
  if (ImGui::Button("Cancel (Esc)")) tools_.cancel();
  static const char* kCats[] = {"housing", "farm", "food",      "industry", "storage", "water",    "health",
                                "religion", "entertainment", "education", "safety",   "civic",   "trade",
                                "infrastructure", "decor", "military", "monument"};
  for (const char* cat : kCats) {
    if (!ImGui::CollapsingHeader(cat)) continue;
    for (u16 i = 0; i < defs_.buildings.size(); ++i) {
      const auto& d = defs_.buildings[i];
      if (d.category != cat) continue;
      const bool sel = tools_.kind == ToolKind::Build && tools_.def.v == i;
      std::string label = d.name + "  (" + std::to_string(d.cost_denarii) + ")";
      if (ImGui::Selectable(label.c_str(), sel)) tools_.select_building(BuildingDefId{i});
      if (ImGui::IsItemHovered()) {
        std::string tip = std::to_string(d.w) + "x" + std::to_string(d.h) + (d.painted ? " (drag to paint)" : "");
        for (const auto& c : d.cost_goods) tip += "\n" + std::to_string(c.amount) + " " + defs_.goods[c.good.v].name;
        if (d.workers) tip += "\nworkers: " + std::to_string(d.workers);
        ImGui::SetTooltip("%s", tip.c_str());
      }
    }
  }
  ImGui::End();
}

void App::draw_stock_window() {
  if (!show_stock_) return;
  ImGui::SetNextWindowPos({1300, 10}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Goods (L)", &show_stock_);
  const auto& e = world_->econ;
  if (ImGui::BeginTable("goods", 4)) {
    ImGui::TableSetupColumn("Good");
    ImGui::TableSetupColumn("Stock");
    ImGui::TableSetupColumn("Made");
    ImGui::TableSetupColumn("Used");
    ImGui::TableHeadersRow();
    for (u32 g = 0; g < e.goods; ++g) {
      if (!e.city_stock[g] && !e.produced_yday[g] && !e.consumed_yday[g]) continue;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(defs_.goods[g].name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%u", e.city_stock[g]);
      ImGui::TableNextColumn();
      ImGui::Text("%u", e.produced_yday[g]);
      ImGui::TableNextColumn();
      ImGui::Text("%u", e.consumed_yday[g]);
    }
    ImGui::EndTable();
  }
  ImGui::Text("Open haul tasks: %u", e.open_tasks);
  ImGui::TextUnformatted("Prices (denarii):");
  for (u32 g = 0; g < e.goods; ++g)
    if (e.city_stock[g] || e.consumed_yday[g])
      ImGui::Text("  %s %.1f  (imported %u, exported %u)", defs_.goods[g].name.c_str(), e.price[g] / 10.0, e.imported_yday[g],
                  e.exported_yday[g]);
  ImGui::End();
}

void App::draw_debug_ui() {
  auto& cam = renderer_.camera();
  const sim::World& w = *world_;
  ImGui::SetNextWindowPos({10, 10}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Debug");
  ImGui::Text("%.1f FPS  (%.2f ms)  sim %.2f ms", static_cast<double>(ImGui::GetIO().Framerate),
              static_cast<double>(frame_ms_), sim_ms_);
  ImGui::Text("Day %llu, %02u:00 (%s, %s)", static_cast<unsigned long long>(sim::day_index(w.now) + 1),
              sim::hour_of_day(w.now), sim::phase_name(sim::phase_of(w.now)), sim::season_name(sim::season(w.now)));
  const char* speeds[] = {"Pause", "1x", "2x", "4x"};
  for (int i = 0; i < 4; ++i) {
    if (i) ImGui::SameLine();
    if (ImGui::RadioButton(speeds[i], speed_ == i)) speed_ = i;
  }
  ImGui::Text("citizens %u  walking %u  buildings %u", w.citizens.ids.count(), w.walking, w.buildings.ids.count());
  ImGui::Text("happiness %u  appeal %d  treasury %lld", w.avg_happiness, w.appeal, static_cast<long long>(w.econ.treasury));
  if (ImGui::Button("Build (B)")) show_build_ = !show_build_;
  ImGui::SameLine();
  if (ImGui::Button("Goods (L)")) show_stock_ = !show_stock_;
  ImGui::SameLine();
  if (ImGui::Button("Labor (J)")) show_labor_ = !show_labor_;
  ImGui::SameLine();
  if (ImGui::Button("Alerts")) show_alerts_ = !show_alerts_;
  {
    int ov = static_cast<int>(overlay_);
    ImGui::SetNextItemWidth(170);
    const char* names[static_cast<int>(Overlay::Count)];
    for (int k = 0; k < static_cast<int>(Overlay::Count); ++k) names[k] = overlay_name(static_cast<Overlay>(k));
    if (ImGui::Combo("Overlay (O)", &ov, names, static_cast<int>(Overlay::Count))) {
      overlay_ = static_cast<Overlay>(ov);
      overlay_age_ = 1 << 20;
    }
    if (overlay_ == Overlay::Service) {
      int sv = overlay_service_.valid() ? overlay_service_.v : 0;
      ImGui::SetNextItemWidth(170);
      if (ImGui::BeginCombo("Service", defs_.services[static_cast<size_t>(sv)].name.c_str())) {
        for (u16 k = 0; k < defs_.services.size(); ++k)
          if (ImGui::Selectable(defs_.services[k].name.c_str(), k == sv)) {
            overlay_service_ = ServiceId{k};
            overlay_age_ = 1 << 20;
          }
        ImGui::EndCombo();
      }
      if (!overlay_service_.valid()) overlay_service_ = ServiceId{0};
    }
  }
  if (ImGui::Button("Finance")) show_finance_ = !show_finance_;
  ImGui::SameLine();
  if (ImGui::Button("Trade")) show_trade_ = !show_trade_;
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
  ImGui::End();
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
    renderer_.time += static_cast<f32>(dt);

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    draw_debug_ui();
    draw_build_menu();
    draw_stock_window();
    draw_citizen_panel(*world_, sel_);
    draw_building_panel(*world_, sel_);
    draw_labor_panel(*world_, &show_labor_);
    draw_finance_panel(*world_, &show_finance_);
    draw_trade_panel(*world_, &show_trade_);
    if (BuildingId jump = draw_alerts_panel(*world_, &show_alerts_); jump.valid() && world_->buildings.alive(jump)) {
      const auto& jb = world_->buildings[jump];
      renderer_.camera().center = {static_cast<f32>(jb.x * sim::kTilePx), static_cast<f32>(jb.y * sim::kTilePx)};
      sel_.building = jump;
    }
    ImGui::Render();
    if (sel_.follow && sel_.citizen.valid() && world_->citizens.alive(sel_.citizen)) {
      Vec2 p;
      const u32 ci = sel_.citizen.index();
      if (world_->citizen_position(ci, alpha_, p, nullptr)) renderer_.camera().center = p;
      else if (world_->citizens.loc[ci].valid() && world_->buildings.alive(world_->citizens.loc[ci])) {
        const auto& lb = world_->buildings[world_->citizens.loc[ci]];
        renderer_.camera().center = {(static_cast<f32>(lb.x) + static_cast<f32>(lb.w) / 2) * sim::kTilePx,
                                     (static_cast<f32>(lb.y) + static_cast<f32>(lb.h) / 2) * sim::kTilePx};
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

    view_.draw(*world_, renderer_, alpha_);
    tools_.draw(*world_, view_, renderer_);
    ++frame_;
    if (!opt_.screenshot.empty() && frame_ == opt_.frames) renderer_.request_screenshot(opt_.screenshot, !opt_.no_ui);
    renderer_.render(ImGui::GetDrawData());
    if (!opt_.screenshot.empty() && frame_ >= opt_.frames) running_ = false;
  }
}

void App::shutdown() {
  renderer_.shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();
  platform::destroy_window(win_);
}

}  // namespace urbs::game
