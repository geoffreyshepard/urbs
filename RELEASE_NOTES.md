# Urbs v1.1.0

The city in three dimensions: Urbs is now isometric, with new art, a new interface and the campaign rewritten as one story. Saves from v1.0 still load.

## What's new since v1.0
- **Isometric city:** every building, citizen, tree and street is rendered in 3D (Blender) and seen from an isometric camera, with cast shadows, a colour grade that follows the hour and the season, lit windows at night, terraced ground with cliff faces, and wide streets with kerbs.
- **Beaches and smooth coasts:** low land meets the sea, lakes and rivers on sandy, pebbly beaches that slope into clear shallows, with waves running up the sand; high ground keeps its rock cliffs, with fallen stones at their foot. Diagonal coastlines run as smooth lines instead of tile steps.
- **Turning buildings:** E and Q turn the building in hand; its door faces the front. Houses turn to face their street by themselves, and each house tier comes in four looks. Drag the housing tool to lay out a row or a block of houses at once.
- **New interface:** a carved travertine and bronze skin; a build rail with pop-out lists; a top bar in three zones that shrinks to fit smaller windows; report panels that hang from their buttons; rendered icons, portraits and goods. The title screen plays over a living city, and the campaign has a painted province map.
- **One story:** the sixteen missions are now one career told through letters from a patron, five advisors with their own voices, and events, with an ending.
- **Alerts that say what is wrong:** a building with no workers, an idle workshop and the input it is missing, a producer with nowhere to store its output, a full store, an empty market, homelessness, unemployment and emigration.
- **While paused** you can still build, demolish and change policies; it all takes effect at once.
- **Sound effects:** generated like the music, no samples: clicks, stone set down for a new building, gravel for roads, crumbling walls, coins for trade and payments, a scroll for letters, a bronze bell for alerts, fire, collapses, sword and shield fights, horns for raids and marching cohorts, and cheering crowds for festivals and victories. Fires, collapses and fights are heard where they happen when they are on or near the screen. A quiet city ambience (murmur, birds) rises with the people on screen. A new **Sound effects volume** in Settings.
- **Mods can ship their own sprites:** buildings (with turns, night lights and regional looks), goods icons and other art. Put PNGs named like the game's sprites in the mod's `sprites/` folder, and they replace or add to the game's art in the mods' load order. The example mod's vintner's cellar and amber wine now have their own art, rendered with the game's Blender kit. See `docs/10-modding.md`, "Sprites".
- **Fixes:**
  - Choices in letters (paying the merchants, the Senate's offers, festivals) now reach the mission scripts; in v1.0 they were ignored.
  - Letters already read no longer come back after loading a save.
  - The Labor panel and the unemployment alert no longer count patricians and soldiers, who never take ordinary jobs, as out of work.
  - Goods staged for export at a trade post are handed back when the city needs them, and a trade post full of unsold goods clears to storage so imports still fit.
  - Letters and the end-of-mission dialog stay in front of panels; Esc leaves the menu screens; the build list gets out of the way once you pick a building.
  - Simulation fixes from a new consistency checker and save round-trips.
- **Under the hood:** save format v4 (building rotation); v1.0 saves load. New test tools: a consistency checker, a chaos player and save round-trips (`tools/audit.sh`), screenshots of every screen (`tools/screens.sh`), and scripted playtests through the real interface (`tools/playtest.sh`).

## Controls (new)
- **E / Q** turn the building in hand. **Drag** the housing tool for a row of houses.
- **O** cycles the map overlays; the one showing is named at the bottom of the screen.

## Requirements and limitations
- Requirements are as for v1.0 (below). The download is larger: the isometric sprite atlas is about 60 MB.
- The autoplay bot wins every mission on every difficulty (144 of 144 runs), and scripted playtests drive the real interface, but human play is still the best test: pacing and clarity reports are welcome (docs/playtest-checklist.md).

---

# Urbs v1.0.0

The complete game: the full sixteen-mission campaign from a hamlet in Latium to the rebuilding of Rome, plus a sandbox, a scenario editor and mod support.

## What's new since v0.1
- **Missions 5–16:**
  - Syracuse, Carthago Nova, Aquileia, Narbo, Campania, Corinth.
  - Lugdunum, where the Republic becomes an Empire.
  - Alexandria and the Nile flood.
  - Colonia Agrippina on the Rhine frontier.
  - Londinium and Boudica's revolt.
  - Antioch and the Silk Road.
  - Rome itself, with 10,000 fully simulated citizens.
- **Goldsmiths:** gold imported from Alexandria or Hispania becomes jewelry. Scrolls and statues sell to Rome.
- **More to build:**
  - Monuments that rise in stages and are dedicated to the gods.
  - Sea trade, harbors and lighthouses.
  - Aqueducts, pipes and sewers.
  - Mints, siege workshops and cart depots.
  - Civic institutions.
- **Imperial politics:** the Emperor, the imperial cult, grants, plague, earthquakes and revolts.
- **Regional building styles:** Italy, the north, the east and Egypt.
- **Sandbox:** desert maps, the Nile flood and a choice of building style.
- **Scenario editor:** paint a map, set goals, gods and trade, save and play. Scenarios live in your user folder and can be shared.
- **Mods:** add or change goods, recipes, buildings, housing, gods, units and trade partners with JSON, and add scenarios with Lua. See `docs/10-modding.md` and the example mod in `mods/`.
- **Accessibility:** UI scale and colour-blind-friendly overlays (Settings).
- **Under the hood:**
  - Many simulation fixes: errands, shopping, fire spread, housing decline, emigration.
  - Buildings whose entrance gets blocked find a new one; a building with no way in shows a "no road access" alert.
  - Deliveries can no longer get stuck forever, which used to stall monuments at their last stage.
  - Faster roads.
  - Save format v3; v0.1 saves still load.

## Requirements
- **macOS:** 13 or later on Apple Silicon. The app is ad-hoc signed, not notarized. Open it the first time with right-click → **Open**, or run `xattr -dr com.apple.quarantine Urbs.app`.
- **Windows and Linux:** archives are built by CI. They are less tested than the Mac build; please report problems.

Saves, settings, custom scenarios and user mods live in the user data folder (`~/Library/Application Support/urbs/urbs/` on macOS).

## Controls
- **Camera:** WASD or arrow keys, or drag with the right or middle mouse button. Scroll to zoom.
- **Speed:** Space pauses; 1, 2 and 3 set the speed.
- **Building:** B opens the build menu, R places roads, X demolishes, Esc cancels or opens the menu.
- **Overlays and following:** O opens overlays; F follows the selected citizen.
- **Saving:** F5 quicksaves, F9 quickloads.
- **In the editor:** left-click paints, WASD moves the camera, and the scroll wheel zooms.

## License
- **Code:** PolyForm Noncommercial 1.0.0.
- **Art, data and docs:** CC BY-NC-SA 4.0.

Play it, modify it, share it and contribute; don't sell it. See LICENSE.md, LICENSE-ASSETS.txt and NOTICE.

## Known limitations
- **Music:** a generated soundtrack (24 tracks, 10 stingers) that follows the city: era, region, raids, battles, disasters and festivals (see docs/11-music.md). There are no sound effects yet.
- Mods cannot ship their own sprites yet; new buildings use a placeholder of their size.
- **Playtesting:** the campaign has been played through by the autoplay bot: every mission on every difficulty, 143 of 144 runs won. It has had little human play yet, so pacing and clarity reports are welcome (docs/playtest-checklist.md).
