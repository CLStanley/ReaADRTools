-- ReaADR_Character_Filter.lua
-- Compatibility launcher for the native Character Filter workflow.

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

if run_native_command("_ReaADRApplyCharacterFilterNative") then return end

reaper.ShowMessageBox(
  "The native ReaADR Character Filter command is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension to use character filtering.",
  "ReaADR Tools",
  0
)
