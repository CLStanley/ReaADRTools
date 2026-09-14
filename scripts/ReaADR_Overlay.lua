-- Refresh the ReaADR video overlay FX from the current session cue data.
--
-- Overlay refresh is owned by the native C++ application service. Keep the Lua
-- implementation only as a compatibility fallback for older extension builds.

local NATIVE_COMMAND = "_ReaADRRefreshVideoOverlayNative"

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

local status, err = ReaADR.refresh_overlay_fx_from_project()
if status then
  reaper.ShowConsoleMsg("[ReaADR] Video overlay refreshed: " .. tostring(status) .. "\n")
else
  ReaADR.message("Video overlay refresh failed:\n\n" .. tostring(err or "unknown error"))
end
