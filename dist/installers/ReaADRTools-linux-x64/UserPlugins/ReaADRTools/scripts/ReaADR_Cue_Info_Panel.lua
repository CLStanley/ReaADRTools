-- Live ADR cue information panel with an expanded cue editor.

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
  editing = false,
  active_field = "line",
  edit_values = nil,
  cursors = {},
  edit_original_key = "",
  close_on_save = false,
}

local launch_options = ReaADR.consume_cue_info_launch_options and ReaADR.consume_cue_info_launch_options() or {}
state.close_on_save = launch_options.close_on_save == true

local edit_fields = {
  { key = "id", label = "Cue Number", row = 1, col = 1 },
  { key = "character", label = "Character", row = 1, col = 2 },
  { key = "cue_type", label = "Cue Type", row = 1, col = 3 },
  { key = "direction", label = "Direction", row = 1, col = 4 },
  { key = "start_time", label = "Start SMPTE or Seconds", row = 2, col = 1 },
  { key = "end_time", label = "End SMPTE or Seconds", row = 2, col = 2 },
  { key = "line", label = "Dialogue", multiline = true },
  { key = "notes", label = "Notes", multiline = true },
}

local function sync_window_size()
  state.width = math.max(state.min_width, gfx.w or state.width)
  state.height = math.max(state.min_height, gfx.h or state.height)
end

local function layout_edit_fields()
  local margin = 28
  local gap = 18
  local content_w = state.width - (margin * 2)
  local top_w = math.max(130, (content_w - (gap * 3)) / 4)
  local time_w = math.max(210, (content_w - gap) / 2)
  for _, field in ipairs(edit_fields) do
    if field.row == 1 then
      field.x = margin + ((field.col - 1) * (top_w + gap))
      field.y = 104
      field.w = top_w
      field.h = 34
    elseif field.row == 2 then
      field.x = margin + ((field.col - 1) * (time_w + gap))
      field.y = 182
      field.w = time_w
      field.h = 34
    elseif field.key == "line" then
      field.x = margin
      field.y = 272
      field.w = content_w
      field.h = math.max(130, state.height - 450)
    elseif field.key == "notes" then
      field.x = margin
      field.y = state.height - 150
      field.w = content_w
      field.h = 86
    end
  end
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
  local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
  gfx.set(hover and 0.20 or 0.16, hover and 0.34 or 0.24, hover and 0.42 or 0.30, 1)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  gfx.set(0.66, 0.70, 0.74, 1)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 14)
  gfx.set(1, 1, 1, 1)
  local tw = gfx.measurestr(rect.label)
  gfx.x = rect.x + math.floor((rect.w - tw) / 2)
  gfx.y = rect.y + 8
  gfx.drawstr(rect.label)
  return hover
end

local function draw_label(label, value, x, y)
  gfx.setfont(1, "Arial", 14)
  gfx.set(0.62, 0.66, 0.70, 1)
  gfx.x = x
  gfx.y = y
  gfx.drawstr(label)
  gfx.setfont(1, "Arial", 22)
  gfx.set(1, 1, 1, 1)
  gfx.x = x
  gfx.y = y + 18
  gfx.drawstr(tostring(value or ""))
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

local function begin_edit(cue)
  if not cue then
    return
  end
  state.editing = true
  state.active_field = "line"
  state.edit_original_key = ReaADR.cue_key(cue)
  state.edit_values = {
    id = tostring(cue.id or ""),
    character = tostring(cue.character or ""),
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
end

local function save_edit(cue)
  if not state.edit_values then
    return false
  end
  local updated = {}
  for key, existing in pairs(cue or {}) do
    updated[key] = existing
  end
  updated._original_key = state.edit_original_key
  updated.id = state.edit_values.id
  updated.character = state.edit_values.character
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
  state.edit_original_key = ReaADR.cue_key(saved)
  return true
end

local function end_edit()
  state.editing = false
  state.edit_values = nil
  state.cursors = {}
  state.active_field = "line"
end

local function draw_edit_field(field)
  local active = field.key == state.active_field
  gfx.setfont(1, "Arial", 13)
  gfx.set(0.64, 0.68, 0.72, 1)
  gfx.x = field.x
  gfx.y = field.y - 20
  gfx.drawstr(field.label)
  gfx.set(0.05, 0.06, 0.07, 1)
  gfx.rect(field.x, field.y, field.w, field.h, true)
  gfx.set(active and 0.95 or 0.40, active and 0.78 or 0.44, active and 0.30 or 0.48, 1)
  gfx.rect(field.x, field.y, field.w, field.h, false)
  gfx.setfont(1, "Arial", field.multiline and 16 or 14)
  gfx.set(1, 1, 1, 1)
  local value = tostring((state.edit_values or {})[field.key] or "")
  local cursor = cursor_for_field(field.key)
  if field.multiline then
    draw_wrapped_text(value, field.x + 8, field.y + 8, field.w - 16, 20, math.floor((field.h - 12) / 20))
    if active then
      local cx, cy, ch = cursor_xy_for_multiline(field, value, cursor)
      gfx.line(cx, cy, cx, math.min(field.y + field.h - 8, cy + ch))
    end
  else
    draw_single_line_value(field, value, cursor, active)
  end
end

local function draw_editor()
  layout_edit_fields()
  gfx.set(0.10, 0.11, 0.12, 1)
  gfx.rect(0, 0, state.width, state.height, true)
  gfx.setfont(1, "Arial", 24)
  gfx.set(1, 1, 1, 1)
  gfx.x = 24
  gfx.y = 22
  gfx.drawstr("Edit ADR Cue")
  gfx.setfont(1, "Arial", 13)
  gfx.set(0.72, 0.76, 0.80, 1)
  gfx.x = 24
  gfx.y = 52
  gfx.drawstr("Click a field and type. Start/end accept SMPTE timecode or seconds.")
  for _, field in ipairs(edit_fields) do
    draw_edit_field(field)
  end
  local save = { x = state.width - 292, y = state.height - 48, w = 110, h = 34, label = "Save" }
  local close = { x = state.width - 154, y = state.height - 48, w = 110, h = 34, label = "Cancel" }
  draw_button(save)
  draw_button(close)
  return save, close
end

local function handle_text_input(char)
  if not state.active_field or not state.edit_values then
    return
  end
  local value = state.edit_values[state.active_field] or ""
  local cursor = cursor_for_field(state.active_field)
  if char == 8 then
    if cursor > 0 then
      state.edit_values[state.active_field] = value:sub(1, cursor - 1) .. value:sub(cursor + 1)
      set_cursor_for_field(state.active_field, cursor - 1)
    end
  elseif char == KEY_DELETE then
    if cursor < #value then
      state.edit_values[state.active_field] = value:sub(1, cursor) .. value:sub(cursor + 2)
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
    if state.active_field == "line" or state.active_field == "notes" then
      state.edit_values[state.active_field] = value:sub(1, cursor) .. "\n" .. value:sub(cursor + 1)
      set_cursor_for_field(state.active_field, cursor + 1)
    end
  elseif char >= 32 and char < 127 then
    state.edit_values[state.active_field] = value:sub(1, cursor) .. string.char(char) .. value:sub(cursor + 1)
    set_cursor_for_field(state.active_field, cursor + 1)
  end
end

local function draw_info(cue)
  gfx.set(0.10, 0.11, 0.12, 1)
  gfx.rect(0, 0, state.width, state.height, true)

  local frame_rate = reaper.TimeMap_curFrameRate(0)
  if not frame_rate or frame_rate <= 0 then
    frame_rate = 24
  end

  gfx.setfont(1, "Arial", 24)
  gfx.set(1, 1, 1, 1)
  gfx.x = 24
  gfx.y = 20
  gfx.drawstr("ADR Cue Information")

  local buttons = {
    edit = { x = state.width - 256, y = 28, w = 106, h = 32, label = "Edit" },
    refresh = { x = state.width - 134, y = 28, w = 106, h = 32, label = "Refresh" },
  }

  if cue then
    local now = ReaADR.current_timeline_position()
    local cue_start = tonumber(cue.start_time) or 0
    local countdown = math.max(0, cue_start - now)
    local take_count = ReaADR.count_recorded_takes_for_cue(cue)

    draw_label("Cue", cue.id or "", 24, 70)
    draw_label("Character", cue.character or "", 180, 70)
    draw_label("Status", cue.status or "Not Recorded", 452, 70)
    draw_label("Start", ReaADR.format_timecode(cue.start_time, frame_rate), 24, 136)
    draw_label("End", ReaADR.format_timecode(cue.end_time, frame_rate), 260, 136)
    draw_label("Length", ("%.2fs"):format(ReaADR.cue_duration(cue)), 496, 136)
    draw_label("Countdown", ("%.2fs"):format(countdown), 24, 202)
    draw_label("Take Count", tostring(take_count), 260, 202)
    draw_label("Current Position", ReaADR.format_timecode(now, frame_rate), 496, 202)

    gfx.setfont(1, "Arial", 16)
    gfx.set(0.62, 0.66, 0.70, 1)
    gfx.x = 24
    gfx.y = 278
    gfx.drawstr("Line")
    gfx.setfont(1, "Arial", 22)
    gfx.set(1, 1, 1, 1)
    draw_wrapped_text(tostring(cue.line or ""), 24, 302, state.width - 56, 26, math.max(3, math.floor((state.height - 330) / 26)))
    draw_button(buttons.edit)
    draw_button(buttons.refresh)
  else
    gfx.setfont(1, "Arial", 18)
    gfx.set(0.86, 0.88, 0.90, 1)
    gfx.x = 24
    gfx.y = 82
    gfx.drawstr("No ADR cues were found.")
  end
  return buttons
end

local function frame()
  sync_window_size()
  local cue = ReaADR.active_cue()
  local buttons

  if state.editing then
    local save_button, close_button = draw_editor()
    gfx.update()
    local char = gfx.getchar()
    if char < 0 or char == 27 then
      end_edit()
      return
    end
    handle_text_input(char)
    local mouse = gfx.mouse_cap % 2
    if mouse == 1 and state.last_mouse == 0 then
      for _, field in ipairs(edit_fields) do
        if inside(field, gfx.mouse_x, gfx.mouse_y) then
          state.active_field = field.key
          set_cursor_from_mouse(field)
        end
      end
      if inside(save_button, gfx.mouse_x, gfx.mouse_y) then
        if save_edit(cue) then
          if state.close_on_save then
            gfx.quit()
            return
          else
            end_edit()
          end
        end
      elseif inside(close_button, gfx.mouse_x, gfx.mouse_y) then
        end_edit()
      end
    end
    state.last_mouse = mouse
    reaper.defer(frame)
    return
  end

  buttons = draw_info(cue)
  gfx.update()
  local char = gfx.getchar()
  if char < 0 or char == 27 then
    gfx.quit()
    return
  end

  local mouse = gfx.mouse_cap % 2
  if mouse == 1 and state.last_mouse == 0 then
    if cue and inside(buttons.edit, gfx.mouse_x, gfx.mouse_y) then
      begin_edit(cue)
    elseif inside(buttons.refresh, gfx.mouse_x, gfx.mouse_y) then
      ReaADR.refresh_overlay_silent()
    end
  end
  state.last_mouse = mouse
  reaper.defer(frame)
end

gfx.init("ReaADR Cue Information", state.width, state.height)
if launch_options.edit then
  begin_edit(ReaADR.active_cue())
end
frame()
