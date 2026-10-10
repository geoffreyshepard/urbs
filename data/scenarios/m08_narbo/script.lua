-- Mission 8: Narbo Martius - Gallia. A trade hub: carts, dynamic markets, basilica and courts, library, great temple.
-- Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Narbo",
    "Three roads, a river and the sea, and merchants on all of them. We are to make this the market of the West.\n\n" ..
    "Open as many routes as you can afford. A Cart depot's carters haul big loads over long distances, so storage " ..
    "can sit far from the workshops. A Basilica houses the courts, which cut crime, and collects taxes like a forum.",
    QUAESTOR)
end

function on_day(day)
  local pop = urbs.population()
  if day == 12 then
    urbs.message("A great temple",
      "A Great temple serves many more worshippers than a temple and pleases its god greatly. The Temple of Jupiter " ..
      "is a monument. It rises in four stages as stone and marble arrive, and it will outlast all of us.", HARUSPEX)
  end
  if pop >= 1200 and not S.r1 then
    S.r1 = urbs.request({kind = "goods", good = "clothing", amount = 60, days = 32, reward = 2200, favor = 10, penalty = -10})
    urbs.message("A letter from Mela",
      "Lucius Mela, commanding the legion in Gaul, presents his compliments and requires 60 bolts of cloth for " ..
      "cloaks within 32 days. He hopes the new man can manage it. I am quoting.", LEGATE)
  end
  if pop >= 2200 and not S.r2 then
    S.r2 = urbs.request({kind = "goods", good = "wine", amount = 60, days = 32, reward = 2400, favor = 10, penalty = -10})
    urbs.message("Wine for the chiefs",
      "Rome wins over the Gaulish chiefs with wine. One amphora buys a friendship; a cartload buys a tribe. " ..
      "Send 60 amphorae within 32 days.", QUAESTOR)
  end
  if day == 40 and not S.fair then
    S.fair = true
    urbs.choice("A fair",
      "Merchants from three provinces want to hold a fair here. It would cost 800 denarii for the field, the stalls " ..
      "and the guards. It would bring goodwill and new customers.",
      {"Hold the fair (-800 denarii)", "Not this year"}, "fair", AEDILE)
  end
  -- Rome keeps asking: a new request every 24 days once the scripted ones are done.
  if S.r2 and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"clothing", 50, 1800, "50 bolts of cloth"}, {"wine", 50, 1800, "50 amphorae of wine"},
                  {"furniture", 20, 2000, "20 pieces of furniture"}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -3})
    urbs.message("Rome writes again", "Rome asks for " .. pick[4] .. " within 30 days.", QUAESTOR)
  end
end

function fair(option)
  if option == 1 then
    urbs.take_money(800)
    urbs.change_favor(4)
    urbs.offer_route("germania")
    urbs.message("The fair",
      "It was loud, crowded and profitable. Traders from beyond the Rhine came, drank our wine, and now want a route " ..
      "of their own. Look for Germania in the Trade panel.", AEDILE)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 24
  if ok then
    urbs.message("On time",
      "Everything arrived on time. Even Mela's quartermaster could find nothing to complain about, though he tried.",
      QUAESTOR)
  end
end
