-- ReaADR_Start_Recording_Workflow.lua
-- Compatibility launcher for the historical recording-workflow action.
-- Native Record Cue now owns the recording workflow.

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
  "The native ReaADR recording workflow is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension to record the current cue.",
  "ReaADR Tools",
  0
)
