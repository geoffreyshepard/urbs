-- Exercises the scripting API from a headless test.
local S = urbs.state
function on_start()
  S.started = true
  S.days = 0
  urbs.message("Test", "Script started")
  S.req = urbs.request({kind = "denarii", amount = 100, days = 2, reward = 0, favor = 5, penalty = -5})
end
function on_day(day)
  S.days = S.days + 1
  if day == 2 then
    urbs.choice("Question", "Pick one", {"A", "B"}, "answered")
    urbs.trigger("drought", 2)
  end
  if day == 4 and S.req_failed and urbs.favor() < 50 then
    S.checked = true
  end
end
function answered(o) S.answer = o end
function on_request(id, ok) if id == S.req and not ok then S.req_failed = true end end
