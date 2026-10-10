-- Mission 9: Campania - the Servile War. Slave treatment and manumission under pressure, revolt, amphitheater,
-- hospital, curia. Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Spartacus",
    "The rebels are camped on the slopes of Vesuvius. Spartacus trained in a ludus at Capua, and Lollius has " ..
    "asked me to remind you that he was against licensing one. I have reminded you.\n\n" ..
    "Hard treatment breeds revolt. The Labor panel sets how the slaves are treated and whether they can earn " ..
    "their freedom; freed slaves become liberti and work for wages. Keep guards in the streets and soldiers ready.",
    LEGATE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 10 then
    urbs.message("The Curia",
      "A Curia gives the city a council: order in the streets, and a voice in the Senate when we need one. " ..
      "We are going to need one.", AEDILE)
  end
  if day == 30 and not S.unrest then
    S.unrest = true
    urbs.choice("Whispers on the estates",
      "The slaves on the estates are whispering Spartacus's name. The landowners want harder discipline: more " ..
      "chains, more floggings, less food. They say it worked for their grandfathers.",
      {"Harder discipline (the landowners approve; the slaves will not)",
       "Better treatment (600 denarii; calms them)"}, "unrest", AEDILE)
  end
  if day == 45 and not S.revolt then
    S.revolt = true
    urbs.trigger("revolt")
    urbs.message("Revolt",
      "The slaves have risen. Some are running for the hills to join Spartacus; others are burning whatever they " ..
      "can reach. Get the vigiles out and the guards into the streets.", LEGATE)
  end
  if day >= 70 and not S.army then
    S.army = true
    urbs.raid({unit = "rebel_slaves", count = 4, side = "east"})
    urbs.message("The rebel army",
      "Spartacus is coming down on the city from the east, thousands strong, with weapons taken from our dead. " ..
      "Man the walls.", LEGATE)
  end
  if day >= 110 and not S.army2 then
    S.army2 = true
    urbs.raid({unit = "rebel_slaves", count = 5, side = "south"})
    urbs.message("One more attempt",
      "He is trying once more before Crassus's legions arrive, from the south this time. Hold him here, " ..
      "and it is over.", LEGATE)
  end
end

function unrest(option)
  if option == 1 then
    urbs.change_favor(3)
    urbs.message("Harder discipline",
      "The landowners are pleased. I have doubled the guard on the estates, and I would sleep with a sword.", LEGATE)
  else
    urbs.take_money(600)
    urbs.message("Better treatment",
      "Food, rest, and freedom promised to those who have served longest. It cost money. It may cost less than the " ..
      "alternative.", AEDILE)
  end
end
