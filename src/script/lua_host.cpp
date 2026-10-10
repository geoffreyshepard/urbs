#include "script/lua_host.hpp"

#include <algorithm>
#include <sol/sol.hpp>
#include <sstream>
#include <vector>

#include "core/log.hpp"

namespace urbs::script {

struct LuaHost::Impl {
  sol::state lua;
};

namespace {
i32 side_of(const std::string& s) {
  if (s == "north") return 0;
  if (s == "east") return 1;
  if (s == "south") return 2;
  if (s == "west") return 3;
  return -1;
}
}  // namespace

LuaHost::LuaHost(sim::World& w, std::string source, std::string chunk_name)
    : impl_(std::make_unique<Impl>()), w_(w), source_(std::move(source)), name_(std::move(chunk_name)) {
  sol::state& L = impl_->lua;
  L.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
  // Sandbox: no file/OS access, no loading of external code.
  for (const char* banned : {"dofile", "loadfile", "load", "require", "collectgarbage"}) L[banned] = sol::lua_nil;
  sim::World& W = w_;
  const data::Defs& D = *w.defs;
  sol::table u = L.create_named_table("urbs");
  u["state"] = L.create_table();
  u["message"] = [&W](const std::string& title, const std::string& text, sol::optional<int> advisor) {
    W.post_message(static_cast<u8>(advisor.value_or(0)), title, text);
  };
  u["choice"] = [&W](const std::string& title, const std::string& text, sol::table opts, const std::string& cb,
                     sol::optional<int> advisor) {
    std::vector<std::string> o;
    for (size_t k = 1; k <= opts.size(); ++k) o.push_back(opts.get<std::string>(k));
    return W.post_message(static_cast<u8>(advisor.value_or(0)), title, text, o, cb);
  };
  u["population"] = [&W]() { return W.population(); };
  u["treasury"] = [&W]() { return static_cast<double>(W.econ.treasury); };
  u["day"] = [&W]() { return static_cast<double>(sim::day_index(W.now)); };
  u["mission_day"] = [&W]() { return W.mission_days; };
  u["year"] = [&W]() { return static_cast<double>(sim::year_index(W.now)); };
  u["season"] = [&W]() { return std::string(sim::season_name(sim::season(W.now))); };
  u["rating"] = [&W](const std::string& r) -> int {
    if (r == "culture") return W.ratings.culture;
    if (r == "prosperity") return W.ratings.prosperity;
    if (r == "security") return W.ratings.security;
    if (r == "favor") return W.favor;
    return 0;
  };
  u["favor"] = [&W]() { return W.favor; };
  u["change_favor"] = [&W](int d) { W.change_favor(d); };
  u["happiness"] = [&W]() { return static_cast<int>(W.avg_happiness); };
  u["stock"] = [&W, &D](const std::string& g) -> int {
    GoodId id = D.good(g);
    return id.valid() ? static_cast<int>(W.econ.city_stock[id.v]) : 0;
  };
  u["give"] = [&W, &D](const std::string& g, int n) {
    GoodId id = D.good(g);
    if (!id.valid() || n <= 0) return 0;
    int left = n;
    W.buildings.for_each([&](BuildingId bid, const sim::Building& b) {
      if (left <= 0 || b.inv == econ::kNoInv || b.state != sim::BState::Active) return;
      const auto k = D.b(b.def).kind;
      if (k != data::BuildingKind::Storage) return;
      const u16 put = static_cast<u16>(std::min<u32>(static_cast<u32>(left), W.econ.free_space(b.inv)));
      W.add_stock(bid, id, put);
      left -= put;
    });
    return n - left;
  };
  u["add_money"] = [&W](double n) { W.earn(static_cast<i64>(n), econ::Money::Tribute); };
  u["take_money"] = [&W](double n) { W.spend(static_cast<i64>(n), econ::Money::Other); };
  u["request"] = [&W, &D](sol::table t) -> int {
    const std::string kind = t.get_or<std::string>("kind", "goods");
    const u8 k = kind == "denarii" ? 1 : kind == "troops" ? 2 : 0;
    GoodId g = k == 0 ? D.good(t.get_or<std::string>("good", "wheat")) : GoodId{};
    if (k == 0 && !g.valid()) return 0;
    return W.add_request(k, g, static_cast<u16>(t.get_or("amount", 50)), static_cast<u16>(t.get_or("days", 12)),
                         t.get_or("reward", 0), static_cast<i8>(t.get_or("favor", 8)), static_cast<i8>(t.get_or("penalty", -12)));
  };
  u["raid"] = [&W, &D](sol::table t) {
    W.spawn_raid(D.unit(t.get_or<std::string>("unit", "barbarian_warrior")), static_cast<u16>(t.get_or("count", 2)),
                 side_of(t.get_or<std::string>("side", "")));
  };
  u["trigger"] = [&W](const std::string& name, sol::optional<int> value) {
    static const char* kTrig[] = {"fire",  "collapse", "plague", "riot",   "revolt", "flood", "earthquake",
                                  "drought", "storm",  "blight", "blessing", "wrath", "raid"};
    for (u16 k = 0; k < 13; ++k)
      if (name == kTrig[k]) W.trigger(static_cast<sim::TriggerId>(k), -1, -1, value.value_or(0));
  };
  // The Republic becomes an Empire: Rome's favor is now the Emperor's, and the imperial cult begins.
  u["set_empire"] = [&W, &D]() {
    W.empire = 1;
    const u8 divus = D.god("divus");
    if (divus < W.god_available.size()) W.god_available[divus] = 1;
  };
  u["unlock"] = [&W, &D](const std::string& b) {
    BuildingDefId d = D.building(b);
    if (d.valid()) W.unlocked[d.v] = 1;
  };
  u["lock"] = [&W, &D](const std::string& b) {
    BuildingDefId d = D.building(b);
    if (d.valid()) W.unlocked[d.v] = 0;
  };
  u["count"] = [&W, &D](const std::string& b) -> int {
    BuildingDefId d = D.building(b);
    return d.valid() ? static_cast<int>(W.count_active(d)) : 0;
  };
  u["god_mood"] = [&W, &D](const std::string& g) -> int {
    const u8 k = D.god(g);
    return k < W.god_mood.size() ? W.god_mood[k] : 0;
  };
  u["change_god"] = [&W, &D](const std::string& g, int d) {
    const u8 k = D.god(g);
    if (k < W.god_mood.size()) W.god_mood[k] = static_cast<u8>(std::clamp(W.god_mood[k] + d, 0, 100));
  };
  u["worship"] = [&W, &D](const std::string& g) {
    const u8 k = D.god(g);
    if (k < W.god_available.size()) W.god_available[k] = 1;
  };
  u["open_route"] = [&W, &D](const std::string& p) {
    for (size_t k = 0; k < D.partners.size(); ++k)
      if (D.partners[k].id == p) {
        W.routes[k].available = 1;
        W.routes[k].open = 1;
        W.routes[k].next_visit = W.now + kTicksPerHour * 6;
      }
  };
  u["offer_route"] = [&W, &D](const std::string& p) {
    for (size_t k = 0; k < D.partners.size(); ++k)
      if (D.partners[k].id == p) W.routes[k].available = 1;
  };
  u["festival_day"] = [&W, &D](const std::string& g) -> bool {
    const u8 k = D.god(g);
    return k < D.gods.size() && sim::day_of_year(W.now) == D.gods[k].festival_day;
  };
  u["win"] = [&W]() { W.mission = sim::MissionState::Won; };
  u["lose"] = [&W](const std::string& reason) {
    W.mission = sim::MissionState::Lost;
    W.lost_reason = reason;
  };
  u["random"] = [&W](int n) -> int {
    return n > 0 ? static_cast<int>(W.stream(rng::StreamId::Events).below(static_cast<u32>(n))) + 1 : 0;
  };
  u["log"] = [](const std::string& s) { URBS_LOG_INFO("[lua] {}", s); };
}

LuaHost::~LuaHost() = default;

bool LuaHost::start() {
  auto r = impl_->lua.safe_script(source_, sol::script_pass_on_error, name_);
  if (!r.valid()) {
    sol::error e = r;
    error_ = e.what();
    URBS_LOG_ERROR("script {}: {}", name_, error_);
    return false;
  }
  return true;
}

namespace {
template <typename... Args>
void call(sol::state& L, const char* fn, std::string& err, Args&&... args) {
  sol::protected_function f = L[fn];
  if (!f.valid()) return;
  auto r = f(std::forward<Args>(args)...);
  if (!r.valid()) {
    sol::error e = r;
    err = e.what();
    URBS_LOG_ERROR("script {}: {}", fn, err);
  }
}
}  // namespace

void LuaHost::on_start() { call(impl_->lua, "on_start", error_); }
void LuaHost::on_day() { call(impl_->lua, "on_day", error_, static_cast<double>(w_.mission_days)); }
void LuaHost::on_choice(const std::string& cb, i32 option) { call(impl_->lua, cb.c_str(), error_, option); }
void LuaHost::on_request(u16 id, bool ok) { call(impl_->lua, "on_request", error_, static_cast<int>(id), ok); }

std::string LuaHost::export_state() const {
  // Sorted "key\ttype\tvalue" lines, so saves are deterministic.
  std::vector<std::string> lines;
  sol::table st = impl_->lua["urbs"]["state"];
  for (auto& [k, v] : st) {
    if (k.get_type() != sol::type::string) continue;
    const std::string key = k.as<std::string>();
    switch (v.get_type()) {
      case sol::type::number: lines.push_back(key + "\tn\t" + std::to_string(v.as<double>())); break;
      case sol::type::boolean: lines.push_back(key + "\tb\t" + (v.as<bool>() ? "1" : "0")); break;
      case sol::type::string: lines.push_back(key + "\ts\t" + v.as<std::string>()); break;
      default: break;
    }
  }
  std::sort(lines.begin(), lines.end());
  std::string out;
  for (auto& l : lines) out += l + "\n";
  return out;
}

void LuaHost::import_state(const std::string& s) {
  sol::table st = impl_->lua["urbs"]["state"];
  std::istringstream in(s);
  std::string line;
  while (std::getline(in, line)) {
    const size_t a = line.find('\t'), b = line.find('\t', a + 1);
    if (a == std::string::npos || b == std::string::npos) continue;
    const std::string key = line.substr(0, a), type = line.substr(a + 1, b - a - 1), val = line.substr(b + 1);
    if (type == "n") st[key] = std::stod(val);
    else if (type == "b") st[key] = val == "1";
    else st[key] = val;
  }
}

}  // namespace urbs::script
