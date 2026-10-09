#include "game/panels.hpp"

#include <imgui.h>

#include <algorithm>

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
    case sim::AlertKind::Raid: return "Raiders are approaching!";
    case sim::AlertKind::GodAngry: return "A god is angry";
    case sim::AlertKind::Request: return "A request from Rome";
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
  ImGui::Separator();
  auto p = w.happiness_parts(i);
  ImGui::Text("Happiness: %d (yesterday %u)", std::clamp(p.total(), 0, 255), C.happiness[i]);
  struct Part {
    const char* n;
    i16 v;
  } parts[] = {{"food", p.food},       {"water", p.water},          {"rest", p.rest},     {"hygiene", p.hygiene},
               {"religion", p.religion}, {"entertainment", p.entertainment}, {"food variety", p.variety},
               {"housing", p.housing}, {"employment", p.employment}, {"taxes", p.taxes},  {"gods", p.gods},
               {"events", p.events},   {"health", p.health}};
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
    for (const auto& c : d.cost_goods)
      ImGui::Text("  %s: %u / %u", D.goods[c.good.v].name.c_str(), b.inv != econ::kNoInv ? w.econ.amt(b.inv, c.good) : 0,
                  c.amount);
  }
  if (b.jobs) ImGui::Text("Workers: %u / %u (present %u)", b.employed, b.jobs, b.present);
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
  ImGui::TextUnformatted("Goods: import when stock is below the level, export above it");
  if (ImGui::BeginTable("tg", 4)) {
    ImGui::TableSetupColumn("Good");
    ImGui::TableSetupColumn("Stock");
    ImGui::TableSetupColumn("Mode");
    ImGui::TableSetupColumn("Level");
    ImGui::TableHeadersRow();
    const char* modes[] = {"-", "Import", "Export"};
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
      bool changed = ImGui::Combo("##m", &m, modes, 3);
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
