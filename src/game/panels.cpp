#include "game/panels.hpp"

#include <imgui.h>

#include <algorithm>

#include "game/advisors.hpp"
#include "game/ui_icons.hpp"

namespace urbs::game {

using data::HousingReq;

std::string requirement_text(const data::Defs& defs, const HousingReq& r) {
  auto list_services = [&]() {
    std::string s;
    for (size_t k = 0; k < r.services.size(); ++k) s += (k ? " or " : "") + defs.services[r.services[k].v].name;
    return s;
  };
  auto list_goods = [&]() {
    std::string s;
    for (size_t k = 0; k < r.goods.size(); ++k) s += (k ? ", " : "") + defs.goods[r.goods[k].v].name;
    return s;
  };
  switch (r.kind) {
    case HousingReq::Service: return "access to " + list_services();
    case HousingReq::Patrol: return "patrols by " + list_services();
    case HousingReq::FoodKinds: return std::to_string(r.value) + " kinds of food";
    case HousingReq::Food: return list_goods() + " in the larder";
    case HousingReq::Goods: return "household goods: " + list_goods();
    case HousingReq::Desirability: return "a more desirable neighbourhood (" + std::to_string(r.value) + "+)";
    case HousingReq::Sewer: return "a sewer connection";
    case HousingReq::PipedWater: return "piped water";
  }
  return "?";
}

std::string alert_text(const sim::World& w, const sim::Alert& a) {
  const auto& D = *w.defs;
  std::string bname = a.b.valid() && w.buildings.alive(a.b) ? D.b(w.buildings[a.b].def).name : "";
  switch (a.kind) {
    case sim::AlertKind::HouseDevolved: {
      std::string s = "A house devolved";
      if (a.b.valid() && w.buildings.alive(a.b)) {
        const auto& b = w.buildings[a.b];
        if (const auto* t = D.tier(D.b(b.def).house_line, static_cast<u8>(b.tier + 1)))
          if (a.arg < t->reqs.size()) s += ": it lost " + requirement_text(D, t->reqs[a.arg]);
      }
      return s;
    }
    case sim::AlertKind::Immigrants: return std::to_string(a.arg) + " immigrants arrived today";
    case sim::AlertKind::Emigrants: return std::to_string(a.arg) + " citizens left the city";
    case sim::AlertKind::NoWorkers: return bname + " has no workers";
    case sim::AlertKind::InputsMissing:
      return bname + " is idle: no " + (a.arg < D.goods.size() ? D.goods[a.arg].name : std::string("inputs"));
    case sim::AlertKind::MarketEmpty: return bname + " has nothing to sell";
    case sim::AlertKind::Homeless: return std::to_string(a.arg) + " citizens are homeless";
    case sim::AlertKind::Unemployment: return "Unemployment is high (" + std::to_string(a.arg) + "%)";
    case sim::AlertKind::NoRoad: return bname + " has no road access";
    case sim::AlertKind::StorageFull: return bname + " is full";
    case sim::AlertKind::TreasuryLow: return "The treasury is nearly empty";
    case sim::AlertKind::Fire: return "Fire! " + bname + " is burning";
    case sim::AlertKind::Collapse: return bname + " collapsed";
    case sim::AlertKind::Disease: return "Disease is spreading near " + bname;
    case sim::AlertKind::Crime: return "Crime near " + bname;
    case sim::AlertKind::Riot: return "Riot!";
    case sim::AlertKind::Revolt: return "Servile revolt!";
    case sim::AlertKind::Dedicated: return bname + " has been dedicated! The city celebrates";
    case sim::AlertKind::Raid: return "Raiders are approaching! (" + std::to_string(a.arg) + " warbands)";
    case sim::AlertKind::GodAngry: {
      const u16 g = a.arg & 0xFF;
      if (g >= D.gods.size()) return "The gods stir";
      return (a.arg & 0x100) ? D.gods[g].blessing_text : D.gods[g].wrath_text;
    }
    case sim::AlertKind::Request: return "A request from Rome";
    case sim::AlertKind::Event:
      if (a.arg == 1000) return "Drought! The fields and wells dry up.";
      if (a.arg == 1001) return "Flood! Low ground near the water is inundated.";
      if (a.arg == 1002) return "Earthquake!";
      if (a.arg == 1003) return "Blight withers the crops.";
      if (a.arg == 1100) return "Not enough weapons and armor in the barracks to raise that unit.";
      if (a.arg == 1101) return "Not enough recruits: too few men of fighting age are free.";
      if (a.arg == 1102) return "That unit cannot be trained here (cavalry at stables, ballistae at a siege workshop, legions at barracks).";
      if (a.arg == 1200) return "The legion returns victorious with plunder and captives!";
      if (a.arg == 1201) return "The campaign failed; the survivors return.";
      if (a.arg < D.gods.size()) return "A festival is held in honour of " + D.gods[a.arg].name;
      return "Event";
    default: return "Event";
  }
}

static void need_bar(const char* label, u8 v) {
  const float f = static_cast<float>(v) / 255.0f;
  ImVec4 col = f > 0.6f ? ImVec4(0.3f, 0.75f, 0.3f, 1) : f > 0.35f ? ImVec4(0.85f, 0.7f, 0.2f, 1) : ImVec4(0.85f, 0.25f, 0.2f, 1);
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
  ImGui::ProgressBar(f, ImVec2(140, 0), "");
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::TextUnformatted(label);
}

void draw_citizen_panel(sim::World& w, Selection& sel) {
  if (!sel.citizen.valid()) return;
  if (!w.citizens.alive(sel.citizen)) {
    sel.citizen = CitizenId{};
    return;
  }
  const u32 i = sel.citizen.index();
  const auto& C = w.citizens;
  bool open = true;
  ImGui::SetNextWindowPos({1220, 300}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({360, 520}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Citizen", &open);
  ImGui::Text("%s", w.citizen_name(i).c_str());
  ImGui::Text("%s, %u years, %s", C.sex[i] ? "female" : "male", C.age_years(i, w.now), sim::class_name(C.cls[i]));
  ImGui::Checkbox("Follow", &sel.follow);
  {
    std::string tr;
    for (int k = 0; k < sim::kMaxTraits; ++k) {
      const u8 t = C.traits[static_cast<size_t>(i) * sim::kMaxTraits + k];
      if (t < w.defs->traits.size()) tr += (tr.empty() ? "" : ", ") + w.defs->traits[t].name;
    }
    if (!tr.empty()) ImGui::TextWrapped("Traits: %s", tr.c_str());
    if (C.goal[i]) ImGui::TextWrapped("Life goal: %s", sim::goal_text(static_cast<sim::Goal>(C.goal[i])));
    auto person = [&](const char* label, CitizenId c) {
      if (!c.valid() || !w.citizens.alive(c)) return;
      std::string t = std::string(label) + ": " + w.citizen_name(c.index()) + "##" + label;
      if (ImGui::Selectable(t.c_str())) sel.citizen = c;
    };
    person("Spouse", C.spouse[i]);
    person("Mother", C.mother[i]);
    person("Father", C.father[i]);
  }
  ImGui::Separator();
  ImGui::TextWrapped("Now: %s", sim::activity_name(C.activity[i]));
  if (C.activity[i] == sim::Activity::Walk && C.next[i] != sim::Activity::None)
    ImGui::TextWrapped("Walking, then %s%s%s", sim::activity_name(C.next[i]),
                       C.dest[i].valid() && w.buildings.alive(C.dest[i]) ? " at " : "",
                       C.dest[i].valid() && w.buildings.alive(C.dest[i]) ? w.defs->b(w.buildings[C.dest[i]].def).name.c_str() : "");
  if (C.reason[i] != sim::Reason::None) ImGui::TextWrapped("Why: %s", sim::reason_text(C.reason[i]));
  if (C.carrying[i]) ImGui::Text("Carrying %u %s", C.carry_amount[i], w.defs->goods[C.carrying[i] - 1u].name.c_str());
  ImGui::Separator();
  auto link = [&](const char* label, BuildingId b) {
    if (!b.valid() || !w.buildings.alive(b)) {
      ImGui::Text("%s: none", label);
      return;
    }
    std::string t = std::string(label) + ": " + w.defs->b(w.buildings[b].def).name + "##" + label;
    if (ImGui::Selectable(t.c_str())) sel.building = b;
  };
  link("Home", C.home[i]);
  link("Work", C.work[i]);
  if (C.household[i].valid() && w.households.ids.alive(C.household[i])) {
    const auto& h = w.households[C.household[i]];
    ImGui::Text("Household: %u members, food %u, water %u, %u kinds of food", h.members, w.household_food(h), h.water,
                w.food_kinds(h));
  }
  ImGui::Separator();
  ImGui::TextUnformatted("Needs");
  for (int n = 0; n < sim::kNeedCount; ++n) need_bar(sim::need_name(static_cast<sim::Need>(n)), C.need(i, static_cast<sim::Need>(n), w.now));
  if (ImGui::TreeNode("Skills")) {
    const char* sk[] = {"farming", "crafting", "building", "trading", "soldiering", "scholarship"};
    for (int k = 0; k < sim::kSkillCount; ++k)
      ImGui::Text("%s %u", sk[k], C.skills[static_cast<size_t>(i) * sim::kSkillCount + k]);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Memories")) {
    for (int k = 0; k < sim::kMemoryEntries; ++k) {
      const int idx = (C.memory_head[i] + sim::kMemoryEntries - 1 - k) % sim::kMemoryEntries;
      const auto& m = C.memory[static_cast<size_t>(i) * sim::kMemoryEntries + idx];
      if (m.kind == 0) continue;
      ImGui::Text("Day %u: %s", m.day + 1, sim::memory_text(static_cast<sim::Memory>(m.kind)));
    }
    ImGui::TreePop();
  }
  ImGui::Separator();
  auto p = w.happiness_parts(i);
  ImGui::Text("Happiness: %d (yesterday %u)", std::clamp(p.total(), 0, 255), C.happiness[i]);
  struct Part {
    const char* n;
    i16 v;
  } parts[] = {{"food", p.food},       {"water", p.water},          {"rest", p.rest},     {"hygiene", p.hygiene},
               {"religion", p.religion}, {"entertainment", p.entertainment}, {"food variety", p.variety},
               {"housing", p.housing}, {"employment", p.employment}, {"taxes", p.taxes},  {"gods", p.gods},
               {"life events", p.events}, {"health", p.health}, {"education", p.education},
               {"prestige", p.prestige}, {"safety", p.safety}, {"bondage", p.freedom}, {"temperament", p.traits}};
  for (const Part& pt : parts)
    if (pt.v != 0) {
      ImGui::TextColored(pt.v > 0 ? ImVec4(0.5f, 0.9f, 0.5f, 1) : ImVec4(0.95f, 0.5f, 0.4f, 1), "  %+d %s", pt.v, pt.n);
    }
  ImGui::End();
  if (!open) sel.citizen = CitizenId{};
}

void draw_building_panel(sim::World& w, Selection& sel) {
  if (!sel.building.valid()) return;
  if (!w.buildings.alive(sel.building)) {
    sel.building = BuildingId{};
    return;
  }
  const auto& b = w.buildings[sel.building];
  const auto& D = *w.defs;
  const auto& d = D.b(b.def);
  bool open = true;
  ImGui::SetNextWindowPos({860, 300}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({350, 480}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Building", &open);
  ImGui::Text("%s", d.name.c_str());
  const char* states[] = {"under construction", "active", "burning", "ruined"};
  ImGui::Text("%s", states[static_cast<int>(b.state)]);
  if (b.state == sim::BState::Construction) {
    ImGui::ProgressBar(b.build_needed ? static_cast<float>(b.build_progress) / static_cast<float>(b.build_needed) : 0.0f);
    const bool monument = d.kind == data::BuildingKind::Monument;
    if (monument)
      ImGui::Text("Stage %u of %u: each stage needs its share of the materials", w.construction_stage(b) + 1,
                  sim::World::kMonumentStages);
    for (const auto& c : d.cost_goods)
      ImGui::Text("  %s: %u / %u%s", D.goods[c.good.v].name.c_str(), b.inv != econ::kNoInv ? w.econ.amt(b.inv, c.good) : 0,
                  c.amount, monument ? (" (this stage " + std::to_string(w.materials_needed_now(b, c.amount)) + ")").c_str() : "");
    if (!w.site_ready(b)) ImGui::TextColored({0.7f, 0.3f, 0.2f, 1}, "Waiting for materials; builders will come when they arrive.");
  }
  if (d.kind == data::BuildingKind::Monument) {
    const auto& ded = d.dedication;
    std::string fx;
    if (ded.favor) fx += " favor +" + std::to_string(ded.favor) + ",";
    if (ded.mood) fx += " happiness +" + std::to_string(ded.mood) + " for " + std::to_string(ded.days) + " days,";
    if (!ded.god.empty()) fx += " pleases " + ded.god + ",";
    if (ded.culture) fx += " culture +" + std::to_string(ded.culture) + ",";
    if (!fx.empty()) {
      fx.pop_back();
      ImGui::TextWrapped("Dedication:%s", fx.c_str());
    }
  }
  if (b.jobs) ImGui::Text("Workers: %u / %u (present %u)", b.employed, b.jobs, b.present);
  if (b.state == sim::BState::Active || b.state == sim::BState::Burning)
    ImGui::Text("Fire risk %u%%  structural damage %u%%%s", b.fire_risk * 100 / 255, b.damage * 100 / 255,
                b.plague ? "  PLAGUE" : "");
  if (d.service.valid() && D.services[d.service.v].need == data::NeedKind::Religion && d.id != "shrine" &&
      b.extra < D.gods.size())
    ImGui::Text("Dedicated to %s", D.gods[b.extra].name.c_str());
  if (d.kind == data::BuildingKind::House) {
    const auto* t = D.tier(d.house_line, b.tier);
    ImGui::Text("%s: %u / %u residents", t ? t->name.c_str() : "House", b.residents, b.capacity);
    if (const auto* nt = D.tier(d.house_line, static_cast<u8>(b.tier + 1))) {
      if (b.extra > 0 && b.extra - 1 < nt->reqs.size())
        ImGui::TextWrapped("To become a %s it needs %s.", nt->name.c_str(), requirement_text(D, nt->reqs[b.extra - 1]).c_str());
      else if (b.residents > 0)
        ImGui::TextWrapped("Will become a %s soon.", nt->name.c_str());
    }
    if (ImGui::TreeNode("Residents")) {
      for (u32 i = 0; i < w.citizens.ids.high_water(); ++i)
        if (w.citizens.ids.alive_index(i) && w.citizens.home[i] == sel.building) {
          std::string n = w.citizen_name(i) + " (" + std::to_string(w.citizens.age_years(i, w.now)) + ")##" + std::to_string(i);
          if (ImGui::Selectable(n.c_str())) sel.citizen = w.citizens.ids.id_at(i);
        }
      ImGui::TreePop();
    }
  }
  if (b.jobs && ImGui::TreeNode("Workers")) {
    for (u32 i = 0; i < w.citizens.ids.high_water(); ++i)
      if (w.citizens.ids.alive_index(i) && w.citizens.work[i] == sel.building) {
        std::string n = w.citizen_name(i) + " - " + sim::activity_name(w.citizens.activity[i]) + "##" + std::to_string(i);
        if (ImGui::Selectable(n.c_str())) sel.citizen = w.citizens.ids.id_at(i);
      }
    ImGui::TreePop();
  }
  if (d.service.valid()) ImGui::Text("Visitors: %u / %u (served today %u)", b.visitors, d.service_capacity, b.served_today);
  if (d.kind == data::BuildingKind::Military && (!d.accepts.empty() || d.id == "militia_post") &&
      b.state == sim::BState::Active) {
    ImGui::TextUnformatted("Raise a unit here:");
    for (u16 u = 0; u < D.units.size(); ++u) {
      const auto& ud = D.units[u];
      if (ud.enemy || !w.is_unlocked_unit(UnitDefId{u})) continue;
      if (d.id == "militia_post" && ud.id != "militia") continue;
      std::string gear;
      for (const auto& g : ud.gear) gear += " " + std::to_string(g.amount * ud.size) + " " + D.goods[g.good.v].name;
      if (ImGui::Button((ud.name + "##raise" + std::to_string(u)).c_str())) {
        sim::Command c;
        c.type = sim::CmdType::RaiseUnit;
        c.def = u;
        c.value = static_cast<i32>(sel.building.raw);
        w.push(c);
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%u soldiers. Needs:%s", ud.size, gear.empty() ? " nothing" : gear.c_str());
    }
  }
  if (d.recipe.valid()) {
    const auto& r = D.recipes[d.recipe.v];
    ImGui::Text("Production: %u / %u", std::min<u32>(b.work_progress, r.work), r.work);
  }
  if (b.inv != econ::kNoInv) {
    ImGui::Text("Stock (%u / %u):", w.econ.total[b.inv], w.econ.capacity[b.inv]);
    for (u32 g = 0; g < w.econ.goods; ++g) {
      GoodId gid{static_cast<u16>(g)};
      if (w.econ.amt(b.inv, gid) || w.econ.rin(b.inv, gid))
        ImGui::Text("  %s: %u%s", D.goods[g].name.c_str(), w.econ.amt(b.inv, gid),
                    w.econ.rin(b.inv, gid) ? (" (+" + std::to_string(w.econ.rin(b.inv, gid)) + " coming)").c_str() : "");
    }
  }
  ImGui::End();
  if (!open) sel.building = BuildingId{};
}

void draw_labor_panel(sim::World& w, bool* open) {
  if (!*open) return;
  ImGui::SetNextWindowPos({300, 10}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Labor", open);
  u32 jobs[sim::kSectorCount] = {}, filled[sim::kSectorCount] = {};
  w.buildings.for_each([&](BuildingId, const sim::Building& b) {
    if (b.jobs == 0) return;
    int s = static_cast<int>(w.sector_of(b));
    jobs[s] += b.jobs;
    filled[s] += b.employed;
  });
  u32 unemployed = 0, workforce = 0;
  for (u32 i = 0; i < w.citizens.ids.high_water(); ++i) {
    if (!w.citizens.ids.alive_index(i)) continue;
    const u32 a = w.citizens.age_years(i, w.now);
    if (a < 14 || a >= 60) continue;
    ++workforce;
    if (!w.citizens.work[i].valid()) ++unemployed;
  }
  ImGui::Text("Workforce %u, unemployed %u", workforce, unemployed);
  if (ImGui::BeginTable("labor", 3)) {
    ImGui::TableSetupColumn("Sector");
    ImGui::TableSetupColumn("Workers");
    ImGui::TableSetupColumn("Priority (1 = first)");
    ImGui::TableHeadersRow();
    for (int s = 0; s < sim::kSectorCount; ++s) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(sim::sector_name(static_cast<sim::Sector>(s)));
      ImGui::TableNextColumn();
      ImGui::Text("%u / %u", filled[s], jobs[s]);
      ImGui::TableNextColumn();
      int p = w.policy.sector_priority[s];
      ImGui::SetNextItemWidth(120);
      if (ImGui::SliderInt(("##p" + std::to_string(s)).c_str(), &p, 1, 5)) w.policy.sector_priority[s] = static_cast<u8>(p);
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void draw_finance_panel(sim::World& w, bool* open) {
  if (!*open) return;
  ImGui::SetNextWindowPos({300, 200}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Finance", open);
  const auto& L = w.econ.money;
  ImGui::Text("Treasury: %lld denarii", static_cast<long long>(w.econ.treasury));
  if (ImGui::BeginTable("fin", 4)) {
    ImGui::TableSetupColumn("");
    ImGui::TableSetupColumn("Yesterday");
    ImGui::TableSetupColumn("This year");
    ImGui::TableSetupColumn("Last year");
    ImGui::TableHeadersRow();
    i64 net[3] = {};
    for (int k = 0; k < econ::kMoneyCount; ++k) {
      const auto m = static_cast<econ::Money>(k);
      const i64 s = econ::money_is_income(m) ? 1 : -1;
      net[0] += s * L.yesterday[k];
      net[1] += s * L.year[k];
      net[2] += s * L.last_year[k];
      if (!L.yesterday[k] && !L.year[k] && !L.last_year[k]) continue;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(s > 0 ? ImVec4(0.5f, 0.9f, 0.5f, 1) : ImVec4(0.95f, 0.6f, 0.5f, 1), "%s", econ::money_name(m));
      ImGui::TableNextColumn();
      ImGui::Text("%lld", static_cast<long long>(L.yesterday[k]));
      ImGui::TableNextColumn();
      ImGui::Text("%lld", static_cast<long long>(L.year[k]));
      ImGui::TableNextColumn();
      ImGui::Text("%lld", static_cast<long long>(L.last_year[k]));
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Net");
    for (i64 v : net) {
      ImGui::TableNextColumn();
      ImGui::Text("%+lld", static_cast<long long>(v));
    }
    ImGui::EndTable();
  }
  ImGui::Separator();
  auto cmd_policy = [&](sim::PolicyId id, int v) {
    sim::Command c;
    c.type = sim::CmdType::SetPolicy;
    c.def = static_cast<u16>(id);
    c.value = v;
    w.push(c);
  };
  int wage = w.policy.wage;
  if (ImGui::SliderInt("Wage (per worker/day)", &wage, 1, 20)) cmd_policy(sim::PolicyId::Wage, wage);
  int tax = w.policy.tax_rate[2];
  if (ImGui::SliderInt("Tax on plebeians (%)", &tax, 0, 30)) cmd_policy(sim::PolicyId::TaxPleb, tax);
  int tax_e = w.policy.tax_rate[1];
  if (ImGui::SliderInt("Tax on equites (%)", &tax_e, 0, 30)) cmd_policy(sim::PolicyId::TaxEques, tax_e);
  int tax_p = w.policy.tax_rate[0];
  if (ImGui::SliderInt("Tax on patricians (%)", &tax_p, 0, 30)) cmd_policy(sim::PolicyId::TaxPatrician, tax_p);
  const char* treat[] = {"Harsh", "Fair", "Kind"};
  int st = w.policy.slave_treatment;
  if (ImGui::Combo("Slave treatment", &st, treat, 3)) cmd_policy(sim::PolicyId::SlaveTreatment, st);
  const char* manu[] = {"Forbidden", "Occasional", "Encouraged"};
  int mn = w.policy.manumission;
  if (ImGui::Combo("Manumission", &mn, manu, 3)) cmd_policy(sim::PolicyId::Manumission, mn);
  if (ImGui::Button("Buy 5 public slaves (600)")) {
    sim::Command c;
    c.type = sim::CmdType::BuySlaves;
    c.value = 5;
    w.push(c);
  }
  const char* dole[] = {"No grain dole", "Grain dole for the poor", "Grain dole for all"};
  int r = w.policy.rations;
  if (ImGui::Combo("Annona", &r, dole, 3)) cmd_policy(sim::PolicyId::Rations, r);
  ImGui::End();
}

void draw_trade_panel(sim::World& w, bool* open) {
  if (!*open) return;
  const auto& D = *w.defs;
  ImGui::SetNextWindowPos({500, 120}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({560, 520}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Trade", open);
  ImGui::TextUnformatted("Routes");
  for (u32 p = 0; p < w.routes.size(); ++p) {
    const auto& rs = w.routes[p];
    if (!rs.available) continue;
    const auto& pd = D.partners[p];
    ImGui::PushID(static_cast<int>(p));
    ImGui::Text("%s (%s)", pd.name.c_str(), pd.sea ? "sea" : "land");
    ImGui::SameLine(260);
    if (rs.open) {
      ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1), "open");
    } else if (ImGui::Button(("Open route (" + std::to_string(pd.open_cost) + ")").c_str())) {
      sim::Command c;
      c.type = sim::CmdType::OpenRoute;
      c.def = static_cast<u16>(p);
      w.push(c);
    }
    std::string buys = "  buys:", sells = "  sells:";
    for (const auto& g : pd.buys)
      buys += " " + D.goods[g.good.v].name + " " + std::to_string(w.trade_price(p, g.good, false) / 10) + "dn";
    for (const auto& g : pd.sells)
      sells += " " + D.goods[g.good.v].name + " " + std::to_string(w.trade_price(p, g.good, true) / 10) + "dn";
    ImGui::TextWrapped("%s", buys.c_str());
    ImGui::TextWrapped("%s", sells.c_str());
    ImGui::PopID();
  }
  ImGui::Separator();
  ImGui::TextUnformatted("Goods: import when stock is below the level, export above it.");
  ImGui::TextUnformatted("Stockpile keeps a good in storage (markets and workshops do not take it), e.g. for Rome.");
  if (ImGui::BeginTable("tg", 4)) {
    ImGui::TableSetupColumn("Good");
    ImGui::TableSetupColumn("Stock");
    ImGui::TableSetupColumn("Mode");
    ImGui::TableSetupColumn("Level");
    ImGui::TableHeadersRow();
    const char* modes[] = {"-", "Import", "Export", "Stockpile"};
    for (u32 g = 0; g < D.goods.size(); ++g) {
      ImGui::PushID(static_cast<int>(g));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(D.goods[g].name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%u", w.econ.city_stock[g]);
      ImGui::TableNextColumn();
      int m = w.trade_mode[g];
      ImGui::SetNextItemWidth(90);
      bool changed = ImGui::Combo("##m", &m, modes, 4);
      ImGui::TableNextColumn();
      int lvl = w.trade_level[g];
      ImGui::SetNextItemWidth(120);
      changed |= ImGui::SliderInt("##l", &lvl, 0, 1000);
      if (changed) {
        sim::Command c;
        c.type = sim::CmdType::SetTrade;
        c.def = static_cast<u16>(g);
        c.value = m;
        c.x0 = lvl;
        w.push(c);
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void draw_religion_panel(sim::World& w, bool* open) {
  if (!*open) return;
  const auto& D = *w.defs;
  ImGui::SetNextWindowPos({620, 60}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Religion", open);
  ImGui::Text("Weather: %s", sim::weather_name(w.weather));
  for (u32 g = 0; g < D.gods.size(); ++g) {
    if (!w.god_available[g]) continue;
    ImGui::PushID(static_cast<int>(g));
    const u8 m = w.god_mood[g];
    const char* mood = m >= 85 ? "exalted" : m >= 65 ? "pleased" : m >= 35 ? "indifferent" : m >= 15 ? "displeased" : "wrathful";
    ImGui::Text("%-14s %3u  %s", D.gods[g].name.c_str(), m, mood);
    ImGui::SameLine(300);
    if (ImGui::SmallButton("Festival")) {
      sim::Command c;
      c.type = sim::CmdType::Festival;
      c.def = static_cast<u16>(g);
      c.value = 1;
      w.push(c);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Grand")) {
      sim::Command c;
      c.type = sim::CmdType::Festival;
      c.def = static_cast<u16>(g);
      c.value = 3;
      w.push(c);
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s. Feast day: day %u of the year.", D.gods[g].domain.c_str(), D.gods[g].festival_day + 1u);
    ImGui::PopID();
  }
  ImGui::End();
}

void draw_military_panel(sim::World& w, Selection& sel, bool* open) {
  if (!*open) return;
  const auto& D = *w.defs;
  ImGui::SetNextWindowPos({880, 60}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({380, 420}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Military", open);
  ImGui::TextWrapped("Select a unit, then right-click the map to move it. Raise units from a barracks.");
  const char* consc[] = {"Volunteers only", "Conscript the employed too", "Full levy"};
  int c = w.policy.conscription;
  if (ImGui::Combo("Recruitment", &c, consc, 3)) {
    sim::Command cmd;
    cmd.type = sim::CmdType::SetPolicy;
    cmd.def = static_cast<u16>(sim::PolicyId::Conscription);
    cmd.value = c;
    w.push(cmd);
  }
  ImGui::Text("Soldiers lost %u, enemies slain %u", w.soldiers_lost_, w.enemies_killed_);
  ImGui::Separator();
  for (u16 u = 0; u < military::kMaxUnits; ++u) {
    const auto& un = w.army.units[u];
    if (!un.alive) continue;
    const auto& ud = D.units[un.def.v];
    std::string label = (un.side ? std::string("[enemy] ") : "Cohors " + std::to_string(un.number) + " - ") + ud.name + " (" +
                        std::to_string(un.count) + ", morale " + std::to_string(un.morale) + ", " +
                        military::order_name(un.order) + ")##u" + std::to_string(u);
    if (ImGui::Selectable(label.c_str(), sel.unit == u)) sel.unit = u;
  }
  if (sel.unit < military::kMaxUnits && w.army.units[sel.unit].alive && w.army.units[sel.unit].side == 0) {
    const auto& un = w.army.units[sel.unit];
    ImGui::Separator();
    auto cmd = [&](sim::CmdType t, i32 v) {
      sim::Command k;
      k.type = t;
      k.def = sel.unit;
      k.value = v;
      w.push(k);
    };
    ImGui::Text("Experience %u", un.exp);
    for (int f = 0; f < 4; ++f) {
      if (f) ImGui::SameLine();
      if (ImGui::RadioButton(military::formation_name(static_cast<military::Formation>(f)), static_cast<int>(un.formation) == f))
        cmd(sim::CmdType::UnitFormation, f);
    }
    if (ImGui::RadioButton("aggressive", un.stance == military::Stance::Aggressive)) cmd(sim::CmdType::UnitStance, 0);
    ImGui::SameLine();
    if (ImGui::RadioButton("defensive", un.stance == military::Stance::Defensive)) cmd(sim::CmdType::UnitStance, 1);
    if (ImGui::Button("Return to barracks")) cmd(sim::CmdType::UnitOrder, static_cast<i32>(military::Order::Return));
    ImGui::SameLine();
    if (ImGui::Button("Disband")) cmd(sim::CmdType::DisbandUnit, 0);
    ImGui::TextUnformatted("Off-map campaign (8 days):");
    for (int lv = 1; lv <= 3; ++lv) {
      if (lv > 1) ImGui::SameLine();
      const char* nm[] = {"", "Skirmish", "Campaign", "Conquest"};
      if (ImGui::Button(nm[lv])) cmd(sim::CmdType::Campaign, lv);
    }
  }
  ImGui::End();
}

void draw_rome_panel(sim::World& w, bool* open) {
  if (!*open) return;
  const auto& D = *w.defs;
  ImGui::SetNextWindowPos({420, 120}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({420, 420}, ImGuiCond_FirstUseEver);
  ImGui::Begin(w.empire ? "Rome (the Emperor)" : "Rome (the Senate)", open);
  auto bar = [](const char* name, int v, int goal) {
    ImGui::ProgressBar(static_cast<float>(v) / 100.0f, ImVec2(180, 0), std::to_string(v).c_str());
    ImGui::SameLine();
    if (goal > 0) ImGui::Text("%s (goal %d)", name, goal);
    else ImGui::TextUnformatted(name);
  };
  bar("Culture", w.ratings.culture, w.goals.culture);
  bar("Prosperity", w.ratings.prosperity, w.goals.prosperity);
  bar("Security", w.ratings.security, w.goals.security);
  bar("Favor", w.favor, w.goals.favor);
  ImGui::Separator();
  ImGui::TextUnformatted("Requests");
  bool any = false;
  for (const auto& r : w.requests) {
    if (!r.active) continue;
    any = true;
    const u64 days = r.deadline > w.now ? (r.deadline - w.now) / kTicksPerDay : 0;
    std::string what = r.kind == 0 ? std::to_string(r.amount) + " " + D.goods[r.good.v].name
                                   : r.kind == 1 ? std::to_string(r.amount) + " denarii"
                                                 : "a unit of " + std::to_string(r.amount) + " soldiers";
    ImGui::Text("Send %s within %llu days", what.c_str(), static_cast<unsigned long long>(days));
    if (r.kind == 0) {
      ImGui::SameLine();
      ImGui::TextDisabled("(%u in storage)", w.stored_amount(r.good));
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(("Send##r" + std::to_string(r.id)).c_str())) {
      sim::Command c;
      c.type = sim::CmdType::FulfillRequest;
      c.def = r.id;
      w.push(c);
    }
  }
  if (!any) ImGui::TextDisabled("No outstanding requests.");
  ImGui::Text("Fulfilled %u, failed %u", w.requests_fulfilled, w.requests_failed);
  ImGui::Separator();
  if (w.loan.remaining > 0) {
    ImGui::Text("Loan outstanding: %lld denarii", static_cast<long long>(w.loan.remaining));
  } else if (ImGui::Button("Ask Rome for a loan of 2000 denarii (20% interest)")) {
    sim::Command c;
    c.type = sim::CmdType::TakeLoan;
    c.value = 2000;
    w.push(c);
  }
  ImGui::Separator();
  ImGui::TextUnformatted("Mission goals");
  auto goal = [](bool ok, const std::string& t) {
    ImGui::TextColored(ok ? ImVec4(0.5f, 0.9f, 0.5f, 1) : ImVec4(0.9f, 0.9f, 0.9f, 1), "%s %s", ok ? "[x]" : "[ ]", t.c_str());
  };
  if (w.goals.population) goal(w.population() >= w.goals.population, "Population " + std::to_string(w.population()) + " / " + std::to_string(w.goals.population));
  if (w.goals.culture) goal(w.ratings.culture >= w.goals.culture, "Culture " + std::to_string(w.goals.culture));
  if (w.goals.prosperity) goal(w.ratings.prosperity >= w.goals.prosperity, "Prosperity " + std::to_string(w.goals.prosperity));
  if (w.goals.security) goal(w.ratings.security >= w.goals.security, "Security " + std::to_string(w.goals.security));
  if (w.goals.favor) goal(w.favor >= w.goals.favor, "Favor " + std::to_string(w.goals.favor));
  if (w.goals.requests) goal(w.requests_fulfilled >= w.goals.requests, "Requests fulfilled " + std::to_string(w.goals.requests));
  if (w.goals.survive_days) goal(w.mission_days >= w.goals.survive_days, "Endure " + std::to_string(w.goals.survive_days) + " days");
  for (const auto& g : w.goals.buildings)
    goal(w.count_active(g.def) >= g.count, std::to_string(g.count) + " x " + D.b(g.def).name);
  ImGui::End();
}

void draw_advisors_panel(const sim::World& w, const UiIcons& icons, int* advisor, bool* open) {
  if (!*open) return;
  ImGui::SetNextWindowPos({420, 560}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({520, 260}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Advisors", open);
  for (int a = 1; a <= 5; ++a) {
    if (a > 1) ImGui::SameLine();
    ImGui::PushID(a);
    if (ImGui::Selectable("##adv", *advisor == a, 0, ImVec2(56, 56))) *advisor = a;
    ImGui::SameLine(ImGui::GetCursorPosX() - 60);
    icons.image(kAdvisorSprites[a], 56);
    ImGui::PopID();
  }
  ImGui::TextUnformatted(kAdvisorNames[*advisor]);
  ImGui::Separator();
  for (const auto& line : advisor_report(w, *advisor)) ImGui::TextWrapped("%s", line.c_str());
  ImGui::End();
}

void draw_messages(sim::World& w, const UiIcons& icons, u16* last_seen) {
  // Show the oldest unread message (or unanswered choice) as a dialogue.
  for (auto& m : w.messages) {
    if (m.id <= *last_seen && (m.options.empty() || m.answered)) continue;
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.35f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize({460, 0}, ImGuiCond_Appearing);
    ImGui::Begin(("##msg" + std::to_string(m.id)).c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    if (m.advisor > 0 && m.advisor <= 5) {
      icons.image(kAdvisorSprites[m.advisor], 64);
      ImGui::SameLine();
      ImGui::BeginGroup();
      ImGui::TextDisabled("%s", kAdvisorNames[m.advisor]);
    } else {
      ImGui::BeginGroup();
    }
    ImGui::Text("%s", m.title.c_str());
    ImGui::EndGroup();
    ImGui::Separator();
    ImGui::TextWrapped("%s", m.text.c_str());
    ImGui::Spacing();
    if (m.options.empty()) {
      if (ImGui::Button("Very well")) *last_seen = m.id;
    } else {
      for (size_t k = 0; k < m.options.size(); ++k)
        if (ImGui::Button((m.options[k] + "##o" + std::to_string(k)).c_str())) {
          sim::Command c;
          c.type = sim::CmdType::Answer;
          c.def = m.id;
          c.value = static_cast<i32>(k + 1);
          w.push(c);
          m.answered = static_cast<u8>(k + 1);  // hide immediately; the sim confirms next tick
          *last_seen = std::max(*last_seen, m.id);
        }
    }
    ImGui::End();
    break;
  }
}

void draw_mission_end(const sim::World& w, bool* dismissed) {
  if (w.mission == sim::MissionState::Running || *dismissed) return;
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.4f), ImGuiCond_Always,
                          ImVec2(0.5f, 0.5f));
  ImGui::Begin("##end", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);
  if (w.mission == sim::MissionState::Won) {
    ImGui::Text("Victory!");
    ImGui::TextWrapped("The Senate is pleased with your work. A greater task awaits.");
  } else {
    ImGui::Text("Defeat");
    ImGui::TextWrapped("%s", w.lost_reason.c_str());
  }
  if (ImGui::Button("Continue")) *dismissed = true;
  ImGui::End();
}

void draw_threats_panel(sim::World& w, const Selection& sel, bool* open) {
  if (!*open) return;
  ImGui::SetNextWindowPos({620, 380}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Threats (debug)", open);
  ImGui::TextWrapped("Triggers act on the selected building, or a random one.");
  i32 x = -1, y = -1;
  if (sel.building.valid() && w.buildings.alive(sel.building)) {
    x = w.buildings[sel.building].x;
    y = w.buildings[sel.building].y;
  }
  const char* names[] = {"Fire", "Collapse", "Plague", "Riot", "Servile revolt", "Flood", "Earthquake", "Drought",
                         "Storm", "Blight", "Blessing", "Wrath", "Raid"};
  static int god = 0;
  for (int t = 0; t < 13; ++t) {
    if (t % 3) ImGui::SameLine();
    if (ImGui::Button(names[t], ImVec2(120, 0))) {
      sim::Command c;
      c.type = sim::CmdType::Trigger;
      c.def = static_cast<u16>(t);
      c.x0 = x;
      c.y0 = y;
      c.value = t == 12 ? 3 : god;
      w.push(c);
    }
  }
  ImGui::SliderInt("god for blessing/wrath", &god, 0, static_cast<int>(w.defs->gods.size()) - 1);
  ImGui::Text("fires %u, collapses %u, plague houses %u, riots %u", w.fires_total, w.collapses_total, w.plague_houses,
              w.riots_total);
  ImGui::End();
}

BuildingId draw_alerts_panel(const sim::World& w, bool* open) {
  BuildingId jump;
  if (!*open) return jump;
  ImGui::SetNextWindowPos({300, 760}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({520, 120}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Alerts", open);
  for (auto it = w.alerts.rbegin(); it != w.alerts.rend(); ++it) {
    std::string t = "Day " + std::to_string(sim::day_index(it->t) + 1) + "  " + alert_text(w, *it);
    if (it->count > 1) t += "  (x" + std::to_string(it->count) + ")";
    t += "##" + std::to_string(it - w.alerts.rbegin());
    if (ImGui::Selectable(t.c_str()) && it->b.valid()) jump = it->b;
  }
  ImGui::End();
  return jump;
}

void draw_selection(const sim::World& w, const Selection& sel, render::Renderer& r, f32 alpha) {
  const f32 T = static_cast<f32>(sim::kTilePx);
  if (sel.unit < military::kMaxUnits && w.army.units[sel.unit].alive) {
    const auto& A = w.army;
    for (u32 i = 0; i < A.ids.high_water(); ++i)
      if (A.ids.alive_index(i) && A.unit[i] == sel.unit) {
        const f32 x = (A.px[i] + (A.x[i] - A.px[i]) * alpha) * T, y = (A.py[i] + (A.y[i] - A.py[i]) * alpha) * T;
        r.push_rect(x - 3, y + 3, 6, 1, 0.01f, render::rgba(255, 240, 120, 220));
      }
    const auto& un = A.units[sel.unit];
    if (un.target_tile >= 0) {
      const f32 tx = static_cast<f32>(un.target_tile % w.map.w) * T, ty = static_cast<f32>(un.target_tile / w.map.w) * T;
      r.push_rect(tx, ty, T, T, 0.01f, render::rgba(255, 240, 120, 90));
    }
  }
  if (sel.building.valid() && w.buildings.alive(sel.building)) {
    const auto& b = w.buildings[sel.building];
    const f32 x = static_cast<f32>(b.x) * T, y = static_cast<f32>(b.y) * T, bw = static_cast<f32>(b.w) * T,
              bh = static_cast<f32>(b.h) * T;
    const u32 c = render::rgba(255, 240, 120, 220);
    r.push_rect(x, y, bw, 1, 0.01f, c);
    r.push_rect(x, y + bh - 1, bw, 1, 0.01f, c);
    r.push_rect(x, y, 1, bh, 0.01f, c);
    r.push_rect(x + bw - 1, y, 1, bh, 0.01f, c);
  }
  if (sel.citizen.valid() && w.citizens.alive(sel.citizen)) {
    Vec2 p;
    if (w.citizen_position(sel.citizen.index(), alpha, p, nullptr))
      r.push_rect(p.x - 4, p.y + 3, 8, 2, 0.01f, render::rgba(255, 240, 120, 230));
  }
}

}  // namespace urbs::game
