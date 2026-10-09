# Urbs v0.1.0 — "Campaign vertical slice"

The first playable release: the opening four missions of the campaign.

## What's in it
- **Four campaign missions:** Latium, Ostia, Beneventum and Capua, from a hamlet of tents to a Campanian town of olive presses, wineries, patrician houses and a theater.
- **Every citizen is simulated:** names, homes, jobs, errands, needs, families, ageing, traits and memories. Click anyone to follow them.
- **Economy:**
  - Production chains and porters that carry goods.
  - Markets, dynamic prices, taxes and wages.
  - Land trade routes with import, export and stockpile settings.
- **Housing and services:** housing that grows with water, food, religion, hygiene, entertainment and goods, plus fire, collapse and disease.
- **Gods:** gods with moods, festivals and offerings.
- **Rome:** requests, favor, loans and civic research.
- **Military:** militia and legions, formations, raids and walls.
- **Interface:** advisors with portraits and plain-language reports; overlays for desirability, fire, crime, disease, water and coverage.
- **Difficulty:** Otium (relaxed), Civitas (normal) and Imperium (hard).
- **Sandbox mode** with generated maps.

## Requirements
macOS 13 or later on Apple Silicon (M1 or newer).

The app is ad-hoc signed, not notarized. To run it:
1. Open it once by right-clicking (or Control-clicking) and choosing **Open**.
2. Or clear the quarantine flag: `xattr -dr com.apple.quarantine Urbs.app`.

Saves and settings live in `~/Library/Application Support/urbs/`.

## Controls
- **Camera:** WASD or arrow keys, or drag with the right or middle mouse button. Scroll the wheel to zoom.
- **Speed:** Space pauses; 1, 2 and 3 set the speed.
- **Building:** B opens the build menu, R places roads, X demolishes, Esc cancels or opens the menu.
- **Overlays and following:** O opens overlays; F follows the selected citizen.
- **Saving:** F5 quicksaves, F9 quickloads.
- **Help:** the **Help** button explains the basics.

## License
- **Code:** PolyForm Noncommercial 1.0.0.
- **Art, data and docs:** CC BY-NC-SA 4.0.

You are free to play, modify and share the game, and to contribute. Selling it is not allowed. See LICENSE.md, LICENSE-ASSETS.txt and NOTICE.

## Known limitations
- Missions 5–16 are not in this release.
- There is no audio.
- Only a macOS build is provided; Windows and Linux build from source.
