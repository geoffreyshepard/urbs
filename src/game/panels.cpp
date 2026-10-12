#include "game/panels.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

#include "game/advisors.hpp"
#include "game/iso_draw.hpp"
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
  std::string bname =
      a.b.valid() && w.buildings.alive(a.b) ? D.b(w.buildings[a.b].def).name : "A building";  // gone since
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
    case sim::AlertKind::StorageFull:  // arg: a producer's output good + 1, or 0 for a store
      if (a.arg > 0 && a.arg - 1u < D.goods.size())
        return bname + " is full: no store has room for its " + D.goods[a.arg - 1u].name;
      return bname + " is full";
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
  if (!panel_header(icons, d.id, Pic::Thumb, tier ? tier->name : d.name, sub, sub_col)) sel = Selection{};
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
  if (!panel_header(icons, "ui/icon_population", Pic::Icon, w.citizen_name(i), sub)) sel = Selection{};
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
  if (!panel_header(icons, "ui/icon_labor", Pic::Icon, "Labor",
                    with_commas(workforce) + " of working age  \xC2\xB7  " + with_commas(unemployed) + " without work",
                    workforce && unemployed * 10 > workforce ? kColBad : 0))
    *open = false;
  ImGui::TextDisabled("Short of hands, the sectors with the lowest number are staffed first.");
  section("Sectors");
  const float fs = ImGui::GetFontSize();
  const float x0 = ImGui::GetCursorScreenPos().x;
  for (int sct = 0; sct < sim::kSectorCount; ++sct) {
    ImGui::PushID(sct);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(sim::sector_name(static_cast<sim::Sector>(sct)));
    ImGui::SameLine(0, 0);
    ImGui::SetCursorScreenPos(ImVec2(x0 + std::round(fs * 6.5f), ImGui::GetCursorScreenPos().y));
    const float f = frac(filled[sct], jobs[sct]);
    bar(std::round(fs * 6), jobs[sct] ? f : 0.0f, jobs[sct] ? level_col(f) : kColPlain);
    ImGui::SameLine(0, std::round(fs * 0.6f));
    ImGui::Text("%s / %s", with_commas(filled[sct]).c_str(), with_commas(jobs[sct]).c_str());
    int pr = w.policy.sector_priority[sct];
    if (stepper("##prio", &pr, 1, 5)) w.policy.sector_priority[sct] = static_cast<u8>(pr);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Priority: 1 is staffed first, 5 last");
    ImGui::PopID();
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
    if (!panel_header(icons, "ui/icon_finance", Pic::Icon, "Finance",
                      with_commas(w.econ.treasury) + " denarii  \xC2\xB7  " + signed_commas(net) + " yesterday",
                      net >= 0 ? kColGood : kColBad))
      *open = false;
  }
  // The ledger: income and spending for yesterday, this year and last year.
  const auto& L = w.econ.money;
  const float fs = ImGui::GetFontSize();

  if (ImGui::BeginTable("##ledger", 4)) {
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
    for (const char* h : {"Yesterday", "This year", "Last year"})
      ImGui::TableSetupColumn(h, ImGuiTableColumnFlags_WidthFixed, std::round(fs * 5.2f));
    const char* heads[] = {"", "Yesterday", "This year", "Last year"};
    table_headings(heads, 4);
    i64 net[3] = {};
    for (int income = 1; income >= 0; --income) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::PushFont(ui_fonts().heading, std::round(fs * 0.82f));
      ImGui::TextDisabled("%s", income ? "Income" : "Spending");
      ImGui::PopFont();
      for (int k = 0; k < econ::kMoneyCount; ++k) {
        const auto m = static_cast<econ::Money>(k);
        if (econ::money_is_income(m) != (income == 1)) continue;
        const i64 sgn = income ? 1 : -1;
        net[0] += sgn * L.yesterday[k];
        net[1] += sgn * L.year[k];
        net[2] += sgn * L.last_year[k];
        if (!L.yesterday[k] && !L.year[k] && !L.last_year[k]) continue;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(econ::money_name(m));
        for (i64 v : {L.yesterday[k], L.year[k], L.last_year[k]}) {
          ImGui::TableNextColumn();
          cell_right(with_commas(v));
        }
      }
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    {
      // A rule above the net line.
      const ImVec2 p = ImGui::GetCursorScreenPos();
      const float x1 = panel_right();
      ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y + 1), ImVec2(x1, p.y + 1),
                                          ImGui::GetColorU32(ImGuiCol_Border), 1.5f);
      ImGui::Dummy(ImVec2(0, 3));
    }
    ImGui::PushFont(ui_fonts().bold, 0.0f);
    ImGui::TextUnformatted("Net");
    ImGui::PopFont();
    for (i64 v : net) {
      ImGui::TableNextColumn();
      ImGui::Dummy(ImVec2(0, 3));
      cell_right(signed_commas(v), v >= 0 ? kColGood : kColBad, true);
    }
    ImGui::EndTable();
  }

  auto cmd_policy = [&](sim::PolicyId id, int v) {
    sim::Command c;
    c.type = sim::CmdType::SetPolicy;
    c.def = static_cast<u16>(id);
    c.value = v;
    w.push(c);
  };
  auto tip = [](const char* t) {
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t);
  };
  section("Wages and taxes");
  struct Rate {
    const char* label;
    int v, lo, hi;
    const char* unit;
    sim::PolicyId id;
    const char* tip;
  } rates[] = {{"Wage per worker a day", w.policy.wage, 1, 20, "", sim::PolicyId::Wage,
                "Better pay draws workers and cheers them; the city pays it every day"},
               {"Tax on plebeians", w.policy.tax_rate[2], 0, 30, "%", sim::PolicyId::TaxPleb,
                "Higher taxes earn more but make people unhappy"},
               {"Tax on equites", w.policy.tax_rate[1], 0, 30, "%", sim::PolicyId::TaxEques,
                "Higher taxes earn more but make people unhappy"},
               {"Tax on patricians", w.policy.tax_rate[0], 0, 30, "%", sim::PolicyId::TaxPatrician,
                "Higher taxes earn more but make people unhappy"}};
  for (Rate& r : rates) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(r.label);
    tip(r.tip);
    if (stepper(r.label, &r.v, r.lo, r.hi, r.unit)) cmd_policy(r.id, r.v);
  }

  section("Slaves and the poor");
  const char* treat[] = {"Harsh", "Fair", "Kind"};
  const char* manu[] = {"Never", "Sometimes", "Often"};
  const char* dole[] = {"None", "The poor", "Everyone"};
  const float each = std::round(fs * 5.4f);
  int st = w.policy.slave_treatment, mn = w.policy.manumission, ra = w.policy.rations;
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Treatment");
  tip("How public and private slaves are treated: harsh treatment risks revolt");
  if (choice("##treat", &st, treat, 3, each)) cmd_policy(sim::PolicyId::SlaveTreatment, st);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Manumission");
  tip("How often masters free their slaves, who become freedmen");
  if (choice("##manu", &mn, manu, 3, each)) cmd_policy(sim::PolicyId::Manumission, mn);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Grain dole");
  tip("The annona: free grain from the granaries for the poor, or for everyone");
  if (choice("##dole", &ra, dole, 3, each)) cmd_policy(sim::PolicyId::Rations, ra);
  ImGui::Spacing();
  if (coin_button(icons, "Buy 5 public slaves", 600, w.econ.treasury >= 600)) {
    sim::Command c;
    c.type = sim::CmdType::BuySlaves;
    c.value = 5;
    w.push(c);
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("120 denarii each; they need room in slave quarters");
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
            icons, "ui/icon_trade", Pic::Icon, "Trade",
            n == 0 ? std::string("No routes open yet") : std::to_string(n) + (n == 1 ? " route open" : " routes open")))
      *open = false;
  }
  const float fs = ImGui::GetFontSize();
  // A row of goods pictures, each with its price under the hover.
  auto goods_row = [&](const char* label, u32 partner, const std::vector<data::GoodAmount>& list, bool buying) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(std::round(fs * 3.2f));
    for (size_t k = 0; k < list.size(); ++k) {
      const auto& g = D.goods[list[k].good.v];
      if (k) ImGui::SameLine(0, std::round(fs * 0.5f));
      if (ImGui::GetCursorScreenPos().x + fs * 3 > ImGui::GetWindowPos().x + ImGui::GetWindowWidth()) {
        ImGui::NewLine();
        ImGui::SameLine(std::round(fs * 3.2f));
      }
      ImGui::BeginGroup();
      good_icon(icons, g.id);
      ImGui::SameLine(0, 2);
      ImGui::Text("%u", w.trade_price(partner, list[k].good, buying) / 10);
      ImGui::EndGroup();
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s: %s at %u denarii", g.name.c_str(), buying ? "they sell" : "they buy",
                          w.trade_price(partner, list[k].good, buying) / 10);
    }
  };
  section("Routes");
  for (u32 p = 0; p < w.routes.size(); ++p) {
    const auto& rs = w.routes[p];
    if (!rs.available) continue;
    const auto& pd = D.partners[p];
    ImGui::PushID(static_cast<int>(p));
    ImGui::AlignTextToFramePadding();
    ImGui::PushFont(ui_fonts().bold, 0.0f);
    ImGui::TextUnformatted(pd.name.c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled(pd.sea ? "by sea" : "by land");
    if (rs.open) {
      text_right("open", kColGood);
    } else {
      ImGui::SameLine(0, 0);
      const float bw = std::round(fs * 8.5f);
      ImGui::SetCursorScreenPos(ImVec2(panel_right() - bw, ImGui::GetCursorScreenPos().y));
      if (coin_button(icons, "Open route", pd.open_cost, w.econ.treasury >= pd.open_cost)) {
        sim::Command c;
        c.type = sim::CmdType::OpenRoute;
        c.def = static_cast<u16>(p);
        w.push(c);
      }
    }
    goods_row("Buys", p, pd.buys, false);
    goods_row("Sells", p, pd.sells, true);
    ImGui::Spacing();
    ImGui::PopID();
  }

  section("What to trade");
  ImGui::TextWrapped(
      "Import keeps a good up to the level, export sells what is above it. Stockpile keeps it in "
      "storage so markets and workshops leave it, e.g. for Rome.");
  if (ImGui::BeginTable("##tg", 4, ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, std::round(fs * 3.2f));
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, std::round(fs * 6.2f));
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::GetFrameHeight() * 2 + std::round(fs * 3.4f) + 10);
    const char* heads[] = {"Good", "Stock", "", "Level"};
    table_headings(heads, 4);
    const char* modes[] = {"-", "Import", "Export", "Stockpile"};
    for (u32 g = 0; g < D.goods.size(); ++g) {
      ImGui::PushID(static_cast<int>(g));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      good_icon(icons, D.goods[g].id);
      ImGui::SameLine();
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(D.goods[g].name.c_str());
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      cell_right(with_commas(w.econ.city_stock[g]));
      ImGui::TableNextColumn();
      int m = w.trade_mode[g];
      ImGui::SetNextItemWidth(-FLT_MIN);
      bool changed = ImGui::Combo("##m", &m, modes, 4);
      ImGui::TableNextColumn();
      int lvl = w.trade_level[g];
      if (m == 1 || m == 2) {
        ImGui::Dummy(ImVec2(0, 0));
        changed |= stepper("##l", &lvl, 0, 1000, "", 25);
      } else {
        ImGui::AlignTextToFramePadding();
        cell_right("\xE2\x80\x94", ImGui::GetColorU32(ImGuiCol_TextDisabled));
      }
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
    if (!panel_header(icons, "ui/icon_religion", Pic::Icon, "Religion",
                      weather + " today  \xC2\xB7  " + std::to_string(pleased) + " pleased, " + std::to_string(angry) +
                          " displeased",
                      angry > pleased ? kColBad : 0))
      *open = false;
  }
  const float fs = ImGui::GetFontSize(), pic = std::round(fs * 2.8f);
  const i64 pop = static_cast<i64>(w.population());
  for (u32 g = 0; g < D.gods.size(); ++g) {
    if (!w.god_available[g]) continue;
    const auto& god = D.gods[g];
    ImGui::PushID(static_cast<int>(g));
    const u8 m = w.god_mood[g];
    const char* mood = m >= 85 ? "exalted" : m >= 65 ? "pleased" : m >= 35 ? "indifferent" : m >= 15 ? "displeased" : "wrathful";
    const ImU32 col = m >= 65 ? kColGood : m >= 35 ? kColWarn : kColBad;
    // Portrait: the rendered medallion carries its own bronze frame.
    const ImVec2 p = ImGui::GetCursorScreenPos();
    icons.draw_icon("ui/god_" + god.id, p, pic);
    ImGui::Dummy(ImVec2(pic, pic));
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s\nFeast day: day %u of the year", god.name.c_str(), god.festival_day + 1u);
    ImGui::SameLine(0, std::round(fs * 0.7f));
    ImGui::BeginGroup();
    ImGui::PushFont(ui_fonts().bold, 0.0f);
    ImGui::TextUnformatted(god.name.c_str());
    ImGui::PopFont();
    ImGui::TextDisabled("%s", god.domain.c_str());
    ImGui::EndGroup();
    ImGui::SameLine(0, 0);
    const float right = panel_right();
    const float bw = std::round(fs * 5), btn = std::round(fs * 5.2f);
    ImGui::SetCursorScreenPos(ImVec2(right - btn - fs - bw, p.y + std::round(fs * 0.25f)));
    ImGui::BeginGroup();
    bar(bw, static_cast<float>(m) / 100.0f, col);
    ImGui::PushStyleColor(ImGuiCol_Text, col == kColWarn ? ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled) : col4(col));
    ImGui::TextUnformatted(mood);
    ImGui::PopStyleColor();
    ImGui::EndGroup();
    ImGui::SameLine(0, 0);
    ImGui::SetCursorScreenPos(ImVec2(right - btn, p.y + std::round((pic - ImGui::GetFrameHeight()) * 0.5f)));
    if (ImGui::Button("Festival", ImVec2(btn, 0))) ImGui::OpenPopup("##fest");
    if (ImGui::BeginPopup("##fest")) {
      const bool feast = sim::day_of_year(w.now) == god.festival_day;
      ImGui::PushFont(ui_fonts().bold, 0.0f);
      ImGui::Text("A festival for %s", god.name.c_str());
      ImGui::PopFont();
      ImGui::TextDisabled(feast ? "Today is the feast day: twice the effect" : "Feast day: day %u of the year",
                          god.festival_day + 1u);
      for (int size : {1, 3}) {
        const i64 cost = pop * size / 2 + 100 * size;
        if (coin_button(icons, size == 1 ? "Festival (1 day)" : "Grand festival (3 days)", cost,
                        w.econ.treasury >= cost)) {
          sim::Command c;
          c.type = sim::CmdType::Festival;
          c.def = static_cast<u16>(g);
          c.value = size;
          w.push(c);
          ImGui::CloseCurrentPopup();
        }
      }
      ImGui::EndPopup();
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + pic + ImGui::GetStyle().ItemSpacing.y));
    ImGui::Dummy(ImVec2(0, 0));
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
    if (!panel_header(icons, "ui/icon_military", Pic::Icon, "Military", sub, enemies ? kColBad : 0)) *open = false;
  }
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextWrapped("Click a soldier or a unit below, then right-click the map to march. Raise units at a barracks.");
  ImGui::PopStyleColor();
  const float fs = ImGui::GetFontSize();
  section("Recruitment");
  {
    const char* consc[] = {"Volunteers", "Employed too", "Full levy"};
    int c = w.policy.conscription;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Who serves");
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip(
          "Volunteers: only men without work. Employed too: workers can be called up. Full levy: everyone");
    if (choice("##consc", &c, consc, 3, std::round(fs * 5.6f))) {
      sim::Command cmd;
      cmd.type = sim::CmdType::SetPolicy;
      cmd.def = static_cast<u16>(sim::PolicyId::Conscription);
      cmd.value = c;
      w.push(cmd);
    }
    value_row("Soldiers lost", with_commas(w.soldiers_lost_));
    value_row("Enemies slain", with_commas(w.enemies_killed_));
  }
  section("Units");
  bool any = false;
  for (u16 u = 0; u < military::kMaxUnits; ++u) {
    const auto& un = w.army.units[u];
    if (!un.alive) continue;
    any = true;
    const auto& ud = D.units[un.def.v];
    ImGui::PushID(u);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float rh = std::round(fs * 2.5f);
    if (ImGui::Selectable("##unit", sel.unit == u, 0, ImVec2(0, rh))) sel.unit = u;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const std::string title =
        un.side ? "Enemy " + ud.name : "Cohors " + std::to_string(un.number) + "  \xC2\xB7  " + ud.name;
    const std::string sub = std::to_string(un.count) + " of " + std::to_string(un.initial) + " men  \xC2\xB7  morale " +
                            std::to_string(un.morale) + "  \xC2\xB7  " + military::order_name(un.order);
    dl->AddText(ui_fonts().bold, fs, ImVec2(p.x + 6, p.y + 2), un.side ? kColBad : ImGui::GetColorU32(ImGuiCol_Text),
                title.c_str());
    dl->AddText(ImVec2(p.x + 6, p.y + fs * 1.2f), ImGui::GetColorU32(ImGuiCol_TextDisabled), sub.c_str());
    ImGui::PopID();
  }
  if (!any) ImGui::TextUnformatted("No units in the field.");
  if (sel.unit < military::kMaxUnits && w.army.units[sel.unit].alive && w.army.units[sel.unit].side == 0) {
    const auto& un = w.army.units[sel.unit];
    section(("Cohors " + std::to_string(un.number)).c_str());
    auto cmd = [&](sim::CmdType t, i32 v) {
      sim::Command k;
      k.type = t;
      k.def = sel.unit;
      k.value = v;
      w.push(k);
    };
    meter_row("Strength", frac(un.count, un.initial), level_col(frac(un.count, un.initial)),
              std::to_string(un.count) + " / " + std::to_string(un.initial));
    meter_row("Morale", frac(un.morale, 100), level_col(frac(un.morale, 100)), std::to_string(un.morale));
    meter_row("Experience", frac(un.exp, 100), kColPlain, std::to_string(un.exp));
    const char* forms[] = {"Line", "Wedge", "Testudo", "Loose"};
    int f = static_cast<int>(un.formation);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Formation");
    if (choice("##form", &f, forms, 4, std::round(fs * 4.2f))) cmd(sim::CmdType::UnitFormation, f);
    const char* stances[] = {"Aggressive", "Defensive"};
    int st = un.stance == military::Stance::Defensive ? 1 : 0;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Stance");
    if (choice("##stance", &st, stances, 2, std::round(fs * 5.6f))) cmd(sim::CmdType::UnitStance, st);
    ImGui::Spacing();
    if (ImGui::Button("Return to barracks")) cmd(sim::CmdType::UnitOrder, static_cast<i32>(military::Order::Return));
    ImGui::SameLine();
    if (ImGui::Button("Disband")) cmd(sim::CmdType::DisbandUnit, 0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Campaign abroad (8 days)");
    ImGui::SameLine();
    const char* nm[] = {"Skirmish", "Campaign", "Conquest"};
    for (int lv = 1; lv <= 3; ++lv) {
      if (lv > 1) ImGui::SameLine();
      if (ImGui::Button(nm[lv - 1])) cmd(sim::CmdType::Campaign, lv);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip(lv == 1   ? "Small risk, small plunder"
                          : lv == 2 ? "More risk, more plunder"
                                    : "Great risk, great plunder");
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
          icons, "ui/icon_rome", Pic::Icon, "Rome",
          std::string(w.empire ? "The Emperor" : "The Senate") + "  \xC2\xB7  favor " + std::to_string(w.favor)))
    *open = false;
  const float fs = ImGui::GetFontSize();
  // One goal: a tick, its picture, its name, a bar towards it and where the city stands.
  auto goal_row = [&](bool ok, auto&& picture, const std::string& name, float frac, float tick,
                      const std::string& value) {
    ImGui::BeginGroup();
    check_mark(ok);
    ImGui::SameLine();
    picture();
    ImGui::SameLine();
    ImGui::TextUnformatted(name.c_str());
    if (frac >= 0) {
      ImGui::SameLine(0, 0);
      const float bx = panel_right() - fs * 13;
      ImGui::SetCursorScreenPos(ImVec2(bx, ImGui::GetCursorScreenPos().y));
      bar(fs * 8, frac, ok ? kColGood : kColWarn, tick);
    }
    text_right(value, ok ? kColGood : 0);
    ImGui::EndGroup();
  };
  auto icon = [&](const char* name) { return [&icons, name]() { inline_icon(icons, name); }; };
  struct Rating {
    const char* name;
    const char* icon;
    int v, goal;
  } ratings[] = {{"Culture", "ui/icon_culture", w.ratings.culture, w.goals.culture},
                 {"Prosperity", "ui/icon_prosperity", w.ratings.prosperity, w.goals.prosperity},
                 {"Security", "ui/icon_security", w.ratings.security, w.goals.security},
                 {"Favor", "ui/icon_favor", w.favor, w.goals.favor}};
  int goals = 0, met = 0;
  auto count = [&](bool want, bool ok) {
    goals += want ? 1 : 0;
    met += want && ok ? 1 : 0;
  };
  const u32 pop = w.population();
  count(w.goals.population > 0, pop >= w.goals.population);
  for (const Rating& r : ratings) count(r.goal > 0, r.v >= r.goal);
  count(w.goals.requests > 0, w.requests_fulfilled >= w.goals.requests);
  count(w.goals.survive_days > 0, w.mission_days >= w.goals.survive_days);
  for (const auto& g : w.goals.buildings) count(true, w.count_active(g.def) >= g.count);

  if (goals) {
    section(("Mission goals  \xC2\xB7  " + std::to_string(met) + " of " + std::to_string(goals) + " met").c_str());
    if (w.goals.population)
      goal_row(pop >= w.goals.population, icon("ui/icon_population"), "Population", frac(pop, w.goals.population), -1,
               with_commas(pop) + " / " + with_commas(w.goals.population));
    for (const Rating& r : ratings)
      if (r.goal)
        goal_row(r.v >= r.goal, icon(r.icon), r.name, static_cast<float>(r.v) / 100.0f,
                 static_cast<float>(r.goal) / 100.0f, std::to_string(r.v) + " / " + std::to_string(r.goal));
    if (w.goals.requests)
      goal_row(w.requests_fulfilled >= w.goals.requests, icon("ui/icon_rome"), "Requests sent",
               frac(w.requests_fulfilled, w.goals.requests), -1,
               std::to_string(w.requests_fulfilled) + " / " + std::to_string(w.goals.requests));
    if (w.goals.survive_days)
      goal_row(w.mission_days >= w.goals.survive_days, icon("ui/icon_play"), "Days endured",
               frac(w.mission_days, w.goals.survive_days), -1,
               std::to_string(w.mission_days) + " / " + std::to_string(w.goals.survive_days));
    for (const auto& g : w.goals.buildings) {
      const u32 have = w.count_active(g.def);
      const std::string id = D.b(g.def).id;
      goal_row(
          have >= g.count, [&icons, id]() { inline_thumb(icons, id); },
          std::to_string(g.count) + " \xC3\x97 " + D.b(g.def).name, -1, -1,
          have >= g.count ? std::string("built") : std::to_string(have) + " / " + std::to_string(g.count));
    }
  }
  // Ratings the mission does not ask for.
  bool others = false;
  for (const Rating& r : ratings) others |= r.goal == 0;
  if (others) {
    section("Standing");
    for (const Rating& r : ratings) {
      if (r.goal) continue;
      ImGui::BeginGroup();
      inline_icon(icons, r.icon);
      ImGui::SameLine();
      ImGui::TextUnformatted(r.name);
      ImGui::SameLine(0, 0);
      const float bx = panel_right() - fs * 13;
      ImGui::SetCursorScreenPos(ImVec2(bx, ImGui::GetCursorScreenPos().y));
      bar(fs * 8, static_cast<float>(r.v) / 100.0f, kColPlain);
      text_right(std::to_string(r.v));
      ImGui::EndGroup();
    }
  }

  section("Requests from Rome");
  bool any = false;
  for (const auto& r : w.requests) {
    if (!r.active) continue;
    any = true;
    const u64 days = r.deadline > w.now ? (r.deadline - w.now) / kTicksPerDay : 0;
    ImGui::PushID(r.id);
    ImGui::BeginGroup();
    if (r.kind == 0)
      good_icon(icons, D.goods[r.good.v].id, std::round(fs * 1.6f));
    else
      inline_icon(icons, r.kind == 1 ? "goods/denarii_16" : "ui/icon_military", std::round(fs * 1.6f));
    ImGui::SameLine();
    ImGui::BeginGroup();
    const std::string what = r.kind == 0   ? with_commas(r.amount) + " " + D.goods[r.good.v].name
                             : r.kind == 1 ? with_commas(r.amount) + " denarii"
                                           : "A unit of " + std::to_string(r.amount) + " soldiers";
    ImGui::PushFont(ui_fonts().bold, 0.0f);
    ImGui::TextUnformatted(what.c_str());
    ImGui::PopFont();
    std::string when = "within " + std::to_string(days) + (days == 1 ? " day" : " days");
    if (r.kind == 0) when += "  \xC2\xB7  " + with_commas(w.stored_amount(r.good)) + " in storage";
    if (r.reward > 0) when += "  \xC2\xB7  pays " + with_commas(r.reward);
    ImGui::PushStyleColor(ImGuiCol_Text, days <= 2 ? col4(kColBad) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted(when.c_str());
    ImGui::PopStyleColor();
    ImGui::EndGroup();
    ImGui::EndGroup();
    ImGui::SameLine(0, 0);
    const float bw = std::round(fs * 4.2f);
    ImGui::SetCursorScreenPos(ImVec2(panel_right() - bw, ImGui::GetCursorScreenPos().y + fs * 0.3f));
    if (ImGui::Button("Send", ImVec2(bw, 0))) {
      sim::Command c;
      c.type = sim::CmdType::FulfillRequest;
      c.def = r.id;
      w.push(c);
    }
    ImGui::PopID();
  }
  if (!any) ImGui::TextUnformatted("Nothing is asked of us now.");
  ImGui::TextDisabled("%u sent  \xC2\xB7  %u missed", w.requests_fulfilled, w.requests_failed);

  section("Loan");
  if (w.loan.remaining > 0) {
    value_row("Still owed", with_commas(w.loan.remaining) + " denarii");
    ImGui::TextDisabled("Repaid at %s a day", with_commas(w.loan.installment).c_str());
  } else {
    if (coin_button(icons, "Borrow", 2000, true)) {
      sim::Command c;
      c.type = sim::CmdType::TakeLoan;
      c.value = 2000;
      w.push(c);
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("repaid with 20%% interest; costs some favor");
  }
  end_panel();
}

void draw_advisors_panel(const sim::World& w, const UiIcons& icons, int* advisor, const Dock& dock, bool* open) {
  if (!begin_panel("##advisors", dock)) {
    end_panel();
    return;
  }
  if (!panel_header(icons, kAdvisorSprites[*advisor], Pic::Portrait, "Advisors", kAdvisorNames[*advisor]))
    *open = false;
  for (int a = 1; a <= 5; ++a) {
    if (a > 1) ImGui::SameLine();
    ImGui::PushID(a);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(std::round(ImGui::GetFontSize() * 0.4f), std::round(ImGui::GetFontSize() * 0.4f)));
    if (icons.icon_button("##adv", kAdvisorSprites[a], std::round(ImGui::GetFontSize() * 2.8f), *advisor == a, true))
      *advisor = a;
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kAdvisorNames[a]);
    ImGui::PopID();
  }
  divider();
  for (const auto& line : advisor_report(w, *advisor)) ImGui::TextWrapped("%s", line.c_str());
  end_panel();
}

void draw_messages(sim::World& w, const UiIcons& icons, u16* last_seen) {
  // Show the oldest unread message (or unanswered choice) as a letter: the advisor's portrait and name, the title,
  // the text, and the answers.
  for (auto& m : w.messages) {
    if (m.answered || (m.options.empty() && m.id <= *last_seen)) continue;  // answered, or read
    // A choice already made is on its way to the sim (applied at the next tick): don't offer it twice.
    if (std::any_of(w.commands.begin(), w.commands.end(),
                    [&](const sim::Command& c) { return c.type == sim::CmdType::Answer && c.def == m.id; }))
      continue;
    const float fs = ImGui::GetFontSize();
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.4f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize({std::round(fs * 31), 0}, ImGuiCond_Appearing);
    begin_window(("##msg" + std::to_string(m.id)).c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    keep_in_front();
    if (m.advisor > 0 && m.advisor <= 5) {
      content_bounds();
      const float pic = std::round(fs * 4.2f);
      const ImVec2 p = ImGui::GetCursorScreenPos();
      framed_picture(icons, kAdvisorSprites[m.advisor], Pic::Portrait, p, pic);
      ImGui::Dummy(ImVec2(pic, pic));
      ImGui::SameLine(0, std::round(fs * 0.8f));
      ImGui::BeginGroup();
      ImGui::Dummy(ImVec2(0, std::round(fs * 0.4f)));
      ImGui::PushFont(ui_fonts().heading, std::round(fs * 0.86f));
      ImGui::PushStyleColor(ImGuiCol_Text, kColSection);
      ImGui::TextUnformatted(kAdvisorNames[m.advisor]);
      ImGui::PopStyleColor();
      ImGui::PopFont();
      ImGui::PushTextWrapPos(panel_right() - ImGui::GetWindowPos().x);
      heading(m.title.c_str(), 1.3f);
      ImGui::PopTextWrapPos();
      ImGui::EndGroup();
      divider();
    } else {
      menu_title(m.title.c_str());
    }
    ImGui::TextWrapped("%s", m.text.c_str());
    ImGui::Dummy(ImVec2(0, std::round(fs * 0.5f)));
    if (m.options.empty()) {
      if (dialog_button("Very well", std::round(fs * 9))) {
        // Read: hidden at once, and marked answered in the world so a saved game remembers it.
        *last_seen = std::max(*last_seen, m.id);
        sim::Command c;
        c.type = sim::CmdType::Answer;
        c.def = m.id;
        c.value = 1;
        w.push(c);
      }
    } else {
      for (size_t k = 0; k < m.options.size(); ++k)
        if (dialog_button((m.options[k] + "##o" + std::to_string(k)).c_str())) {
          sim::Command c;
          c.type = sim::CmdType::Answer;
          c.def = m.id;
          c.value = static_cast<i32>(k + 1);
          w.push(c);  // the sim marks it answered and runs the script's callback
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
  ImGui::SetNextWindowSize(ImVec2(std::round(ImGui::GetFontSize() * 26), 0), ImGuiCond_Always);
  begin_window("##end", nullptr, ImGuiWindowFlags_NoTitleBar);
  if (w.mission == sim::MissionState::Won) {
    menu_title("Victoria!");
    ImGui::TextWrapped("The Senate is pleased with your work. A greater task awaits.");
  } else {
    menu_title("Defeat");
    ImGui::TextWrapped("%s", w.lost_reason.c_str());
  }
  ImGui::Spacing();
  if (dialog_button("Continue")) *dismissed = true;
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
  if (!panel_header(icons, "ui/icon_alerts", Pic::Icon, "Alerts",
                    w.alerts.empty() ? std::string("All is quiet")
                                     : std::to_string(w.alerts.size()) + (w.alerts.size() == 1 ? " alert" : " alerts") +
                                           ", newest first"))
    *open = false;
  // Newest first, under a heading per day; a coloured stripe says how bad it is. A click goes to the building.
  const float fs = ImGui::GetFontSize();
  u64 day = ~0ull;
  int k = 0;
  for (auto it = w.alerts.rbegin(); it != w.alerts.rend(); ++it, ++k) {
    const u64 d = sim::day_index(it->t);
    if (d != day) {
      day = d;
      section(("Day " + std::to_string(d + 1)).c_str());
    }
    using K = sim::AlertKind;
    const K kind = it->kind;
    const bool bad = kind == K::Fire || kind == K::Collapse || kind == K::Disease || kind == K::Crime ||
                     kind == K::Riot || kind == K::Revolt || kind == K::Raid || kind == K::GodAngry ||
                     kind == K::TreasuryLow || kind == K::Homeless;
    const bool good = kind == K::Immigrants || kind == K::Dedicated || kind == K::Request ||
                      (kind == K::GodAngry && (it->arg & 0x100));
    const ImU32 col = good ? kColGood : bad ? kColBad : kColWarn;
    std::string t = alert_text(w, *it);
    // How many times it came up; not for the daily tallies, whose number already says it.
    if (it->count > 1 && it->kind != sim::AlertKind::Immigrants && it->kind != sim::AlertKind::Emigrants)
      t += "  \xC3\x97" + std::to_string(it->count);
    ImGui::PushID(k);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool there = it->b.valid() && w.buildings.alive(it->b);
    const float sw = std::max(3.0f, std::round(fs * 0.22f));
    // Measured at the width it is drawn at, so a long alert never spills out of its row.
    const float wrap = ImGui::GetContentRegionAvail().x - sw - std::round(fs * 0.5f) - std::round(fs * 0.25f);
    const float h = ImGui::CalcTextSize(t.c_str(), nullptr, false, wrap).y + 4;
    if (ImGui::Selectable("##a", false, there ? 0 : ImGuiSelectableFlags_Disabled, ImVec2(0, h)) && there) jump = it->b;
    if (there && ImGui::IsItemHovered()) ImGui::SetTooltip("Go there");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(p.x, p.y + 2), ImVec2(p.x + sw, p.y + h - 2), col, 1);
    dl->AddText(ui_fonts().body, fs, ImVec2(p.x + sw + fs * 0.5f, p.y + 2), ImGui::GetColorU32(ImGuiCol_Text),
                t.c_str(), nullptr, wrap);
    ImGui::PopID();
  }
  end_panel();
  return jump;
}

void draw_selection(const sim::World& w, const Selection& sel, render::Renderer& r, f32 alpha) {
  namespace iso = render::iso;
  const u32 mark = render::rgba(255, 240, 120, 220);
  // A flat ring under someone's feet.
  auto foot_ring = [&](Vec2 foot, f32 rx) {
    for (int k = 0; k < 16; ++k) {
      const f32 a = static_cast<f32>(k) * 0.3927f;
      r.push_rect(foot.x + std::cos(a) * rx - 3.0f, foot.y + std::sin(a) * rx * 0.5f - 2.0f, 6.0f, 4.0f, 0.01f, mark);
    }
  };
  if (sel.unit < military::kMaxUnits && w.army.units[sel.unit].alive) {
    const auto& A = w.army;
    for (u32 i = 0; i < A.ids.high_water(); ++i)
      if (A.ids.alive_index(i) && A.unit[i] == sel.unit) {
        const f32 tx = A.px[i] + (A.x[i] - A.px[i]) * alpha, ty = A.py[i] + (A.y[i] - A.py[i]) * alpha;
        foot_ring(iso::from_tile(tx, ty, iso::level_at(w.map, static_cast<i32>(tx), static_cast<i32>(ty))), 14.0f);
      }
    const auto& un = A.units[sel.unit];
    if (un.target_tile >= 0) {
      const i32 tx = un.target_tile % w.map.w, ty = un.target_tile / w.map.w;
      r.push_diamond(tx, ty, 1, 1, iso::level_at(w.map, tx, ty), 0.01f, render::rgba(255, 240, 120, 90));
    }
  }
  if (sel.building.valid() && w.buildings.alive(sel.building)) {
    const auto& b = w.buildings[sel.building];
    // Corner brackets (fx/select_WxH, ground-attached) where the art has them, else an outline.
    static render::Sprite brackets[4][4];
    static bool resolved = false;
    if (!resolved) {
      for (int sw = 1; sw <= 4; ++sw)
        for (int sh = 1; sh <= 4; ++sh)
          brackets[sw - 1][sh - 1] = r.atlas().find("fx/select_" + std::to_string(sw) + "x" + std::to_string(sh));
      resolved = true;
    }
    const f32 lv = iso::level_at(w.map, b.x, b.y);
    const render::Sprite* br = b.w <= 4 && b.h <= 4 ? &brackets[b.w - 1][b.h - 1] : nullptr;
    if (br && br->valid)
      r.push_translucent(iso_instance(*br, iso::footprint_anchor(b.x, b.y, b.w, b.h, lv), 0.01f, mark));
    else
      push_diamond_outline(r, b.x, b.y, b.w, b.h, lv, 0.01f, mark, 4.0f);
  }
  if (sel.citizen.valid() && w.citizens.alive(sel.citizen)) {
    Vec2 p;
    if (w.citizen_position(sel.citizen.index(), alpha, p, nullptr))
      foot_ring(iso::project(p, iso::level_at_world(w.map, p.x, p.y)), 16.0f);
  }
}

}  // namespace urbs::game
