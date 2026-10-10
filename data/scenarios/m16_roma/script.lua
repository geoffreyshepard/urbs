-- Mission 16: Roma - caput mundi. Rebuild Rome after the great fire: the Colosseum, the Pantheon and the
-- Circus Maximus; every system at once.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Rome burns",
    "Nine days of fire have left ten of Rome's fourteen districts in ashes. The Emperor names you Prefect of the " ..
    "City: rebuild Rome greater than it was.\n\n" ..
    "A million people once lived here; bring back ten thousand to start. Then give them the great works: " ..
    "an amphitheatre to outshine all others, a temple to all the gods, and the greatest circus in the world.", AEDILE)
end

function on_day(day)
  if day % 20 == 0 and day > 0 then
    urbs.add_money(4000)
    urbs.message("The fiscus", "The Emperor's treasury sends 4,000 denarii for the rebuilding of the City.", QUAESTOR)
  end
  if day == 45 and not S.plague then
    S.plague = true
    urbs.trigger("plague")
    urbs.message("Sickness", "Disease spreads in the crowded quarters. Physicians and hospitals, and clean water, " ..
      "will keep it in check.", ENGINEER)
  end
end
