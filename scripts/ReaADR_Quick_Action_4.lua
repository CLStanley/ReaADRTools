local SLOT = 4
local DEFAULT_ACTION = "overlay_settings"
local NAMESPACE = "ReaADRTools"

local function run_native_command(name)
  if type(reaper.NamedCommandLookup) ~= "function" or
     type(reaper.Main_OnCommand) ~= "function" then
    return false
  end
  local command_id = reaper.NamedCommandLookup(name)
  if not command_id or command_id == 0 then return false end
  reaper.Main_OnCommand(command_id, 0)
  return true
end

local action = type(reaper.GetExtState) == "function"
  and reaper.GetExtState(NAMESPACE, "quick_action_" .. SLOT) or ""
if action == "" then action = DEFAULT_ACTION end

if action == "cue_manager" and run_native_command("_ReaADRShowCueManagerNative") then return end
if action == "refresh_overlay" and run_native_command("_ReaADRRefreshVideoOverlayNative") then return end
if action == "overlay_settings" and type(reaper.SetProjExtState) == "function" then
  reaper.SetProjExtState(0, NAMESPACE, "ui.manager.launch_tab", "overlay")
  if run_native_command("_ReaADRShowCueManagerNative") then return end
  reaper.SetProjExtState(0, NAMESPACE, "ui.manager.launch_tab", "")
end

-- Import, reports, Record Cue, Character Filter, and older extension builds
-- still use the compatibility App path until their native presentation is at parity.
local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local App = dofile(script_dir() .. "/ReaADR_App.lua")
App.run_quick_action(SLOT)
