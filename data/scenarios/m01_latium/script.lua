-- Mission 1: Latium - The Founding. The tutorial: each step is taught by the advisor whose trade it is.
-- Story and voices: docs/12-story.md. Persistent state lives in urbs.state (saved with the game).
local S = urbs.state

local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.step = 0
  urbs.message("The founding",
    "At first light I ploughed the furrow around your town with a bronze share, the old way, and set the boundary " ..
    "stone by the river. While I worked, six kites came over from the north and turned south, toward Rome. " ..
    "A road that begins here ends there. I cannot tell you when.\n\n" ..
    "The families are waiting on the road with their carts. Give them somewhere to stand: lay out Housing plots " ..
    "beside the road, and they will pitch their tents.",
    HARUSPEX)
end

local function step_done(n, title, text, advisor)
  if S.step == n then
    S.step = n + 1
    urbs.message(title, text, advisor)
  end
end

function on_day(day)
  local pop = urbs.population()
  if pop >= 15 then
    step_done(0, "Water",
      "Nobody should carry water further than they must. They carry it badly and come to work late. " ..
      "Dig Wells among the houses. A tent with a well close by becomes a hut.",
      ENGINEER)
  end
  if urbs.count("well") >= 1 and pop >= 30 then
    step_done(1, "Bread before anything",
      "The settlers came with what they could carry, and most of it is eaten. Paint a Wheat farm on the dark soil " ..
      "near the river. Build a Granary to keep the harvest dry and a Market where the families can buy it. " ..
      "The farm finds its workers in the houses.",
      AEDILE)
  end
  if urbs.count("market") >= 1 and urbs.count("granary") >= 1 then
    step_done(2, "Taxes",
      "Every farmhand and porter is paid from the treasury, and so far nothing comes back into it. Build a Forum: " ..
      "its clerks collect taxes from the houses around it. Without one we are generous for about a season, " ..
      "and then we are bankrupt.", QUAESTOR)
  end
  if urbs.count("forum") >= 1 then
    step_done(3, "The gods",
      "Ceres gave you good soil, and she will want something back. A Shrine keeps the families content and lets " ..
      "the huts grow. Hold a festival for Ceres from the Religion panel before the harvest, not after.",
      HARUSPEX)
  end
  if urbs.count("shrine") >= 1 and S.step == 4 then
    S.step = 5
    S.request = urbs.request({kind = "goods", good = "wheat", amount = 40, days = 16, reward = 600, favor = 10, penalty = -10})
    urbs.message("A letter from Postumius",
      "Rome's poor are short of bread, and the Senate is asking every new settlement for something. Send 40 sacks " ..
      "of wheat within sixteen days from the Rome panel; keep the grain in the granary until then.\n\n" ..
      "The Senate pays for what it is sent. It also remembers what it is not sent.", QUAESTOR)
  end
  if S.step >= 5 and day >= 20 and not S.merchants then
    S.merchants = true
    urbs.choice("Merchants on the road",
      "Traders from the Latin villages want to bring timber and grain down our road. They ask 200 denarii for " ..
      "the privilege, which is robbery, and they know we have no other road.",
      {"Pay them and open the route", "Send them away"}, "merchants_answer", QUAESTOR)
  end
  if pop >= 200 and not S.half then
    S.half = true
    urbs.message("Two hundred",
      "Two hundred people now, and they quarrel over the well like a real town. Keep them fed, watered and on good " ..
      "terms with the gods, and there will be three hundred by the next market day. A Tavern and a Physician " ..
      "would make them stay.", AEDILE)
  end
end

function merchants_answer(option)
  if option == 1 then
    urbs.take_money(200)
    urbs.open_route("latin_villages")
    urbs.message("The first caravan",
      "The first caravan is unloading already. Decide what to buy and what to sell in the Trade panel. " ..
      "I suggest buying cheaply and selling dearly.", QUAESTOR)
  else
    urbs.change_favor(-2)
    urbs.message("They went home",
      "The merchants left, telling everyone on the road that we are hard to deal with. Postumius heard about it " ..
      "before I did.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.request then
    if ok then
      urbs.message("The grain arrived",
        "The grain reached Rome. The Senate sends 600 denarii, and Postumius writes that two senators asked him " ..
        "who you were.", QUAESTOR)
    else
      urbs.message("The grain did not",
        "The deadline passed and the grain stayed here. Postumius does not mention it in his letter, " ..
        "which is how I know he noticed.", QUAESTOR)
    end
  end
end
