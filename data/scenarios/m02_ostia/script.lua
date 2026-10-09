-- Mission 2: Ostia - River Port. Fish, salt, warehouses, the first trade route, fire.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.stage = 0
  urbs.message("The mouth of the Tiber",
    "Rome needs a port. Here, where the river meets the sea, you will build Ostia.\n\n" ..
    "Settle families along the road, give them wells and food, and build a Forum to collect taxes. " ..
    "Fishing wharves on the shore feed a city cheaply; salt works turn sea water into salt for trade.", QUAESTOR)
end

function on_day(day)
  local pop = urbs.population()
  if S.stage == 0 and pop >= 120 then
    S.stage = 1
    urbs.message("Fire!",
      "Tents and huts packed close together burn easily. Build a Vigiles post: its watchmen patrol the streets, " ..
      "keep fire risk down and put out fires. Check the fire risk in a building's panel.", ENGINEER)
  end
  if S.stage == 1 and pop >= 200 then
    S.stage = 2
    urbs.message("Trade",
      "Build a Trade post near the road to the east. Open a route in the Trade panel, then mark goods to export " ..
      "above a level you choose. Latin villages buy pottery and bread; Rome buys pottery, oil and wine.", QUAESTOR)
  end
  if S.stage == 2 and pop >= 300 and not S.req then
    S.stage = 3
    S.req = urbs.request({kind = "goods", good = "fish", amount = 60, days = 24, reward = 900, favor = 10, penalty = -12})
    urbs.message("Fish for Rome", "Rome asks for 60 fish within 24 days. Store fish in a warehouse or granary and send it " ..
      "from the Rome panel.", QUAESTOR)
  end
  if S.stage == 3 and day >= 40 and not S.second then
    S.second = true
    urbs.request({kind = "goods", good = "salt", amount = 40, days = 20, reward = 700, favor = 8, penalty = -8})
    urbs.message("Salt", "The Senate now wants 40 salt for the army. Salt works on the shore produce it.", QUAESTOR)
  end
  if day == 30 and not S.neptune then
    S.neptune = true
    urbs.message("The sea god", "Sailors say Neptune grows restless. A temple is beyond us yet, but shrines and a " ..
      "festival in his honour would calm the waters.", HARUSPEX)
  end
end

function on_request(id, ok)
  if ok then
    urbs.message("Well done", "The Senate is pleased with Ostia's work.", QUAESTOR)
  else
    urbs.message("A missed request", "Rome waited in vain. Favor has fallen; do better next time.", QUAESTOR)
  end
end
