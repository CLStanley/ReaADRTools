-- Move the edit cursor to the next ADR cue.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local cues = ReaADR.navigation_cues()
if not cues or #cues == 0 then
  ReaADR.message("No cue regions or markers were found.")
  return
end

local cue = ReaADR.find_next_cue(cues, ReaADR.current_timeline_position())
ReaADR.jump_to_cue(cue)
ReaADR.refresh_overlay_silent()
