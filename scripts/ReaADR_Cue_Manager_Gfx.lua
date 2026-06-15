-- Cue list manager for ReaADR sessions.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local function manager_source_cues()
  local loaded = ReaADR.load_last_import_cues()
  if loaded then
    return loaded, "cached ReaADR session"
  end
  local navigated, nav_source = ReaADR.navigation_cues()
  return navigated or {}, nav_source or "project"
end

local all_cues, source = manager_source_cues()
if not all_cues or #all_cues == 0 then
  ReaADR.message("No ADR cues were found. Import a script or generate cues first.")
  return
end
local cues = ReaADR.filter_cues_by_active_characters(all_cues)

local state = {
  width = 980,
  height = 700,
  min_width = 520,
  min_height = 560,
  selected = 1,
  scroll = 0,
  last_mouse = 0,
  last_click_time = 0,
  last_click_index = nil,
  dropdown_field = nil,
  session_revision = ReaADR.session_revision and ReaADR.session_revision() or 0,
  filter_signature = select(2, ReaADR.active_character_filter()),
  closed = false,
  editing = nil,
  dropdown_rect = nil,
  last_poll = 0,
  dragging_scrollbar = false,
  scrollbar_drag_offset = 0,
}

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

local function sync_window_size()
  state.width = math.max(state.min_width, gfx.w or state.width)
  state.height = math.max(state.min_height, gfx.h or state.height)
end

local function cue_index_by_key(key)
  if not key or key == "" then
    return nil
  end
  for index, cue in ipairs(cues or {}) do
    if ReaADR.cue_key(cue) == key then
      return index
    end
  end
  return nil
end

local function cue_index_at_position(position)
  local cue = ReaADR.find_cue_at_position(cues, position)
  if cue then
    return cue_index_by_key(ReaADR.cue_key(cue))
  end
  return nil
end

local function sync_scroll_to_selection(visible_rows)
  visible_rows = visible_rows or 14
  if state.selected <= state.scroll then
    state.scroll = math.max(0, state.selected - 1)
  elseif state.selected > state.scroll + visible_rows then
    state.scroll = math.max(0, state.selected - visible_rows)
  end
end

local selected_key = ReaADR.selected_region_cue_key()
if selected_key == "" then
  selected_key = ReaADR.manager_selected_cue_key()
end
state.selected = cue_index_by_key(selected_key) or cue_index_at_position(ReaADR.current_timeline_position()) or 1

local function inside(rect, x, y)
  return x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
end

local function button(rect)
  local theme = ReaADR.ui_theme()
  local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
  ReaADR.set_gfx_color(hover and (rect.hover_accent or theme.accent_blue) or (rect.accent or theme.panel_alt))
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 14)
  ReaADR.set_gfx_color(theme.text)
  gfx.x = rect.x + 10
  gfx.y = rect.y + 8
  gfx.drawstr(rect.label)
  return hover
end

local function refresh_cues()
  local selected = cues[state.selected]
  local selected_key = selected and ReaADR.cue_key(selected) or ReaADR.manager_selected_cue_key()
  all_cues, source = manager_source_cues()
  all_cues = all_cues or {}
  cues = ReaADR.filter_cues_by_active_characters(all_cues)
  local restored = cue_index_by_key(selected_key)
  if restored then
    state.selected = restored
  end
  if state.selected > #cues then
    state.selected = math.max(1, #cues)
  end
end

local function maybe_refresh_external_changes()
  local now = reaper.time_precise()
  local revision = ReaADR.session_revision and ReaADR.session_revision() or 0
  local _, filter_signature = ReaADR.active_character_filter()
  filter_signature = tostring(filter_signature or "")
  if revision ~= state.session_revision or filter_signature ~= tostring(state.filter_signature or "") then
    state.session_revision = revision
    state.filter_signature = filter_signature
    refresh_cues()
    state.last_poll = now
  elseif not state.editing and (now - (state.last_poll or 0)) >= 0.25 then
    refresh_cues()
    state.last_poll = now
  end
end

local function select_cue(index, jump)
  if not cues[index] then
    return
  end
  state.selected = index
  local cue = cues[state.selected]
  ReaADR.set_manager_selected_cue(cue)
  if jump then
    ReaADR.jump_to_cue(cue)
  end
  ReaADR.refresh_overlay_silent()
end

local function set_status_for_selected(status)
  local cue = cues[state.selected]
  if not cue then
    return
  end
  if not status then
    return
  end
  ReaADR.set_cue_status_at_position(status, cue.start_time)
  state.dropdown_field = nil
  state.dropdown_rect = nil
  state.editing = nil
  refresh_cues()
end

local function set_cue_type_for_selected(cue_type)
  local cue = cues[state.selected]
  if not cue or not cue_type or cue_type == "" then
    return
  end
  local updated = {}
  for key, value in pairs(cue) do
    updated[key] = value
  end
  updated.cue_type = cue_type
  local saved, err = ReaADR.update_cached_cue(updated)
  if not saved then
    ReaADR.message("Cue type update failed:\n\n" .. tostring(err))
    return
  end
  state.dropdown_field = nil
  state.dropdown_rect = nil
  state.editing = nil
  refresh_cues()
end

local function launch_character_filter()
  local path = script_dir() .. "/ReaADR_Character_Filter.lua"
  local command_id = reaper.AddRemoveReaScript(true, 0, path, true)
  if command_id and command_id > 0 then
    reaper.Main_OnCommand(command_id, 0)
  else
    ReaADR.message("Could not open Character Filter.")
  end
end

local function prompt_jump_to_cue()
  local current = cues[state.selected]
  local ok, value = reaper.GetUserInputs("Jump To Cue", 1, "Cue number:", tostring(current and current.id or ""))
  if not ok then
    return
  end
  value = tostring(value or ""):match("^%s*(.-)%s*$")
  for index, cue in ipairs(cues or {}) do
    if tostring(cue.id or "") == value then
      select_cue(index, true)
      return
    end
  end
  ReaADR.message("Cue not found: " .. value)
end

local function add_cue_from_manager()
  local position = ReaADR.current_timeline_position()
  local next_id = tostring(#cues + 1)
  local defaults = table.concat({
    next_id,
    "ADR",
    ("%.3f"):format(position),
    ("%.3f"):format(position + 2.0),
    "",
    "Dialogue",
  }, ",")
  local ok, values = reaper.GetUserInputs("Add ADR Cue", 6, "Cue Number,Character,Start seconds,End seconds,Dialogue,Cue Type", defaults)
  if not ok then
    return
  end

  local parts = {}
  for value in (values .. ","):gmatch("([^,]*),") do
    parts[#parts + 1] = value
  end
  local cue, all_cues = ReaADR.add_cached_cue({
    id = parts[1],
    character = parts[2],
    start_time = tonumber(parts[3]),
    end_time = tonumber(parts[4]),
    line = parts[5] or "",
    cue_type = parts[6] or "Dialogue",
  })
  if not cue then
    ReaADR.message("Cue could not be added.")
    return
  end

  local summary, err = ReaADR.rebuild_cached_session({})
  if not summary then
    ReaADR.message("Cue was added to the session cache, but project rebuild failed:\n\n" .. tostring(err))
  end
  refresh_cues()
  local index = cue_index_by_key(ReaADR.cue_key(cue))
  if index then
    select_cue(index, true)
  end
end

local function launch_record_cue()
  local cue = cues[state.selected]
  if cue then
    ReaADR.set_manager_selected_cue(cue)
  end
  local path = script_dir() .. "/ReaADR_Record_Cue.lua"
  local command_id = reaper.AddRemoveReaScript(true, 0, path, true)
  if command_id and command_id > 0 then
    reaper.Main_OnCommand(command_id, 0)
  else
    ReaADR.message("Could not open Record Current Cue.")
  end
end

local function cell_columns(content_w)
  local trailing_w = math.max(280, content_w - 620)
  local line_w = math.max(130, math.floor(trailing_w * 0.56))
  local notes_w = math.max(100, trailing_w - line_w)
  return {
    { key = "id", x = 30, w = 46 },
    { key = "character", x = 84, w = 108 },
    { key = "start_time", x = 194, w = 108 },
    { key = "end_time", x = 312, w = 108 },
    { key = "status", x = 430, w = 108 },
    { key = "cue_type", x = 548, w = 82 },
    { key = "line", x = 640, w = line_w },
    { key = "notes", x = 640 + line_w + 10, w = notes_w - 10 },
  }
end

local function display_value_for_field(cue, field_key, frame_rate)
  if field_key == "id" then
    return tostring(cue.id or "")
  elseif field_key == "character" then
    return tostring(cue.character or "")
  elseif field_key == "start_time" then
    return ReaADR.format_timecode(cue.start_time, frame_rate)
  elseif field_key == "end_time" then
    return ReaADR.format_timecode(cue.end_time, frame_rate)
  elseif field_key == "status" then
    return tostring(cue.status or "Not Recorded")
  elseif field_key == "cue_type" then
    return tostring(cue.cue_type or "Dialogue")
  elseif field_key == "line" then
    return tostring(cue.line or "")
  elseif field_key == "notes" then
    return tostring(cue.notes or "")
  end
  return ""
end

local function trim_cell_text(value)
  return tostring(value or ""):gsub("[\r\n]", " ")
end

local function trim_to_width(value, max_w, font_size)
  local text = tostring(value or "")
  gfx.setfont(1, "Arial", font_size or 13)
  if gfx.measurestr(text) <= max_w then
    return text
  end
  local trimmed = text
  while #trimmed > 1 and gfx.measurestr(trimmed .. "...") > max_w do
    trimmed = trimmed:sub(1, #trimmed - 1)
  end
  return trimmed .. "..."
end

local function begin_inline_edit(cue_index, field_key, rect, frame_rate)
  local cue = cues[cue_index]
  if not cue or not field_key then
    return
  end
  state.editing = {
    cue_index = cue_index,
    cue_key = ReaADR.cue_key(cue),
    field_key = field_key,
    rect = {
      x = rect.x,
      y = rect.y,
      w = rect.w,
      h = rect.h,
    },
    value = trim_cell_text(display_value_for_field(cue, field_key, frame_rate)),
  }
  state.editing.cursor = #(state.editing.value or "")
  state.dropdown_field = nil
end

local function set_inline_cursor_from_mouse()
  local editing = state.editing
  if not editing then
    return
  end
  local value = tostring(editing.value or "")
  local local_x = gfx.mouse_x - editing.rect.x - 6
  if local_x <= 0 then
    editing.cursor = 0
    return
  end
  gfx.setfont(1, "Arial", 13)
  local best = #value
  for i = 0, #value do
    if gfx.measurestr(value:sub(1, i)) >= local_x then
      best = i
      break
    end
  end
  editing.cursor = best
end

local function cancel_inline_edit()
  state.editing = nil
end

local function commit_inline_edit()
  local editing = state.editing
  if not editing then
    return true
  end

  local cue = cues[editing.cue_index]
  if not cue or ReaADR.cue_key(cue) ~= editing.cue_key then
    refresh_cues()
    state.editing = nil
    return true
  end

  local updated = {}
  for key, value in pairs(cue) do
    updated[key] = value
  end
  updated._original_key = editing.cue_key

  if editing.field_key == "id" then
    updated.id = editing.value
  elseif editing.field_key == "character" then
    updated.character = editing.value
  elseif editing.field_key == "start_time" then
    local frame_rate = reaper.TimeMap_curFrameRate(0)
    if not frame_rate or frame_rate <= 0 then
      frame_rate = 24
    end
    local parsed_start, start_error = ReaADR.parse_timecode(editing.value, frame_rate)
    if not parsed_start then
      ReaADR.message("Cue update failed:\n\nStart time is invalid: " .. tostring(start_error))
      return false
    end
    updated.start_time = parsed_start
  elseif editing.field_key == "end_time" then
    local frame_rate = reaper.TimeMap_curFrameRate(0)
    if not frame_rate or frame_rate <= 0 then
      frame_rate = 24
    end
    local parsed_end, end_error = ReaADR.parse_timecode(editing.value, frame_rate)
    if not parsed_end then
      ReaADR.message("Cue update failed:\n\nEnd time is invalid: " .. tostring(end_error))
      return false
    end
    updated.end_time = parsed_end
  elseif editing.field_key == "line" then
    updated.line = editing.value
  elseif editing.field_key == "notes" then
    updated.notes = editing.value
  elseif editing.field_key == "cue_type" then
    updated.cue_type = editing.value
  end

  local saved, err = ReaADR.update_cached_cue(updated)
  if not saved then
    ReaADR.message("Cue update failed:\n\n" .. tostring(err))
    return false
  end

  state.editing = nil
  refresh_cues()
  local saved_index = cue_index_by_key(ReaADR.cue_key(saved))
  if saved_index then
    state.selected = saved_index
    sync_scroll_to_selection()
  end
  return true
end

local function handle_inline_input(char)
  local editing = state.editing
  if not editing then
    return false
  end

  local value = tostring(editing.value or "")
  local cursor = math.max(0, math.min(tonumber(editing.cursor) or #value, #value))
  editing.cursor = cursor

  if char == 8 then
    if cursor > 0 then
      editing.value = value:sub(1, cursor - 1) .. value:sub(cursor + 1)
      editing.cursor = cursor - 1
    end
    return true
  elseif char == KEY_DELETE then
    if cursor < #value then
      editing.value = value:sub(1, cursor) .. value:sub(cursor + 2)
    end
    return true
  elseif char == KEY_LEFT then
    editing.cursor = math.max(0, cursor - 1)
    return true
  elseif char == KEY_RIGHT then
    editing.cursor = math.min(#value, cursor + 1)
    return true
  elseif char == KEY_HOME or char == 1 then
    editing.cursor = 0
    return true
  elseif char == KEY_END or char == 5 then
    editing.cursor = #value
    return true
  elseif char == 13 then
    commit_inline_edit()
    return true
  elseif char >= 32 and char < 127 then
    editing.value = value:sub(1, cursor) .. string.char(char) .. value:sub(cursor + 1)
    editing.cursor = cursor + 1
    return true
  end

  return false
end

local function draw_inline_editor()
  local editing = state.editing
  if not editing then
    return
  end
  local rect = editing.rect
  gfx.set(0.05, 0.06, 0.07, 1)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  gfx.set(0.95, 0.78, 0.30, 1)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 13)
  gfx.set(1, 1, 1, 1)

  local value = tostring(editing.value or "")
  local cursor = math.max(0, math.min(tonumber(editing.cursor) or #value, #value))
  local max_w = rect.w - 12
  local visible_start = 1
  local display = value:sub(visible_start)
  while gfx.measurestr(display) > max_w and visible_start <= cursor do
    visible_start = visible_start + 1
    display = value:sub(visible_start)
  end
  while gfx.measurestr(display) > max_w and #display > 0 do
    display = display:sub(1, #display - 1)
  end

  gfx.x = rect.x + 6
  gfx.y = rect.y + 7
  gfx.drawstr(display)

  local prefix = cursor >= visible_start and value:sub(visible_start, cursor) or ""
  local cursor_x = math.min(rect.x + 6 + gfx.measurestr(prefix), rect.x + rect.w - 6)
  gfx.line(cursor_x, rect.y + 5, cursor_x, rect.y + rect.h - 5)
end

local function remove_selected_cue()
  local cue = cues[state.selected]
  if not cue then
    return
  end

  local answer = reaper.ShowMessageBox(
    ("Remove cue %s (%s)?\n\nRemaining cues will be renumbered and cue regions/audio will be rebuilt."):format(
      tostring(cue.id or ""),
      tostring(cue.character or "")
    ),
    "ReaADR",
    4
  )
  if answer ~= 6 then
    return
  end

  local removed, err = ReaADR.remove_cached_cue(cue, { select_index = state.selected, renumber = true })
  if not removed then
    ReaADR.message("Cue remove failed:\n\n" .. tostring(err))
    return
  end

  local summary, rebuild_err = ReaADR.rebuild_cached_session({
    clear_generated_items = true,
    clear_generated_regions = true,
  })
  if not summary then
    ReaADR.message("Cue was removed from the session cache, but project rebuild failed:\n\n" .. tostring(rebuild_err))
  end

  refresh_cues()
  state.selected = math.min(state.selected, math.max(1, #cues))
  sync_scroll_to_selection()
  local selected = cues[state.selected]
  if selected then
    ReaADR.set_manager_selected_cue(selected)
  else
    ReaADR.set_manager_selected_cue(nil)
  end
end

local function refresh_session_from_manager()
  local selected = cues[state.selected]
  local selected_key = selected and ReaADR.cue_key(selected) or ""
  local summary, refresh_err = ReaADR.refresh_session()
  if not summary then
    ReaADR.message("Refresh failed:\n\n" .. tostring(refresh_err))
    return
  end

  refresh_cues()
  local index = cue_index_by_key(selected_key)
  if index then
    state.selected = index
  end
  sync_scroll_to_selection()
  ReaADR.refresh_overlay_silent()
  ReaADR.message(
    ("Session refreshed.\n\nRegions synced: %d\nCue audio created: %d, updated: %d, skipped: %d\nOverlap splits: %d\nCompleted characters skipped: %d"):format(
      summary.sync.updated or 0,
      summary.rebuild.cue_audio_created or 0,
      summary.rebuild.cue_audio_updated or 0,
      summary.rebuild.cue_audio_skipped or 0,
      summary.rebuild.overlap_conflicts or 0,
      summary.rebuild.skipped_completed_characters or 0
    )
  )
end

local function launch_info_panel()
  local cue = cues[state.selected]
  if not cue then
    return
  end
  ReaADR.set_manager_selected_cue(cue)
  local path = script_dir() .. "/ReaADR_Cue_Info_Panel.lua"
  local command_id = reaper.AddRemoveReaScript(true, 0, path, true)
  if command_id and command_id > 0 then
    reaper.Main_OnCommand(command_id, 0)
  else
    ReaADR.message("Could not open Cue Information Panel.")
  end
end

local function draw_text_input(rect, value, label)
  local theme = ReaADR.ui_theme()
  gfx.setfont(1, "Arial", 12)
  ReaADR.set_gfx_color(theme.muted)
  gfx.x = rect.x
  gfx.y = rect.y - 16
  gfx.drawstr(label)
  ReaADR.set_gfx_color(theme.panel_alt)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 13)
  ReaADR.set_gfx_color(theme.text)
  gfx.x = rect.x + 6
  gfx.y = rect.y + 7
  local display = tostring(value or "")
  if #display > math.floor(rect.w / 7) then
    display = display:sub(1, math.max(1, math.floor(rect.w / 7) - 3)) .. "..."
  end
  gfx.drawstr(display)
end

local function draw_dropdown(anchor, selected_value, field_key)
  local theme = ReaADR.ui_theme()
  local options_source = field_key == "cue_type" and ReaADR.cue_types() or ReaADR.cue_statuses()
  local option_h = 24
  local menu_h = #options_source * option_h
  local y = anchor.y - menu_h - 4
  if y < 78 then
    y = anchor.y + anchor.h + 4
  end
  local options = {}
  ReaADR.set_gfx_color(theme.panel)
  gfx.rect(anchor.x, y, anchor.w, menu_h, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(anchor.x, y, anchor.w, menu_h, false)
  for index, value in ipairs(options_source) do
    local rect = { x = anchor.x, y = y + ((index - 1) * option_h), w = anchor.w, h = option_h, value = value }
    options[#options + 1] = rect
    local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
    local active = tostring(value) == tostring(selected_value)
    ReaADR.set_gfx_color(active and theme.accent_gold or (hover and theme.highlight or theme.panel_alt))
    gfx.rect(rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, true)
    gfx.setfont(1, "Arial", 13)
    ReaADR.set_gfx_color(theme.text)
    gfx.x = rect.x + 8
    gfx.y = rect.y + 5
    gfx.drawstr(value)
  end
  return options
end

local button_specs = {
  { key = "jump", label = "Jump...", min_w = 92, hint = "Type a cue number and jump to its region start." },
  { key = "prev", label = "Previous", min_w = 92, hint = "Select the previous cue in the list." },
  { key = "next", label = "Next", min_w = 92, hint = "Select the next cue in the list." },
  { key = "record", label = "Record Current Cue", min_w = 148, hint = "Arm the current cue and start the dedicated record workflow.", accent = { 0.42, 0.20, 0.16, 1.0 }, hover_accent = { 0.55, 0.24, 0.19, 1.0 } },
  { key = "add", label = "Add Cue", min_w = 92, hint = "Create a cue at the current timeline position." },
  { key = "remove", label = "Remove Cue", min_w = 104, hint = "Delete the selected cue, renumber the remaining cues, and rebuild cue regions/audio." },
  { key = "filter", label = "Character Filter", min_w = 132, hint = "Enable or disable character tracks for focused recording passes." },
  { key = "sync", label = "Refresh Session", min_w = 134, hint = "Rebuild cue tracks, reapply filters, resync regions, and refresh overlay/session state." },
  { key = "overlay", label = "Refresh Overlay", min_w = 142, hint = "Refresh the video overlay for the selected/current cue state." },
  { key = "info", label = "Info Panel", min_w = 112, hint = "Open the large cue information panel for the selected cue." },
}

local function layout_buttons(content_w)
  local gap = 8
  local button_h = 32
  local rows = {
    { "jump", "prev", "next", "record", "add", "remove" },
    { "filter", "sync", "overlay", "info" },
  }
  local by_key = {}
  for _, spec in ipairs(button_specs) do
    by_key[spec.key] = spec
  end

  local toolbar_h = (#rows * button_h) + ((#rows - 1) * gap)
  local toolbar_y = state.height - 52 - toolbar_h
  local buttons = {}

  for row_index, row_keys in ipairs(rows) do
    local row = {}
    for _, key in ipairs(row_keys) do
      if by_key[key] then
        row[#row + 1] = by_key[key]
      end
    end
    local min_total = 0
    for _, spec in ipairs(row) do
      min_total = min_total + spec.min_w
    end
    local extra = math.max(0, content_w - min_total - ((#row - 1) * gap))
    local grow = math.floor(extra / #row)
    local remainder = extra - (grow * #row)
    local x = 22
    local y = toolbar_y + ((row_index - 1) * (button_h + gap))
    for index, spec in ipairs(row) do
      local w = spec.min_w + grow + (index <= remainder and 1 or 0)
      buttons[spec.key] = {
        x = x,
        y = y,
        w = w,
        h = button_h,
        label = spec.label,
        hint = spec.hint,
        accent = spec.accent,
        hover_accent = spec.hover_accent,
      }
      x = x + w + gap
    end
  end

  return buttons, toolbar_y, toolbar_h
end

local function frame()
  sync_window_size()
  maybe_refresh_external_changes()
  local hover_hint = ""
  local hover_preview = nil
  local hover_preview_enabled = ReaADR.cue_hover_preview_enabled()
  local theme = ReaADR.ui_theme()
  ReaADR.set_gfx_color(theme.bg)
  gfx.rect(0, 0, state.width, state.height, true)

  local frame_rate = reaper.TimeMap_curFrameRate(0)
  if not frame_rate or frame_rate <= 0 then
    frame_rate = 24
  end

  local header = ReaADR.draw_window_header(
    "ReaADR Cue Manager",
    ("Source: %s | Cues: %d | Double-click to edit; status and cue type use dropdowns."):format(tostring(source or "session"), #cues),
    { x = 22, y = 18, width = state.width - 44, height = 76 }
  )

  local content_w = state.width - 44
  local buttons, toolbar_y = layout_buttons(content_w)
  local columns = cell_columns(content_w)
  local selected_y = toolbar_y - 38

  local header_y = math.max(108, header.content_y)
  ReaADR.set_gfx_color(theme.panel_alt)
  gfx.rect(22, header_y, content_w, 28, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(22, header_y, content_w, 28, false)
  gfx.setfont(1, "Arial", 13)
  ReaADR.set_gfx_color(theme.text)
  gfx.x = 30
  gfx.y = header_y + 7
  gfx.drawstr("Cue")
  gfx.x = 84
  gfx.drawstr("Character")
  gfx.x = 194
  gfx.drawstr("Start SMPTE")
  gfx.x = 312
  gfx.drawstr("End SMPTE")
  gfx.x = 430
  gfx.drawstr("Status")
  gfx.x = 548
  gfx.drawstr("Type")
  gfx.x = 640
  gfx.drawstr("Line")
  gfx.x = columns[8].x
  gfx.drawstr("Notes")

  local row_h = 30
  local list_y = header_y + 30
  local visible_rows = math.max(1, math.floor((selected_y - list_y - 8) / row_h))
  local max_scroll = math.max(0, #cues - visible_rows)
  state.scroll = math.max(0, math.min(state.scroll, max_scroll))
  local scrollbar_rect = nil
  local scrollbar_thumb = nil
  local scrollbar_track_x = state.width - 30
  local scrollbar_track_y = list_y
  local scrollbar_track_h = math.max(24, (visible_rows * row_h) - 2)
  local scrollbar_track_w = 12

  if #cues > visible_rows then
    local visible_ratio = math.max(0.08, visible_rows / math.max(1, #cues))
    local thumb_h = math.max(32, math.floor(scrollbar_track_h * visible_ratio))
    local travel = math.max(0, scrollbar_track_h - thumb_h)
    local scroll_ratio = max_scroll > 0 and (state.scroll / max_scroll) or 0
    local thumb_y = scrollbar_track_y + math.floor(travel * scroll_ratio)
    scrollbar_rect = { x = scrollbar_track_x, y = scrollbar_track_y, w = scrollbar_track_w, h = scrollbar_track_h }
    scrollbar_thumb = { x = scrollbar_track_x + 1, y = thumb_y, w = scrollbar_track_w - 2, h = thumb_h }
  end

  for row = 1, visible_rows do
    local cue_index = state.scroll + row
    local cue = cues[cue_index]
    if cue then
      local y = list_y + ((row - 1) * row_h)
      local selected = cue_index == state.selected
      ReaADR.set_gfx_color(selected and theme.highlight or theme.panel)
      gfx.rect(22, y, content_w, row_h - 2, true)
      ReaADR.set_gfx_color(theme.border)
      gfx.rect(22, y, content_w, row_h - 2, false)
      gfx.setfont(1, "Arial", 13)
      ReaADR.set_gfx_color(theme.text)
      gfx.x = 30
      gfx.y = y + 7
      gfx.drawstr(tostring(cue.id or ""))
      gfx.x = 84
      gfx.drawstr(tostring(cue.character or ""))
      gfx.x = 194
      gfx.drawstr(ReaADR.format_timecode(cue.start_time, frame_rate))
      gfx.x = 312
      gfx.drawstr(ReaADR.format_timecode(cue.end_time, frame_rate))
      gfx.x = 430
      gfx.drawstr(tostring(cue.status or "Not Recorded"))
      gfx.x = 548
      gfx.drawstr(tostring(cue.cue_type or "Dialogue"))
      gfx.x = 640
      local line = tostring(cue.line or "")
      local line_chars = math.max(14, math.floor((columns[7].w - 8) / 7))
      if #line > line_chars then
        line = line:sub(1, math.max(1, line_chars - 3)) .. "..."
      end
      gfx.drawstr(line)
      gfx.x = columns[8].x
      local notes = tostring(cue.notes or "")
      local notes_chars = math.max(12, math.floor((columns[8].w - 8) / 7))
      if #notes > notes_chars then
        notes = notes:sub(1, math.max(1, notes_chars - 3)) .. "..."
      end
      gfx.drawstr(notes)

      local row_rect = { x = 22, y = y, w = content_w, h = row_h - 2 }
      if inside(row_rect, gfx.mouse_x, gfx.mouse_y) then
        for _, column in ipairs(columns) do
          local cell_rect = { x = column.x, y = row_rect.y, w = column.w, h = row_rect.h }
          if inside(cell_rect, gfx.mouse_x, gfx.mouse_y) then
            local full_value = trim_cell_text(display_value_for_field(cue, column.key, frame_rate))
            local limit = math.max(12, math.floor((column.w - 8) / 7))
            if #full_value > limit then
              if hover_preview_enabled and (column.key == "line" or column.key == "notes") then
                hover_preview = {
                  text = full_value,
                  x = gfx.mouse_x + 18,
                  y = gfx.mouse_y + 18,
                  w = math.min(560, math.max(260, column.w + 120)),
                }
              else
                hover_hint = full_value
              end
            end
            break
          end
        end
      end
    end
  end

  if scrollbar_rect and scrollbar_thumb then
    ReaADR.set_gfx_color(theme.panel_alt)
    gfx.rect(scrollbar_rect.x, scrollbar_rect.y, scrollbar_rect.w, scrollbar_rect.h, true)
    ReaADR.set_gfx_color(theme.border)
    gfx.rect(scrollbar_rect.x, scrollbar_rect.y, scrollbar_rect.w, scrollbar_rect.h, false)
    local thumb_hover = inside(scrollbar_thumb, gfx.mouse_x, gfx.mouse_y)
    ReaADR.set_gfx_color((thumb_hover or state.dragging_scrollbar) and theme.accent_gold or theme.highlight)
    gfx.rect(scrollbar_thumb.x, scrollbar_thumb.y, scrollbar_thumb.w, scrollbar_thumb.h, true)
  end

  draw_inline_editor()

  for _, rect in pairs(buttons) do
    if button(rect) and rect.hint then
      hover_hint = rect.hint
    end
  end

  local selected_cue = cues[state.selected]
  local dropdown_options = {}
  if selected_cue then
    gfx.setfont(1, "Arial", 14)
    ReaADR.set_gfx_color(theme.muted)
    gfx.x = 22
    gfx.y = selected_y
    gfx.drawstr(trim_to_width(
      ("Selected Cue %s | %s | %s | %s | %.2fs | Double-click status/type cells to edit"):format(
        tostring(selected_cue.id or ""),
        tostring(selected_cue.character or ""),
        tostring(selected_cue.status or "Not Recorded"),
        tostring(selected_cue.cue_type or "Dialogue"),
        ReaADR.cue_duration(selected_cue)
      ),
      math.max(240, content_w - 16),
      14
    ))
  end

  if selected_cue and state.dropdown_field and state.dropdown_rect then
    local current_value = state.dropdown_field == "cue_type" and (selected_cue.cue_type or "Dialogue") or (selected_cue.status or "Not Recorded")
    dropdown_options = draw_dropdown(state.dropdown_rect, current_value, state.dropdown_field)
  end

  if hover_hint ~= "" then
    ReaADR.set_gfx_color(theme.panel)
    gfx.rect(22, state.height - 40, state.width - 44, 24, true)
    ReaADR.set_gfx_color(theme.border)
    gfx.rect(22, state.height - 40, state.width - 44, 24, false)
    gfx.setfont(1, "Arial", 13)
    ReaADR.set_gfx_color(theme.text)
    gfx.x = 30
    gfx.y = state.height - 34
    gfx.drawstr(hover_hint)
  end

  if hover_preview and hover_preview.text ~= "" then
    local max_preview_w = math.min(620, state.width - 32)
    local preview_w = math.min(max_preview_w, math.max(260, hover_preview.w))
    local preview_x = math.max(16, math.min(hover_preview.x, state.width - preview_w - 16))
    local wrapped = {}
    local current = ""
    gfx.setfont(1, "Arial", 13)
    local max_line_w = preview_w - 20
    for raw_word in hover_preview.text:gmatch("%S+") do
      local word = raw_word
      if gfx.measurestr(word) > max_line_w then
        if current ~= "" then
          wrapped[#wrapped + 1] = current
          current = ""
        end
        local segment = ""
        for i = 1, #word do
          local char = word:sub(i, i)
          local candidate = segment .. char
          if gfx.measurestr(candidate) <= max_line_w then
            segment = candidate
          else
            if segment ~= "" then
              wrapped[#wrapped + 1] = segment
            end
            segment = char
          end
        end
        if segment ~= "" then
          current = segment
        end
      else
        local candidate = current == "" and word or (current .. " " .. word)
        if gfx.measurestr(candidate) <= max_line_w then
          current = candidate
        else
          if current ~= "" then
            wrapped[#wrapped + 1] = current
          end
          current = word
        end
      end
    end
    if current ~= "" then
      wrapped[#wrapped + 1] = current
    end
    local longest = 0
    for _, line in ipairs(wrapped) do
      longest = math.max(longest, gfx.measurestr(line))
    end
    preview_w = math.min(max_preview_w, math.max(preview_w, math.ceil(longest) + 20))
    preview_x = math.max(16, math.min(hover_preview.x, state.width - preview_w - 16))
    local preview_h = math.min(state.height - 32, math.max(28, (#wrapped * 16) + 12))
    local preview_y = math.max(16, math.min(hover_preview.y, state.height - preview_h - 16))
    ReaADR.set_gfx_color(theme.panel)
    gfx.rect(preview_x, preview_y, preview_w, preview_h, true)
    ReaADR.set_gfx_color(theme.border)
    gfx.rect(preview_x, preview_y, preview_w, preview_h, false)
    ReaADR.set_gfx_color(theme.text)
    for index, line in ipairs(wrapped) do
      gfx.x = preview_x + 10
      gfx.y = preview_y + 6 + ((index - 1) * 16)
      gfx.drawstr(line)
    end
  end

  gfx.update()

  local char = gfx.getchar()
  if char < 0 or char == 27 then
    if state.editing then
      cancel_inline_edit()
      reaper.defer(frame)
      return
    end
    ReaADR.save_window_state("cue_manager")
    gfx.quit()
    return
  elseif state.editing and handle_inline_input(char) then
    reaper.defer(frame)
    return
  elseif char == 30064 then
    if state.editing and not commit_inline_edit() then
      reaper.defer(frame)
      return
    end
    select_cue(math.max(1, state.selected - 1), true)
    sync_scroll_to_selection(visible_rows)
  elseif char == 1685026670 then
    if state.editing and not commit_inline_edit() then
      reaper.defer(frame)
      return
    end
    select_cue(math.min(#cues, state.selected + 1), true)
    sync_scroll_to_selection(visible_rows)
  end

  local wheel = gfx.mouse_wheel
  if wheel ~= 0 then
    state.scroll = math.max(0, math.min(max_scroll, state.scroll - (wheel > 0 and 1 or -1)))
    gfx.mouse_wheel = 0
  end

  local mouse = gfx.mouse_cap % 2
  if state.dragging_scrollbar and mouse == 1 and scrollbar_rect and scrollbar_thumb then
    local travel = math.max(0, scrollbar_rect.h - scrollbar_thumb.h)
    if travel > 0 then
      local thumb_y = math.max(scrollbar_rect.y, math.min(gfx.mouse_y - state.scrollbar_drag_offset, scrollbar_rect.y + travel))
      local ratio = (thumb_y - scrollbar_rect.y) / travel
      state.scroll = math.max(0, math.min(max_scroll, math.floor((ratio * max_scroll) + 0.5)))
    end
  elseif state.dragging_scrollbar and mouse == 0 then
    state.dragging_scrollbar = false
  end

  if mouse == 1 and state.last_mouse == 0 then
    if state.dropdown_field then
      local clicked_dropdown = false
      for _, option in ipairs(dropdown_options or {}) do
        if inside(option, gfx.mouse_x, gfx.mouse_y) then
          if state.dropdown_field == "cue_type" then
            set_cue_type_for_selected(option.value)
          else
            set_status_for_selected(option.value)
          end
          clicked_dropdown = true
          break
        end
      end
      local clicked_anchor = state.dropdown_rect and inside(state.dropdown_rect, gfx.mouse_x, gfx.mouse_y)
      if clicked_dropdown then
        state.last_mouse = mouse
        reaper.defer(frame)
        return
      elseif not clicked_anchor then
        state.dropdown_field = nil
        state.dropdown_rect = nil
      end
    end

    if state.editing then
      if inside(state.editing.rect, gfx.mouse_x, gfx.mouse_y) then
        set_inline_cursor_from_mouse()
        state.last_mouse = mouse
        reaper.defer(frame)
        return
      elseif not commit_inline_edit() then
        state.last_mouse = mouse
        reaper.defer(frame)
        return
      else
        state.last_mouse = mouse
        reaper.defer(frame)
        return
      end
    end

    local clicked_dropdown = false
    if scrollbar_thumb and inside(scrollbar_thumb, gfx.mouse_x, gfx.mouse_y) then
      state.dragging_scrollbar = true
      state.scrollbar_drag_offset = gfx.mouse_y - scrollbar_thumb.y
      state.last_mouse = mouse
      reaper.defer(frame)
      return
    elseif scrollbar_rect and inside(scrollbar_rect, gfx.mouse_x, gfx.mouse_y) then
      local travel = math.max(0, scrollbar_rect.h - scrollbar_thumb.h)
      if travel > 0 then
        local thumb_y = math.max(scrollbar_rect.y, math.min(gfx.mouse_y - math.floor(scrollbar_thumb.h * 0.5), scrollbar_rect.y + travel))
        local ratio = (thumb_y - scrollbar_rect.y) / travel
        state.scroll = math.max(0, math.min(max_scroll, math.floor((ratio * max_scroll) + 0.5)))
      end
      state.last_mouse = mouse
      reaper.defer(frame)
      return
    end

    if not clicked_dropdown then
      for row = 1, visible_rows do
        local cue_index = state.scroll + row
        local rect = { x = 22, y = list_y + ((row - 1) * row_h), w = content_w, h = row_h - 2 }
        if cues[cue_index] and inside(rect, gfx.mouse_x, gfx.mouse_y) then
          local now = reaper.time_precise()
          local double_click = state.last_click_index == cue_index and (now - state.last_click_time) <= 0.35
          select_cue(cue_index, not double_click)
          state.last_click_index = cue_index
          state.last_click_time = now
          if double_click then
            for _, column in ipairs(columns) do
              local cell_rect = { x = column.x, y = rect.y, w = column.w, h = rect.h }
              if inside(cell_rect, gfx.mouse_x, gfx.mouse_y) then
                if column.key == "status" or column.key == "cue_type" then
                  state.dropdown_field = column.key
                  state.dropdown_rect = {
                    x = cell_rect.x,
                    y = cell_rect.y,
                    w = cell_rect.w,
                    h = cell_rect.h,
                  }
                else
                  begin_inline_edit(cue_index, column.key, cell_rect, frame_rate)
                end
                break
              end
            end
            break
          end
        end
      end
    end

    local cue = cues[state.selected]
    if clicked_dropdown then
      -- Dropdown selection already handled.
    elseif cue and inside(buttons.jump, gfx.mouse_x, gfx.mouse_y) then
      prompt_jump_to_cue()
    elseif inside(buttons.prev, gfx.mouse_x, gfx.mouse_y) then
      select_cue(math.max(1, state.selected - 1), true)
    elseif inside(buttons.next, gfx.mouse_x, gfx.mouse_y) then
      select_cue(math.min(#cues, state.selected + 1), true)
    elseif inside(buttons.add, gfx.mouse_x, gfx.mouse_y) then
      add_cue_from_manager()
    elseif cue and inside(buttons.remove, gfx.mouse_x, gfx.mouse_y) then
      remove_selected_cue()
    elseif inside(buttons.filter, gfx.mouse_x, gfx.mouse_y) then
      launch_character_filter()
    elseif cue and inside(buttons.record, gfx.mouse_x, gfx.mouse_y) then
      launch_record_cue()
    elseif inside(buttons.sync, gfx.mouse_x, gfx.mouse_y) then
      refresh_session_from_manager()
    elseif inside(buttons.overlay, gfx.mouse_x, gfx.mouse_y) then
      ReaADR.refresh_overlay_silent()
    elseif cue and inside(buttons.info, gfx.mouse_x, gfx.mouse_y) then
      launch_info_panel()
    end
  end
  state.last_mouse = mouse

  reaper.defer(frame)
end

local restored = ReaADR.init_persistent_window("cue_manager", "ReaADR Cue Manager", {
  width = state.width,
  height = state.height,
})
state.width = restored.width
state.height = restored.height
frame()
