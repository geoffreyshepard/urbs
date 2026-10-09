-- Mission 4: Capua - rich Campania. Olives, vines, pottery and amphorae, equites and domus, theater, schools, baths.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Campania felix",
    "Capua sits on the richest soil in Italy. Olive groves and vineyards thrive here, and Rome is thirsty for oil " ..
    "and wine. Presses and wineries need amphorae from the potters.\n\n" ..
    "Wealthy equites and patricians will come if there are Domus plots in pleasant neighbourhoods: gardens, statues " ..
    "and a theater make a district desirable.", AEDILE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 8 then
    urbs.message("Better houses",
      "Casae become small insulae when the families have bread, pottery, clothing and oil, and the district has baths, " ..
      "a tavern and a physician. Click a house to see what it lacks.", ENGINEER)
  end
  if pop >= 600 and not S.r1 then
    S.r1 = urbs.request({kind = "goods", good = "olive_oil", amount = 50, days = 30, reward = 1500, favor = 10, penalty = -10})
    urbs.message("Oil for Rome", "Rome asks for 50 amphorae of olive oil within 30 days.", QUAESTOR)
  end
  if pop >= 1000 and not S.r2 then
    S.r2 = urbs.request({kind = "goods", good = "wine", amount = 50, days = 30, reward = 1800, favor = 10, penalty = -10})
    urbs.message("Wine for Rome", "The Senate's banquets need 50 amphorae of Campanian wine.", QUAESTOR)
  end
  if day == 45 and not S.choice then
    S.choice = true
    urbs.choice("A gladiator school",
      "A lanista offers to open a gladiator school in Capua. It would bring coin, but such men are dangerous.",
      {"Allow it (+600 denarii)", "Refuse"}, "lanista")
  end
end

function lanista(option)
  if option == 1 then
    urbs.add_money(600)
    urbs.state.lanista = true
    urbs.message("The ludus opens", "The gladiators train behind high walls. Keep an eye on them.", LEGATE)
  else
    urbs.change_favor(2)
  end
end

function on_request(id, ok)
  if ok then urbs.message("Rome is pleased", "Campania's riches flow to Rome. Your reputation grows.", QUAESTOR) end
end
