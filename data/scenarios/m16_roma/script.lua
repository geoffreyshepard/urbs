-- Mission 16: Roma - caput mundi. Rebuild Rome after the great fire: the Colosseum, the Pantheon and the
-- Circus Maximus; every system at once. Story and voices: docs/12-story.md (the ending is in data/campaign.json).
local S = urbs.state
local ENGINEER, QUAESTOR, HARUSPEX, LEGATE, AEDILE = 1, 2, 3, 4, 5

function on_start()
  urbs.message("Rome",
    "You can smell it from the Alban hills: smoke and wet ash. The people are living in tents on the Field of Mars " ..
    "and in the gardens across the river. A million people lived here once. Bring back ten thousand to begin.\n\n" ..
    "Then give them the great works: the Colosseum, the Pantheon, and the Circus Maximus.", AEDILE)
end

-- The fiscus pays every 20 days; Calidius does not say the same thing twice.
local kGrants = {
  "The Emperor's treasury sends 4,000 denarii for the rebuilding of the City. It came in a wagon with forty guards.",
  "Another 4,000 from the fiscus. In the margin, the palace asks how the amphitheatre is coming along.",
  "4,000 more. I am told the Emperor reads our accounts himself, so I have made them very clear.",
  "The fiscus again: 4,000 denarii, and a note that the treasury is not bottomless. It is not. I have seen the bottom.",
}

function on_day(day)
  if day % 20 == 0 and day > 0 then
    S.grants = (S.grants or 0) + 1
    urbs.add_money(4000)
    urbs.message("The fiscus", kGrants[(S.grants - 1) % #kGrants + 1], QUAESTOR)
  end
  if day == 8 and not S.omen then
    S.omen = true
    urbs.message("The auspices",
      "I took the auspices on the Capitol this morning. Six kites came over from the north and turned above the " ..
      "river. The same six, I think, that crossed your furrow in Latium. The road ends here.", HARUSPEX)
  end
  if day == 45 and not S.plague then
    S.plague = true
    urbs.trigger("plague")
    urbs.message("Sickness",
      "Disease in the crowded quarters, where the refugees are packed into tents. Physicians and hospitals will " ..
      "help. Clean water will help more.", ENGINEER)
  end
end
