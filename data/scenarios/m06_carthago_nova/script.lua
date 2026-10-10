-- Mission 6: Carthago Nova - Hispania. Mines, smelters, the armory, barracks and legions; the Punic threat.
-- Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.raids = 0
  urbs.message("New Carthage",
    "Scipio waded across the lagoon at low tide and took this city in a day. Keeping it will take longer.\n\n" ..
    "Mine iron and silver on the rock, smelt them, and forge weapons and armour at an Armory. A Barracks raises " ..
    "legionary cohorts once it holds enough weapons and armour. Militia will do until then. They will have to.",
    LEGATE)
end

local function raid(n)
  S.raids = S.raids + 1
  if S.raids == 1 then
    urbs.raid({unit = "barbarian_warrior", count = 1, side = "west"})
    urbs.message("Over the western hills",
      "Iberian tribesmen, paid in Punic silver, coming over the western hills. Raise the militia.", LEGATE)
    return
  end
  urbs.raid({unit = "punic_infantry", count = n - 1, side = "west"})
  urbs.raid({unit = "barbarian_warrior", count = 1, side = "west"})
  if S.raids == 2 then
    urbs.message("Carthage",
      "Carthaginian infantry, in good order, marching on the city from the west. Raise your cohorts and meet them " ..
      "before they reach the mines.", LEGATE)
  else
    urbs.message("Everything they have left",
      "Carthage is throwing in everything it still has in Spain. Beat them here and the war in Spain is over.", LEGATE)
  end
end

function on_day(day)
  local pop = urbs.population()
  if day == 6 then
    urbs.message("The mines",
      "The mines run on slaves, public slaves who belong to the city. Work them too hard and they will turn on us " ..
      "when the Carthaginians come. The Labor panel shows who works where and how they are treated.", AEDILE)
  end
  if day == 14 then
    urbs.message("Vulcan",
      "Smelters and smithies burn day and night. Keep Vigiles near them, and give Vulcan his due, " ..
      "or he will take it.", HARUSPEX)
  end
  if day == 28 and not S.warned then
    S.warned = true
    urbs.message("Scouts",
      "Scouts report a Punic column gathering in the hills to the west. Raise militia now. Then build an Armory " ..
      "and a Barracks for real legions, because militia will not hold against Carthaginian infantry for long.", LEGATE)
  end
  if day >= 36 and S.raids == 0 then raid(1) end
  if day >= 66 and S.raids == 1 then raid(2) end
  if day >= 100 and S.raids == 2 then raid(3) end
  if pop >= 1200 and not S.req then
    S.req = urbs.request({kind = "goods", good = "silver", amount = 40, days = 36, reward = 3000, favor = 12, penalty = -10})
    urbs.message("Silver for the legions",
      "The Senate needs silver to pay the legions: 40 ingots within 36 days. That is to say, it needs our silver " ..
      "to pay the men defending our silver.", QUAESTOR)
  end
  -- Rome keeps asking: a new request every 28 days once the scripted ones are done.
  if S.req and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"iron", 40, 1500, "40 bars of iron"}, {"silver", 30, 2400, "30 ingots of silver"},
                  {"weapons", 30, 2000, "30 sets of weapons"}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -3})
    urbs.message("Rome writes again", "Rome asks for " .. pick[4] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 28
  if ok then
    urbs.message("Spanish silver",
      "Spanish silver is in the treasury in Rome. I weighed the receipt twice. It is correct.", QUAESTOR)
  end
end
