-- Mission 12: Alexandria - Egypt. The Nile flood, the grain fleet, glassworks, the circus, Egyptian gods and
-- the great lighthouse.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("The gift of the Nile",
    "Egypt belongs to the Emperor himself, and its grain feeds Rome. Every summer the Nile floods its banks: " ..
    "the low land by the river is renewed and black with silt, and anything built there is soaked.\n\n" ..
    "Farm the floodplain, build above it. Isis and Serapis are honoured here as much as Jupiter.", AEDILE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 10 then
    urbs.message("Glass and sand", "Sand pits and glassworks make glassware, which sells across the empire and " ..
      "to the Parthians. A Circus for chariot races is what this city loves most.", QUAESTOR)
  end
  if day == 14 then
    urbs.message("The Pharos", "Ships need a light on Pharos island. The Lighthouse is a monument: it rises in " ..
      "stages as stone and concrete arrive.", ENGINEER)
  end
  if pop >= 1500 and not S.r1 then
    S.r1 = urbs.request({kind = "goods", good = "wheat", amount = 300, days = 30, reward = 3000, favor = 10, penalty = -10})
    urbs.message("The grain fleet", "The grain fleet sails for Ostia. Fill it: 300 wheat within 30 days.", QUAESTOR)
  end

  -- Rome keeps asking: a new request every 20 days once the scripted ones are done.
  if S.r1 and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"wheat", 300, 3000}, {"glassware", 30, 3000}, {"wheat", 400, 3600}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -6})
    urbs.message("Another request", "Rome asks for " .. pick[2] .. " " .. pick[1] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 20
  if ok then urbs.message("Well done", "The fleet sails full. Rome is fed.", QUAESTOR) end
end
