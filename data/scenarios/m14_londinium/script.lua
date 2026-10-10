-- Mission 14: Londinium - Britannia. A revolt burns the city midway; rebuild under pressure.
-- Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Londinium",
    "Wooden houses, a muddy river, and merchants who came here to get rich quickly. The Britons resent them, and " ..
    "the governor is far away on Mona with most of the army.\n\n" ..
    "Build well, and keep Vigiles everywhere. Wooden towns burn.", AEDILE)
end

function on_day(day)
  if day == 40 and not S.warning then
    S.warning = true
    urbs.message("The Iceni",
      "Bad news from the Iceni. Their king died and left half his kingdom to the Emperor. Our officials took all " ..
      "of it, flogged his widow, Boudica, and did worse to her daughters. The tribes are arming. Raise militia, " ..
      "and keep the vigiles ready.", LEGATE)
  end
  if day == 50 and not S.revolt then
    S.revolt = true
    for i = 1, 30 do urbs.trigger("fire") end
    urbs.raid({unit = "barbarian_warrior", count = 6, side = "north"})
    urbs.raid({unit = "barbarian_horse", count = 2, side = "east"})
    urbs.message("Boudica",
      "Boudica has burned Camulodunum to the ground, and now she is here. Londinium is on fire. Fight the fires, " ..
      "hold the streets, and save what you can.", LEGATE)
  end
  if day == 58 and S.revolt and not S.aid then
    S.aid = true
    urbs.add_money(10000)
    urbs.message("Ten thousand denarii",
      "The governor marched back from Mona and broke the rebellion. The Emperor sends 10,000 denarii to rebuild " ..
      "Londinium, greater than before. He would like it in stone this time.", QUAESTOR)
  end
end
