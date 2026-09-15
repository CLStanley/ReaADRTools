-- ReaADR_Export_Cue_Sheet.lua
-- Compatibility launcher for native cue-sheet export in the Manager Reports surface.

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
  reaper.SetProjExtState(0, "ReaADRTools", "ui.manager.launch_tab", "reports")
end

if run_native_command("_ReaADRShowCueManagerNative") then return end

reaper.ShowMessageBox(
  "The native ReaADR cue-sheet export surface is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension to export cue sheets.",
  "ReaADR Tools",
  0
)
