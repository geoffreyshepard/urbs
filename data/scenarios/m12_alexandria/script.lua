-- Mission 12: Alexandria - Egypt. The Nile flood, the grain fleet, glassworks, the circus, Egyptian gods and
-- the great lighthouse. Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Alexandria",
    "Greeks, Egyptians, Jews, Syrians and a Roman garrison, all in one city and all sure it is theirs. Every " ..
    "summer the Nile floods its banks: the low land by the river comes back black with silt, and anything built " ..
    "on it is soaked.\n\n" ..
    "Farm the floodplain; build above it. Isis and Serapis are honoured here as much as Jupiter, and their priests " ..
    "will make sure you know it.", AEDILE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 10 then
    urbs.message("Glass and chariots",
      "Sand pits and glassworks make glassware, which sells across the empire and even to the Parthians, who pay " ..
      "in silver. What this city loves most is a Circus with chariot races. They will riot over the Greens and the " ..
      "Blues. Better that than rioting over us.", QUAESTOR)
  end
  if day == 14 then
    urbs.message("The light on Pharos",
      "The old lighthouse on Pharos was a wonder of the world until a storm took half of it. The new Lighthouse is " ..
      "a monument: it rises in stages as stone and concrete arrive. There is old Ptolemaic stone in the " ..
      "warehouses, and Greece sells more by sea, so keep the harbor working.\n\n" ..
      "The Library is two streets from my lodgings. I have not slept much.", ENGINEER)
  end
  if pop >= 1500 and not S.r1 then
    S.r1 = urbs.request({kind = "goods", good = "wheat", amount = 120, days = 30, reward = 2500, favor = 10, penalty = -8})
    urbs.message("The grain fleet",
      "The grain fleet sails for Ostia in thirty days. Fill it: 120 sacks of wheat.", QUAESTOR)
  end

  -- Rome keeps asking: a new request every 30 days once the scripted ones are done.
  if S.r1 and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"glassware", 20, 2500, "20 crates of glassware"}, {"olive_oil", 40, 2000, "40 amphorae of olive oil"},
                  {"wheat", 100, 1800, "100 sacks of wheat"}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -3})
    urbs.message("The palace writes", "The palace asks for " .. pick[4] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 30
  if ok then urbs.message("Full holds", "The fleet sailed full. In Rome, the price of bread holds steady.", QUAESTOR) end
end
