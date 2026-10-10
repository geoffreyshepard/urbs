-- Mission 7: Aquileia - the northern frontier. Aqueducts, reservoirs, pipes, fountains, thermae, sewers, forts.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.raids = 0
  urbs.message("Beyond the Po",
    "Aquileia is the gate to the north. Gauls and Illyrians watch from the passes.\n\n" ..
    "A great city needs running water. Build a Reservoir beside the river or a lake (or feed it with an Aqueduct " ..
    "that runs downhill from a spring), then lay Pipes to Fountains and to Thermae. Pipes carry water uphill.", ENGINEER)
end

local function raid(n)
  S.raids = S.raids + 1
  urbs.raid({unit = "barbarian_warrior", count = n, side = "north"})
  if n > 2 then urbs.raid({unit = "barbarian_horse", count = 1, side = "north"}) end
  urbs.message("Raiders from the Alps!", "Barbarian warbands pour out of the passes. A Fort holds a garrison ready.", LEGATE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 10 then
    urbs.message("Concrete", "Roman concrete needs lime (limestone burned in a kiln), pozzolana and stone. " ..
      "Reservoirs, thermae and the great works are built with it.", ENGINEER)
  end
  if day == 18 then
    urbs.message("Sewers", "The large insulae need sewers draining into the river. Sewers also keep disease down.", ENGINEER)
  end
  if day >= 25 and S.raids == 0 then raid(2) end
  if day >= 55 and S.raids == 1 then raid(3) end
  if day >= 85 and S.raids == 2 then raid(4) end
  if urbs.season() == "Winter" and not S.winter then
    S.winter = true
    urbs.message("Winter in the north", "The winters are hard here. Keep the granaries full before the snow.", AEDILE)
  end
end
