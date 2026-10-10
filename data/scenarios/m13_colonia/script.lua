-- Mission 13: Colonia Agrippina - the Rhine frontier. Limes forts, siege engines, large battles, harsh winters.
-- Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.raids = 0
  urbs.message("The Rhine",
    "On the far bank the forest goes on for weeks, and the tribes in it have never stayed beaten for long. Three " ..
    "legions went in there once and did not come out.\n\n" ..
    "Forts hold garrisons. A Siege workshop builds ballistae that break a warband before it reaches the walls. " ..
    "Winters are long: fill the granaries before the snow. And I am too old for this, in case anyone was wondering.",
    LEGATE)
end

local kRaids = {
  {"Across the river", "They are across the Rhine. Man the forts and bring up the ballistae."},
  {"A bigger host", "A bigger host, with archers, and horsemen coming round from the north. They have been " ..
   "watching how we fight."},
  {"War", "The whole far bank is moving. This is not raiding any more; it is a war."},
  {"The largest yet", "The last and the largest. If we hold today, they will remember it for a generation."},
}

local function raid(n, horse)
  S.raids = S.raids + 1
  urbs.raid({unit = "barbarian_warrior", count = n, side = "east"})
  urbs.raid({unit = "barbarian_archer", count = math.max(1, n // 2), side = "east"})
  if horse then urbs.raid({unit = "barbarian_horse", count = horse, side = "north"}) end
  local r = kRaids[math.min(S.raids, #kRaids)]
  urbs.message(r[1], r[2], LEGATE)
end

function on_day(day)
  if day >= 30 and S.raids == 0 then raid(2) end
  if day >= 60 and S.raids == 1 then raid(3, 1) end
  if day >= 90 and S.raids == 2 then raid(4, 2) end
  if day >= 125 and S.raids == 3 then raid(5, 2) end
  if S.raids == 3 and day >= 92 and not S.wound then
    S.wound = true
    urbs.message("An arrow",
      "An arrow in the shoulder. The surgeon says I was lucky. I have been lucky for forty years; at some point " ..
      "people should start calling it skill.", LEGATE)
  end
  if urbs.season() == "Winter" and S.winter_year ~= urbs.year() then
    S.winter_year = urbs.year()
    urbs.message("Winter",
      "Snow has closed the roads. Food goes faster in the cold, and nothing grows until spring.", AEDILE)
  end
end
