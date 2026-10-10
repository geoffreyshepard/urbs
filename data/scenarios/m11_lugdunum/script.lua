-- Mission 11: Lugdunum - capital of the Three Gauls. The Republic becomes an Empire: the Emperor's favor,
-- the imperial cult, the mint and a triumphal arch.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Where the rivers meet",
    "Lugdunum stands where the Rhodanus meets the Arar, the capital of the Three Gauls. Its hills hold silver.\n\n" ..
    "A Mint strikes silver into denarii for the treasury: each ingot is worth more coined than sold. " ..
    "Gaul's chieftains respect strength and splendour; a Triumphal arch will show both.", QUAESTOR)
end

function on_day(day)
  local pop = urbs.population()
  if day == 30 and not S.empire then
    S.empire = true
    urbs.set_empire()
    urbs.message("Ave, Imperator",
      "The Senate has given Octavian the name Augustus. Rome has an Emperor now, and it is his favor you must win. " ..
      "Temples to the Divine Julius (the imperial cult) please him greatly.", HARUSPEX)
  end
  if pop >= 2500 and not S.req then
    S.req = urbs.request({kind = "goods", good = "silver", amount = 60, days = 36, reward = 4000, favor = 12, penalty = -10})
    urbs.message("Silver for the Emperor", "The Emperor's treasury needs Gallic silver: 60 ingots within 36 days.", QUAESTOR)
  end

  -- Rome keeps asking: a new request every 26 days once the scripted ones are done.
  if S.req and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"silver", 40, 3000}, {"tools", 40, 2000}, {"wine", 60, 1800}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -6})
    urbs.message("Another request", "Rome asks for " .. pick[2] .. " " .. pick[1] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 26
  if ok then urbs.message("Well done", "The Emperor is pleased with Lugdunum.", QUAESTOR) end
end
