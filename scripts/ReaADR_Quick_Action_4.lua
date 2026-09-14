local SLOT = 4
local DEFAULT_ACTION = "overlay_settings"
local NAMESPACE = "ReaADRTools"

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

local function open_native_manager_tab(tab)
  if type(reaper.SetProjExtState) ~= "function" then return false end
  reaper.SetProjExtState(0, NAMESPACE, "ui.manager.launch_tab", tab)
  if run_native_command("_ReaADRShowCueManagerNative") then return true end
  reaper.SetProjExtState(0, NAMESPACE, "ui.manager.launch_tab", "")
  return false
end

local action = type(reaper.GetExtState) == "function"
  and reaper.GetExtState(NAMESPACE, "quick_action_" .. SLOT) or ""
if action == "" then action = DEFAULT_ACTION end

if action == "import" and run_native_command("_ReaADRImportCueSheetNative") then return end
if action == "cue_manager" and run_native_command("_ReaADRShowCueManagerNative") then return end
if action == "record_cue" and run_native_command("_ReaADRRecordCueNative") then return end
if action == "character_filter" and run_native_command("_ReaADRApplyCharacterFilterNative") then return end
if action == "refresh_overlay" and run_native_command("_ReaADRRefreshVideoOverlayNative") then return end
if action == "export_reports" and open_native_manager_tab("reports") then return end
if action == "overlay_settings" and open_native_manager_tab("overlay") then return end

-- Older extension builds retain the compatibility App fallback until every
-- native command above is registered by the loaded extension.
local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local App = dofile(script_dir() .. "/ReaADR_App.lua")
App.run_quick_action(SLOT)
