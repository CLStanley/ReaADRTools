-- Open the unified ReaADR Tools manager.
--
-- The native C++ Manager owns the public launch path. Keep this ReaScript as a
-- compatibility shim for older installs and user shortcuts that still point at
-- the historical Lua action.

local NATIVE_COMMAND = "_ReaADRShowCueManagerNative"

local function launch_native_manager()
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

if launch_native_manager() then
  return
end

-- Compatibility fallback for projects opened with an older extension build
-- that does not yet register the native Manager command.
local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local App = dofile(script_dir() .. "/ReaADR_App.lua")
App.launch_manager()
