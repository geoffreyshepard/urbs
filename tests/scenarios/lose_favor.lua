-- Rome asks for what the town cannot make, again and again: favor runs out and the mission is lost
-- (on Civitas and Imperium; tests/scenarios/otium_no_loss.json runs the same on Otium).
local S = urbs.state
local function ask()
  S.req = urbs.request({kind = "goods", good = "jewelry", amount = 500, days = 1, reward = 0, favor = 5, penalty = -30})
end
function on_start() ask() end
function on_request(id, ok) if not ok then ask() end end
