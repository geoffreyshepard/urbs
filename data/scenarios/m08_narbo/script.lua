-- Mission 8: Narbo Martius - Gallia. A trade hub: carts, dynamic markets, basilica and courts, library, great temple.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("The Via Domitia",
    "Narbo sits where the road from Italy to Spain meets the road to Gaul and the sea. Make it the market of the West.\n\n" ..
    "Open many routes. A Cart depot's carters haul big loads over long distances, so storage can sit far from the " ..
    "workshops. A Basilica houses courts (less crime) and collects taxes like a forum.", QUAESTOR)
end

function on_day(day)
  local pop = urbs.population()
  if day == 12 then
    urbs.message("A great temple", "A Great temple serves far more worshippers than a temple and pleases its god " ..
      "greatly. The Temple of Jupiter, a monument, rises in four stages as stone and marble arrive.", HARUSPEX)
  end
  if pop >= 1200 and not S.r1 then
    S.r1 = urbs.request({kind = "goods", good = "clothing", amount = 60, days = 32, reward = 2200, favor = 10, penalty = -10})
    urbs.message("Cloaks for the legions", "The army in Gaul needs 60 bolts of clothing within 32 days.", LEGATE)
  end
  if pop >= 2200 and not S.r2 then
    S.r2 = urbs.request({kind = "goods", good = "wine", amount = 60, days = 32, reward = 2400, favor = 10, penalty = -10})
    urbs.message("Wine for the Gauls", "Rome wins Gaulish chieftains over with wine. Send 60 amphorae within 32 days.", QUAESTOR)
  end
  if day == 40 and not S.fair then
    S.fair = true
    urbs.choice("A great fair",
      "Merchants from three provinces propose a fair in Narbo. It would cost 800 denarii in preparations, but bring " ..
      "goodwill and trade.", {"Hold the fair (-800 denarii)", "Not this year"}, "fair")
  end
  -- Rome keeps asking: a new request every 24 days once the scripted ones are done.
  if S.r2 and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"clothing", 50, 1800}, {"wine", 50, 1800}, {"furniture", 20, 2000}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -6})
    urbs.message("Another request", "Rome asks for " .. pick[2] .. " " .. pick[1] .. " within 30 days.", QUAESTOR)
  end
end

function fair(option)
  if option == 1 then
    urbs.take_money(800)
    urbs.change_favor(4)
    urbs.offer_route("germania")
    urbs.message("The fair", "Traders from beyond the Rhine came to the fair and now ask for a route.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 24
  if ok then urbs.message("Well supplied", "Your deliveries arrive on time. Narbo's name is known in Rome.", QUAESTOR) end
end
