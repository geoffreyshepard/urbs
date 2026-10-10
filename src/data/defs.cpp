#include "data/defs.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace urbs::data {

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

struct Merged {
  std::vector<std::string> order;
  std::unordered_map<std::string, json> by_id;
  std::unordered_map<std::string, std::string> source;
};

void merge_file(Merged& m, const fs::path& path) {
  if (!fs::exists(path)) return;
  std::ifstream f(path);
  json arr;
  try {
    arr = json::parse(f, nullptr, true, /*ignore_comments=*/true);
  } catch (const json::parse_error& e) {
    throw std::runtime_error(path.string() + ": " + e.what());
  }
  if (!arr.is_array()) throw std::runtime_error(path.string() + ": top level must be an array");
  for (auto& obj : arr) {
    if (!obj.contains("id") || !obj["id"].is_string())
      throw std::runtime_error(path.string() + ": entry without string 'id': " + obj.dump().substr(0, 80));
    std::string id = obj["id"];
    auto it = m.by_id.find(id);
    if (it == m.by_id.end()) {
      m.order.push_back(id);
      m.by_id[id] = obj;
    } else {
      it->second.merge_patch(obj);
    }
    m.source[id] = path.string();
  }
}

Merged load_kind(const std::string& root, const std::vector<std::string>& mods, const char* file) {
  Merged m;
  merge_file(m, fs::path(root) / "defs" / file);
  for (auto& mod : mods) merge_file(m, fs::path(mod) / "defs" / file);
  return m;
}

[[noreturn]] void fail(const Merged& m, const std::string& id, const std::string& what) {
  auto it = m.source.find(id);
  throw std::runtime_error((it != m.source.end() ? it->second : std::string("?")) + ": '" + id + "': " + what);
}

template <typename T>
T get_or(const json& j, const char* key, T def) {
  auto it = j.find(key);
  if (it == j.end() || it->is_null()) return def;
  return it->get<T>();
}

GoodCategory parse_category(const Merged& m, const std::string& id, const std::string& s) {
  if (s == "raw_food") return GoodCategory::RawFood;
  if (s == "food") return GoodCategory::Food;
  if (s == "material") return GoodCategory::Material;
  if (s == "metal") return GoodCategory::Metal;
  if (s == "crafted") return GoodCategory::Crafted;
  if (s == "luxury") return GoodCategory::Luxury;
  if (s == "special") return GoodCategory::Special;
  fail(m, id, "unknown good category '" + s + "'");
}

BuildingKind parse_kind(const Merged& m, const std::string& id, const std::string& s) {
  static const std::pair<const char*, BuildingKind> kKinds[] = {
      {"house", BuildingKind::House},         {"farm", BuildingKind::Farm},
      {"workplace", BuildingKind::Workplace}, {"storage", BuildingKind::Storage},
      {"market", BuildingKind::Market},       {"service", BuildingKind::Service},
      {"patrol", BuildingKind::Patrol},       {"water", BuildingKind::Water},
      {"decor", BuildingKind::Decor},         {"civic", BuildingKind::Civic},
      {"military", BuildingKind::Military},   {"monument", BuildingKind::Monument},
      {"infrastructure", BuildingKind::Infrastructure}, {"trade_post", BuildingKind::TradePost}};
  for (auto& [n, k] : kKinds)
    if (s == n) return k;
  fail(m, id, "unknown building kind '" + s + "'");
}

u8 parse_classes(const json& arr) {
  u8 mask = 0;
  for (auto& c : arr) {
    std::string s = c;
    if (s == "patrician") mask |= class_bit(0);
    else if (s == "eques") mask |= class_bit(1);
    else if (s == "pleb") mask |= class_bit(2);
    else if (s == "libertus") mask |= class_bit(3);
    else if (s == "slave") mask |= class_bit(4);
    else if (s == "soldier") mask |= class_bit(5);
  }
  return mask;
}

u8 parse_resource(const Merged& m, const std::string& id, const std::string& s) {
  static const char* kRes[] = {"", "clay", "stone", "iron", "silver", "marble", "limestone", "pozzolana", "fish"};
  for (u8 i = 0; i < 9; ++i)
    if (s == kRes[i]) return i;
  fail(m, id, "unknown resource '" + s + "'");
}

}  // namespace

GoodId Defs::good(const std::string& id) const {
  auto it = good_ids.find(id);
  return it == good_ids.end() ? GoodId{} : GoodId{it->second};
}
RecipeId Defs::recipe(const std::string& id) const {
  auto it = recipe_ids.find(id);
  return it == recipe_ids.end() ? RecipeId{} : RecipeId{it->second};
}
ServiceId Defs::service(const std::string& id) const {
  auto it = service_ids.find(id);
  return it == service_ids.end() ? ServiceId{} : ServiceId{it->second};
}
BuildingDefId Defs::building(const std::string& id) const {
  auto it = building_ids.find(id);
  return it == building_ids.end() ? BuildingDefId{} : BuildingDefId{it->second};
}

Defs load_defs(const std::string& data_root, const std::vector<std::string>& mods) {
  Defs d;
  // Goods
  Merged mg = load_kind(data_root, mods, "goods.json");
  for (auto& id : mg.order) {
    const json& j = mg.by_id[id];
    GoodDef g;
    g.id = id;
    g.name = get_or<std::string>(j, "name", id);
    g.icon = get_or<std::string>(j, "icon", "goods/" + id);
    g.category = parse_category(mg, id, get_or<std::string>(j, "category", "material"));
    g.food = get_or<bool>(j, "food", false);
    g.base_price = get_or<u16>(j, "price", 5);
    g.nutrition = get_or<u8>(j, "nutrition", g.food ? 1 : 0);
    g.unlock = get_or<u8>(j, "unlock", 1);
    d.good_ids[id] = static_cast<u16>(d.goods.size());
    d.goods.push_back(std::move(g));
  }
  auto goods_list = [&](const Merged& m, const std::string& id, const json& j, const char* key) {
    std::vector<GoodAmount> out;
    auto it = j.find(key);
    if (it == j.end()) return out;
    for (auto& [gname, amt] : it->items()) {
      if (gname == "denarii") continue;
      GoodId g = d.good(gname);
      if (!g) fail(m, id, std::string(key) + ": unknown good '" + gname + "'");
      out.push_back({g, amt.get<u16>()});
    }
    return out;
  };

  // Recipes
  Merged mr = load_kind(data_root, mods, "recipes.json");
  for (auto& id : mr.order) {
    const json& j = mr.by_id[id];
    RecipeDef r;
    r.id = id;
    r.inputs = goods_list(mr, id, j, "inputs");
    r.outputs = goods_list(mr, id, j, "outputs");
    r.work = get_or<u16>(j, "work", 100);
    r.coins = get_or<i32>(j, "coins", 0);
    if (r.outputs.empty() && r.coins == 0) fail(mr, id, "recipe has no outputs");
    d.recipe_ids[id] = static_cast<u16>(d.recipes.size());
    d.recipes.push_back(std::move(r));
  }

  // Services
  Merged ms = load_kind(data_root, mods, "services.json");
  for (auto& id : ms.order) {
    const json& j = ms.by_id[id];
    ServiceDef s;
    s.id = id;
    s.name = get_or<std::string>(j, "name", id);
    s.patrol = get_or<bool>(j, "patrol", false);
    s.visit_ticks = get_or<u16>(j, "visit_ticks", 50);
    s.range = get_or<u16>(j, "range", 60);
    s.quality = get_or<u8>(j, "quality", 100);
    static const std::pair<const char*, NeedKind> kNeeds[] = {
        {"food", NeedKind::Food},         {"water", NeedKind::Water},     {"rest", NeedKind::Rest},
        {"hygiene", NeedKind::Hygiene},   {"religion", NeedKind::Religion}, {"entertainment", NeedKind::Entertainment},
        {"health", NeedKind::Health},     {"education", NeedKind::Education}};
    std::string nk = get_or<std::string>(j, "need", "");
    for (auto& [n, k] : kNeeds)
      if (nk == n) s.need = k;
    d.service_ids[id] = static_cast<u16>(d.services.size());
    d.services.push_back(std::move(s));
  }

  // Buildings
  Merged mb = load_kind(data_root, mods, "buildings.json");
  for (auto& id : mb.order) {
    const json& j = mb.by_id[id];
    BuildingDef b;
    b.id = id;
    b.name = get_or<std::string>(j, "name", id);
    b.category = get_or<std::string>(j, "category", "misc");
    b.kind = parse_kind(mb, id, get_or<std::string>(j, "kind", "workplace"));
    if (j.contains("size")) {
      b.w = j["size"][0].get<u8>();
      b.h = j["size"][1].get<u8>();
    }
    if (b.w == 0 || b.h == 0 || b.w > 32 || b.h > 32) fail(mb, id, "size must be 1..32");
    b.sprite = get_or<std::string>(j, "sprite", "bld/" + id);
    b.painted = get_or<bool>(j, "painted", false);
    if (j.contains("min_size")) {
      b.min_w = j["min_size"][0].get<u8>();
      b.min_h = j["min_size"][1].get<u8>();
    }
    if (j.contains("max_size")) {
      b.max_w = j["max_size"][0].get<u8>();
      b.max_h = j["max_size"][1].get<u8>();
    }
    if (j.contains("cost")) {
      b.cost_denarii = get_or<i32>(j["cost"], "denarii", 0);
      b.cost_goods = goods_list(mb, id, j, "cost");
    }
    b.build_work = get_or<u16>(j, "build_work", 50);
    b.workers = get_or<u16>(j, "workers", 0);
    if (j.contains("worker_classes")) b.worker_classes = parse_classes(j["worker_classes"]);
    if (j.contains("recipe")) {
      b.recipe = d.recipe(j["recipe"].get<std::string>());
      if (!b.recipe) fail(mb, id, "unknown recipe '" + j["recipe"].get<std::string>() + "'");
    }
    if (j.contains("service")) {
      b.service = d.service(j["service"].get<std::string>());
      if (!b.service) fail(mb, id, "unknown service '" + j["service"].get<std::string>() + "'");
    }
    b.service_capacity = get_or<u16>(j, "service_capacity", 0);
    b.housing = get_or<u16>(j, "housing", 0);
    {
      std::string hl = get_or<std::string>(j, "house_line", "pleb");
      b.house_line = hl == "patrician" ? 1 : hl == "slave" ? 2 : 0;
    }
    b.storage = get_or<u16>(j, "storage", 0);
    if (j.contains("accepts"))
      for (auto& gname : j["accepts"]) {
        GoodId g = d.good(gname.get<std::string>());
        if (!g) fail(mb, id, "accepts: unknown good '" + gname.get<std::string>() + "'");
        b.accepts.push_back(g);
      }
    if (j.contains("desirability")) {
      b.desirability = j["desirability"][0].get<i8>();
      b.desirability_range = j["desirability"][1].get<u8>();
    }
    b.fire_risk = get_or<u8>(j, "fire_risk", 0);
    b.collapse_risk = get_or<u8>(j, "collapse_risk", 0);
    b.unlock = get_or<u8>(j, "unlock", 1);
    if (j.contains("resource")) b.resource = parse_resource(mb, id, j["resource"].get<std::string>());
    b.needs_water_adjacent = get_or<bool>(j, "needs_water", false);
    b.needs_road = get_or<bool>(j, "needs_road", b.kind != BuildingKind::Decor && b.kind != BuildingKind::Farm);
    b.water_radius = get_or<u16>(j, "water_radius", 0);
    b.patrol_range = get_or<u8>(j, "patrol_range", 0);
    {
      std::string layer = get_or<std::string>(j, "layer", "");
      b.layer = layer == "pipe" ? 1 : layer == "sewer" ? 2 : layer == "bridge" ? 3 : 0;
    }
    b.passable = get_or<bool>(j, "passable", false);
    b.needs_pipe = get_or<bool>(j, "needs_pipe", false);
    b.carts = get_or<bool>(j, "carts", false);
    b.crime_cut = get_or<u8>(j, "crime_cut", 0);
    b.favor_days = get_or<u8>(j, "favor_days", 0);
    if (j.contains("dedication")) {
      const json& dj = j["dedication"];
      b.dedication.favor = get_or<i8>(dj, "favor", 0);
      b.dedication.mood = get_or<i8>(dj, "mood", 0);
      b.dedication.days = get_or<u8>(dj, "days", 0);
      b.dedication.god = get_or<std::string>(dj, "god", "");
      b.dedication.god_mood = get_or<i8>(dj, "god_mood", 0);
      b.dedication.culture = get_or<u8>(dj, "culture", 0);
    }
    if (b.kind == BuildingKind::House && b.housing == 0) fail(mb, id, "house needs 'housing' > 0");
    if ((b.kind == BuildingKind::Service || b.kind == BuildingKind::Patrol) && !b.service)
      fail(mb, id, "service building needs 'service'");
    d.building_ids[id] = static_cast<u16>(d.buildings.size());
    d.buildings.push_back(std::move(b));
  }
  // Housing tiers
  {
    fs::path hp = fs::path(data_root) / "defs" / "housing.json";
    Merged mh = load_kind(data_root, mods, "housing.json");
    for (auto& id : mh.order) {
      const json& j = mh.by_id[id];
      HousingTierDef t;
      t.id = id;
      t.name = get_or<std::string>(j, "name", id);
      t.line = get_or<std::string>(j, "line", "pleb") == "patrician" ? 1 : 0;
      t.tier = get_or<u8>(j, "tier", 0);
      t.capacity = get_or<u16>(j, "capacity", 5);
      if (j.contains("requires"))
        for (auto& r : j["requires"]) {
          HousingReq q;
          if (r.contains("service")) {
            q.kind = HousingReq::Service;
            for (auto& s : r["service"]) {
              ServiceId sid = d.service(s.get<std::string>());
              if (!sid) fail(mh, id, "unknown service " + s.get<std::string>());
              q.services.push_back(sid);
            }
          } else if (r.contains("patrol")) {
            q.kind = HousingReq::Patrol;
            ServiceId sid = d.service(r["patrol"].get<std::string>());
            if (!sid) fail(mh, id, "unknown patrol service");
            q.services.push_back(sid);
          } else if (r.contains("food_kinds")) {
            q.kind = HousingReq::FoodKinds;
            q.value = r["food_kinds"].get<i32>();
          } else if (r.contains("food")) {
            q.kind = HousingReq::Food;
            GoodId g = d.good(r["food"].get<std::string>());
            if (!g) fail(mh, id, "unknown food good");
            q.goods.push_back(g);
          } else if (r.contains("goods")) {
            q.kind = HousingReq::Goods;
            for (auto& s : r["goods"]) {
              GoodId g = d.good(s.get<std::string>());
              if (!g) fail(mh, id, "unknown good " + s.get<std::string>());
              q.goods.push_back(g);
            }
          } else if (r.contains("desirability")) {
            q.kind = HousingReq::Desirability;
            q.value = r["desirability"].get<i32>();
          } else if (r.contains("sewer")) {
            q.kind = HousingReq::Sewer;
          } else if (r.contains("piped_water")) {
            q.kind = HousingReq::PipedWater;
          } else {
            fail(mh, id, "unknown requirement " + r.dump());
          }
          t.reqs.push_back(std::move(q));
        }
      d.housing.push_back(std::move(t));
    }
    std::sort(d.housing.begin(), d.housing.end(),
              [](const HousingTierDef& a, const HousingTierDef& b) { return a.line != b.line ? a.line < b.line : a.tier < b.tier; });
    (void)hp;
  }
  // Trade partners
  {
    Merged mt = load_kind(data_root, mods, "trade_partners.json");
    for (auto& id : mt.order) {
      const json& j = mt.by_id[id];
      TradePartnerDef t;
      t.id = id;
      t.name = get_or<std::string>(j, "name", id);
      t.sea = get_or<std::string>(j, "route", "land") == "sea";
      t.open_cost = get_or<i32>(j, "open_cost", 0);
      t.interval_days = get_or<u16>(j, "interval_days", 5);
      t.buys = goods_list(mt, id, j, "buys");
      t.sells = goods_list(mt, id, j, "sells");
      d.partners.push_back(std::move(t));
    }
  }
  // Traits
  {
    Merged mt = load_kind(data_root, mods, "traits.json");
    for (auto& id : mt.order) {
      const json& j = mt.by_id[id];
      TraitDef t;
      t.id = id;
      t.name = get_or<std::string>(j, "name", id);
      t.desc = get_or<std::string>(j, "desc", "");
      t.religion_rate = get_or<f32>(j, "religion_rate", 1);
      t.rest_rate = get_or<f32>(j, "rest_rate", 1);
      t.entertainment_rate = get_or<f32>(j, "entertainment_rate", 1);
      t.hygiene_rate = get_or<f32>(j, "hygiene_rate", 1);
      t.education_rate = get_or<f32>(j, "education_rate", 1);
      t.work = get_or<f32>(j, "work", 1);
      t.skill_rate = get_or<f32>(j, "skill_rate", 1);
      t.goal_rate = get_or<f32>(j, "goal_rate", 1);
      t.spend = get_or<f32>(j, "spend", 1);
      t.health = get_or<f32>(j, "health", 1);
      t.lifespan = get_or<f32>(j, "lifespan", 1);
      t.crime = get_or<f32>(j, "crime", 1);
      t.fertility = get_or<f32>(j, "fertility", 1);
      t.happiness = get_or<i8>(j, "happiness", 0);
      d.traits.push_back(std::move(t));
    }
  }
  // Gods
  {
    Merged mg2 = load_kind(data_root, mods, "gods.json");
    for (auto& id : mg2.order) {
      const json& j = mg2.by_id[id];
      GodDef g;
      g.id = id;
      g.name = get_or<std::string>(j, "name", id);
      g.domain = get_or<std::string>(j, "domain", "");
      g.blessing = get_or<std::string>(j, "blessing", "");
      g.wrath = get_or<std::string>(j, "wrath", "");
      g.blessing_text = get_or<std::string>(j, "blessing_text", "");
      g.wrath_text = get_or<std::string>(j, "wrath_text", "");
      g.festival_day = get_or<u8>(j, "festival_day", 0);
      d.gods.push_back(std::move(g));
    }
  }
  // Units
  {
    Merged mu = load_kind(data_root, mods, "units.json");
    for (auto& id : mu.order) {
      const json& j = mu.by_id[id];
      UnitDef u;
      u.id = id;
      u.name = get_or<std::string>(j, "name", id);
      u.sprite = get_or<std::string>(j, "sprite", "legionary");
      u.enemy = get_or<std::string>(j, "side", "city") == "enemy";
      u.hp = get_or<u16>(j, "hp", 100);
      u.attack = get_or<u16>(j, "attack", 10);
      u.defense = get_or<u16>(j, "defense", 5);
      u.speed = get_or<f32>(j, "speed", 0.09f);
      u.range = get_or<f32>(j, "range", 1.2f);
      u.size = get_or<u16>(j, "size", 32);
      u.upkeep = get_or<u16>(j, "upkeep", 3);
      u.gear = goods_list(mu, id, j, "gear");
      u.morale = get_or<u8>(j, "morale", 60);
      u.charge = get_or<f32>(j, "charge", 1.0f);
      u.siege = get_or<f32>(j, "siege", 0.5f);
      u.unlock = get_or<u8>(j, "unlock", 1);
      if (j.contains("trained_at"))
        for (auto& t : j["trained_at"]) u.trained_at.push_back(t.get<std::string>());
      d.units.push_back(std::move(u));
    }
  }
  // Techniques (civic research)
  {
    Merged mt = load_kind(data_root, mods, "techniques.json");
    for (auto& id : mt.order) {
      const json& j = mt.by_id[id];
      d.techniques.push_back(TechniqueDef{id, get_or<std::string>(j, "name", id), get_or<std::string>(j, "desc", ""),
                                          get_or<u16>(j, "cost", 50)});
    }
  }
  // Names
  {
    fs::path np = fs::path(data_root) / "defs" / "names.json";
    if (fs::exists(np)) {
      std::ifstream f(np);
      json j = json::parse(f);
      d.names.praenomina = j.value("praenomina", std::vector<std::string>{});
      d.names.nomina = j.value("nomina", std::vector<std::string>{});
      d.names.cognomina = j.value("cognomina", std::vector<std::string>{});
      d.names.slave = j.value("slave", std::vector<std::string>{});
    }
    if (d.names.praenomina.empty()) d.names.praenomina = {"Gaius"};
    if (d.names.nomina.empty()) d.names.nomina = {"Julius"};
    if (d.names.cognomina.empty()) d.names.cognomina = {"Rufus"};
    if (d.names.slave.empty()) d.names.slave = {"Syrus"};
  }
  return d;
}

}  // namespace urbs::data
