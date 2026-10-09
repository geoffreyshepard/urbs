#pragma once
#include <string>
#include <vector>

#include "sim/world.hpp"

namespace urbs::game {

inline constexpr const char* kAdvisorNames[] = {"", "Vitruvius, chief engineer", "Marcus Calidius, quaestor",
                                                 "Spurinna, haruspex", "Quintus Labienus, legate", "Gnaeus Lollius, aedile"};
inline constexpr const char* kAdvisorSprites[] = {"", "ui/advisor_engineer", "ui/advisor_quaestor", "ui/advisor_haruspex",
                                                  "ui/advisor_legate", "ui/advisor_aedile"};

// Plain-language summary of one domain (advisor 1..5).
std::vector<std::string> advisor_report(const sim::World& w, int advisor);

}  // namespace urbs::game
