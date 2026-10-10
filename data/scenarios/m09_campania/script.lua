-- Mission 9: Campania - the Servile War. Slave treatment and manumission under pressure, revolt, amphitheater,
-- hospital, curia.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Spartacus",
    "The estates of Campania are worked by thousands of slaves, and a gladiator named Spartacus has escaped from " ..
    "Capua. Unrest is spreading.\n\n" ..
    "Harsh treatment breeds revolt. The Labor panel sets slave treatment and manumission: freed slaves become " ..
    "liberti who work for wages. Keep guards in the streets and soldiers ready.", LEGATE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 10 then
    urbs.message("The Curia", "A Curia gives the city a council: order in the streets and a voice in the Senate.", AEDILE)
  end
  if day == 30 and not S.unrest then
    S.unrest = true
    urbs.choice("Unrest in the estates",
      "Slaves on the estates are whispering of Spartacus. The landowners demand harsher discipline.",
      {"Harsher discipline (+favor with the elite, more anger)", "Better treatment (costs money, calms them)"}, "unrest")
  end
  if day == 45 and not S.revolt then
    S.revolt = true
    urbs.trigger("revolt")
    urbs.message("Revolt!", "The slaves have risen! Some flee to Spartacus, some burn what they can.", LEGATE)
  end
  if day >= 70 and not S.army then
    S.army = true
    urbs.raid({unit = "rebel_slaves", count = 4, side = "east"})
    urbs.message("Spartacus comes", "The rebel army descends on the city. Hold the walls!", LEGATE)
  end
  if day >= 110 and not S.army2 then
    S.army2 = true
    urbs.raid({unit = "rebel_slaves", count = 5, side = "south"})
    urbs.message("The last stand", "Spartacus's host makes one more attempt before Crassus's legions arrive.", LEGATE)
  end
end

function unrest(option)
  if option == 1 then
    urbs.change_favor(3)
  else
    urbs.take_money(600)
    urbs.message("Better treatment", "Food and rest for the slaves cost money but cool tempers.", AEDILE)
  end
end
