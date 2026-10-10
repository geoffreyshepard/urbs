-- Mission 13: Colonia Agrippina - the Rhine frontier. Limes forts, siege engines, large battles, harsh winters.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.raids = 0
  urbs.message("The edge of the world",
    "Across the Rhine the forests of Germania go on forever, and the tribes never stop testing the limes.\n\n" ..
    "Forts hold garrisons; a Siege workshop builds ballistae that break warbands at long range. " ..
    "Winters here are long: keep the granaries full.", LEGATE)
end

local function raid(n, horse)
  S.raids = S.raids + 1
  urbs.raid({unit = "barbarian_warrior", count = n, side = "east"})
  urbs.raid({unit = "barbarian_archer", count = math.max(1, n // 2), side = "east"})
  if horse then urbs.raid({unit = "barbarian_horse", count = horse, side = "north"}) end
  urbs.message("The tribes cross the Rhine!", "A great host is coming. Man the forts and bring up the ballistae.", LEGATE)
end

function on_day(day)
  if day >= 30 and S.raids == 0 then raid(2) end
  if day >= 60 and S.raids == 1 then raid(3, 1) end
  if day >= 90 and S.raids == 2 then raid(4, 2) end
  if day >= 125 and S.raids == 3 then raid(5, 2) end
  if urbs.season() == "Winter" and S.winter_year ~= urbs.year() then
    S.winter_year = urbs.year()
    urbs.message("Winter", "Snow closes the roads. Food is eaten faster and nothing grows until spring.", AEDILE)
  end
end
