#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>

#include "data/defs.hpp"
#include "game/sfx_rules.hpp"
#include "sim/scenario.hpp"
#include "sim/world.hpp"

using namespace urbs;
using namespace urbs::game;

namespace {
const std::string kRoot = std::string(URBS_SOURCE_DIR) + "/";

SfxManifest manifest() {
  SfxManifest m;
  m.load(kRoot + "data/defs/sfx.json");
  return m;
}

const data::Defs& defs() {
  static data::Defs d = data::load_defs(kRoot + "data");
  return d;
}

std::unique_ptr<sim::World> town() {
  auto w = std::make_unique<sim::World>();
  auto spec = sim::load_scenario(kRoot + "tests/scenarios/small_town.json");
  sim::setup_scenario(*w, defs(), spec);
  for (int t = 0; t < 50; ++t) w->tick();
  return w;
}

struct Cues {
  SfxCue c[SfxWatch::kMaxCues];
  int n = 0;
  [[nodiscard]] const SfxCue* find(std::string_view ev) const {
    for (int i = 0; i < n; ++i)
      if (ev == c[i].event) return &c[i];
    return nullptr;
  }
};

Cues poll(SfxWatch& watch, const sim::World& w, i64 coin_min = 25) {
  Cues out;
  out.n = watch.poll(w, coin_min, 0, 0, out.c);
  return out;
}
}  // namespace

TEST_CASE("Sound effects: every event the game plays is in the manifest, with rendered files") {
  const SfxManifest m = manifest();
  // The ids src/game asks for (sfx.cpp, app.cpp, app_ui.cpp) and SfxWatch's cues.
  for (const char* id : {"ui_click", "place_building", "place_road", "demolish", "coin", "message", "alert", "fire",
                         "collapse", "battle", "raid", "legion", "festival", "mission_won"}) {
    INFO(id);
    CHECK(m.index(id) >= 0);
  }
  for (const char* id : {"fire", "collapse", "battle"}) CHECK(m.events[static_cast<size_t>(m.index(id))].positional);
  for (const SfxEvent& e : m.events) {
    INFO(e.id);
    CHECK(e.gain > 0.0f);
    CHECK(e.gain <= 1.0f);
    CHECK(e.cooldown > 0.0f);
    CHECK(e.max_playing >= 1);
    for (const std::string& f : e.files) CHECK(std::filesystem::exists(kRoot + "assets/sfx/" + f + ".ogg"));
  }
  REQUIRE_FALSE(m.ambient.file.empty());
  CHECK(std::filesystem::exists(kRoot + "assets/sfx/" + m.ambient.file + ".ogg"));
}

TEST_CASE("Sound effects: a sound in the city is heard on or near the screen, quieter further off") {
  const SfxPositional p = manifest().positional;
  const SfxPlacement centre = place_sound(p, 0, 0);
  CHECK(centre.gain == doctest::Approx(1.0f));
  CHECK(centre.pan == doctest::Approx(0.0f));
  const SfxPlacement right = place_sound(p, 0.5f, 0);
  CHECK(right.pan > 0.0f);
  CHECK(place_sound(p, -0.5f, 0).pan < 0.0f);
  const SfxPlacement off = place_sound(p, (p.full + p.max) / 2, 0);
  CHECK(off.gain < 1.0f);
  CHECK(off.gain > p.far_gain);
  CHECK(place_sound(p, 0, p.max + 0.1f).gain == 0.0f);
}

TEST_CASE("Sound effects: the ambient bed follows the people on screen") {
  const SfxAmbient a = manifest().ambient;
  CHECK(ambient_level(a, 0) == 0.0f);
  CHECK(ambient_level(a, a.people_min) == 0.0f);
  const f32 some = ambient_level(a, (a.people_min + a.people_full) / 2);
  CHECK(some > 0.0f);
  CHECK(some < 1.0f);
  CHECK(ambient_level(a, a.people_full) == doctest::Approx(1.0f));
  CHECK(ambient_level(a, a.people_full * 10) == doctest::Approx(1.0f));
}

TEST_CASE("Sound effects: the watcher hears what changed in the city, once") {
  auto w = town();
  SfxWatch watch;
  watch.reset(*w);
  CHECK(poll(watch, *w).n == 0);  // nothing new: silence

  // A fire is heard at the building; the same alert is not heard twice.
  BuildingId burning;
  w->buildings.for_each([&](BuildingId id, const sim::Building& b) {
    if (!burning.valid() && b.state == sim::BState::Active) burning = id;
  });
  REQUIRE(burning.valid());
  w->tick();
  poll(watch, *w);  // whatever the tick itself brought
  w->tick();
  w->add_alert(sim::AlertKind::Fire, burning);
  Cues c = poll(watch, *w);
  const SfxCue* fire = c.find("fire");
  REQUIRE(fire != nullptr);
  CHECK(fire->at);
  const sim::Building& b = w->buildings[burning];
  CHECK(fire->x >= static_cast<f32>(b.x));
  CHECK(fire->x <= static_cast<f32>(b.x + b.w));
  CHECK(poll(watch, *w).find("fire") == nullptr);

  // A letter and an alert in the same look: only the letter's sound.
  w->tick();
  sim::Message m;
  m.id = w->next_message_id++;
  m.t = w->now;
  m.title = "test";
  w->messages.push_back(m);
  w->add_alert(sim::AlertKind::Riot, BuildingId{});
  c = poll(watch, *w);
  CHECK(c.find("message") != nullptr);
  CHECK(c.find("alert") == nullptr);

  // A festival starting, trade income, a mission won.
  w->festival_until = w->now + 1000;
  w->econ.money.add(econ::Money::Exports, 400);
  w->mission = sim::MissionState::Won;
  c = poll(watch, *w);
  CHECK(c.find("festival") != nullptr);
  CHECK(c.find("coin") != nullptr);
  CHECK(c.find("mission_won") != nullptr);
  c = poll(watch, *w);
  CHECK(c.find("festival") == nullptr);
  CHECK(c.find("coin") == nullptr);
  CHECK(c.find("mission_won") == nullptr);

  // Small change stays quiet; taxes never clink.
  w->econ.money.add(econ::Money::Exports, 5);
  w->econ.money.add(econ::Money::Taxes, 5000);
  CHECK(poll(watch, *w).find("coin") == nullptr);
}

TEST_CASE("Sound effects: a reset hears nothing that was already there") {
  auto w = town();
  w->add_alert(sim::AlertKind::Raid, BuildingId{});
  w->festival_until = w->now + 1000;
  SfxWatch watch;
  watch.reset(*w);
  const Cues c = poll(watch, *w);
  CHECK(c.find("raid") == nullptr);
  CHECK(c.find("festival") == nullptr);
}
