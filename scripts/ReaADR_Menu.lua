-- ReaADR script launcher menu.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local base_dir = script_dir()

local actions = {
  { label = "Import Cue Sheet", script = "ReaADR_Import_Cue_Sheet.lua" },
  { label = "Overlay Settings", script = "ReaADR_Overlay_Settings.lua" },
}

local function menu_text()
  local labels = {}
  for i, action in ipairs(actions) do
    labels[i] = action.label
  end
  return table.concat(labels, "|")
end

local mouse_x, mouse_y = reaper.GetMousePosition()
gfx.init("ReaADR", 0, 0, 0, mouse_x, mouse_y)
local choice = gfx.showmenu(menu_text())
gfx.quit()

local action = actions[choice]
if action then
  dofile(base_dir .. "/" .. action.script)
end
