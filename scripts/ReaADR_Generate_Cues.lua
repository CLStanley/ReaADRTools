-- ReaADR_Generate_Cues.lua
-- Compatibility launcher for native marker/region cue synchronization.

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

if run_native_command("_ReaADRUpdateCuesFromRegionsNative") then return end

reaper.ShowMessageBox(
  "The native ReaADR marker/region cue synchronization command is unavailable.\n\n" ..
  "Install or update the ReaADR Tools extension to generate or update cues from project markers and regions.",
  "ReaADR Tools",
  0
)
