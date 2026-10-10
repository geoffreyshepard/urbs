-- Mission 2: Ostia - River Port. Fish, salt, warehouses, the first trade route, fire.
-- Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.stage = 0
  urbs.message("The mouth of the Tiber",
    "Everything Rome buys from overseas passes this way, and so far it passes a beach.\n\n" ..
    "Settle families along the road and give them wells, food and a Forum. Fishing wharves on the shore feed " ..
    "a town cheaply. Salt works turn sea water into salt, which Rome buys by the cartload.", QUAESTOR)
end

function on_day(day)
  local pop = urbs.population()
  if S.stage == 0 and pop >= 120 then
    S.stage = 1
    urbs.message("Fire",
      "The tents are pitched so close that one spark would cross the quarter before anyone woke. Build a Vigiles " ..
      "post. Its watchmen patrol the streets, keep the fire risk down, and put out what starts anyway. " ..
      "A building's panel shows its fire risk.", ENGINEER)
  end
  if S.stage == 1 and pop >= 200 then
    S.stage = 2
    urbs.message("Trade",
      "Build a Trade post by the road. Open a route in the Trade panel, then mark which goods to export and how " ..
      "much to keep at home. The Latin villages buy pottery and bread. Rome buys pottery, oil and wine, " ..
      "and pays in silver.", QUAESTOR)
  end
  if S.stage == 2 and pop >= 300 and not S.req then
    S.stage = 3
    S.req = urbs.request({kind = "goods", good = "fish", amount = 60, days = 24, reward = 900, favor = 10, penalty = -12})
    urbs.message("Fish for the public dinners",
      "The Senate wants 60 baskets of fish within 24 days for the public dinners. Keep it in a warehouse or " ..
      "granary and send it from the Rome panel.", QUAESTOR)
  end
  if S.stage == 3 and day >= 40 and not S.second then
    S.second = true
    urbs.request({kind = "goods", good = "salt", amount = 40, days = 20, reward = 700, favor = 8, penalty = -8})
    urbs.message("Salt for the army",
      "The army wants 40 sacks of salt within 20 days. Soldiers will march on bad bread, but not on unsalted meat. " ..
      "The salt works on the shore make it.", QUAESTOR)
  end
  if day == 30 and not S.neptune then
    S.neptune = true
    urbs.message("Neptune",
      "The fishermen say Neptune is restless. The catch has been thin, and two boats did not come back. " ..
      "A temple is beyond us yet, but shrines and a festival in his honour would show him we know whose " ..
      "water this is.", HARUSPEX)
  end
end

function on_request(id, ok)
  if ok then
    urbs.message("Delivered", "The cargo reached Rome. The Senate says that Ostia is earning its keep.", QUAESTOR)
  else
    urbs.message("Too late",
      "The deadline passed. Our favor in Rome has fallen, and Postumius will be hearing other men's names " ..
      "for a while.", QUAESTOR)
  end
end
