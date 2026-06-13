-- Export ReaADR cues, regions, or markers to a CSV cue sheet.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local function split_path(path)
  local folder, file = tostring(path or ""):match("^(.*[/\\])([^/\\]*)$")
  if folder then
    return folder:gsub("[/\\]$", ""), file
  end
  return "", tostring(path or "")
end

local function join_path(folder, file)
  folder = tostring(folder or "")
  file = tostring(file or "")
  if folder == "" then
    return file
  end
  if folder:match("[/\\]$") then
    return folder .. file
  end
  return folder .. "/" .. file
end

local function file_exists(path)
  local file = io.open(path, "rb")
  if file then
    file:close()
    return true
  end
  return false
end

local function unique_export_path(path)
  path = tostring(path or "")
  if path == "" or not file_exists(path) then
    return path
  end

  local folder, file = split_path(path)
  local prefix = folder ~= "" and (folder .. "/") or ""
  local name, extension = file:match("^(.*)(%.[^%.]*)$")
  if not name or name == "" then
    name = file
    extension = ""
  end

  local index = 2
  local candidate = prefix .. name .. " " .. tostring(index) .. extension
  while file_exists(candidate) do
    index = index + 1
    candidate = prefix .. name .. " " .. tostring(index) .. extension
  end
  return candidate
end

local function browse_for_save_path(current_path)
  local folder, file = split_path(current_path)
  if file == "" then
    file = "reaadr_cue_sheet.csv"
  end

  if reaper.GetUserFileName then
    local initial = current_path
    if initial == "" then
      initial = join_path(folder, file)
    end
    local ok, selected = reaper.GetUserFileName(0, "Export ADR cue sheet", initial, "CSV files|*.csv|All files|*.*")
    if ok and selected and selected ~= "" then
      return selected
    end
    return current_path
  end

  if reaper.JS_Dialog_BrowseForSaveFile then
    local success, ok, selected = pcall(
      reaper.JS_Dialog_BrowseForSaveFile,
      "Export ADR cue sheet",
      folder,
      file,
      "CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0",
      "",
      4096
    )
    if success and ok == 1 and selected and selected ~= "" then
      return selected
    end
  end

  if reaper.JS_Dialog_BrowseForFolder then
    local success, ok, selected = pcall(
      reaper.JS_Dialog_BrowseForFolder,
      "Select ADR cue sheet export folder",
      folder,
      "",
      4096
    )
    if success and ok == 1 and selected and selected ~= "" then
      return join_path(selected, file)
    end
  end

  return current_path
end

local function key_code(name)
  local value = 0
  for i = 1, #name do
    value = value * 256 + name:byte(i)
  end
  return value
end

local KEY_LEFT = key_code("left")
local KEY_RIGHT = key_code("rght")
local KEY_HOME = key_code("home")
local KEY_END = key_code("end")
local KEY_DELETE = key_code("del")

local function draw_fit_text(text, cursor, max_width)
  text = tostring(text or "")
  cursor = math.max(0, math.min(cursor or #text, #text))
  local start = 1
  local display = text:sub(start)
  while gfx.measurestr(display) > max_width and start < cursor do
    start = start + 1
    display = text:sub(start)
  end

  local prefix = ""
  if cursor >= start then
    prefix = text:sub(start, cursor)
  end
  local prefix_width = gfx.measurestr(prefix)
  local width = gfx.measurestr(display)
  gfx.drawstr(display)
  return display, width, prefix_width, start
end

local function path_dialog(default_path, on_done)
  local state = {
    path = default_path or "reaadr_cue_sheet.csv",
    cursor = #(default_path or "reaadr_cue_sheet.csv"),
    last_mouse = 0,
    width = 860,
    height = 174,
    min_width = 720,
    min_height = 174,
    closed = false,
    focused = true,
  }

  local field = {}
  local browse = {}
  local export = {}
  local cancel = {}

  local function layout()
    state.width = math.max(state.min_width, gfx.w or state.width)
    state.height = math.max(state.min_height, gfx.h or state.height)
    field = { x = 24, y = 70, w = state.width - 220, h = 32 }
    browse = { x = state.width - 180, y = 70, w = 74, h = 32, label = "Browse" }
    export = { x = state.width - 226, y = state.height - 52, w = 100, h = 32, label = "Export" }
    cancel = { x = state.width - 114, y = state.height - 52, w = 90, h = 32, label = "Cancel" }
  end

  local function inside(rect, x, y)
    return x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
  end

  local function button(rect)
    gfx.set(0.22, 0.24, 0.26, 1)
    gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
    gfx.set(0.70, 0.74, 0.78, 1)
    gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
    gfx.set(1, 1, 1, 1)
    gfx.setfont(1, "Arial", 15)
    local tw, th = gfx.measurestr(rect.label)
    gfx.x = rect.x + (rect.w - tw) * 0.5
    gfx.y = rect.y + (rect.h - th) * 0.5
    gfx.drawstr(rect.label)
  end

  local function set_cursor_from_mouse()
    local x = gfx.mouse_x - field.x - 8
    if x <= 0 then
      state.cursor = 0
      return
    end

    local visible_start = state.visible_start or 1
    local best = #state.path
    for i = visible_start - 1, #state.path do
      local fragment = state.path:sub(visible_start, i)
      local width = gfx.measurestr(fragment)
      if width >= x then
        best = i
        break
      end
    end
    state.cursor = math.max(0, math.min(best, #state.path))
  end

  local function finish(path)
    if state.closed then
      return
    end
    state.closed = true
    gfx.quit()
    on_done(path)
  end

  local function frame()
    layout()
    gfx.set(0.12, 0.12, 0.12, 1)
    gfx.rect(0, 0, state.width, state.height, true)

    gfx.setfont(1, "Arial", 20)
    gfx.set(1, 1, 1, 1)
    gfx.x = 24
    gfx.y = 18
    gfx.drawstr("Export ADR Cue Sheet")

    gfx.setfont(1, "Arial", 14)
    gfx.set(0.82, 0.84, 0.86, 1)
    gfx.x = 24
    gfx.y = 48
    gfx.drawstr("CSV output path")

    gfx.set(0.05, 0.05, 0.05, 1)
    gfx.rect(field.x, field.y, field.w, field.h, true)
    if state.focused then
      gfx.set(0.95, 0.78, 0.30, 1)
    else
      gfx.set(0.55, 0.58, 0.62, 1)
    end
    gfx.rect(field.x, field.y, field.w, field.h, false)
    gfx.setfont(1, "Arial", 15)
    gfx.set(1, 1, 1, 1)
    gfx.x = field.x + 8
    gfx.y = field.y + 8
    local _, _, cursor_width, visible_start = draw_fit_text(state.path, state.cursor, field.w - 28)
    state.visible_start = visible_start
    if state.focused then
      local caret_x = math.min(field.x + 8 + cursor_width, field.x + field.w - 12)
      gfx.set(1, 1, 1, 1)
      gfx.line(caret_x, field.y + 7, caret_x, field.y + field.h - 7)
    end

    button(browse)
    button(export)
    button(cancel)

    gfx.update()

    local char = gfx.getchar()
    if char < 0 or char == 27 then
      finish(nil)
      return
    elseif char == 13 then
      finish(state.path)
      return
    elseif state.focused and char == KEY_LEFT then
      state.cursor = math.max(0, state.cursor - 1)
    elseif state.focused and char == KEY_RIGHT then
      state.cursor = math.min(#state.path, state.cursor + 1)
    elseif state.focused and (char == KEY_HOME or char == 1) then
      state.cursor = 0
    elseif state.focused and (char == KEY_END or char == 5) then
      state.cursor = #state.path
    elseif state.focused and char == KEY_DELETE then
      if state.cursor < #state.path then
        state.path = state.path:sub(1, state.cursor) .. state.path:sub(state.cursor + 2)
      end
    elseif state.focused and char == 8 then
      if state.cursor > 0 then
        state.path = state.path:sub(1, state.cursor - 1) .. state.path:sub(state.cursor + 1)
        state.cursor = state.cursor - 1
      end
    elseif state.focused and char >= 32 and char < 127 then
      state.path = state.path:sub(1, state.cursor) .. string.char(char) .. state.path:sub(state.cursor + 1)
      state.cursor = state.cursor + 1
    end

    local mouse = gfx.mouse_cap % 2
    if mouse == 1 and state.last_mouse == 0 then
      if inside(field, gfx.mouse_x, gfx.mouse_y) then
        state.focused = true
        set_cursor_from_mouse()
      elseif inside(browse, gfx.mouse_x, gfx.mouse_y) then
        state.focused = false
        state.path = browse_for_save_path(state.path)
        state.cursor = #state.path
      elseif inside(export, gfx.mouse_x, gfx.mouse_y) then
        finish(state.path)
        return
      elseif inside(cancel, gfx.mouse_x, gfx.mouse_y) then
        finish(nil)
        return
      else
        state.focused = false
      end
    end
    state.last_mouse = mouse

    reaper.defer(frame)
  end

  gfx.init("Export ADR Cue Sheet", state.width, state.height)
  frame()
end

local cues, source_message = ReaADR.load_last_import_cues()
local source = "cached ReaADR cues"
if not cues then
  cues = ReaADR.collect_project_marker_cues({
    include_markers = true,
    include_regions = true,
    character = "",
    cue_type = "",
    flexible_export = true,
  })
  source = "project markers/regions"
end

if not cues or #cues == 0 then
  ReaADR.message("No cue data was found to export.\n\nImport a cue sheet first, or create project markers/regions.")
  return
end

local project_path = ""
if reaper.GetProjectPath then
  project_path = ({ reaper.GetProjectPath("") })[1] or ""
end

local default_path = "reaadr_cue_sheet.csv"
if project_path ~= "" then
  default_path = project_path .. "/reaadr_cue_sheet.csv"
end
default_path = unique_export_path(default_path)

path_dialog(default_path, function(output)
  if not output or output == "" then
    return
  end

  local path = output
  if not path:lower():match("%.csv$") then
    path = path .. ".csv"
  end
  path = unique_export_path(path)

  local exported, export_error = ReaADR.export_cues_to_csv(cues, path)
  if not exported then
    ReaADR.message("Cue sheet export failed:\n\n" .. tostring(export_error))
    return
  end

  ReaADR.message(
    ("Exported %d cue(s) from %s.\n\n%s"):format(
      #cues,
      source,
      path
    )
  )
end)
