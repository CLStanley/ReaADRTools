-- ReaADR_Quick_Action_2.lua
-- Compatibility launcher. Quick Action preference resolution and dispatch are native.

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

if run_native_command("_ReaADRQuickAction2Native") then return end

reaper.ShowMessageBox(
  "The native ReaADR Quick Action 2 command is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension to use Quick Actions.",
  "ReaADR Tools",
  0
)
