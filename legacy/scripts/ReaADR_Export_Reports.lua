-- Export ReaADR reporting data through the application framework.
-- The current report format is the cue-sheet CSV export.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local App = dofile(script_dir() .. "/ReaADR_App.lua")
App.export_reports()
