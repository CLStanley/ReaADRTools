-- ReaADR_Jump_To_Selected_Cue.lua
-- Compatibility launcher for the historical selected-cue jump action.
-- Native cue navigation owns cursor movement and canonical cue selection.

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

if run_native_command("_ReaADRJumpToCueNative") then return end

reaper.ShowMessageBox(
  "The native ReaADR cue navigation command is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension to jump to the selected cue.",
  "ReaADR Tools",
  0
)
