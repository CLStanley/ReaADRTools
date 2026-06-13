local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local App = dofile(script_dir() .. "/ReaADR_App.lua")
App.run_quick_action(1)
