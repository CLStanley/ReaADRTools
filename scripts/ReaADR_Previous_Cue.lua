-- Move the edit cursor to the previous ADR cue.
--
-- Navigation is owned by the native C++ service. Keep the historical Lua path
-- only as a compatibility fallback for installs that predate the native action.

local NATIVE_COMMAND = "_ReaADRPreviousCueNative"

local function run_native()
  if type(reaper.NamedCommandLookup) ~= "function" or
     type(reaper.Main_OnCommand) ~= "function" then
    return false
  end

  local command_id = reaper.NamedCommandLookup(NATIVE_COMMAND)
  if not command_id or command_id == 0 then
    return false
  end

  reaper.Main_OnCommand(command_id, 0)
  return true
end

if run_native() then
  return
end

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

local cue = ReaADR.find_previous_cue(cues, ReaADR.current_timeline_position())
ReaADR.jump_to_cue(cue)
ReaADR.refresh_overlay_silent()
