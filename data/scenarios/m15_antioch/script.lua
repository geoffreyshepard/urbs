-- Mission 15: Antioch - the East. Luxury trade, huge markets, an earthquake.
-- Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Antioch",
    "Colonnaded streets lit all night, theatres, bath houses, and people who argue philosophy in the market. " ..
    "The caravans bring silk, pepper and gems; the jewelers and glassmakers turn them into what the rich of the " ..
    "East, and of Rome, desire.\n\n" ..
    "Mela governs the province from the palace on the island. He has sent a list of what he expects from the city. " ..
    "It is long.", QUAESTOR)
end

function on_day(day)
  local pop = urbs.population()
  if day == 75 and not S.quake then
    S.quake = true
    urbs.trigger("earthquake")
    urbs.message("Before dawn",
      "A great earthquake, before dawn. Whole streets are down. Clear the ruins and rebuild; the engineers can still " ..
      "save what is standing.\n\nThe palace on the island has collapsed. Mela was inside.", ENGINEER)
  end
  if day == 77 and S.quake and not S.mela then
    S.mela = true
    urbs.message("Alive",
      "We dug Mela out of the palace this morning, under a beam that held. He asked for water, and then for my name. " ..
      "He asked for the names of the engineers as well, and wrote them down.", ENGINEER)
  end
  if pop >= 3000 and not S.req then
    S.req = urbs.request({kind = "goods", good = "jewelry", amount = 20, days = 36, reward = 5000, favor = 10, penalty = -8})
    urbs.message("For the Empress",
      "The palace wants Antiochene jewelry for the Empress: 20 pieces within 36 days.", QUAESTOR)
  end

  -- Rome keeps asking: a new request every 24 days once the scripted ones are done.
  if S.req and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"glassware", 40, 3000, "40 crates of glassware"}, {"dyed_clothing", 40, 3200, "40 bolts of dyed cloth"},
                  {"jewelry", 15, 4000, "15 pieces of jewelry"}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -3})
    urbs.message("The palace writes", "The palace asks for " .. pick[4] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 24
  if ok then
    urbs.message("Acknowledged", "The riches of the East reached Rome. The palace acknowledges them, in the third person.",
      QUAESTOR)
  end
end
