-- Mission 11: Lugdunum - capital of the Three Gauls. The Republic becomes an Empire: the Emperor's favor,
-- the imperial cult, the mint and a triumphal arch. Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Where the rivers meet",
    "Two rivers, a hill, and sixty Gaulish tribes who send their delegates here every summer.\n\n" ..
    "A Mint strikes silver into denarii for the treasury; an ingot is worth more coined than sold. The chiefs of " ..
    "Gaul respect strength and splendour, and a Triumphal arch shows both.", QUAESTOR)
end

function on_day(day)
  local pop = urbs.population()
  if day == 30 and not S.empire then
    S.empire = true
    urbs.set_empire()
    urbs.message("Augustus",
      "The Senate has given Octavian the name Augustus, and powers no one has held before. Rome has an emperor now, " ..
      "though nobody in Rome uses the word. From today it is his favor you must keep, not the Senate's.\n\n" ..
      "Temples to the Divine Julius, the imperial cult, please him greatly. I have read the omens for his reign. " ..
      "They were good. They had better be.", HARUSPEX)
  end
  if day == 50 and not S.mela then
    S.mela = true
    urbs.message("Friend of the Emperor",
      "Lucius Mela has been named a friend of the Emperor, which is a title now. He dines at the palace. " ..
      "He sends you his compliments, which worries me more than his insults ever did.", QUAESTOR)
  end
  if pop >= 2500 and not S.req then
    S.req = urbs.request({kind = "goods", good = "silver", amount = 60, days = 36, reward = 4000, favor = 12, penalty = -10})
    urbs.message("Silver for the Emperor",
      "The Emperor's treasury needs Gallic silver: 60 ingots within 36 days. Note whose treasury it is now.", QUAESTOR)
  end

  -- Rome keeps asking: a new request every 26 days once the scripted ones are done.
  if S.req and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"silver", 40, 3000, "40 ingots of silver"}, {"tools", 40, 2000, "40 sets of tools"},
                  {"wine", 60, 1800, "60 amphorae of wine"}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -3})
    urbs.message("The palace writes", "The palace asks for " .. pick[4] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 26
  if ok then
    urbs.message("Acknowledged",
      "The palace acknowledges receipt, in the third person. That is how the palace says thank you.", QUAESTOR)
  end
end
