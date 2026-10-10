-- Mission 15: Antioch - the East. Luxury trade, huge markets, an earthquake.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("The Queen of the East",
    "Antioch is the third city of the empire, where the caravans from Parthia and India meet the ships of the " ..
    "Mediterranean. Its people love luxury, theatre and argument.\n\n" ..
    "Jewelers, glassworks and dyers make what the rich of the East and of Rome desire.", QUAESTOR)
end

function on_day(day)
  local pop = urbs.population()
  if day == 75 and not S.quake then
    S.quake = true
    urbs.trigger("earthquake")
    urbs.message("The earth shakes", "A great earthquake strikes Antioch! Clear the ruins and rebuild; engineers " ..
      "will save what still stands.", ENGINEER)
  end
  if pop >= 3000 and not S.req then
    S.req = urbs.request({kind = "goods", good = "jewelry", amount = 20, days = 36, reward = 5000, favor = 10, penalty = -8})
    urbs.message("Gifts for the Empress", "The palace wants Antiochene jewelry: 20 pieces within 36 days.", QUAESTOR)
  end

  -- Rome keeps asking: a new request every 24 days once the scripted ones are done.
  if S.req and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"glassware", 40, 3000}, {"dyed_clothing", 40, 3200}, {"jewelry", 15, 4000}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -3})
    urbs.message("Another request", "Rome asks for " .. pick[2] .. " " .. pick[1] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 24
  if ok then urbs.message("Well done", "The East's riches reach Rome.", QUAESTOR) end
end
