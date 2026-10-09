-- Mission 1: Latium - The Founding. A tutorial that teaches through advisors.
-- Persistent state lives in urbs.state (saved with the game).
local S = urbs.state

local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  S.step = 0
  urbs.message("A new beginning",
    "The Senate has granted you a stretch of land in Latium, by the river. Families are waiting to settle " ..
    "if you give them a place to live.\n\nBuild some Housing plots near the road. Settlers will arrive and pitch tents.",
    ENGINEER)
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
      "People need water. Build Wells close to the houses; a tent with a well nearby will improve into a hut.",
      ENGINEER)
  end
  if urbs.count("well") >= 1 and pop >= 30 then
    step_done(1, "Food",
      "Settlers have brought little food. Paint a Wheat farm on fertile land (the darker soil near the river), " ..
      "build a Granary to store the harvest, and a Market where families can buy it. Workers come from the houses.",
      AEDILE)
  end
  if urbs.count("market") >= 1 and urbs.count("granary") >= 1 then
    step_done(2, "Taxes",
      "The city pays its workers from the treasury. Build a Forum: its clerks collect taxes from every house nearby, " ..
      "and the coins come back to us. Without it the treasury will run dry.", QUAESTOR)
  end
  if urbs.count("forum") >= 1 then
    step_done(3, "The gods",
      "The gods watch over us. A Shrine keeps people content and lets huts grow. " ..
      "Ceres especially should be honoured if we want good harvests.",
      HARUSPEX)
  end
  if urbs.count("shrine") >= 1 and S.step == 4 then
    S.step = 5
    S.request = urbs.request({kind = "goods", good = "wheat", amount = 40, days = 16, reward = 600, favor = 10, penalty = -10})
    urbs.message("A request from Rome",
      "Rome asks for 40 wheat to feed the city's poor. Store the grain in your granary and send it from the Rome panel " ..
      "before the deadline. The Senate will remember it.", QUAESTOR)
  end
  if S.step >= 5 and day >= 20 and not S.merchants then
    S.merchants = true
    urbs.choice("Latin merchants",
      "Traders from the Latin villages offer to bring timber and grain if we let them use our road. " ..
      "They ask 200 denarii for safe passage.",
      {"Pay them (open the trade route)", "Refuse"}, "merchants_answer", QUAESTOR)
  end
  if pop >= 200 and not S.half then
    S.half = true
    urbs.message("A growing village",
      "Two hundred souls now call this place home. Keep them fed, watered and pious and the village will reach " ..
      "three hundred. A Tavern and a Physician would make life better still.", AEDILE)
  end
end

function merchants_answer(option)
  if option == 1 then
    urbs.take_money(200)
    urbs.open_route("latin_villages")
    urbs.message("Trade opens", "Caravans will now arrive every few days. Set what to import and export in the Trade panel.",
      QUAESTOR)
  else
    urbs.change_favor(-2)
    urbs.message("Merchants leave", "The merchants grumble and go home. Perhaps another time.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.request then
    if ok then
      urbs.message("Rome is grateful", "The grain reached Rome. The Senate sends 600 denarii in thanks.", QUAESTOR)
    else
      urbs.message("Rome is disappointed", "The grain never came. The Senate thinks less of us.", QUAESTOR)
    end
  end
end
