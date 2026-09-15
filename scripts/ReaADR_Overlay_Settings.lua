-- ReaADR_Overlay_Settings.lua
-- Compatibility launcher for the native Manager Overlay surface.

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

-- The native Manager consumes this one-shot project state and opens directly
-- on its Overlay surface.
if type(reaper.SetProjExtState) == "function" then
  reaper.SetProjExtState(0, "ReaADRTools", "ui.manager.launch_tab", "overlay")
end

if run_native_command("_ReaADRShowCueManagerNative") then return end

reaper.ShowMessageBox(
  "The native ReaADR Overlay Settings surface is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension to configure overlays.",
  "ReaADR Tools",
  0
)
