-- Open the native Cue Manager Reports surface.
-- Keep the Lua application path only as an upgrade fallback for older builds.

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

if type(reaper.SetProjExtState) == "function" then
  reaper.SetProjExtState(0, NAMESPACE, "ui.manager.launch_tab", "reports")
  if run_native_command("_ReaADRShowCueManagerNative") then return end
  reaper.SetProjExtState(0, NAMESPACE, "ui.manager.launch_tab", "")
end

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local App = dofile(script_dir() .. "/ReaADR_App.lua")
App.export_reports()
