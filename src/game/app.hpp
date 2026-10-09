#pragma once
#include <memory>
#include <string>

#include "platform/platform.hpp"
#include "render/renderer.hpp"
#include "data/defs.hpp"
#include "game/overlays.hpp"
#include "game/panels.hpp"
#include "game/tools.hpp"
#include "game/world_view.hpp"
#include "sim/scenario.hpp"
#include "sim/world.hpp"

namespace urbs::game {

struct AppOptions {
  std::string screenshot;  // write a PNG after `frames` frames and quit
  int frames = 3;
  int zoom = -1;
  i32 map_size = 256;
  u64 seed = 7;
  f32 cam_x = -1, cam_y = -1;  // tile coordinates
  std::string scenario;          // headless scenario json to load
  int speed = 1;
  int ticks_before = 0;          // run this many ticks before the first frame
  int inspect = -1;              // select (and follow) this citizen index at start
  bool no_ui = false;
  int overlay = 0;
};

AppOptions parse_args(int argc, char** argv);

class App {
 public:
  bool init(const AppOptions& opt);
  void run();
  void shutdown();

 private:
  void handle_event(const SDL_Event& e);
  void update_camera(f32 dt);
  void draw_debug_ui();
  void new_world(const std::string& scenario);
  void step_sim(f64 dt);
  void draw_build_menu();
  void draw_stock_window();
  [[nodiscard]] Vec2i mouse_tile() const;
  void pick_at_mouse();

  AppOptions opt_;
  platform::Window win_;
  render::Renderer renderer_;
  data::Defs defs_;
  std::unique_ptr<sim::World> world_;
  sim::ScenarioSpec spec_;
  WorldView view_;
  Tools tools_;
  bool show_build_ = true, show_stock_ = false, show_labor_ = false, show_alerts_ = true, show_finance_ = false,
       show_trade_ = false, show_religion_ = false, show_threats_ = false, show_military_ = false;
  f32 right_drag_ = 0;
  Selection sel_;
  Overlay overlay_ = Overlay::None;
  ServiceId overlay_service_;
  std::vector<u32> overlay_px_;
  int overlay_age_ = 0;
  sim::MapGenParams gen_;
  int speed_ = 1;  // 0 pause, 1, 2, 4 (index into kSpeeds)
  f64 accum_ = 0;
  f32 alpha_ = 0;
  f64 sim_ms_ = 0;
  bool running_ = true;
  bool dragging_ = false;
  int frame_ = 0;
  u64 last_counter_ = 0;
  f32 frame_ms_ = 0;
};

}  // namespace urbs::game
