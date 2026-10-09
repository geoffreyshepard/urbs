#include "game/app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <chrono>
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
    sim::ScenarioSpec spec = sim::load_scenario(scenario);
    sim::setup_scenario(*world_, defs_, spec);
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
  for (int t = 0; t < opt.ticks_before; ++t) world_->tick();
  auto& cam = renderer_.camera();
  if (opt.cam_x >= 0) cam.center = {opt.cam_x * sim::kTilePx, opt.cam_y * sim::kTilePx};
  if (opt.zoom >= 0) cam.zoom = std::min(opt.zoom, render::kZoomCount - 1);
  speed_ = std::clamp(opt.speed, 0, 3);
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
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (e.button.button == SDL_BUTTON_MIDDLE || e.button.button == SDL_BUTTON_RIGHT) dragging_ = false;
      break;
    case SDL_EVENT_MOUSE_MOTION:
      if (dragging_) {
        f32 k = cam.density / cam.scale();
        cam.center = cam.center - Vec2{e.motion.xrel * k, e.motion.yrel * k};
      }
      break;
    case SDL_EVENT_KEY_DOWN:
      if (ImGui::GetIO().WantCaptureKeyboard) break;
      if (e.key.key == SDLK_ESCAPE) running_ = false;
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
    world_->tick();
    accum_ -= kTickSeconds;
    ++n;
  }
  if (n == 8) accum_ = 0;  // behind: slow down instead of spiralling
  if (n > 0) sim_ms_ = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
  alpha_ = static_cast<f32>(accum_ / kTickSeconds);
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
    ImGui::Render();

    view_.draw(*world_, renderer_, alpha_);
    ++frame_;
    if (!opt_.screenshot.empty() && frame_ == opt_.frames) renderer_.request_screenshot(opt_.screenshot);
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
