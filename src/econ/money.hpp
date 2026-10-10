#pragma once
#include "core/types.hpp"

namespace urbs::econ {

// Income categories first (money_is_income relies on it). Mint was added in save v3.
enum class Money : u8 { Taxes, Sales, Exports, Tribute, Mint, Wages, Construction, Imports, Upkeep, Festivals, Military, Other, Count };
inline constexpr int kMoneyCount = static_cast<int>(Money::Count);
inline constexpr bool money_is_income(Money m) { return m <= Money::Mint; }
inline constexpr const char* money_name(Money m) {
  constexpr const char* n[] = {"Taxes",  "Market sales", "Exports",   "Grants from Rome", "Coins from the mint", "Wages",
                               "Construction", "Imports", "Upkeep", "Festivals", "Military", "Other"};
  return n[static_cast<int>(m)];
}

struct Ledger {
  i64 today[kMoneyCount] = {};
  i64 yesterday[kMoneyCount] = {};
  i64 year[kMoneyCount] = {};
  i64 last_year[kMoneyCount] = {};
  void add(Money m, i64 v) {
    today[static_cast<int>(m)] += v;
    year[static_cast<int>(m)] += v;
  }
  void roll_day() {
    for (int k = 0; k < kMoneyCount; ++k) {
      yesterday[k] = today[k];
      today[k] = 0;
    }
  }
  void roll_year() {
    for (int k = 0; k < kMoneyCount; ++k) {
      last_year[k] = year[k];
      year[k] = 0;
    }
  }
};

}  // namespace urbs::econ
