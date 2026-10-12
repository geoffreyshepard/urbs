// Playtest scripts (--play FILE): mouse clicks, drags and keys sent through the same event handler as
// the player's, so the build menu, the placement ghost, rotation, panels and dialogs are all exercised
// for real.  The file is read as it grows: write a whole script up front, or append commands while the
// game runs and look at the screenshots it takes (docs/testing.md lists the commands).  Real mouse and
// keyboard input is ignored while a script plays.
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "core/log.hpp"
#include "game/app.hpp"
#include "game/iso_draw.hpp"
#include "sim/scenario.hpp"

// ImGui's item hooks (IMGUI_ENABLE_TEST_ENGINE; called only while a script plays): every labelled item drawn
// in a frame, so `button LABEL` can click a button by its text.
namespace {
struct PlayItem {
  std::string label;
  ImRect rect;  // window points
};
std::vector<PlayItem> g_items, g_items_prev;
}  // namespace

void ImGuiTestEngineHook_ItemAdd(ImGuiContext*, ImGuiID, const ImRect&, const ImGuiLastItemData*) {}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext* ctx, ImGuiID, const char* label, ImGuiItemStatusFlags) {
  if (!label) return;
  // The visible text; an item that shows none (an icon, a row drawn by hand) by its hidden id.
  std::string s(label);
  if (const size_t k = s.find("##"); k != std::string::npos)
    s = k > 0 ? s.substr(0, k) : s.substr(s.find_first_not_of('#'));
  if (!s.empty()) g_items.push_back({std::move(s), ctx->LastItemData.Rect});
}
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return nullptr; }

namespace urbs::game {

void play_frame_items() {
  // The finished frame's items are the ones on screen now.
  std::swap(g_items, g_items_prev);
  g_items.clear();
  ImGui::GetCurrentContext()->TestEngineHookItems = true;
}

namespace {
constexpr int kPlayIdleQuit = 60 * 60 * 15;  // frames: a script that stops growing for ~15 minutes ends the run

u8 button_of(const std::string& s) {
  if (s == "right") return SDL_BUTTON_RIGHT;
  if (s == "middle") return SDL_BUTTON_MIDDLE;
  return SDL_BUTTON_LEFT;
}

const char* screen_name(Screen s) {
  switch (s) {
    case Screen::Menu:
      return "menu";
    case Screen::Campaign:
      return "campaign";
    case Screen::Sandbox:
      return "sandbox";
    case Screen::Playing:
      return "playing";
    case Screen::Mods:
      return "mods";
    case Screen::Custom:
      return "custom";
    case Screen::Editor:
      return "editor";
  }
  return "?";
}
}  // namespace

Vec2 App::mouse_pos() const {
  if (!opt_.play.empty()) return play_mouse_;
  float mx = 0, my = 0;
  SDL_GetMouseState(&mx, &my);
  return {mx, my};
}

void App::play_mouse_event(u32 type, f32 x, f32 y, u8 button) {
  SDL_Event e{};
  e.type = type;
  const SDL_WindowID wid = SDL_GetWindowID(win_.window);
  if (type == SDL_EVENT_MOUSE_MOTION) {
    e.motion.windowID = wid;
    e.motion.x = x;
    e.motion.y = y;
    e.motion.xrel = x - play_mouse_.x;
    e.motion.yrel = y - play_mouse_.y;
    e.motion.state = dragging_ ? SDL_BUTTON_RMASK : 0;
    play_mouse_ = {x, y};
  } else {
    e.button.windowID = wid;
    e.button.button = button;
    e.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.clicks = 1;
    e.button.x = x;
    e.button.y = y;
  }
  handle_event(e);
}

void App::play_log_state() {
  std::string tool = "inspect";
  if (tools_.kind == ToolKind::Build && tools_.def.v < defs_.buildings.size()) tool = "build " + defs_.b(tools_.def).id;
  if (tools_.kind == ToolKind::Road) tool = "road";
  if (tools_.kind == ToolKind::Demolish) tool = "demolish";
  std::string sel = "-";
  if (world_ && sel_.building.valid() && world_->buildings.alive(sel_.building)) {
    const sim::Building& b = world_->buildings[sel_.building];
    sel = world_->defs->b(b.def).id + "@" + std::to_string(b.x) + "," + std::to_string(b.y) + " rot" +
          std::to_string(b.rot);
  }
  const Vec2i t = world_ ? mouse_tile() : Vec2i{-1, -1};
  if (!world_) return;
  const sim::World& w = *world_;
  const char* mission = w.mission == sim::MissionState::Won    ? "won"
                        : w.mission == sim::MissionState::Lost ? "lost"
                                                               : "running";
  URBS_LOG_INFO(
      "play: state screen={} mission={} day={} speed={} treasury={} pop={} buildings={} favor={} tool={} "
      "selected={} panel={} build_group={} paused={} messages={} unread_from={} mouse={:.0f},{:.0f} tile={},{} "
      "status=\"{}\"",
      screen_name(screen_), mission, w.mission_days, speed_, w.econ.treasury, w.population(), w.buildings.ids.count(),
      w.favor, tool, sel, static_cast<int>(panel_), build_group_, paused_menu_, w.messages.size(), last_message_,
      play_mouse_.x, play_mouse_.y, t.x, t.y, status_time_ > 0 ? status_ : std::string());
}

void App::play_command(const std::string& line) {
  std::istringstream in(line);
  std::string cmd;
  in >> cmd;
  if (cmd.empty() || cmd[0] == '#') return;
  std::vector<std::string> a;
  for (std::string s; in >> s;) a.push_back(s);
  const auto f = [&](size_t k) { return k < a.size() ? static_cast<f32>(std::atof(a[k].c_str())) : 0.0f; };
  const auto i = [&](size_t k) { return k < a.size() ? std::atoi(a[k].c_str()) : 0; };
  // Expanded steps run in order before the rest of the script.
  const auto then = [&](std::vector<std::string> steps) {
    for (auto it = steps.rbegin(); it != steps.rend(); ++it) play_queue_.push_front(*it);
  };
  const auto tile_screen = [&](i32 tx, i32 ty) {
    tx = std::clamp(tx, 0, world_->map.w - 1);
    ty = std::clamp(ty, 0, world_->map.h - 1);
    return renderer_.camera().world_to_screen(tile_center_iso(world_->map, tx, ty));
  };
  const auto xy = [](Vec2 p) { return std::to_string(p.x) + " " + std::to_string(p.y); };

  if (cmd == "wait") {
    play_wait_ = std::max(1, i(0));
  } else if (cmd == "mouse") {
    play_mouse_event(SDL_EVENT_MOUSE_MOTION, f(0), f(1), 0);
  } else if (cmd == "press" || cmd == "release") {
    play_mouse_event(SDL_EVENT_MOUSE_MOTION, f(0), f(1), 0);
    play_mouse_event(cmd == "press" ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP, f(0), f(1),
                     button_of(a.size() > 2 ? a[2] : ""));
  } else if (cmd == "click") {
    const std::string p = a.size() > 1 ? a[0] + " " + a[1] : "0 0", b = a.size() > 2 ? " " + a[2] : "";
    then({"mouse " + p, "wait 2", "press " + p + b, "wait 2", "release " + p + b, "wait 2"});
  } else if (cmd == "drag") {
    const std::string b = a.size() > 4 ? " " + a[4] : "";
    std::vector<std::string> s = {"mouse " + a.at(0) + " " + a.at(1), "wait 2", "press " + a[0] + " " + a[1] + b,
                                  "wait 2"};
    for (int k = 1; k <= 8; ++k) {
      const f32 u = static_cast<f32>(k) / 8.0f;
      s.push_back("mouse " + xy({f(0) + (f(2) - f(0)) * u, f(1) + (f(3) - f(1)) * u}));
      s.push_back("wait 1");
    }
    s.push_back("release " + a.at(2) + " " + a.at(3) + b);
    s.push_back("wait 2");
    then(s);
  } else if (cmd == "button" || cmd == "button?") {
    // button LABEL: click the button (or selectable, or icon by its id) showing this text, the topmost if
    // several; waits up to a second for it to appear.  button? LABEL: click it if it is there now.
    const bool optional = cmd == "button?";
    const std::string label = line.size() > cmd.size() + 1 ? line.substr(cmd.size() + 1) : std::string();
    const PlayItem* hit = nullptr;
    for (const PlayItem& it : g_items_prev)
      if (it.label == label) hit = &it;
    if (!hit && !optional && ++play_retry_ < 60) {
      play_queue_.push_front(line);
      play_wait_ = 1;
      return;
    }
    play_retry_ = 0;
    if (!hit) {
      if (!optional) {
        URBS_LOG_ERROR("play: no button '{}' on screen", label);
        ++play_failures_;
      }
      return;
    }
    URBS_LOG_INFO("play: button '{}' at {:.0f},{:.0f}", label, (hit->rect.Min.x + hit->rect.Max.x) * 0.5f,
                  (hit->rect.Min.y + hit->rect.Max.y) * 0.5f);
    then({"click " + xy({(hit->rect.Min.x + hit->rect.Max.x) * 0.5f, (hit->rect.Min.y + hit->rect.Max.y) * 0.5f})});
  } else if (cmd == "items") {
    // items: list the labelled UI items on screen (to find a `button` name).
    for (const PlayItem& it : g_items_prev)
      URBS_LOG_INFO("play: item '{}' at {:.0f},{:.0f} {:.0f}x{:.0f}", it.label, it.rect.Min.x, it.rect.Min.y,
                    it.rect.GetWidth(), it.rect.GetHeight());
  } else if (cmd == "read") {
    // read: answer "Very well" to every plain letter in turn (a letter takes a few frames to appear).
    const PlayItem* hit = nullptr;
    for (const PlayItem& it : g_items_prev)
      if (it.label == "Very well") hit = &it;
    if (!hit) {
      if (++play_retry_ < 15) {
        play_queue_.push_front(line);
        play_wait_ = 1;
      } else {
        play_retry_ = 0;
      }
      return;
    }
    play_retry_ = 0;
    then({"click " + xy({(hit->rect.Min.x + hit->rect.Max.x) * 0.5f, (hit->rect.Min.y + hit->rect.Max.y) * 0.5f}),
          "read"});
  } else if (cmd == "tile" || cmd == "clicktile") {
    if (!world_) return;
    const Vec2 p = tile_screen(i(0), i(1));
    then({std::string(cmd == "tile" ? "mouse " : "click ") + xy(p) + (a.size() > 2 ? " " + a[2] : "")});
  } else if (cmd == "dragtile") {
    if (!world_) return;
    then(
        {"drag " + xy(tile_screen(i(0), i(1))) + " " + xy(tile_screen(i(2), i(3))) + (a.size() > 4 ? " " + a[4] : "")});
  } else if (cmd == "key") {
    const SDL_Keycode k = SDL_GetKeyFromName(a.empty() ? "" : a[0].c_str());
    if (k == SDLK_UNKNOWN) {
      URBS_LOG_ERROR("play: unknown key '{}'", a.empty() ? "" : a[0]);
      return;
    }
    SDL_Event e{};
    e.key.windowID = SDL_GetWindowID(win_.window);
    e.key.key = k;
    e.key.scancode = SDL_GetScancodeFromKey(k, nullptr);
    e.key.mod = std::find(a.begin(), a.end(), "shift") != a.end() ? SDL_KMOD_LSHIFT : SDL_KMOD_NONE;
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.down = true;
    handle_event(e);
    e.type = SDL_EVENT_KEY_UP;
    e.key.down = false;
    handle_event(e);
    play_wait_ = 2;
  } else if (cmd == "type") {
    const std::string text = line.size() > 5 ? line.substr(5) : std::string();
    SDL_Event e{};
    e.type = SDL_EVENT_TEXT_INPUT;
    e.text.windowID = SDL_GetWindowID(win_.window);
    e.text.text = text.c_str();
    handle_event(e);
    play_wait_ = 2;
  } else if (cmd == "wheel") {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_WHEEL;
    e.wheel.windowID = SDL_GetWindowID(win_.window);
    e.wheel.y = f(0);
    e.wheel.integer_y = i(0);
    e.wheel.mouse_x = play_mouse_.x;
    e.wheel.mouse_y = play_mouse_.y;
    handle_event(e);
    play_wait_ = 2;
  } else if (cmd == "shot") {
    std::string path = a.empty() ? "play.png" : a[0];
    if (!opt_.play_out.empty() && path[0] != '/') path = opt_.play_out + "/" + path;
    renderer_.request_screenshot(path, true);
    URBS_LOG_INFO("play: shot {}", path);
    play_wait_ = 2;
  } else if (cmd == "advance") {
    // Fast-forward: the ticks the clock would run at speed, without waiting for them.
    if (!world_ || screen_ != Screen::Playing) return;
    for (int n = 0; n < i(0); ++n) {
      sim::push_due_orders(*world_, spec_);
      if (bot_) bot_->step(*world_);
      world_->tick();
    }
    play_wait_ = 2;
  } else if (cmd == "autoplay") {
    // autoplay: the bot takes over this game (a campaign mission started from the menus, say).
    if (!world_ || screen_ != Screen::Playing) return;
    bot_ = std::make_unique<sim::Autoplay>();
    bot_->init(*world_);
  } else if (cmd == "cam") {
    renderer_.camera().center = world_ ? tile_center_iso(world_->map, i(0), i(1)) : renderer_.camera().center;
    play_wait_ = 2;
  } else if (cmd == "zoom") {
    renderer_.camera().zoom = std::clamp(i(0), 0, render::kZoomCount - 1);
    play_wait_ = 2;
  } else if (cmd == "expect") {
    // expect KEY OP VALUE: numbers (treasury, spent, pop, buildings, day, favor, speed, messages, unread) with
    // < <= > >= == !=, or words (screen, mission, tool, selected) with == != (selected: building id).
    if (a.size() < 3 || !world_) {
      URBS_LOG_ERROR("play: bad expect '{}'", line);
      ++play_failures_;
      return;
    }
    const sim::World& w = *world_;
    const std::string& key = a[0];
    const std::string& op = a[1];
    std::string have;
    i64 num = 0;
    bool is_num = true;
    if (key == "treasury")
      num = w.econ.treasury;
    else if (key == "pop")
      num = w.population();
    else if (key == "buildings")
      num = w.buildings.ids.count();
    else if (key == "day")
      num = w.mission_days;
    else if (key == "favor")
      num = w.favor;
    else if (key == "spent")
      num = play_mark_ - w.econ.treasury;  // since the last `mark`
    else if (key == "speed")
      num = speed_;
    else if (key == "messages")
      num = static_cast<i64>(w.messages.size());
    else if (key == "unread") {
      for (const auto& m : w.messages) num += !m.answered && m.id > last_message_;
    } else {
      is_num = false;
      if (key == "screen")
        have = screen_name(screen_);
      else if (key == "mission")
        have = w.mission == sim::MissionState::Won ? "won" : w.mission == sim::MissionState::Lost ? "lost" : "running";
      else if (key == "tool")
        have = tools_.kind == ToolKind::Build && tools_.def.valid() ? defs_.b(tools_.def).id
               : tools_.kind == ToolKind::Road                      ? "road"
               : tools_.kind == ToolKind::Demolish                  ? "demolish"
                                                                    : "inspect";
      else if (key == "selected")
        have = sel_.building.valid() && w.buildings.alive(sel_.building) ? w.defs->b(w.buildings[sel_.building].def).id
                                                                         : "-";
      else {
        URBS_LOG_ERROR("play: unknown expect key '{}'", key);
        ++play_failures_;
        return;
      }
    }
    bool ok = false;
    if (is_num) {
      const i64 v = std::atoll(a[2].c_str());
      ok = op == "<"    ? num < v
           : op == "<=" ? num <= v
           : op == ">"  ? num > v
           : op == ">=" ? num >= v
           : op == "==" ? num == v
           : op == "!=" ? num != v
                        : false;
      have = std::to_string(num);
    } else {
      ok = op == "==" ? have == a[2] : op == "!=" ? have != a[2] : false;
    }
    if (ok) {
      URBS_LOG_INFO("play: ok   {}", line);
    } else {
      URBS_LOG_ERROR("play: FAIL {} (is {})", line, have);
      ++play_failures_;
    }
  } else if (cmd == "mark") {
    play_mark_ = world_ ? world_->econ.treasury : 0;
  } else if (cmd == "state") {
    play_log_state();
  } else if (cmd == "quit") {
    running_ = false;
  } else {
    URBS_LOG_ERROR("play: unknown command '{}'", line);
  }
}

void App::play_step() {
  if (play_wait_ > 0) {
    --play_wait_;
    return;
  }
  while (play_wait_ == 0 && running_) {
    if (play_queue_.empty()) {
      // Read what was written since the last look (reopening the file: a stream at its end stays there).
      // A line without its newline is still being written.
      std::ifstream in(opt_.play, std::ios::binary);
      in.seekg(static_cast<std::streamoff>(play_pos_));
      const std::string fresh((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      play_pos_ += fresh.size();
      play_partial_ += fresh;
      for (size_t nl; (nl = play_partial_.find('\n')) != std::string::npos;) {
        play_queue_.push_back(play_partial_.substr(0, nl));
        play_partial_.erase(0, nl + 1);
      }
      if (play_queue_.empty()) {
        if (++play_idle_ > kPlayIdleQuit) {
          URBS_LOG_INFO("play: the script stopped growing; quitting");
          running_ = false;
        }
        return;
      }
      play_idle_ = 0;
    }
    const std::string line = play_queue_.front();
    play_queue_.pop_front();
    play_command(line);
  }
}

}  // namespace urbs::game
