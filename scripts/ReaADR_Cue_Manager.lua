-- ReaADR_Cue_Manager.lua
-- Historical Cue Manager entry point retained for toolbar/action compatibility.
-- The Cue Manager is owned exclusively by the native C++ extension.

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

if run_native_command("_ReaADRShowCueManagerNative") then return end

reaper.ShowMessageBox(
  "The native ReaADR Cue Manager is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension to open the Cue Manager.",
  "ReaADR Tools",
  0
)
