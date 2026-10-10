-- Example mod scenario: shows messages, a request for the mod's own good, and urbs.state.
local S = urbs.state

function on_start()
  urbs.message("The vintners' valley",
    "Vineyards cover these hills. Wineries make wine; the new Vintner's cellar ages it into amber wine, which " ..
    "the traders of Rhodes pay well for.", 2)
end

function on_day(day)
  if urbs.population() >= 800 and not S.req then
    S.req = urbs.request({kind = "goods", good = "amber_wine", amount = 10, days = 40, reward = 2000, favor = 8, penalty = -4})
    urbs.message("A taste for amber", "Rome wants 10 amphorae of amber wine within 40 days.", 2)
  end
end
