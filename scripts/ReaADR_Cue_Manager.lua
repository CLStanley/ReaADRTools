-- Historical Cue Manager entry point.
--
-- The Cue Manager is now owned by the native C++ Manager. Keep this script as
-- a compatibility shim so old REAPER actions, toolbar buttons, and shortcuts
-- continue to work after the migration.

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

-- Older extension builds do not expose the native command. Preserve the
-- existing Lua implementation as a fallback rather than breaking those installs.
local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local base = script_dir()
local ReaADR = dofile(base .. "/ReaADR_Core.lua")
if ReaADR.cue_manager_auto_dock_enabled() then
  dofile(base .. "/ReaADR_Cue_Manager_Gfx.lua")
  return
end

if type(reaper.ImGui_CreateContext) == "function" then
  local ok, err = pcall(dofile, base .. "/ReaADR_Cue_Manager_ImGui.lua")
  if ok then
    return
  end
  reaper.ShowMessageBox(
    "ReaADR could not open the ReaImGui cue manager.\n\nFalling back to the legacy manager.\n\n"
      .. tostring(err),
    "ReaADR",
    0
  )
end

dofile(base .. "/ReaADR_Cue_Manager_Gfx.lua")
