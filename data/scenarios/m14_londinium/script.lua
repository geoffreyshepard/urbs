-- Mission 14: Londinium - Britannia. A revolt burns the city midway; rebuild under pressure.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("A trading town on the Tamesis",
    "Londinium is new and growing fast, a town of merchants on the river. The Britons resent the colonists' " ..
    "greed, and the governor is far away in Mona.\n\n" ..
    "Build well, and keep Vigiles everywhere: wooden towns burn.", AEDILE)
end

function on_day(day)
  if day == 40 and not S.warning then
    S.warning = true
    urbs.message("Rumours", "The Iceni queen Boudica has been flogged by Roman officials. The tribes are arming.", LEGATE)
  end
  if day == 50 and not S.revolt then
    S.revolt = true
    for i = 1, 30 do urbs.trigger("fire") end
    urbs.raid({unit = "barbarian_warrior", count = 6, side = "north"})
    urbs.raid({unit = "barbarian_horse", count = 2, side = "east"})
    urbs.message("Boudica!", "The Iceni have burned Camulodunum and now fall on Londinium. The city is in flames! " ..
      "Fight the fires, hold the streets, and rebuild.", LEGATE)
  end
  if day == 58 and S.revolt and not S.aid then
    S.aid = true
    urbs.add_money(10000)
    urbs.message("The Emperor's help", "The rebellion is broken. The Emperor sends 10,000 denarii to rebuild " ..
      "Londinium, greater than before.", QUAESTOR)
  end
end
