#pragma once
#include <array>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "data/defs.hpp"
#include "game/lighting.hpp"
#include "game/music.hpp"
#include "game/overlays.hpp"
#include "game/panel_kit.hpp"
#include "game/panels.hpp"
#include "game/profile.hpp"
#include "game/tools.hpp"
#include "game/ui_icons.hpp"
#include "game/ui_picture.hpp"
#include "game/ui_skin.hpp"
#include "game/world_view.hpp"
#include "platform/platform.hpp"
#include "render/renderer.hpp"
#include "sim/autoplay.hpp"
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
  std::string weather;         // screenshots: show this weather (presentation only, the sim is untouched)
  float hour = -1.0f;          // screenshots: light the world at this hour (0..24; presentation only)
  int overlay = 0;
  int build_group = -1;   // screenshots: open this build-menu group's list (0 = roads)
  std::string show;       // screenshots: comma-separated panels to open (finance,labor,...)
  bool autoplay = false;  // let the autoplay bot play (demo / screenshots)
  bool mute = false;      // no audio device (screenshot runs are always silent)
  int career = -1;        // screenshots: pretend the first N campaign missions are won (never saved)
  int perf = 0;           // measure this many frames (after a warm-up), log frame-time percentiles, quit
  bool no_vsync = false;  // present without waiting for vsync (perf runs: shows the real frame cost)
  std::string play;       // playtest: input commands read from this file as it grows (play.cpp, docs/testing.md)
  std::string user_dir;   // saves, settings and career here instead of the player's own folder
  std::string play_out;   // playtest screenshots with a relative path go here
};

AppOptions parse_args(int argc, char** argv);
// Playtest scripts: start recording this frame's labelled UI items (play.cpp).
void play_frame_items();

// Darkens the living city behind the menu screens (campaign, sandbox, custom scenarios, mods, dialogs).
void dim_backdrop(int alpha = 200);

// The report panels, in top-bar order. One is open at a time, hanging from its button.
enum class Panel : u8 {
  Rome,
  Advisors,
  Finance,
  Goods,
  Trade,
  Labor,
  Religion,
  Military,
  Research,
  Alerts,
  Help,
  None
};
inline constexpr int kPanelCount = static_cast<int>(Panel::None);

enum class Screen : u8 { Menu, Campaign, Sandbox, Playing, Mods, Custom, Editor };

// Scenario editor state (src/game/editor.cpp).
struct EditorState {
  std::string id = "my_scenario", name = "My scenario", path;  // path: folder of the scenario being edited
  int brush = 0;       // 0 terrain, 1 raise, 2 lower, 3 resource, 4 clear resource, 5 road, 6 erase road
  int terrain = 0;     // sim::Terrain
  int resource = 1;    // sim::Resource
  int size = 2;        // brush radius
  int mission = 16, era = 0, region = 0, entry = 2, start_hour = 7;
  int treasury = 15000, population = 2000, culture = 0, prosperity = 0, security = 0;
  int wood = 200, wheat = 200;
  std::vector<u8> gods, partners;
  std::string status;
  bool painting = false;
};

class App {
 public:
  bool init(const AppOptions& opt);
  void run();
  void shutdown();
  [[nodiscard]] int play_failures() const { return play_failures_; }

 private:
  void handle_event(const SDL_Event& e);
  void update_camera(f32 dt);
  void record_perf(f32 frame, f32 sim, f32 ui, f32 world, f32 render);
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
  // The pointer in window points: the real mouse, or the playtest script's.
  [[nodiscard]] Vec2 mouse_pos() const;

  // Playtest (--play, play.cpp): scripted mouse and keys through the same paths as the player's.
  void play_step();
  void play_command(const std::string& line);
  void play_mouse_event(u32 type, f32 x, f32 y, u8 button);
  void play_log_state();

  // UI
  void draw_ui();
  void draw_hud();
  void draw_main_menu();
  void draw_campaign();
  void draw_sandbox_setup();
  void draw_pause();
  void draw_settings();
  void draw_saveload();
  void draw_research(const Dock& dock, bool* open);
  void draw_build_menu();
  void draw_stock_window(const Dock& dock, bool* open);
  void draw_debug_ui();
  void draw_help(const Dock& dock, bool* open);
  // The open report panel's place: at the right edge, hanging from its top-bar button.
  [[nodiscard]] Dock panel_dock(Panel p) const;
  void toggle_panel(Panel p) { panel_ = panel_ == p ? Panel::None : p; }
  // Mods, custom scenarios, scenario editor (editor.cpp)
  // Loads the defs and the sprites of the enabled mods, then re-resolves the views' sprites.
  bool reload_defs(std::string* err);
  [[nodiscard]] std::string mod_sprite_status() const;  // " Mod sprite refused: ..." or empty
  void draw_mods();
  void draw_custom();
  void draw_editor();
  void editor_new();
  void editor_open(const std::string& dir);
  bool editor_save();
  void editor_event(const SDL_Event& e);
  void editor_paint(Vec2i t);
  [[nodiscard]] std::vector<std::pair<std::string, std::string>> custom_scenarios() const;  // (title, folder)

  AppOptions opt_;
  platform::Window win_;
  render::Renderer renderer_;
  platform::Audio audio_;
  MusicDirector music_;
  data::Defs defs_, base_defs_;
  std::string root_, user_dir_;
  std::unique_ptr<sim::World> world_;
  std::unique_ptr<sim::Autoplay> bot_;  // --autoplay
  sim::ScenarioSpec spec_;
  WorldView view_;
  Tools tools_;
  UiIcons icons_;
  UiSkin skin_;
  UiPicture province_map_;  // the campaign's painted map (assets/ui), loaded the first time it is shown
  bool province_map_tried_ = false;
  std::vector<std::array<f32, 7>> perf_;  // --perf: per-frame timings (ms) and instance counts
  Settings settings_;
  CampaignDef campaign_;
  CampaignProfile profile_;
  Screen screen_ = Screen::Menu;
  int mission_index_ = -1;  // campaign mission being played
  int selected_mission_ = 0;
  sim::MapGenParams gen_;
  int sandbox_region_ = 0;
  bool sandbox_nile_ = false;
  std::vector<ModInfo> mods_;
  std::vector<std::string> mod_sprite_warnings_;  // files the enabled mods' sprites/ folders had refused
  EditorState ed_;

  int speed_ = 1;  // 0 pause, 1, 2, 3 = 1x 2x 4x
  // Build menu: the open group on the icon rail (-1 = none), where its list pops out, and the HUD's height.
  int build_group_ = -1;
  float build_fly_y_ = 0, build_fly_h_ = 0, hud_h_ = 64, rail_r_ = 64;
  float hud_need_em_ = 0;  // the top bar's width without rating names, in font sizes (draw_hud)
  Tick alerts_seen_ = 0;  // alerts newer than this count as new on the HUD badge
  f64 accum_ = 0;
  f32 alpha_ = 0;
  Lighting lighting_;
  f64 sim_ms_ = 0;
  bool running_ = true;
  bool dragging_ = false;
  f32 right_drag_ = 0;
  int frame_ = 0;
  u64 last_counter_ = 0;
  f32 frame_ms_ = 0;
  u16 last_autosave_day_ = 0;
  // The menu backdrop: a living city (true) or a plain landscape, and the camera's drift around it.
  bool menu_live_ = false;
  Vec2 menu_focus_{0, 0};
  f32 menu_radius_ = 0, menu_clock_ = 0;
  u16 salary_day_ = 0;

  // Panels
  bool show_build_ = true, show_threats_ = false, show_debug_ = false;
  // The report panel that is open (one at a time), and the screen x of each one's top-bar button.
  Panel panel_ = Panel::None;
  float panel_x_[kPanelCount] = {};
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

  // Playtest script state.
  size_t play_pos_ = 0;                 // bytes of the script read so far
  std::string play_partial_;            // a line still being written
  std::deque<std::string> play_queue_;  // expanded steps (a click is move, press, release)
  int play_wait_ = 0;                   // frames until the next command
  Vec2 play_mouse_{0, 0};               // window points
  int play_idle_ = 0;                   // frames spent waiting for the script to grow
  int play_failures_ = 0;               // failed `expect` lines
  i64 play_mark_ = 0;                   // treasury at the last `mark`
  int play_retry_ = 0;                  // frames a `button` has waited for its item
};

}  // namespace urbs::game
