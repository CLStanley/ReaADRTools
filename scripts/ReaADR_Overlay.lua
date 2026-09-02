-- ReaADR_Overlay.lua
-- Refresh the ReaADR video overlay FX from the current session cue data.
-- Assign this to a keyboard shortcut for a one-key overlay rebuild.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local status, err = ReaADR.refresh_overlay_fx_from_project()
if status then
  reaper.ShowConsoleMsg("[ReaADR] Video overlay refreshed: " .. tostring(status) .. "\n")
else
  ReaADR.message("Video overlay refresh failed:\n\n" .. tostring(err or "unknown error"))
end
