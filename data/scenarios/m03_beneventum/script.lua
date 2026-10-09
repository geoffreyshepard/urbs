-- Mission 3: Beneventum - the Samnite frontier. Walls, militia, raids, temples and the first gods.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.raids = 0
  urbs.message("A frontier town",
    "Beneventum guards the road through the Samnite hills. The Samnites have not forgotten their defeat.\n\n" ..
    "Grow the town, build temples to win the gods' favour, and prepare a Militia post. When raiders come, " ..
    "raise militia from the post and send them against the enemy (select a soldier, right-click the map).", LEGATE)
end

local function raid(count)
  S.raids = S.raids + 1
  urbs.raid({unit = "barbarian_warrior", count = count, side = "north"})
  urbs.message("Samnites!", "Samnite warbands are coming down from the hills! Raise the militia.", LEGATE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 6 then
    urbs.message("Stone and bricks",
      "The hills are full of stone. A Quarry on rock and a Stonemason give cut stone; a Brick kiln turns clay and wood " ..
      "into bricks. Temples need cut stone.", ENGINEER)
  end
  if day == 12 then
    urbs.message("Temples",
      "Shrines satisfy simple folk, but a proper Temple, dedicated to one god, pleases that god greatly. Mars " ..
      "watches over soldiers; Ceres over the harvest.", HARUSPEX)
  end
  if day >= 20 and S.raids == 0 then raid(1) end
  if day >= 40 and S.raids == 1 then raid(2) end
  if day >= 64 and S.raids == 2 then raid(2) end
  if pop >= 500 and not S.req then
    S.req = urbs.request({kind = "goods", good = "cut_stone", amount = 30, days = 24, reward = 1200, favor = 10, penalty = -10})
    urbs.message("Stone for Rome", "Rome is rebuilding its walls and asks for 30 cut stone.", QUAESTOR)
  end
end

function on_request(id, ok)
  if ok then urbs.message("Rome thanks you", "The stone arrived. The Senate sends its thanks and silver.", QUAESTOR) end
end
