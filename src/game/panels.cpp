#include "game/panels.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "game/advisors.hpp"
#include "game/panel_kit.hpp"
#include "game/style.hpp"
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

namespace {

// Green when well met, amber when half, red when short (needs, workers, mood).
ImU32 level_col(float f) { return f > 0.6f ? kColGood : f > 0.35f ? kColWarn : kColBad; }

float frac(u32 v, u32 of) { return of ? static_cast<float>(v) / static_cast<float>(of) : 0.0f; }

std::string pct(u8 v) { return std::to_string(v * 100 / 255) + "%"; }

// What a house needs for its next tier, as a sentence and, for goods and food, picture chips.
void draw_requirement(const sim::World& w, const UiIcons& icons, const data::HousingReq& r) {
  const auto& D = *w.defs;
  const bool goods = r.kind == data::HousingReq::Goods || r.kind == data::HousingReq::Food;
  if (!goods) {
    ImGui::TextWrapped("It needs %s.", requirement_text(D, r).c_str());
    return;
  }
  ImGui::TextUnformatted(r.kind == data::HousingReq::Goods ? "It needs household goods:" : "It needs in the larder:");
  for (size_t k = 0; k < r.goods.size(); ++k) {
    if (k) ImGui::SameLine();
    const auto& g = D.goods[r.goods[k].v];
    good_chip(icons, g.id, g.name);
  }
  ImGui::TextDisabled(
      "%s", r.kind == data::HousingReq::Goods ? "Markets nearby must stock them." : "A market nearby must sell it.");
}

void draw_building(sim::World& w, Selection& sel, const UiIcons& icons) {
  const auto& b = w.buildings[sel.building];
  const auto& D = *w.defs;
  const auto& d = D.b(b.def);
  const bool house = d.kind == data::BuildingKind::House;
  const auto* tier = house ? D.tier(d.house_line, b.tier) : nullptr;
  // Header: what it is, and its state in a few words.
  std::string sub;
  ImU32 sub_col = 0;
  if (b.state == sim::BState::Construction) {
    sub = "Being built";
  } else if (b.state == sim::BState::Burning) {
    sub = "On fire!";
    sub_col = kColBad;
  } else if (b.state == sim::BState::Ruin) {
    sub = "In ruins";
    sub_col = kColBad;
  } else if (house) {
    sub = d.name + "  \xC2\xB7  " + std::to_string(b.residents) + " of " + std::to_string(b.capacity) + " residents";
  } else if (b.jobs) {
    sub = b.employed == 0 ? "No workers" : b.employed < b.jobs ? "Short of workers" : "Working";
    if (b.employed < b.jobs) sub_col = b.employed == 0 ? kColBad : 0;
    sub += "  \xC2\xB7  " + std::to_string(b.employed) + " of " + std::to_string(b.jobs) + " workers";
  } else {
    sub = "Standing";
  }
  if (!panel_header(icons, d.id, true, tier ? tier->name : d.name, sub, sub_col)) sel = Selection{};
  if (b.plague || (b.state == sim::BState::Active && b.fire_risk > 150) || b.damage > 150) {
    bool first = true;
    auto t = [&](const char* label, ImU32 col) {
      if (!first) ImGui::SameLine();
      first = false;
      tag(label, col);
    };
    if (b.plague) t("Plague", kColBad);
    if (b.state == sim::BState::Active && b.fire_risk > 150) t("Fire risk", kColWarn);
    if (b.damage > 150) t("Crumbling", kColWarn);
  }

  // Under construction: progress and materials.
  if (b.state == sim::BState::Construction) {
    const bool monument = d.kind == data::BuildingKind::Monument;
    section(monument ? ("Stage " + std::to_string(w.construction_stage(b) + 1) + " of " +
                        std::to_string(sim::World::kMonumentStages))
                           .c_str()
                     : "Construction");
    meter_row("Progress", frac(b.build_progress, b.build_needed), kColPlain,
              std::to_string(b.build_needed ? b.build_progress * 100 / b.build_needed : 0) + "%");
    for (const auto& c : d.cost_goods) {
      const u32 have = b.inv != econ::kNoInv ? w.econ.amt(b.inv, c.good) : 0;
      const u32 need = monument ? w.materials_needed_now(b, c.amount) : c.amount;
      good_icon(icons, D.goods[c.good.v].id);
      ImGui::SameLine();
      ImGui::TextUnformatted(D.goods[c.good.v].name.c_str());
      text_right(std::to_string(have) + " / " + std::to_string(need), have >= need ? kColGood : 0);
    }
    if (!w.site_ready(b)) {
      ImGui::PushStyleColor(ImGuiCol_Text, kColBad);
      ImGui::TextWrapped("Waiting for materials; builders will come when they arrive.");
      ImGui::PopStyleColor();
    }
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
      ImGui::TextWrapped("When dedicated:%s", fx.c_str());
    }
  }

  // Houses: the next tier, then the home itself.
  if (house) {
    if (const auto* nt = D.tier(d.house_line, static_cast<u8>(b.tier + 1))) {
      section(("To become a " + nt->name).c_str());
      if (b.extra > 0 && b.extra - 1 < nt->reqs.size()) {
        draw_requirement(w, icons, nt->reqs[b.extra - 1]);
      } else if (b.residents > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, kColGood);
        ImGui::TextWrapped("It has everything it needs and will grow soon.");
        ImGui::PopStyleColor();
      } else {
        ImGui::TextDisabled("Nobody lives here yet.");
      }
    }
    section("Home");
    meter_row("Residents", frac(b.residents, b.capacity), kColGood,
              std::to_string(b.residents) + " / " + std::to_string(b.capacity));
  }

  // Production: what goes in and what comes out.
  if (d.recipe.valid() && b.state == sim::BState::Active) {
    const auto& r = D.recipes[d.recipe.v];
    section("Production");
    const float fs = ImGui::GetFontSize();
    auto goods_line = [&](const std::vector<data::GoodAmount>& list) {
      ImGui::BeginGroup();
      for (const auto& g : list) {
        const u32 have = b.inv != econ::kNoInv ? w.econ.amt(b.inv, g.good) : 0;
        const u32 coming = b.inv != econ::kNoInv ? w.econ.rin(b.inv, g.good) : 0;
        good_icon(icons, D.goods[g.good.v].id, std::round(fs * 1.6f));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::PushFont(ui_fonts().bold, 0.0f);
        ImGui::Text("%s  %u", D.goods[g.good.v].name.c_str(), have);
        ImGui::PopFont();
        if (coming) ImGui::TextDisabled("+%u on the way", coming);
        ImGui::EndGroup();
      }
      ImGui::EndGroup();
    };
    if (!r.inputs.empty()) {
      goods_line(r.inputs);
      ImGui::SameLine(0, fs);
      // An arrow from the inputs to the outputs.
      const ImVec2 p = ImGui::GetCursorScreenPos();
      ImDrawList* dl = ImGui::GetWindowDrawList();
      const float y = std::round(p.y + fs * 0.8f), len = std::round(fs * 2.2f);
      const ImU32 col = ImGui::GetColorU32(ImGuiCol_Border);
      dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + len, y), col, 3.0f);
      dl->AddTriangleFilled(ImVec2(p.x + len, y - fs * 0.35f), ImVec2(p.x + len + fs * 0.5f, y),
                            ImVec2(p.x + len, y + fs * 0.35f), col);
      ImGui::Dummy(ImVec2(len + fs * 0.5f, fs * 1.6f));
      ImGui::SameLine(0, fs);
    }
    goods_line(r.outputs);
    meter_row("Next batch", frac(std::min<u32>(b.work_progress, r.work), r.work),
              b.work_progress >= r.work ? kColGood : kColPlain,
              b.work_progress >= r.work ? std::string("ready") : std::to_string(b.work_progress * 100 / r.work) + "%");
  }

  // Stores and markets: everything held.
  const bool store = d.kind == data::BuildingKind::Storage || d.kind == data::BuildingKind::Market;
  if (b.inv != econ::kNoInv && !d.recipe.valid() && b.state != sim::BState::Construction &&
      (store || w.econ.total[b.inv] > 0)) {
    section(d.kind == data::BuildingKind::Market ? "On sale" : "Stock");
    meter_row("Space used", frac(w.econ.total[b.inv], w.econ.capacity[b.inv]), kColPlain,
              std::to_string(w.econ.total[b.inv]) + " / " + std::to_string(w.econ.capacity[b.inv]));
    for (u32 g = 0; g < w.econ.goods; ++g) {
      const GoodId gid{static_cast<u16>(g)};
      const u32 have = w.econ.amt(b.inv, gid), coming = w.econ.rin(b.inv, gid);
      if (!have && !coming) continue;
      good_icon(icons, D.goods[g].id);
      ImGui::SameLine();
      ImGui::TextUnformatted(D.goods[g].name.c_str());
      if (coming) {
        ImGui::SameLine();
        ImGui::TextDisabled("+%u coming", coming);
      }
      text_right(std::to_string(have));
    }
  } else if (d.recipe.valid() && b.inv != econ::kNoInv && b.state == sim::BState::Active) {
    meter_row("Storage", frac(w.econ.total[b.inv], w.econ.capacity[b.inv]), kColPlain,
              std::to_string(w.econ.total[b.inv]) + " / " + std::to_string(w.econ.capacity[b.inv]));
  }

  // Services: who comes, and the god of a temple.
  if (d.service.valid() && b.state == sim::BState::Active) {
    section("Visitors");
    if (D.services[d.service.v].need == data::NeedKind::Religion && d.id != "shrine" && b.extra < D.gods.size()) {
      icons.image("ui/god_" + D.gods[b.extra].id, std::round(ImGui::GetFontSize() * 1.4f));
      ImGui::SameLine();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Dedicated to %s", D.gods[b.extra].name.c_str());
    }
    meter_row("Here now", frac(b.visitors, d.service_capacity), kColPlain,
              std::to_string(b.visitors) + " / " + std::to_string(d.service_capacity));
    value_row("Served today", with_commas(b.served_today));
  }

  // Barracks: raise a unit.
  if (d.kind == data::BuildingKind::Military && (!d.accepts.empty() || d.id == "militia_post") &&
      b.state == sim::BState::Active) {
    section("Raise a unit");
    bool first = true;
    for (u16 u = 0; u < D.units.size(); ++u) {
      const auto& ud = D.units[u];
      if (ud.enemy || !w.is_unlocked_unit(UnitDefId{u})) continue;
      if (d.id == "militia_post" && ud.id != "militia") continue;
      if (!first) ImGui::SameLine();
      first = false;
      std::string gear;
      for (const auto& g : ud.gear) gear += "\n  " + std::to_string(g.amount * ud.size) + " " + D.goods[g.good.v].name;
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

  // The building itself: workers and risks.
  if (b.state == sim::BState::Active || b.state == sim::BState::Burning) {
    if (!house) section("Building");
    if (b.jobs) {
      const std::string tip = std::to_string(b.present) + " at work right now";
      meter_row("Workers", frac(b.employed, b.jobs), level_col(frac(b.employed, b.jobs)),
                std::to_string(b.employed) + " / " + std::to_string(b.jobs), -1, tip.c_str());
    }
    meter_row("Fire risk", frac(b.fire_risk, 255), b.fire_risk > 150 ? kColBad : kColWarn, pct(b.fire_risk), -1,
              "Engineers' and vigiles' patrols bring it down");
    meter_row("Wear", frac(b.damage, 255), b.damage > 150 ? kColBad : kColWarn, pct(b.damage), -1,
              "Engineers' patrols repair it before it collapses");
  }

  // Who lives or works here.
  auto people = [&](const char* label, bool residents) {
    u32 n = 0;
    for (u32 i = 0; i < w.citizens.ids.high_water(); ++i)
      if (w.citizens.ids.alive_index(i) && (residents ? w.citizens.home[i] : w.citizens.work[i]) == sel.building) ++n;
    if (!n) return;
    if (ImGui::TreeNode((std::string(label) + " (" + std::to_string(n) + ")###people").c_str())) {
      for (u32 i = 0; i < w.citizens.ids.high_water(); ++i)
        if (w.citizens.ids.alive_index(i) && (residents ? w.citizens.home[i] : w.citizens.work[i]) == sel.building) {
          std::string t =
              w.citizen_name(i) + (residents ? " (" + std::to_string(w.citizens.age_years(i, w.now)) + ")"
                                             : std::string(", ") + sim::activity_name(w.citizens.activity[i]));
          if (ImGui::Selectable((t + "##" + std::to_string(i)).c_str())) sel.citizen = w.citizens.ids.id_at(i);
        }
      ImGui::TreePop();
    }
  };
  if (house) people("Who lives here", true);
  if (b.jobs) people("Workers", false);
}

void draw_citizen(sim::World& w, Selection& sel, const UiIcons& icons) {
  const u32 i = sel.citizen.index();
  const auto& C = w.citizens;
  const auto& D = *w.defs;
  std::string sub = std::string(C.sex[i] ? "Woman" : "Man") + ", " + std::to_string(C.age_years(i, w.now)) +
                    "  \xC2\xB7  " + sim::class_name(C.cls[i]);
  if (!panel_header(icons, "ui/icon_population", false, w.citizen_name(i), sub)) sel = Selection{};
  if (sel.building.valid() && w.buildings.alive(sel.building)) {
    if (link(("Back to the " + D.b(w.buildings[sel.building].def).name).c_str())) sel.citizen = CitizenId{};
    ImGui::SameLine();
  }
  ImGui::Checkbox("Follow (F)", &sel.follow);
  {
    std::string tr;
    for (int k = 0; k < sim::kMaxTraits; ++k) {
      const u8 t = C.traits[static_cast<size_t>(i) * sim::kMaxTraits + k];
      if (t < D.traits.size()) tr += (tr.empty() ? "" : ", ") + D.traits[t].name;
    }
    if (!tr.empty()) ImGui::TextWrapped("%s", tr.c_str());
    if (C.goal[i]) ImGui::TextDisabled("Hopes to %s", sim::goal_text(static_cast<sim::Goal>(C.goal[i])));
  }

  section("Now");
  if (C.activity[i] == sim::Activity::Walk && C.next[i] != sim::Activity::None) {
    const bool to = C.dest[i].valid() && w.buildings.alive(C.dest[i]);
    ImGui::TextWrapped("Walking, then %s%s%s", sim::activity_name(C.next[i]), to ? " at the " : "",
                       to ? D.b(w.buildings[C.dest[i]].def).name.c_str() : "");
  } else {
    ImGui::TextWrapped("%s", sim::activity_name(C.activity[i]));
  }
  if (C.reason[i] != sim::Reason::None) ImGui::TextDisabled("%s", sim::reason_text(C.reason[i]));
  if (C.carrying[i]) {
    good_icon(icons, D.goods[C.carrying[i] - 1u].id);
    ImGui::SameLine();
    ImGui::Text("Carrying %u %s", C.carry_amount[i], D.goods[C.carrying[i] - 1u].name.c_str());
  }

  section("Home and work");
  auto place = [&](const char* label, BuildingId b) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(std::round(ImGui::GetFontSize() * 4));
    if (!b.valid() || !w.buildings.alive(b)) {
      ImGui::TextDisabled("none");
      return;
    }
    ImGui::PushID(label);
    if (link(D.b(w.buildings[b].def).name.c_str())) {
      sel.building = b;
      sel.citizen = CitizenId{};
    }
    ImGui::PopID();
  };
  place("Home", C.home[i]);
  place("Work", C.work[i]);
  if (C.household[i].valid() && w.households.ids.alive(C.household[i])) {
    const auto& h = w.households[C.household[i]];
    ImGui::TextDisabled("Household of %u: food %u, water %u, %u kinds of food", h.members, w.household_food(h), h.water,
                        w.food_kinds(h));
  }
  auto person = [&](const char* label, CitizenId c) {
    if (!c.valid() || !w.citizens.alive(c)) return;
    ImGui::TextUnformatted(label);
    ImGui::SameLine(std::round(ImGui::GetFontSize() * 4));
    ImGui::PushID(label);
    if (link(w.citizen_name(c.index()).c_str())) sel.citizen = c;
    ImGui::PopID();
  };
  person("Spouse", C.spouse[i]);
  person("Mother", C.mother[i]);
  person("Father", C.father[i]);

  section("Needs");
  for (int n = 0; n < sim::kNeedCount; ++n) {
    const u8 v = C.need(i, static_cast<sim::Need>(n), w.now);
    const char* name = sim::need_name(static_cast<sim::Need>(n));
    std::string label = name;
    if (!label.empty()) label[0] = static_cast<char>(label[0] - (label[0] >= 'a' && label[0] <= 'z' ? 32 : 0));
    meter_row(label.c_str(), frac(v, 255), level_col(frac(v, 255)), std::to_string(v * 100 / 255) + "%");
  }

  auto p = w.happiness_parts(i);
  section("Happiness");
  const int total = std::clamp(p.total(), 0, 255);
  meter_row("Mood", frac(static_cast<u32>(total), 255), total >= 128 ? kColGood : kColWarn, std::to_string(total),
            128.0f / 255.0f, "128 is content");
  struct Part {
    const char* n;
    i16 v;
  } parts[] = {{"food", p.food},
               {"water", p.water},
               {"rest", p.rest},
               {"hygiene", p.hygiene},
               {"religion", p.religion},
               {"entertainment", p.entertainment},
               {"food variety", p.variety},
               {"housing", p.housing},
               {"employment", p.employment},
               {"taxes", p.taxes},
               {"gods", p.gods},
               {"life events", p.events},
               {"health", p.health},
               {"education", p.education},
               {"prestige", p.prestige},
               {"safety", p.safety},
               {"bondage", p.freedom},
               {"temperament", p.traits}};
  // Two columns: what helps, what hurts.
  if (ImGui::BeginTable("##parts", 2)) {
    std::vector<const Part*> up, down;
    for (const Part& pt : parts) (pt.v > 0 ? up : down).push_back(&pt);
    std::erase_if(down, [](const Part* q) { return q->v == 0; });
    const size_t rows = std::max(up.size(), down.size());
    for (size_t r = 0; r < rows; ++r) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (r < up.size()) ImGui::TextColored(col4(kColGood), "%+d %s", up[r]->v, up[r]->n);
      ImGui::TableNextColumn();
      if (r < down.size()) ImGui::TextColored(col4(kColBad), "%+d %s", down[r]->v, down[r]->n);
    }
    ImGui::EndTable();
  }
  if (ImGui::TreeNode("Skills")) {
    const char* sk[] = {"farming", "crafting", "building", "trading", "soldiering", "scholarship"};
    for (int k = 0; k < sim::kSkillCount; ++k)
      value_row(sk[k], std::to_string(C.skills[static_cast<size_t>(i) * sim::kSkillCount + k]));
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Memories")) {
    for (int k = 0; k < sim::kMemoryEntries; ++k) {
      const int idx = (C.memory_head[i] + sim::kMemoryEntries - 1 - k) % sim::kMemoryEntries;
      const auto& m = C.memory[static_cast<size_t>(i) * sim::kMemoryEntries + idx];
      if (m.kind == 0) continue;
      ImGui::TextDisabled("Day %u", m.day + 1);
      ImGui::SameLine();
      ImGui::TextUnformatted(sim::memory_text(static_cast<sim::Memory>(m.kind)));
    }
    ImGui::TreePop();
  }
}

}  // namespace

void draw_inspector(sim::World& w, Selection& sel, const UiIcons& icons, const Dock& dock) {
  if (sel.citizen.valid() && !w.citizens.alive(sel.citizen)) sel.citizen = CitizenId{};
  if (sel.building.valid() && !w.buildings.alive(sel.building)) sel.building = BuildingId{};
  if (!sel.citizen.valid() && !sel.building.valid()) return;
  if (begin_panel("##inspector", dock)) {
    if (sel.citizen.valid())
      draw_citizen(w, sel, icons);
    else
      draw_building(w, sel, icons);
  }
  end_panel();
}

void draw_labor_panel(sim::World& w, const UiIcons& icons, const Dock& dock, bool* open) {
  if (!begin_panel("##labor", dock)) {
    end_panel();
    return;
  }
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
  if (!panel_header(icons, "ui/icon_labor", false, "Labor",
                    with_commas(workforce) + " of working age  \xC2\xB7  " + with_commas(unemployed) + " without work",
                    workforce && unemployed * 10 > workforce ? kColBad : 0))
    *open = false;
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
  end_panel();
}

void draw_finance_panel(sim::World& w, const UiIcons& icons, const Dock& dock, bool* open) {
  if (!begin_panel("##finance", dock)) {
    end_panel();
    return;
  }
  {
    const auto& M = w.econ.money;
    i64 net = 0;
    for (int k = 0; k < econ::kMoneyCount; ++k)
      net += (econ::money_is_income(static_cast<econ::Money>(k)) ? 1 : -1) * M.yesterday[k];
    if (!panel_header(icons, "ui/icon_finance", false, "Finance",
                      with_commas(w.econ.treasury) + " denarii  \xC2\xB7  " + signed_commas(net) + " yesterday",
                      net >= 0 ? kColGood : kColBad))
      *open = false;
  }
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
      ImGui::TextColored(col4(s > 0 ? kColGood : kColBad), "%s", econ::money_name(m));
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
  end_panel();
}

void draw_trade_panel(sim::World& w, const UiIcons& icons, const Dock& dock, bool* open) {
  const auto& D = *w.defs;
  if (!begin_panel("##trade", dock)) {
    end_panel();
    return;
  }
  {
    int n = 0;
    for (const auto& r : w.routes) n += r.open ? 1 : 0;
    if (!panel_header(
            icons, "ui/icon_trade", false, "Trade",
            n == 0 ? std::string("No routes open yet") : std::to_string(n) + (n == 1 ? " route open" : " routes open")))
      *open = false;
  }
  ImGui::TextUnformatted("Routes");
  for (u32 p = 0; p < w.routes.size(); ++p) {
    const auto& rs = w.routes[p];
    if (!rs.available) continue;
    const auto& pd = D.partners[p];
    ImGui::PushID(static_cast<int>(p));
    ImGui::Text("%s (%s)", pd.name.c_str(), pd.sea ? "sea" : "land");
    ImGui::SameLine(260);
    if (rs.open) {
      ImGui::TextColored(col4(kColGood), "open");
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
  end_panel();
}

void draw_religion_panel(sim::World& w, const UiIcons& icons, const Dock& dock, bool* open) {
  const auto& D = *w.defs;
  if (!begin_panel("##religion", dock)) {
    end_panel();
    return;
  }
  {
    int pleased = 0, angry = 0;
    for (u32 g = 0; g < w.defs->gods.size(); ++g) {
      if (!w.god_available[g]) continue;
      pleased += w.god_mood[g] >= 65 ? 1 : 0;
      angry += w.god_mood[g] < 35 ? 1 : 0;
    }
    std::string weather = sim::weather_name(w.weather);
    if (!weather.empty())
      weather[0] = static_cast<char>(weather[0] >= 'a' && weather[0] <= 'z' ? weather[0] - 32 : weather[0]);
    if (!panel_header(icons, "ui/icon_religion", false, "Religion",
                      weather + " today  \xC2\xB7  " + std::to_string(pleased) + " pleased, " + std::to_string(angry) +
                          " displeased",
                      angry > pleased ? kColBad : 0))
      *open = false;
  }
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
  end_panel();
}

void draw_military_panel(sim::World& w, Selection& sel, const UiIcons& icons, const Dock& dock, bool* open) {
  const auto& D = *w.defs;
  if (!begin_panel("##military", dock)) {
    end_panel();
    return;
  }
  {
    int units = 0, enemies = 0;
    for (u16 u = 0; u < military::kMaxUnits; ++u)
      if (w.army.units[u].alive) (w.army.units[u].side ? enemies : units)++;
    std::string sub =
        units == 0 ? std::string("No units raised") : std::to_string(units) + (units == 1 ? " unit" : " units");
    if (enemies) sub += "  \xC2\xB7  " + std::to_string(enemies) + " enemy " + (enemies == 1 ? "band" : "bands") + "!";
    if (!panel_header(icons, "ui/icon_military", false, "Military", sub, enemies ? kColBad : 0)) *open = false;
  }
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
  end_panel();
}

void draw_rome_panel(sim::World& w, const UiIcons& icons, const Dock& dock, bool* open) {
  const auto& D = *w.defs;
  if (!begin_panel("##rome", dock)) {
    end_panel();
    return;
  }
  if (!panel_header(
          icons, "ui/icon_rome", false, "Rome",
          std::string(w.empire ? "The Emperor" : "The Senate") + "  \xC2\xB7  favor " + std::to_string(w.favor)))
    *open = false;
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
    ImGui::TextColored(ok ? col4(kColGood) : ImGui::GetStyleColorVec4(ImGuiCol_Text), "%s %s", ok ? "[x]" : "[ ]",
                       t.c_str());
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
  end_panel();
}

void draw_advisors_panel(const sim::World& w, const UiIcons& icons, int* advisor, const Dock& dock, bool* open) {
  if (!begin_panel("##advisors", dock)) {
    end_panel();
    return;
  }
  if (!panel_header(icons, kAdvisorSprites[*advisor], false, "Advisors", kAdvisorNames[*advisor])) *open = false;
  for (int a = 1; a <= 5; ++a) {
    if (a > 1) ImGui::SameLine();
    ImGui::PushID(a);
    if (icons.icon_button("##adv", kAdvisorSprites[a], 56, *advisor == a)) *advisor = a;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kAdvisorNames[a]);
    ImGui::PopID();
  }
  divider();
  for (const auto& line : advisor_report(w, *advisor)) ImGui::TextWrapped("%s", line.c_str());
  end_panel();
}

void draw_messages(sim::World& w, const UiIcons& icons, u16* last_seen) {
  // Show the oldest unread message (or unanswered choice) as a dialogue.
  for (auto& m : w.messages) {
    if (m.id <= *last_seen && (m.options.empty() || m.answered)) continue;
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.35f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize({460, 0}, ImGuiCond_Appearing);
    begin_window(("##msg" + std::to_string(m.id)).c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
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
  begin_window("##end", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);
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
  begin_window("Threats (debug)", open);
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

BuildingId draw_alerts_panel(const sim::World& w, const UiIcons& icons, const Dock& dock, bool* open) {
  BuildingId jump;
  if (!begin_panel("##alerts", dock)) {
    end_panel();
    return jump;
  }
  if (!panel_header(icons, "ui/icon_alerts", false, "Alerts",
                    w.alerts.empty() ? std::string("All is quiet")
                                     : std::to_string(w.alerts.size()) + (w.alerts.size() == 1 ? " alert" : " alerts") +
                                           ", newest first"))
    *open = false;
  for (auto it = w.alerts.rbegin(); it != w.alerts.rend(); ++it) {
    std::string t = "Day " + std::to_string(sim::day_index(it->t) + 1) + "  " + alert_text(w, *it);
    if (it->count > 1) t += "  (x" + std::to_string(it->count) + ")";
    t += "##" + std::to_string(it - w.alerts.rbegin());
    if (ImGui::Selectable(t.c_str()) && it->b.valid()) jump = it->b;
  }
  end_panel();
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
