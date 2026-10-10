-- Mission 6: Carthago Nova - Hispania. Mines, smelters, the armory, barracks and legions; the Punic threat.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.raids = 0
  urbs.message("New Carthage",
    "Scipio has taken Carthago Nova from the Carthaginians. Its hills are full of silver and iron, and Punic " ..
    "armies will try to take it back.\n\n" ..
    "Mine iron and silver on the rock, smelt them, and forge weapons and armour at an Armory. A Barracks raises " ..
    "legionary cohorts when it holds enough weapons and armour.", LEGATE)
end

local function raid(n)
  S.raids = S.raids + 1
  if S.raids == 1 then
    urbs.raid({unit = "barbarian_warrior", count = 1, side = "west"})
    urbs.message("Iberian raiders", "Tribesmen paid with Punic silver are coming over the hills. Raise the militia.", LEGATE)
    return
  end
  urbs.raid({unit = "punic_infantry", count = n - 1, side = "west"})
  urbs.raid({unit = "barbarian_warrior", count = 1, side = "west"})
  urbs.message("The Punic army!", "Carthaginian infantry march on the city. Raise your cohorts and meet them.", LEGATE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 6 then
    urbs.message("Slaves", "Public slaves work where free men will not. Treat them harshly and they may revolt; " ..
      "the Labor panel shows who works where.", AEDILE)
  end
  if day == 14 then
    urbs.message("Vulcan's fire", "Smelters and smithies burn hot. Keep Vigiles near them, and honour Vulcan.", HARUSPEX)
  end
  if day == 28 and not S.warned then
    S.warned = true
    urbs.message("Scouts report", "A Punic column is gathering in the hills to the west. Raise militia now; " ..
      "build an Armory and a Barracks for proper legions.", LEGATE)
  end
  if day >= 36 and S.raids == 0 then raid(1) end
  if day >= 66 and S.raids == 1 then raid(2) end
  if day >= 100 and S.raids == 2 then raid(3) end
  if pop >= 1200 and not S.req then
    S.req = urbs.request({kind = "goods", good = "silver", amount = 40, days = 36, reward = 3000, favor = 12, penalty = -10})
    urbs.message("Silver for the war", "The Senate needs silver to pay the legions: 40 ingots within 36 days.", QUAESTOR)
  end
  -- Rome keeps asking: a new request every 28 days once the scripted ones are done.
  if S.req and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"iron", 40, 1500}, {"silver", 30, 2400}, {"weapons", 30, 2000}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -6})
    urbs.message("Another request", "Rome asks for " .. pick[2] .. " " .. pick[1] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 28
  if ok then urbs.message("The treasury of Rome", "Spanish silver fills the aerarium. Well done.", QUAESTOR) end
end
