-- Mission 10: Corinth - refounded by Caesar. Greek culture: odeon, academy, marble, dyes, scriptorium, research.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Laus Iulia Corinthiensis",
    "A century after its destruction, Caesar refounds Corinth with freedmen and veterans. Greek learning still " ..
    "lives here.\n\n" ..
    "Quarry marble for sculptors, the Odeon and the Academy. Scholars at libraries and academies produce scientia " ..
    "for civic research (the Research panel).", AEDILE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 10 then
    urbs.message("Purple and papyrus", "Dye works crush murex shells for dye; dyers make dyed clothing that the " ..
      "wealthy crave. Papyrus from the marshes feeds the scriptorium's scrolls.", QUAESTOR)
  end
  if pop >= 2000 and not S.req then
    S.req = urbs.request({kind = "goods", good = "statues", amount = 10, days = 40, reward = 4000, favor = 12, penalty = -10})
    urbs.message("Statues for Rome", "Rome's new forum needs Greek statues: 10 within 40 days.", AEDILE)
  end
  if day == 60 and not S.quake then
    S.quake = true
    urbs.trigger("earthquake")
    urbs.message("The earth shakes", "An earthquake! Damaged buildings need engineers before they collapse.", ENGINEER)
  end
  -- Rome keeps asking: a new request every 30 days once the scripted ones are done.
  if S.req and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"statues", 8, 3200}, {"dyed_clothing", 30, 2400}, {"marble", 40, 2000}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -3})
    urbs.message("Another request", "Rome asks for " .. pick[2] .. " " .. pick[1] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 30
  if ok then urbs.message("Greek beauty for Rome", "The statues delight the city. Corinth's name rises.", AEDILE) end
end
