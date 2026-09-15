-- ReaADR_Record_Cue.lua
-- Compatibility launcher for the native Record Cue workflow.

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

if run_native_command("_ReaADRRecordCueNative") then return end

reaper.ShowMessageBox(
  "The native ReaADR Record Cue command is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension before recording ADR cues.",
  "ReaADR Tools",
  0
)
