-- Mission 5: Syracuse - Sicilia. Harbor and sea trade, grain exports, garum, Rome's demands.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("The granary of Rome",
    "Sicily's wheat feeds Rome, and Syracuse is its greatest port. Build a Harbor on the shore: ships from Carthage, " ..
    "Massalia and Greece will call there. Land caravans still use a Trade post.\n\n" ..
    "Rome will ask for grain. Keep your granaries full and set wheat to Stockpile when a request comes.", QUAESTOR)
end

function on_day(day)
  local pop = urbs.population()
  if day == 8 then
    urbs.message("Garum",
      "The fish sauce garum sells dearly across the sea. A Garum workshop needs fish, salt and amphorae: fishing " ..
      "wharves, salt works on the shore and potters.", AEDILE)
  end
  if day == 20 then
    urbs.message("Neptune",
      "Sailors pray to Neptune before every voyage. An angry Neptune sends storms that wreck ships in port. " ..
      "A temple to him would be wise in a harbour city.", HARUSPEX)
  end
  if pop >= 700 and not S.r1 then
    S.r1 = urbs.request({kind = "goods", good = "wheat", amount = 150, days = 32, reward = 1600, favor = 10, penalty = -10})
    urbs.message("Grain for Rome", "The Roman mob is hungry. The Senate asks for 150 wheat within 32 days.", QUAESTOR)
  end
  if pop >= 1300 and not S.r2 then
    S.r2 = urbs.request({kind = "goods", good = "garum", amount = 40, days = 32, reward = 2200, favor = 10, penalty = -10})
    urbs.message("Garum for the Senate", "The Senators' tables want Sicilian garum: 40 amphorae within 32 days.", QUAESTOR)
  end
  if day == 55 and not S.storm then
    S.storm = true
    urbs.trigger("storm")
    urbs.message("A storm at sea", "A great storm lashes the coast. Roofs are damaged and ships run for shelter.", ENGINEER)
  end
  -- Rome keeps asking: a new request every 24 days once the scripted ones are done.
  if S.r2 and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"wheat", 120, 1200}, {"garum", 30, 1800}, {"olive_oil", 40, 1200}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -6})
    urbs.message("Another request", "Rome asks for " .. pick[2] .. " " .. pick[1] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 24
  if ok then urbs.message("Rome is fed", "The ships arrive at Ostia. The Senate remembers who feeds Rome.", QUAESTOR) end
end
