#pragma once
#include <memory>
#include <string>

#include "data/defs.hpp"
#include "game/overlays.hpp"
#include "game/panels.hpp"
#include "game/profile.hpp"
#include "game/tools.hpp"
#include "game/ui_icons.hpp"
#include "game/world_view.hpp"
#include "platform/platform.hpp"
#include "render/renderer.hpp"
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
  std::string scenario;        // load this scenario directly (dev/testing), skipping the menus
  std::string screen;          // "menu", "campaign" (for screenshots of menus)
  int speed = 1;
  int ticks_before = 0;  // run this many ticks before the first frame
  int inspect = -1;      // select (and follow) this citizen index at start
  bool no_ui = false;
  int overlay = 0;
};

AppOptions parse_args(int argc, char** argv);

enum class Screen : u8 { Menu, Campaign, Sandbox, Playing };

class App {
 public:
  bool init(const AppOptions& opt);
  void run();
  void shutdown();

 private:
  void handle_event(const SDL_Event& e);
  void update_camera(f32 dt);
  void step_sim(f64 dt);
  void new_world(const std::string& scenario_path);
  void start_scenario(sim::ScenarioSpec spec);
  void start_mission(int index);
  void defs_for_mission(int index);
  void start_sandbox();
  void menu_background();
  void after_world_change();
  void save_slot(const std::string& name);
  bool load_slot(const std::string& name);
  [[nodiscard]] std::string save_path(const std::string& name) const;
  [[nodiscard]] Vec2i mouse_tile() const;
  void pick_at_mouse();
  void update_light();

  // UI
  void draw_ui();
  void draw_hud();
  void draw_main_menu();
  void draw_campaign();
  void draw_sandbox_setup();
  void draw_pause();
  void draw_settings();
  void draw_saveload();
  void draw_research();
  void draw_build_menu();
  void draw_stock_window();
  void draw_debug_ui();
  void draw_help();

  AppOptions opt_;
  platform::Window win_;
  render::Renderer renderer_;
  data::Defs defs_, base_defs_;
  std::string root_, user_dir_;
  std::unique_ptr<sim::World> world_;
  sim::ScenarioSpec spec_;
  WorldView view_;
  Tools tools_;
  UiIcons icons_;
  Settings settings_;
  CampaignDef campaign_;
  CampaignProfile profile_;
  Screen screen_ = Screen::Menu;
  int mission_index_ = -1;  // campaign mission being played
  int selected_mission_ = 0;
  sim::MapGenParams gen_;

  int speed_ = 1;  // 0 pause, 1, 2, 3 = 1x 2x 4x
  f64 accum_ = 0;
  f32 alpha_ = 0;
  f64 sim_ms_ = 0;
  bool running_ = true;
  bool dragging_ = false;
  f32 right_drag_ = 0;
  int frame_ = 0;
  u64 last_counter_ = 0;
  f32 frame_ms_ = 0;
  u16 last_autosave_day_ = 0;
  u16 salary_day_ = 0;

  // Panels
  bool show_build_ = true, show_stock_ = false, show_labor_ = false, show_alerts_ = false, show_finance_ = false,
       show_trade_ = false, show_religion_ = false, show_threats_ = false, show_military_ = false, show_rome_ = false,
       show_advisors_ = false, show_research_ = false, show_debug_ = false, show_help_ = false;
  bool paused_menu_ = false, show_settings_ = false, show_saveload_ = false, saveload_saving_ = false;
  bool end_dismissed_ = false;
  int advisor_ = 1;
  u16 last_message_ = 0;
  Selection sel_;
  Overlay overlay_ = Overlay::None;
  ServiceId overlay_service_;
  std::vector<u32> overlay_px_;
  int overlay_age_ = 0;
  std::string status_;  // transient status line (saved, loaded, errors)
  f32 status_time_ = 0;
};

}  // namespace urbs::game
