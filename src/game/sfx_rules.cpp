#include "game/sfx_rules.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "sim/world.hpp"

namespace urbs::game {

using json = nlohmann::json;

void SfxManifest::load(const std::string& path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error("cannot open " + path);
  const json j = json::parse(f);
  *this = SfxManifest{};
  if (j.contains("positional")) {
    const json& p = j["positional"];
    positional.full = p.value("full", positional.full);
    positional.max = std::max(p.value("max", positional.max), positional.full + 0.01f);
    positional.far_gain = p.value("far_gain", positional.far_gain);
    positional.pan = p.value("pan", positional.pan);
  }
  if (j.contains("ambient")) {
    const json& a = j["ambient"];
    ambient.file = a.value("file", std::string());
    ambient.gain = a.value("gain", ambient.gain);
    ambient.people_min = a.value("people_min", ambient.people_min);
    ambient.people_full = std::max(a.value("people_full", ambient.people_full), ambient.people_min + 1);
    ambient.fade_s = a.value("fade_s", ambient.fade_s);
  }
  coin_min = j.value("coin_min_denarii", coin_min);
  for (const json& e : j.at("events")) {
    SfxEvent ev;
    ev.id = e.at("id").get<std::string>();
    ev.files = e.at("files").get<std::vector<std::string>>();
    if (ev.files.empty()) throw std::runtime_error("sfx event " + ev.id + " has no files");
    ev.gain = e.value("gain", ev.gain);
    ev.cooldown = e.value("cooldown_s", ev.cooldown);
    ev.max_playing = e.value("max_playing", ev.max_playing);
    ev.pitch_var = e.value("pitch_var", ev.pitch_var);
    ev.gain_var = e.value("gain_var", ev.gain_var);
    ev.positional = e.value("positional", ev.positional);
    events.push_back(std::move(ev));
  }
}

int SfxManifest::index(std::string_view id) const {
  for (size_t k = 0; k < events.size(); ++k)
    if (events[k].id == id) return static_cast<int>(k);
  return -1;
}

SfxPlacement place_sound(const SfxPositional& p, f32 dx, f32 dy) {
  const f32 d = std::sqrt(dx * dx + dy * dy);
  SfxPlacement out;
  if (d > p.max) return out;
  if (d <= p.full) {
    out.gain = 1;
  } else {
    const f32 k = (d - p.full) / (p.max - p.full);  // 0..1 across the fall-off
    out.gain = 1 + (p.far_gain - 1) * k;
  }
  out.pan = std::clamp(dx, -1.0f, 1.0f) * p.pan;
  return out;
}

f32 ambient_level(const SfxAmbient& a, f32 people) {
  if (people <= a.people_min) return 0;
  const f32 k = std::min(1.0f, (people - a.people_min) / (a.people_full - a.people_min));
  return std::sqrt(k);  // a few people are already heard; a crowd only a little more
}

namespace {
// Money that moves in lumps the player notices: trade both ways, grants from Rome, festivals and
// payments asked for in letters. Taxes and wages trickle in and out all the time and stay quiet.
constexpr econ::Money kCoinMoney[] = {econ::Money::Exports, econ::Money::Tribute, econ::Money::Imports,
                                      econ::Money::Festivals, econ::Money::Other};
}  // namespace

void SfxWatch::reset(const sim::World& w) {
  last_now_ = w.now;
  last_message_ = 0;
  for (const sim::Message& m : w.messages) last_message_ = std::max(last_message_, m.id);
  mission_ = static_cast<u8>(w.mission);
  festival_ = w.festival_until > w.now;
  away_ = 0;
  for (const auto& u : w.army.units)
    away_ += u.alive && u.side == 0 && (u.order == military::Order::Campaign || u.order == military::Order::Away);
  for (int k = 0; k < econ::kMoneyCount; ++k) money_[k] = w.econ.money.year[k];
}

int SfxWatch::poll(const sim::World& w, i64 coin_min, f32 fx, f32 fy, SfxCue* out) {
  int n = 0;
  auto cue = [&](const char* ev) -> SfxCue* {
    if (n >= kMaxCues) return nullptr;
    out[n] = SfxCue{ev};
    return &out[n++];
  };
  if (w.now < last_now_) reset(w);  // another world, or one loaded from a save
  const bool advanced = w.now != last_now_;

  // Letters and alerts: one notification sound per look, a letter first.
  bool letter = false, chime = false;
  for (const sim::Message& m : w.messages)
    if (m.id > last_message_) {
      last_message_ = m.id;
      letter = true;
    }
  // Repeats of an alert update the old entry in place, so the whole (capped) list is scanned.
  for (const sim::Alert& a : w.alerts) {
    if (a.t <= last_now_) continue;
    switch (a.kind) {
      case sim::AlertKind::Fire:
      case sim::AlertKind::Collapse:
        if (SfxCue* c = cue(a.kind == sim::AlertKind::Fire ? "fire" : "collapse");
            c && a.b.valid() && w.buildings.alive(a.b)) {
          const sim::Building& b = w.buildings[a.b];
          c->at = true;
          c->x = static_cast<f32>(b.x) + static_cast<f32>(b.w) * 0.5f;
          c->y = static_cast<f32>(b.y) + static_cast<f32>(b.h) * 0.5f;
        }
        break;
      case sim::AlertKind::Raid:
        cue("raid");
        break;
      case sim::AlertKind::Dedicated:
        cue("festival");
        break;
      case sim::AlertKind::Event:
        if (a.arg == 1002) cue("collapse");  // an earthquake: heard everywhere
        if (a.arg == 1200) cue("festival");  // a campaign won: a triumph
        break;
      case sim::AlertKind::Riot:
      case sim::AlertKind::Revolt:
      case sim::AlertKind::Disease:
      case sim::AlertKind::GodAngry:
      case sim::AlertKind::Request:
      case sim::AlertKind::TreasuryLow:
        chime = true;
        break;
      default:
        break;
    }
  }
  if (letter)
    cue("message");
  else if (chime)
    cue("alert");

  // Fighting: heard from the soldier striking nearest the view, only while the city's time runs.
  if (advanced) {
    const auto& A = w.army;
    f32 best = 1e30f, bx = 0, by = 0;
    for (u32 i = 0; i < A.ids.high_water(); ++i) {
      if (!A.ids.alive_index(i) || A.attacking[i] == 0) continue;
      const f32 dx = A.x[i] - fx, dy = A.y[i] - fy, d = dx * dx + dy * dy;
      if (d < best) {
        best = d;
        bx = A.x[i];
        by = A.y[i];
      }
    }
    if (best < 1e30f)
      if (SfxCue* c = cue("battle")) {
        c->at = true;
        c->x = bx;
        c->y = by;
      }
  }

  // Cohorts marching off on campaign, or home again.
  u32 away = 0;
  for (const auto& u : w.army.units)
    away += u.alive && u.side == 0 && (u.order == military::Order::Campaign || u.order == military::Order::Away);
  if (away != away_) cue("legion");
  away_ = away;

  const bool festival = w.festival_until > w.now;
  if (festival && !festival_) cue("festival");
  festival_ = festival;

  if (static_cast<u8>(w.mission) != mission_ && w.mission == sim::MissionState::Won) cue("mission_won");
  mission_ = static_cast<u8>(w.mission);

  i64 moved = 0;
  for (econ::Money m : kCoinMoney) {
    const int k = static_cast<int>(m);
    const i64 now = w.econ.money.year[k];
    moved += now >= money_[k] ? now - money_[k] : now;  // a new year starts the tally again
  }
  for (int k = 0; k < econ::kMoneyCount; ++k) money_[k] = w.econ.money.year[k];
  if (moved >= coin_min) cue("coin");

  last_now_ = w.now;
  return n;
}

}  // namespace urbs::game
