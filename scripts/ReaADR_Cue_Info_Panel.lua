-- Live ADR cue information panel.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local state = {
  width = 1100,
  height = 740,
  min_width = 820,
  min_height = 560,
  last_mouse = 0,
  active_field = nil,
  edit_values = nil,
  edit_key = "",
  cursors = {},
  edit_original_key = "",
  dirty = false,
  last_click_field = nil,
  last_click_time = 0,
  field_rects = {},
  dock_button = nil,
  close_on_save = false,
}

local launch_options = ReaADR.consume_cue_info_launch_options and ReaADR.consume_cue_info_launch_options() or {}
state.close_on_save = launch_options.close_on_save == true

local edit_fields = {
  { key = "id", label = "Cue Number", row = 1, col = 1 },
  { key = "character", label = "Character", row = 1, col = 2 },
  { key = "status", label = "Status", row = 1, col = 3, dropdown = true },
  { key = "cue_type", label = "Cue Type", row = 1, col = 4, dropdown = true },
  { key = "start_time", label = "Start SMPTE or Seconds", row = 2, col = 1 },
  { key = "end_time", label = "End SMPTE or Seconds", row = 2, col = 2 },
  { key = "direction", label = "Direction", row = 2, col = 3 },
  { key = "line", label = "Dialogue", multiline = true },
  { key = "notes", label = "Notes", multiline = true },
}

local function sync_window_size()
  state.width = math.max(state.min_width, gfx.w or state.width)
  state.height = math.max(state.min_height, gfx.h or state.height)
end

local function trim_to_width(value, width)
  value = tostring(value or "")
  width = tonumber(width) or 0
  if width <= 0 or gfx.measurestr(value) <= width then
    return value
  end
  local ellipsis = "..."
  local limit = math.max(1, width - gfx.measurestr(ellipsis))
  while #value > 1 and gfx.measurestr(value) > limit do
    value = value:sub(1, #value - 1)
  end
  return value .. ellipsis
end

local function inside(rect, x, y)
  return x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
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

local function draw_button(rect)
  local theme = ReaADR.ui_theme()
  local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
  ReaADR.set_gfx_color(hover and theme.highlight or theme.panel_alt)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 14)
  ReaADR.set_gfx_color(theme.text)
  local label = tostring(rect.label or "")
  local tw = gfx.measurestr(label)
  gfx.x = rect.x + math.floor((rect.w - tw) / 2)
  gfx.y = rect.y + 8
  gfx.drawstr(label)
  return hover
end

local function dock_info_panel()
  if not gfx or not gfx.dock then
    return
  end
  local ok, dock = pcall(gfx.dock, -1, 0, 0, 0, 0)
  dock = ok and tonumber(dock) or 0
  if dock == 0 then
    gfx.dock(1)
  end
  ReaADR.save_window_state("cue_info")
end

local function draw_label(label, value, x, y, w)
  local theme = ReaADR.ui_theme()
  gfx.setfont(1, "Arial", 14)
  ReaADR.set_gfx_color(theme.muted)
  gfx.x = x
  gfx.y = y
  gfx.drawstr(trim_to_width(label, w))
  gfx.setfont(1, "Arial", 22)
  ReaADR.set_gfx_color(theme.text)
  gfx.x = x
  gfx.y = y + 18
  gfx.drawstr(trim_to_width(value, w))
end

local function draw_wrapped_text(text, x, y, width, line_height, max_lines)
  text = tostring(text or "")
  local line = ""
  local lines = 0
  for token in (text:gsub("\n", " \n ")):gmatch("%S+") do
    if token == "\n" then
      gfx.x = x
      gfx.y = y + (lines * line_height)
      gfx.drawstr(line)
      lines = lines + 1
      line = ""
    elseif gfx.measurestr((line == "" and token or (line .. " " .. token))) > width and line ~= "" then
      gfx.x = x
      gfx.y = y + (lines * line_height)
      gfx.drawstr(line)
      lines = lines + 1
      line = token
    else
      line = line == "" and token or (line .. " " .. token)
    end
    if lines >= max_lines then
      return
    end
  end
  if line ~= "" and lines < max_lines then
    gfx.x = x
    gfx.y = y + (lines * line_height)
    gfx.drawstr(line)
  end
end

local function cursor_for_field(key)
  local value = tostring((state.edit_values or {})[key] or "")
  state.cursors[key] = math.max(0, math.min(tonumber(state.cursors[key]) or #value, #value))
  return state.cursors[key]
end

local function set_cursor_for_field(key, cursor)
  local value = tostring((state.edit_values or {})[key] or "")
  state.cursors[key] = math.max(0, math.min(tonumber(cursor) or #value, #value))
end

local function draw_single_line_value(field, value, cursor, active)
  gfx.setfont(1, "Arial", 14)
  local max_w = field.w - 18
  local visible_start = 1
  local display = value:sub(visible_start)
  while gfx.measurestr(display) > max_w and visible_start <= cursor do
    visible_start = visible_start + 1
    display = value:sub(visible_start)
  end
  while gfx.measurestr(display) > max_w and #display > 0 do
    display = display:sub(1, #display - 1)
  end

  gfx.x = field.x + 8
  gfx.y = field.y + 8
  gfx.drawstr(display)

  if active then
    local prefix = cursor >= visible_start and value:sub(visible_start, cursor) or ""
    local cursor_x = math.min(field.x + 8 + gfx.measurestr(prefix), field.x + field.w - 8)
    gfx.line(cursor_x, field.y + 7, cursor_x, field.y + field.h - 7)
  end
end

local function cursor_xy_for_multiline(field, value, cursor)
  local x = field.x + 8
  local y = field.y + 8
  local max_x = field.x + field.w - 16
  local line_h = 20
  local current = ""
  for i = 1, cursor do
    local char = value:sub(i, i)
    if char == "\n" then
      x = field.x + 8
      y = y + line_h
      current = ""
    else
      local next_text = current .. char
      if gfx.measurestr(next_text) > max_x - field.x and current ~= "" then
        x = field.x + 8
        y = y + line_h
        current = char
      else
        current = next_text
      end
      x = field.x + 8 + gfx.measurestr(current)
    end
  end
  return math.min(x, field.x + field.w - 8), math.min(y, field.y + field.h - 10), line_h
end

local function set_cursor_from_mouse(field)
  local value = tostring((state.edit_values or {})[field.key] or "")
  gfx.setfont(1, "Arial", field.multiline and 16 or 14)
  if not field.multiline then
    local local_x = gfx.mouse_x - field.x - 8
    if local_x <= 0 then
      set_cursor_for_field(field.key, 0)
      return
    end
    local best = #value
    for i = 0, #value do
      if gfx.measurestr(value:sub(1, i)) >= local_x then
        best = i
        break
      end
    end
    set_cursor_for_field(field.key, best)
    return
  end

  local target_y = gfx.mouse_y
  local target_x = gfx.mouse_x
  local best = #value
  local best_distance = math.huge
  for i = 0, #value do
    local x, y = cursor_xy_for_multiline(field, value, i)
    local distance = math.abs(y - target_y) * 4 + math.abs(x - target_x)
    if distance < best_distance then
      best_distance = distance
      best = i
    end
  end
  set_cursor_for_field(field.key, best)
end

local function begin_edit(cue, active_field)
  if not cue then
    return
  end
  state.active_field = active_field or state.active_field
  state.edit_original_key = ReaADR.cue_key(cue)
  state.edit_key = state.edit_original_key
  state.edit_values = {
    id = tostring(cue.id or ""),
    character = tostring(cue.character or ""),
    status = tostring(cue.status or "Not Recorded"),
    cue_type = tostring(cue.cue_type or ""),
    direction = tostring(cue.direction or ""),
    start_time = ReaADR.format_timecode(cue.start_time, reaper.TimeMap_curFrameRate(0) > 0 and reaper.TimeMap_curFrameRate(0) or 24),
    end_time = ReaADR.format_timecode(cue.end_time, reaper.TimeMap_curFrameRate(0) > 0 and reaper.TimeMap_curFrameRate(0) or 24),
    line = tostring(cue.line or ""),
    notes = tostring(cue.notes or ""),
  }
  state.cursors = {}
  for key, value in pairs(state.edit_values) do
    state.cursors[key] = #tostring(value or "")
  end
  state.dirty = false
end

local function sync_edit_values(cue)
  if not cue then
    state.edit_values = nil
    state.edit_key = ""
    state.edit_original_key = ""
    state.active_field = nil
    state.dirty = false
    return
  end
  local key = ReaADR.cue_key(cue)
  if key ~= state.edit_key then
    begin_edit(cue, nil)
  end
end

local function save_edit(cue)
  if not state.edit_values or not cue then
    return false
  end
  local updated = {}
  for key, existing in pairs(cue or {}) do
    updated[key] = existing
  end
  updated._original_key = state.edit_original_key
  updated.id = state.edit_values.id
  updated.character = state.edit_values.character
  updated.status = state.edit_values.status
  updated.cue_type = state.edit_values.cue_type
  updated.direction = state.edit_values.direction
  local frame_rate = reaper.TimeMap_curFrameRate(0)
  if not frame_rate or frame_rate <= 0 then
    frame_rate = 24
  end
  local parsed_start, start_error = ReaADR.parse_timecode(state.edit_values.start_time, frame_rate)
  local parsed_end, end_error = ReaADR.parse_timecode(state.edit_values.end_time, frame_rate)
  if not parsed_start then
    ReaADR.message("Cue update failed:\n\nStart time is invalid: " .. tostring(start_error))
    return false
  end
  if not parsed_end then
    ReaADR.message("Cue update failed:\n\nEnd time is invalid: " .. tostring(end_error))
    return false
  end
  if parsed_end <= parsed_start then
    ReaADR.message("Cue update failed:\n\nEnd time must be after start time.")
    return false
  end
  updated.start_time = parsed_start
  updated.end_time = parsed_end
  updated.line = state.edit_values.line
  updated.notes = state.edit_values.notes
  local saved, err = ReaADR.update_cached_cue(updated)
  if not saved then
    ReaADR.message("Cue update failed:\n\n" .. tostring(err))
    return false
  end
  local refreshed, refresh_err = ReaADR.refresh_session({
    source = "cue_info_inline_edit",
  })
  if not refreshed then
    ReaADR.message("Cue was saved, but session refresh failed:\n\n" .. tostring(refresh_err))
  end
  state.edit_original_key = ReaADR.cue_key(saved)
  state.edit_key = state.edit_original_key
  state.dirty = false
  state.active_field = nil
  ReaADR.set_manager_selected_cue(saved)
  ReaADR.refresh_overlay_silent()
  return true
end

local function end_edit()
  state.active_field = nil
end

local function dropdown_values_for_field(field_key)
  if field_key == "status" then
    return ReaADR.cue_statuses()
  elseif field_key == "cue_type" then
    return ReaADR.cue_types()
  end
  return {}
end

local function choose_dropdown_value(field, cue)
  local choices = dropdown_values_for_field(field.key)
  if #choices == 0 then
    return
  end
  local current = tostring((state.edit_values or {})[field.key] or "")
  local labels = {}
  for index, value in ipairs(choices) do
    labels[index] = (current == tostring(value) and "!" or "") .. tostring(value)
  end
  gfx.x = field.x
  gfx.y = field.y + field.h
  local choice = gfx.showmenu(table.concat(labels, "|"))
  if choice and choice > 0 and choices[choice] then
    state.edit_values[field.key] = choices[choice]
    state.active_field = field.key
    state.dirty = true
    save_edit(cue)
  end
end

local function field_by_key(key)
  for _, field in ipairs(edit_fields) do
    if field.key == key then
      return field
    end
  end
  return nil
end

local function register_field_rect(key, rect)
  local field = field_by_key(key)
  if not field then
    return nil
  end
  field.x = rect.x
  field.y = rect.y
  field.w = rect.w
  field.h = rect.h
  state.field_rects[#state.field_rects + 1] = field
  return field
end

local function draw_single_line_editor(field)
  local theme = ReaADR.ui_theme()
  ReaADR.set_gfx_color(theme.panel_alt)
  gfx.rect(field.x, field.y, field.w, field.h, true)
  ReaADR.set_gfx_color(theme.accent_gold)
  gfx.rect(field.x, field.y, field.w, field.h, false)
  gfx.setfont(1, "Arial", 18)
  ReaADR.set_gfx_color(theme.text)
  local value = tostring((state.edit_values or {})[field.key] or "")
  draw_single_line_value(field, value, cursor_for_field(field.key), true)
end

local function draw_multiline_editor(field, font_size, line_height)
  local theme = ReaADR.ui_theme()
  ReaADR.set_gfx_color(theme.panel_alt)
  gfx.rect(field.x, field.y, field.w, field.h, true)
  ReaADR.set_gfx_color(theme.accent_gold)
  gfx.rect(field.x, field.y, field.w, field.h, false)
  gfx.setfont(1, "Arial", font_size)
  ReaADR.set_gfx_color(theme.text)
  local value = tostring((state.edit_values or {})[field.key] or "")
  draw_wrapped_text(value, field.x + 8, field.y + 8, field.w - 16, line_height, math.max(1, math.floor((field.h - 12) / line_height)))
  local cx, cy, ch = cursor_xy_for_multiline(field, value, cursor_for_field(field.key))
  gfx.line(cx, cy, cx, math.min(field.y + field.h - 8, cy + ch))
end

local function draw_editable_label(key, label, value, x, y, w)
  local field = register_field_rect(key, { x = x, y = y + 18, w = w, h = 32 })
  if field and state.active_field == key then
    local theme = ReaADR.ui_theme()
    gfx.setfont(1, "Arial", 14)
    ReaADR.set_gfx_color(theme.muted)
    gfx.x = x
    gfx.y = y
    gfx.drawstr(label)
    draw_single_line_editor(field)
  else
    draw_label(label, value, x, y, w)
  end
end

local function handle_text_input(char, cue)
  if not state.active_field or not state.edit_values then
    return
  end
  local field_key = state.active_field
  if field_key == "status" or field_key == "cue_type" then
    return
  end
  local value = state.edit_values[state.active_field] or ""
  local cursor = cursor_for_field(state.active_field)
  if char == 8 then
    if cursor > 0 then
      state.edit_values[state.active_field] = value:sub(1, cursor - 1) .. value:sub(cursor + 1)
      set_cursor_for_field(state.active_field, cursor - 1)
      state.dirty = true
    end
  elseif char == KEY_DELETE then
    if cursor < #value then
      state.edit_values[state.active_field] = value:sub(1, cursor) .. value:sub(cursor + 2)
      state.dirty = true
    end
  elseif char == KEY_LEFT then
    set_cursor_for_field(state.active_field, cursor - 1)
  elseif char == KEY_RIGHT then
    set_cursor_for_field(state.active_field, cursor + 1)
  elseif char == KEY_HOME or char == 1 then
    set_cursor_for_field(state.active_field, 0)
  elseif char == KEY_END or char == 5 then
    set_cursor_for_field(state.active_field, #value)
  elseif char == 13 then
    save_edit(cue)
  elseif char >= 32 and char < 127 then
    state.edit_values[state.active_field] = value:sub(1, cursor) .. string.char(char) .. value:sub(cursor + 1)
    set_cursor_for_field(state.active_field, cursor + 1)
    state.dirty = true
  end
end

local function draw_info(cue)
  ReaADR.mark_cue_info_panel_active()
  local theme = ReaADR.ui_theme()
  ReaADR.set_gfx_color(theme.bg)
  gfx.rect(0, 0, state.width, state.height, true)
  sync_edit_values(cue)
  state.field_rects = {}

  local frame_rate = reaper.TimeMap_curFrameRate(0)
  if not frame_rate or frame_rate <= 0 then
    frame_rate = 24
  end

  local header = ReaADR.draw_window_header(
    "ADR Cue Information",
    cue and ("Cue %s | %s"):format(tostring(cue.id or ""), tostring(cue.character or "")) or "No cue selected",
    { x = 24, y = 20, width = state.width - 48, height = 76 }
  )
  state.dock_button = nil

  if cue then
    local now = ReaADR.current_timeline_position()
    local cue_start = tonumber(cue.start_time) or 0
    local countdown = math.max(0, cue_start - now)
    local take_count = ReaADR.count_recorded_takes_for_cue(cue)
    local info_top = math.max(118, header.content_y + 8)
    local row_gap = 66
    local row1_y = info_top
    local row2_y = row1_y + row_gap
    local row3_y = row2_y + row_gap
    local gap = 24
    local content_w = state.width - 48
    local col_w = math.floor((content_w - (gap * 3)) / 4)
    local col1 = 24
    local col2 = col1 + col_w + gap
    local col3 = col2 + col_w + gap
    local col4 = col3 + col_w + gap

    draw_editable_label("id", "Cue", cue.id or "", col1, row1_y, col_w)
    draw_editable_label("character", "Character", cue.character or "", col2, row1_y, col_w)
    draw_editable_label("status", "Status", cue.status or "Not Recorded", col3, row1_y, col_w)
    draw_editable_label("cue_type", "Cue Type", cue.cue_type or "", col4, row1_y, col_w)
    state.dock_button = { x = col4, y = row2_y + 18, w = math.min(96, col_w), h = 32, label = "Dock" }
    draw_button(state.dock_button)
    draw_editable_label("start_time", "Start", ReaADR.format_timecode(cue.start_time, frame_rate), col1, row2_y, col_w)
    draw_editable_label("end_time", "End", ReaADR.format_timecode(cue.end_time, frame_rate), col2, row2_y, col_w)
    draw_label("Length", ("%.2fs"):format(ReaADR.cue_duration(cue)), col3, row2_y, col_w)
    draw_label("Countdown", ("%.2fs"):format(countdown), col1, row3_y, col_w)
    draw_label("Take Count", tostring(take_count), col2, row3_y, col_w)
    draw_label("Current Position", ReaADR.format_timecode(now, frame_rate), col3, row3_y, col_w)

    gfx.setfont(1, "Arial", 16)
    ReaADR.set_gfx_color(theme.muted)
    gfx.x = 24
    local dialogue_label_y = row3_y + 76
    gfx.y = dialogue_label_y
    gfx.drawstr("Dialogue")
    local notes_y = state.height - 176
    local dialogue_text_y = dialogue_label_y + 24
    local dialogue_h = math.max(68, notes_y - dialogue_text_y - 16)
    local line_field = register_field_rect("line", { x = 24, y = dialogue_text_y - 4, w = state.width - 56, h = dialogue_h })
    if state.active_field == "line" and line_field then
      draw_multiline_editor(line_field, 22, 26)
    else
      gfx.setfont(1, "Arial", 22)
      ReaADR.set_gfx_color(theme.text)
      local dialogue_max_lines = math.max(3, math.floor(dialogue_h / 26))
      draw_wrapped_text(tostring(cue.line or ""), 24, dialogue_text_y, state.width - 56, 26, dialogue_max_lines)
    end

    gfx.setfont(1, "Arial", 16)
    ReaADR.set_gfx_color(theme.muted)
    gfx.x = 24
    gfx.y = notes_y
    gfx.drawstr("Notes")
    local notes_field = register_field_rect("notes", { x = 24, y = notes_y + 24, w = state.width - 56, h = math.max(54, state.height - notes_y - 56) })
    if state.active_field == "notes" and notes_field then
      draw_multiline_editor(notes_field, 18, 22)
    else
      gfx.setfont(1, "Arial", 18)
      ReaADR.set_gfx_color(theme.text)
      draw_wrapped_text(tostring(cue.notes or ""), 24, notes_y + 24, state.width - 56, 22, math.max(2, math.floor((state.height - notes_y - 32) / 22)))
    end

    if state.dirty then
      gfx.setfont(1, "Arial", 13)
      ReaADR.set_gfx_color(theme.muted)
      gfx.x = 28
      gfx.y = state.height - 28
      gfx.drawstr("Unsaved edit: press Enter to save.")
    end
  else
    gfx.setfont(1, "Arial", 18)
    ReaADR.set_gfx_color(theme.text)
    gfx.x = 24
    gfx.y = 82
    gfx.drawstr("No ADR cues were found.")
  end
end

local function frame()
  sync_window_size()
  local cue = ReaADR.active_cue()

  draw_info(cue)
  gfx.update()
  local char = gfx.getchar()
  if char < 0 or char == 27 then
    ReaADR.save_window_state("cue_info")
    ReaADR.clear_cue_info_panel_active()
    gfx.quit()
    return
  elseif ReaADR.handle_gfx_transport_key(char, state.active_field ~= nil) then
    reaper.defer(frame)
    return
  end
  handle_text_input(char, cue)
  local mouse = gfx.mouse_cap % 2
  if mouse == 1 and state.last_mouse == 0 and state.dock_button and inside(state.dock_button, gfx.mouse_x, gfx.mouse_y) then
    dock_info_panel()
  elseif mouse == 1 and state.last_mouse == 0 and cue then
    local clicked_field = nil
    for _, field in ipairs(state.field_rects or {}) do
      if inside(field, gfx.mouse_x, gfx.mouse_y) then
        clicked_field = field
        break
      end
    end
    if clicked_field then
      local now = reaper.time_precise()
      local double_click = state.last_click_field == clicked_field.key and (now - state.last_click_time) <= 0.35
      state.last_click_field = clicked_field.key
      state.last_click_time = now
      if clicked_field.dropdown and double_click then
        choose_dropdown_value(clicked_field, cue)
      elseif not clicked_field.dropdown and (double_click or state.active_field == clicked_field.key) then
        state.active_field = clicked_field.key
        set_cursor_from_mouse(clicked_field)
      end
    else
      if not state.dirty then
        end_edit()
      end
    end
  end
  state.last_mouse = mouse
  reaper.defer(frame)
end

local restored = ReaADR.init_persistent_window("cue_info", "ReaADR Cue Information", {
  width = state.width,
  height = state.height,
  min_width = state.min_width,
  min_height = state.min_height,
})
state.width = restored.width
state.height = restored.height
frame()
