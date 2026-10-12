#!/usr/bin/env bash
# Rebuilds the main menu's backdrop city (data/menu/backdrop): the autoplay bot plays a campaign
# mission headless, and the finished city is frozen into a scripted scenario (urbs_headless
# --export-city).  Needs a release build.  Usage: tools/menu_backdrop.sh [mission] [ticks]
# Re-run it when building defs change in a way the old backdrop no longer loads (the menu then
# falls back to a plain landscape and logs "menu backdrop: ..."); see docs/07-art-pipeline.md.
set -euo pipefail
cd "$(dirname "$0")/.."
MISSION=${1:-m05_syracuse}
TICKS=${2:-110000}
rm -rf data/menu/backdrop
./build/release/urbs_headless --scenario "data/scenarios/$MISSION/scenario.json" --ticks "$TICKS" --autoplay \
  --keep-playing --export-city data/menu/backdrop | tail -n 3
