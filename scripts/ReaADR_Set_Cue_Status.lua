-- ReaADR_Set_Cue_Status.lua
-- Compatibility launcher for native cue-status mutation.

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

if run_native_command("_ReaADRSetCueStatusNative") then return end

reaper.ShowMessageBox(
  "The native ReaADR Set Cue Status command is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension to change cue status.",
  "ReaADR Tools",
  0
)
