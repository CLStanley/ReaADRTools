-- Prompt for an ADR cue number and move the edit cursor to it.

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

local ok, cue_id = reaper.GetUserInputs("Jump To ADR Cue", 1, "Cue number or ID:", "")
if not ok then
  return
end

local cue = ReaADR.find_cue_by_id(cues, cue_id)
if not cue then
  ReaADR.message(("Cue not found: %s"):format(tostring(cue_id or "")))
  return
end

ReaADR.jump_to_cue(cue)
