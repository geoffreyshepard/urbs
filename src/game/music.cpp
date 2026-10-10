#include "game/music.hpp"

#include <algorithm>
#include <exception>

#include "core/log.hpp"
#include "sim/world.hpp"

namespace urbs::game {

namespace {
constexpr f32 kPollSeconds = 0.5f;
constexpr f32 kBattleHold = 8.0f, kDisasterHold = 10.0f;
constexpr f32 kAftermathAfter = 30.0f;  // a disaster this long earns the lament
constexpr f32 kStingerCooldown = 20.0f;

bool is_battle(Mood m) { return m == Mood::Skirmish || m == Mood::Siege || m == Mood::LargeBattle; }

const char* kMoodNames[] = {"city",     "festival", "aftermath", "unrest",       "raid near",
                            "disaster", "skirmish", "siege",     "large battle", "mission over"};
}  // namespace

void MusicDirector::init(const std::string& data_root, platform::Audio* audio) {
  root_ = data_root;
  audio_ = audio;
  try {
    m_.load(root_ + "data/defs/music.json");
    ok_ = true;
  } catch (const std::exception& e) {
    URBS_LOG_WARN("music disabled: {}", e.what());
  }
}

const char* MusicDirector::mood_name() const { return kMoodNames[static_cast<int>(mood_)]; }

void MusicDirector::update(f32 dt, MusicContext ctx, const sim::World* w, bool end_screen) {
  if (!ok_ || !audio_ || !audio_->enabled()) return;
  clock_ += dt;
  if (!ctx_seen_ || ctx != ctx_) {
    ctx_seen_ = true;
    ctx_ = ctx;
    meta_track_.clear();
    if (ctx == MusicContext::Playing && w) mission_started(*w);
  }
  switch (ctx) {
    case MusicContext::Menu:
      meta("menu");
      break;
    case MusicContext::Map:
      if (promotion_) {
        promotion_ = false;
        stinger("promotion");
      }
      meta("map");
      break;
    case MusicContext::Setup:
      meta("briefing");
      break;
    case MusicContext::Playing:
      if (w) playing(*w, end_screen);
      break;
  }
}

void MusicDirector::meta(std::string_view use) {
  const MusicTrack* t = m_.by_use("meta", use);
  if (!t) return;
  if (meta_track_ != t->id) {
    meta_track_ = t->id;
    meta_started_ = false;
    meta_wait_until_ = -1;
  }
  if (t->loop) {
    audio_->play_music(file(t->id), true, m_.crossfade);
    return;
  }
  if (!meta_started_) {
    audio_->play_music(file(t->id), false, m_.crossfade);
    meta_started_ = true;
  } else if (!audio_->music_playing()) {
    if (meta_wait_until_ < 0) meta_wait_until_ = clock_ + uniform(m_.meta_silence[0], m_.meta_silence[1]);
    if (clock_ >= meta_wait_until_) meta_started_ = false;  // play it again next frame
  }
}

void MusicDirector::mission_started(const sim::World& w) {
  if (!ok_ || !audio_ || !audio_->enabled()) return;
  ctx_ = MusicContext::Playing;
  ctx_seen_ = true;
  mood_ = Mood::City;
  hold_until_ = 0;
  aftermath_due_ = festival_played_ = once_started_ = false;
  city_track_.clear();
  silence_until_ = clock_ + 4.0f;  // let the opening stinger breathe
  last_alert_ = w.now;
  last_state_ = static_cast<u8>(w.mission);
  next_poll_ = 0;
  audio_->play_music("", false, m_.crossfade);
  stinger("mission_start");
}

void MusicDirector::playing(const sim::World& w, bool end_screen) {
  if (clock_ >= next_poll_) {
    next_poll_ = clock_ + kPollSeconds;
    sig_ = read_signals(w);
    scan_alerts(w);
  }
  if (static_cast<u8>(w.mission) != last_state_) {
    last_state_ = static_cast<u8>(w.mission);
    if (w.mission == sim::MissionState::Won) stinger("mission_won");
    if (w.mission == sim::MissionState::Lost) stinger("mission_failed");
  }
  MusicSignals s = sig_;
  if (!end_screen) s.mission_over = false;  // "keep building this city"
  Mood want = base_mood(s);

  // Battles and disasters outlast their cause a little, so the music doesn't flicker. The hold is
  // refreshed only by the cause itself, never by the held mood.
  if (is_battle(want)) hold_until_ = clock_ + kBattleHold;
  if (want == Mood::Disaster) hold_until_ = std::max(hold_until_, clock_ + kDisasterHold);
  if (want < mood_ && (is_battle(mood_) || mood_ == Mood::Disaster) && clock_ < hold_until_) want = mood_;
  // After a long disaster, the lament plays once the danger has passed.
  if (mood_ == Mood::Disaster && want < Mood::Disaster && clock_ - disaster_since_ >= kAftermathAfter)
    aftermath_due_ = true;
  if (aftermath_due_ && want <= Mood::Aftermath) want = Mood::Aftermath;
  // A festival's music plays once per festival.
  if (!s.festival) festival_played_ = false;
  if (want == Mood::Festival && festival_played_) want = Mood::City;

  if (want != mood_) enter(want);

  if (mood_ == Mood::City) {
    if (!city_track_.empty()) {
      if (!audio_->music_playing()) {
        last_city_ = city_track_;
        city_track_.clear();
        silence_until_ = clock_ + uniform(m_.city_silence[0], m_.city_silence[1]);
      }
      return;
    }
    if (clock_ < silence_until_) return;
    const int k = pick_city(m_, city_context(w), last_city_, uniform(0, 1));
    if (k < 0) {
      silence_until_ = clock_ + 60;
      return;
    }
    city_track_ = m_.tracks[static_cast<size_t>(k)].id;
    URBS_LOG_INFO("music: city track {}", city_track_);
    audio_->play_music(file(city_track_), false, 1.0f);
    return;
  }

  const auto [kind, use] = mood_track(mood_, s.empire);
  const MusicTrack* t = m_.by_use(kind, use);
  if (mood_ == Mood::MissionOver && w.mission == sim::MissionState::Won) t = m_.by_use("meta", "menu");
  if (!t) return;
  const f32 fade = is_battle(mood_) ? m_.crossfade_battle : m_.crossfade;
  if (t->loop) {
    audio_->play_music(file(t->id), true, fade);
  } else if (play_once(t, fade)) {
    if (mood_ == Mood::Festival) festival_played_ = true;
    if (mood_ == Mood::Aftermath) aftermath_due_ = false;
  }
}

bool MusicDirector::play_once(const MusicTrack* t, f32 fade) {
  if (!once_started_) {
    audio_->play_music(file(t->id), false, fade);
    once_started_ = true;
    return false;
  }
  return !audio_->music_playing();
}

void MusicDirector::enter(Mood m) {
  URBS_LOG_INFO("music: {} -> {} (enemies {}, defenders {}, combat {}, burning {}, favor {})",
                kMoodNames[static_cast<int>(mood_)], kMoodNames[static_cast<int>(m)], sig_.enemies, sig_.defenders,
                sig_.combat, sig_.burning, sig_.favor);
  if (m == Mood::Disaster && mood_ != Mood::Disaster) disaster_since_ = clock_;
  if (m == Mood::City) {
    // Back to the city: a short breath, then the playlist resumes with a new track.
    audio_->play_music("", false, m_.crossfade);
    if (!city_track_.empty()) last_city_ = city_track_;
    city_track_.clear();
    silence_until_ = clock_ + (is_battle(mood_) ? 8.0f : 4.0f);
  }
  mood_ = m;
  once_started_ = false;
}

void MusicDirector::scan_alerts(const sim::World& w) {
  // The most important new event this poll gets a stinger; the rest pass quietly.
  static constexpr const char* kPriority[] = {"god_wrath",    "raid_sighted",    "monument_complete",
                                              "god_blessing", "festival_begins", "request_from_rome",
                                              "promotion"};
  int best = -1;
  // Repeats of an alert update the old entry in place, so the whole (capped) list is scanned.
  for (auto it = w.alerts.begin(); it != w.alerts.end(); ++it) {
    if (it->t <= last_alert_) continue;
    const char* ev = nullptr;
    switch (it->kind) {
      case sim::AlertKind::Raid:
        ev = "raid_sighted";
        break;
      case sim::AlertKind::GodAngry:
        ev = (it->arg & 0x100) ? "god_blessing" : "god_wrath";
        break;
      case sim::AlertKind::Request:
        if (!(it->arg & 0x8000)) ev = "request_from_rome";  // 0x8000 marks a failed request
        break;
      case sim::AlertKind::Dedicated:
        ev = "monument_complete";
        break;
      case sim::AlertKind::Event:
        if (it->arg < 1000)
          ev = "festival_begins";  // arg = the god honoured
        else if (it->arg == 1200)
          ev = "promotion";  // a campaign won: triumph
        break;
      default:
        break;
    }
    if (!ev) continue;
    for (int k = 0; k < static_cast<int>(std::size(kPriority)); ++k)
      if (std::string_view(kPriority[k]) == ev && (best < 0 || k < best)) best = k;
  }
  last_alert_ = w.now;
  if (best >= 0) stinger(kPriority[best]);
}

void MusicDirector::stinger(std::string_view event) {
  const std::string id = m_.stinger(event);
  if (id.empty()) return;
  auto it = std::find_if(stinger_at_.begin(), stinger_at_.end(), [&](const auto& p) { return p.first == event; });
  const bool big = event == "mission_won" || event == "mission_failed";
  if (it != stinger_at_.end()) {
    if (!big && clock_ - it->second < kStingerCooldown) return;
    it->second = clock_;
  } else {
    stinger_at_.emplace_back(std::string(event), clock_);
  }
  if (audio_->stinger_playing() && !big) return;  // don't stack; the outcome always plays
  URBS_LOG_INFO("music: stinger {}", id);
  audio_->play_stinger(file(id), m_.duck_db);
}

}  // namespace urbs::game
