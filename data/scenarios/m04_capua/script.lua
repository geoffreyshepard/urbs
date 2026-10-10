-- Mission 4: Capua - rich Campania. Olives, vines, pottery and amphorae, equites and domus, theater, schools, baths.
-- Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Campania",
    "Two harvests a year here, and a festival for each. Olive groves and vineyards grow like weeds. Rome is " ..
    "thirsty for oil and wine, and the presses and wineries need amphorae from the potters.\n\n" ..
    "The rich will come if there are Domus plots in pleasant streets. Gardens, statues and a theater make a " ..
    "street pleasant. A tannery next door does not.", AEDILE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 8 then
    urbs.message("Better houses",
      "A casa becomes a small insula when the family has bread, pottery, clothing and oil, and the street has " ..
      "baths, a tavern and a physician. Click a house to see what it still lacks. It will tell you, at length.",
      ENGINEER)
  end
  if pop >= 600 and not S.r1 then
    S.r1 = urbs.request({kind = "goods", good = "olive_oil", amount = 50, days = 30, reward = 1500, favor = 10, penalty = -10})
    urbs.message("Oil for Rome",
      "Rome wants 50 amphorae of olive oil within 30 days. It pays well for anything it cannot make itself.", QUAESTOR)
  end
  if pop >= 1000 and not S.r2 then
    S.r2 = urbs.request({kind = "goods", good = "wine", amount = 50, days = 30, reward = 1800, favor = 10, penalty = -10})
    urbs.message("Wine for the Senate",
      "The Senate's dinners need 50 amphorae of Campanian wine within 30 days. Not Latin wine. They were specific.",
      QUAESTOR)
  end
  if day == 45 and not S.choice then
    S.choice = true
    urbs.choice("A gladiator school",
      "A lanista wants to open a gladiator school here, and offers 600 denarii for the licence. The men he trains " ..
      "are prisoners and slaves who learn to kill for a crowd. They are also locked up together, armed, " ..
      "with nothing to lose.",
      {"Grant the licence (+600 denarii)", "Refuse him"}, "lanista", AEDILE)
  end
  -- Rome keeps asking: a new request every 24 days once the scripted ones are done.
  if S.r2 and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"olive_oil", 40, 1200, "40 amphorae of olive oil"}, {"wine", 40, 1400, "40 amphorae of wine"},
                  {"pottery", 60, 1000, "60 crates of pottery"}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -3})
    urbs.message("Rome writes again", "Rome asks for " .. pick[4] .. " within 30 days.", QUAESTOR)
  end
end

function lanista(option)
  if option == 1 then
    urbs.add_money(600)
    urbs.state.lanista = true
    urbs.message("The ludus",
      "The school opens behind high walls. I have put a guard on the gate. I would put two.", LEGATE)
  else
    urbs.change_favor(2)
    urbs.message("Refused",
      "He took his money to a magistrate in another town who needed it more. Rome has heard that you said no.", AEDILE)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 24
  if ok then
    urbs.message("Delivered",
      "The amphorae reached Rome unbroken, which Calidius says is the real miracle. Your name is spoken at better " ..
      "dinners now.", AEDILE)
  end
end
