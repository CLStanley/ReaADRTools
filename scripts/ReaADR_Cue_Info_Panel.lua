-- ReaADR_Cue_Info_Panel.lua
-- Compatibility launcher for the native Cue Info workflow.

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

if run_native_command("_ReaADRShowCueInfoNative") then return end

reaper.ShowMessageBox(
  "The native ReaADR Cue Info command is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension to open Cue Info.",
  "ReaADR Tools",
  0
)
