-- Mission 3: Beneventum - the Samnite frontier. Walls, militia, raids, temples and the first gods.
-- Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.raids = 0
  urbs.message("Quintus Labienus, reporting",
    "I know these hills. I left two fingers in them. The Samnites will watch us build for a month, " ..
    "and then they will come down to see what we have.\n\n" ..
    "Grow the town and give the gods proper temples. Build a Militia post now, not when we see them. " ..
    "When they come, raise militia at the post from the Military panel, select your men, " ..
    "and right-click where you want them.", LEGATE)
end

local kRaids = {
  {"Samnites", "Samnite warbands on the north ridge, coming down. Raise the militia and meet them before they " ..
   "reach the houses."},
  {"They are back", "More of them this time. They have seen our walls and want to know whether we will stand " ..
   "behind them."},
  {"The last war chief", "The last of their war chiefs is coming down with everything he has left. Beat this one " ..
   "and they will send envoys instead."},
}

local function raid(count)
  S.raids = S.raids + 1
  urbs.raid({unit = "barbarian_warrior", count = count, side = "north"})
  local r = kRaids[math.min(S.raids, #kRaids)]
  urbs.message(r[1], r[2], LEGATE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 6 then
    urbs.message("Stone",
      "These hills are stone all the way down, which is the first good news I have had here. A Quarry on the " ..
      "rock and a Stonemason give cut stone. A Brick kiln turns clay and wood into bricks. Temples need cut stone, " ..
      "and I will not build a temple out of mud.", ENGINEER)
  end
  if day == 12 then
    urbs.message("Temples",
      "A shrine is a promise; a temple is a debt paid. A proper Temple to one god pleases that god greatly. " ..
      "Mars watches over soldiers, and we will need him. Ceres watches over the harvest, and we will need her more.",
      HARUSPEX)
  end
  if day >= 20 and S.raids == 0 then raid(1) end
  if day >= 40 and S.raids == 1 then raid(2) end
  if day >= 64 and S.raids == 2 then raid(2) end
  if pop >= 500 and not S.req then
    S.req = urbs.request({kind = "goods", good = "cut_stone", amount = 30, days = 24, reward = 1200, favor = 10, penalty = -10})
    urbs.message("Stone for Rome",
      "Rome is rebuilding its walls and asks for 30 blocks of cut stone within 24 days. Rome is always " ..
      "rebuilding its walls.", QUAESTOR)
  end
end

function on_request(id, ok)
  if ok then
    urbs.message("Received",
      "The stone reached Rome with our mason's mark on every block. The Senate sends silver and its thanks.", QUAESTOR)
  else
    urbs.message("Not received",
      "The stone never left. Mela will mention it in the Curia, probably more than once.", QUAESTOR)
  end
end
