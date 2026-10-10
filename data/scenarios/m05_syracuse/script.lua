-- Mission 5: Syracuse - Sicilia. Harbor and sea trade, grain exports, garum, Rome's demands.
-- Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Syracuse",
    "Wheat from the whole island comes through this port, and Rome counts on every sack. Build a Harbor on the " ..
    "shore: ships from Carthage, Massalia and Greece will call there. Caravans by land still need a Trade post.\n\n" ..
    "Rome will ask for grain. Keep the granaries full, and when a request comes, set wheat to Stockpile in the " ..
    "Trade panel so the markets do not sell what Rome has ordered.", QUAESTOR)
end

function on_day(day)
  local pop = urbs.population()
  if day == 8 then
    urbs.message("Garum",
      "You will smell the garum works before you see them. Fish, salt and a summer in the sun make a sauce that " ..
      "sells across the sea for more than wine. A Garum workshop needs fish, salt from the salt works, and " ..
      "amphorae from the potters.", AEDILE)
  end
  if day == 14 then
    urbs.message("Archimedes",
      "Archimedes lived here. He held off a Roman fleet for two years with cranes, mirrors and arithmetic, and in " ..
      "the end a soldier killed him because he would not stop drawing. His tomb is somewhere near the Agrigentine " ..
      "gate, under the brambles. Nobody here remembers where. I am going to find it.", ENGINEER)
  end
  if day == 20 then
    urbs.message("Neptune",
      "Every captain here pours wine into the sea before he sails. An angry Neptune sends storms that wreck ships " ..
      "in port. A temple to him would be wise in a harbor town, and cheaper than a fleet.", HARUSPEX)
  end
  if day == 40 and not S.tomb then
    S.tomb = true
    urbs.message("Found",
      "I found it. A small column in a thicket by the gate, with a sphere and a cylinder carved on top, as he asked. " ..
      "I had the thorns cut back. The Syracusans came to look and were embarrassed, as they should be.", ENGINEER)
  end
  if pop >= 700 and not S.r1 then
    S.r1 = urbs.request({kind = "goods", good = "wheat", amount = 150, days = 32, reward = 1600, favor = 10, penalty = -10})
    urbs.message("Bread for Rome",
      "The crowds in Rome are hungry and the Senate is frightened. It asks for 150 sacks of wheat within 32 days.",
      QUAESTOR)
  end
  if pop >= 1300 and not S.r2 then
    S.r2 = urbs.request({kind = "goods", good = "garum", amount = 40, days = 32, reward = 2200, favor = 10, penalty = -10})
    urbs.message("Garum for the Senate",
      "The senators have tasted our garum. They want 40 amphorae of it within 32 days.", AEDILE)
  end
  if day == 55 and not S.storm then
    S.storm = true
    urbs.trigger("storm")
    urbs.message("A storm from the south",
      "Roof tiles in the streets, and every ship in the bay running for the harbor. Send the engineers round " ..
      "before the damaged walls come down.", ENGINEER)
  end
  -- Rome keeps asking: a new request every 24 days once the scripted ones are done.
  if S.r2 and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"wheat", 120, 1200, "120 sacks of wheat"}, {"garum", 30, 1800, "30 amphorae of garum"},
                  {"olive_oil", 40, 1200, "40 amphorae of olive oil"}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -3})
    urbs.message("Rome writes again", "Rome asks for " .. pick[4] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 24
  if ok then
    urbs.message("The ships are in",
      "The cargo reached Ostia, and the price of bread in Rome fell by a quarter. The Senate remembers who fed " ..
      "the city.", QUAESTOR)
  end
end
