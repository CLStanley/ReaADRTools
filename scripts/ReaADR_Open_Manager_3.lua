-- ReaADR_Open_Manager_3.lua
-- Compatibility launcher for the third historical Manager slot.

local SLOT = 3
local NAMESPACE = "ReaADRTools"
local NATIVE_LAUNCH_TAB_KEY = "ui.manager.launch_tab"

local function slot_key(suffix)
  return ("ui.manager_slot.%d.%s"):format(SLOT, suffix)
end

local function consume_launch_tab()
  local _, tab = reaper.GetProjExtState(0, NAMESPACE, slot_key("launch_tab"))
  reaper.SetProjExtState(0, NAMESPACE, slot_key("launch_tab"), "")
  return tab ~= "" and tab or nil
end

local function release_legacy_slot()
  reaper.SetProjExtState(0, NAMESPACE, slot_key("active"), "")
  reaper.SetProjExtState(0, NAMESPACE, slot_key("heartbeat"), "")
  reaper.SetProjExtState(0, NAMESPACE, slot_key("launching"), "")
end

local launch_tab = consume_launch_tab()
if type(reaper.NamedCommandLookup) == "function" and type(reaper.Main_OnCommand) == "function" then
  local command_id = reaper.NamedCommandLookup("_ReaADRShowCueManagerNative")
  if command_id and command_id ~= 0 then
    if launch_tab then reaper.SetProjExtState(0, NAMESPACE, NATIVE_LAUNCH_TAB_KEY, launch_tab) end
    release_legacy_slot()
    reaper.Main_OnCommand(command_id, 0)
    return
  end
end

release_legacy_slot()
reaper.ShowMessageBox("The native ReaADR Cue Manager is unavailable.\n\nInstall or update the ReaADR Tools extension to open the Cue Manager.", "ReaADR Tools", 0)
