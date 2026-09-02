-- Prefer ReaImGui for the main cue manager. Fall back to the legacy gfx
-- manager when ReaImGui is not installed.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local base = script_dir()

local ReaADR = dofile(base .. "/ReaADR_Core.lua")
if ReaADR.cue_manager_auto_dock_enabled() then
  dofile(base .. "/ReaADR_Cue_Manager_Gfx.lua")
  return
end

if type(reaper.ImGui_CreateContext) == "function" then
  local ok, err = pcall(dofile, base .. "/ReaADR_Cue_Manager_ImGui.lua")
  if ok then
    return
  end
  reaper.ShowMessageBox(
    "ReaADR could not open the ReaImGui cue manager.\n\nFalling back to the legacy manager.\n\n"
      .. tostring(err),
    "ReaADR",
    0
  )
end

dofile(base .. "/ReaADR_Cue_Manager_Gfx.lua")
