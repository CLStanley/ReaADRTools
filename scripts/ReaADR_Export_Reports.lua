-- ReaADR_Export_Reports.lua
-- Compatibility launcher for the native Cue Manager Reports surface.

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

reaper.ShowMessageBox(
  "The native ReaADR Reports surface is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension to export reports.",
  "ReaADR Tools",
  0
)
