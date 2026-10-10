-- Mission 10: Corinth - refounded by Caesar. Greek culture: odeon, academy, marble, dyes, scriptorium, research.
-- Story and voices: docs/12-story.md.
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Corinth",
    "Grass grows in the old marketplace. The columns of the temple of Apollo still stand, the only ones that do. " ..
    "The settlers are freedmen from Rome, veterans, and a few Greeks who never left.\n\n" ..
    "Quarry marble for the sculptors, the Odeon and the Academy. Scholars in libraries and academies produce " ..
    "learning for civic research, which you direct from the Research panel.", AEDILE)
end

function on_day(day)
  local pop = urbs.population()
  if day == 10 then
    urbs.message("Purple and papyrus",
      "Two trades worth knowing. Dye works crush murex shells into purple, and dyers turn it into dyed cloth that " ..
      "the rich pay absurd prices for. Papyrus from the marsh feeds a scriptorium, whose scrolls sell in Rome and " ..
      "in Gaul, where there is apparently a market for reading.", QUAESTOR)
  end
  if day == 20 and not S.postumius then
    S.postumius = true
    urbs.message("A letter from Rome",
      "From Postumius's freedman. Aulus Postumius died in his sleep eleven days ago. He left you his books and a " ..
      "silver cup, and asked to be remembered at the Parentalia.", QUAESTOR)
  end
  if pop >= 2000 and not S.req then
    S.req = urbs.request({kind = "goods", good = "statues", amount = 10, days = 40, reward = 4000, favor = 12, penalty = -10})
    urbs.message("Statues for Rome",
      "Rome's new forum needs Greek statues: 10 within 40 days. The sculptors say it cannot be done, and have " ..
      "already started.", AEDILE)
  end
  if day == 60 and not S.quake then
    S.quake = true
    urbs.trigger("earthquake")
    urbs.message("The earth shakes",
      "An earthquake. Cracked walls everywhere, and half the new houses are leaning. Get the engineers round before " ..
      "the damaged buildings come down.", ENGINEER)
  end
  if day == 75 and not S.ides then
    S.ides = true
    urbs.message("The Ides of March",
      "News from Rome. Caesar is dead, stabbed in the Senate house on the Ides of March.\n\n" ..
      "I met him once. I read the liver of his sacrifice and told him to beware the Ides. On the morning itself he " ..
      "passed me on the steps and laughed: the Ides have come, Spurinna. I said: come, but not gone.", HARUSPEX)
  end
  -- Rome keeps asking: a new request every 30 days once the scripted ones are done.
  if S.req and day >= (S.next_req or 100000) and not S.open_req then
    local list = {{"statues", 8, 3200, "8 statues"}, {"dyed_clothing", 30, 2400, "30 bolts of dyed cloth"},
                  {"marble", 40, 2000, "40 blocks of marble"}}
    local pick = list[(S.nreq or 0) % #list + 1]
    S.nreq = (S.nreq or 0) + 1
    S.open_req = urbs.request({kind = "goods", good = pick[1], amount = pick[2], days = 30, reward = pick[3], favor = 6, penalty = -3})
    urbs.message("Rome writes again", "Rome asks for " .. pick[4] .. " within 30 days.", QUAESTOR)
  end
end

function on_request(id, ok)
  if id == S.open_req then S.open_req = nil end
  S.next_req = urbs.mission_day() + 30
  if ok then
    urbs.message("Greek beauty",
      "The shipment delighted Rome. Somewhere in the new forum, a Corinthian bronze made last month is being admired " ..
      "as an old master.", AEDILE)
  end
end
