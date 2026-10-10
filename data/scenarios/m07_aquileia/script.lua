-- Mission 7: Aquileia - the northern frontier. Aqueducts, reservoirs, pipes, fountains, thermae, sewers, forts.
-- Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.raids = 0
  urbs.message("A city worth plumbing",
    "At last. There is good water in the hills, good gravel for concrete, and I have brought my instruments.\n\n" ..
    "Build a Reservoir beside the river or a lake, or feed one with an Aqueduct that falls gently from a spring. " ..
    "Water will not run uphill in an open channel, whatever anyone tells you. From the reservoir, lay Pipes to " ..
    "Fountains and to Thermae. Pipes are under pressure and can carry water uphill.", ENGINEER)
end

local kRaids = {
  {"Out of the passes", "Raiders out of the passes, coming down the valley. A Fort keeps a garrison ready for " ..
   "exactly this."},
  {"Better armed", "More of them, and better armed. Somebody up in the mountains is organizing them."},
  {"Horsemen", "Horsemen with them this time. Hold the fords, and keep them away from the aqueduct."},
}

local function raid(n)
  S.raids = S.raids + 1
  urbs.raid({unit = "barbarian_warrior", count = n, side = "north"})
  if n > 2 then urbs.raid({unit = "barbarian_horse", count = 1, side = "north"}) end
  local r = kRaids[math.min(S.raids, #kRaids)]
  urbs.message(r[1], r[2], LEGATE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 10 then
    urbs.message("Concrete",
      "Roman concrete: lime from limestone burned in a kiln, pozzolana dug from volcanic ash, and stone. It sets " ..
      "under water and lasts for ever, if it is mixed properly, which it usually is not. Reservoirs, thermae and " ..
      "the great works need it.", ENGINEER)
  end
  if day == 18 then
    urbs.message("Sewers",
      "The tall insulae need sewers draining into the river. Sewers also keep disease down, which I mention for " ..
      "the physicians' sake. Personally I like them for the gradients.", ENGINEER)
  end
  if day == 40 and not S.book then
    S.book = true
    urbs.message("A book",
      "I have started writing a book. Ten books, in fact: on building, on water, on machines. Somebody should write " ..
      "down how it is done properly before everyone forgets.", ENGINEER)
  end
  if day >= 25 and S.raids == 0 then raid(2) end
  if day >= 55 and S.raids == 1 then raid(3) end
  if day >= 85 and S.raids == 2 then raid(4) end
  if urbs.season() == "Winter" and not S.winter then
    S.winter = true
    urbs.message("Snow",
      "The first snow. Winters are hard this close to the mountains. Fill the granaries before the passes close " ..
      "and the roads freeze.", AEDILE)
  end
end
