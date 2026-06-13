-- Cue list manager for ReaADR sessions.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local cues, source = ReaADR.session_cues()
if not cues or #cues == 0 then
  ReaADR.message("No ADR cues were found. Import a script or generate cues first.")
  return
end

local state = {
  width = 980,
  height = 700,
  selected = 1,
  scroll = 0,
  last_mouse = 0,
  closed = false,
}

local function inside(rect, x, y)
  return x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
end

local function button(rect)
  local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
  gfx.set(hover and 0.20 or 0.16, hover and 0.36 or 0.24, hover and 0.46 or 0.30, 1)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  gfx.set(0.66, 0.70, 0.74, 1)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 14)
  gfx.set(1, 1, 1, 1)
  gfx.x = rect.x + 10
  gfx.y = rect.y + 8
  gfx.drawstr(rect.label)
  return hover
end

local function status_menu()
  local statuses = ReaADR.cue_statuses()
  local mouse_x, mouse_y = reaper.GetMousePosition()
  gfx.init("Set Cue Status", 0, 0, 0, mouse_x, mouse_y)
  local choice = gfx.showmenu(table.concat(statuses, "|"))
  gfx.quit()
  return statuses[choice]
end

local function refresh_cues()
  cues = ReaADR.session_cues()
  cues = cues or {}
  if state.selected > #cues then
    state.selected = math.max(1, #cues)
  end
end

local function set_status_for_selected()
  local cue = cues[state.selected]
  if not cue then
    return
  end
  local status = status_menu()
  if not status then
    return
  end
  ReaADR.set_cue_status_at_position(status, cue.start_time)
  refresh_cues()
end

local function frame()
  local hover_hint = ""
  gfx.set(0.10, 0.11, 0.12, 1)
  gfx.rect(0, 0, state.width, state.height, true)

  local frame_rate = reaper.TimeMap_curFrameRate(0)
  if not frame_rate or frame_rate <= 0 then
    frame_rate = 24
  end

  gfx.setfont(1, "Arial", 22)
  gfx.set(1, 1, 1, 1)
  gfx.x = 22
  gfx.y = 18
  gfx.drawstr("ReaADR Cue Manager")

  gfx.setfont(1, "Arial", 13)
  gfx.set(0.74, 0.78, 0.82, 1)
  gfx.x = 22
  gfx.y = 46
  gfx.drawstr(("Source: %s | Cues: %d"):format(tostring(source or "session"), #cues))

  local buttons_y = state.height - 84
  local selected_y = buttons_y - 38
  local buttons = {
    jump = { x = 22, y = buttons_y, w = 92, h = 32, label = "Jump", hint = "Move the edit cursor to the selected cue." },
    prev = { x = 124, y = buttons_y, w = 92, h = 32, label = "Previous", hint = "Select the previous cue in the list." },
    next = { x = 226, y = buttons_y, w = 92, h = 32, label = "Next", hint = "Select the next cue in the list." },
    status = { x = 328, y = buttons_y, w = 110, h = 32, label = "Set Status", hint = "Update the selected cue status and refresh cue data." },
    overlay = { x = 448, y = buttons_y, w = 128, h = 32, label = "Refresh Overlay", hint = "Refresh the video overlay for the selected/current cue state." },
    info = { x = 586, y = buttons_y, w = 112, h = 32, label = "Info Panel", hint = "Open the large cue information panel for booth visibility." },
  }

  local header_y = 78
  gfx.set(0.15, 0.16, 0.18, 1)
  gfx.rect(22, header_y, 936, 28, true)
  gfx.setfont(1, "Arial", 13)
  gfx.set(0.84, 0.87, 0.90, 1)
  gfx.x = 30
  gfx.y = header_y + 7
  gfx.drawstr("Cue")
  gfx.x = 92
  gfx.drawstr("Character")
  gfx.x = 214
  gfx.drawstr("Start SMPTE")
  gfx.x = 340
  gfx.drawstr("End SMPTE")
  gfx.x = 466
  gfx.drawstr("Status")
  gfx.x = 594
  gfx.drawstr("Line")

  local row_h = 30
  local list_y = header_y + 30
  local visible_rows = math.max(6, math.floor((selected_y - list_y - 8) / row_h))
  local max_scroll = math.max(0, #cues - visible_rows)
  state.scroll = math.max(0, math.min(state.scroll, max_scroll))

  for row = 1, visible_rows do
    local cue_index = state.scroll + row
    local cue = cues[cue_index]
    if cue then
      local y = list_y + ((row - 1) * row_h)
      local selected = cue_index == state.selected
      gfx.set(selected and 0.24 or 0.13, selected and 0.28 or 0.14, selected and 0.32 or 0.15, 1)
      gfx.rect(22, y, 936, row_h - 2, true)
      gfx.setfont(1, "Arial", 13)
      gfx.set(1, 1, 1, 1)
      gfx.x = 30
      gfx.y = y + 7
      gfx.drawstr(tostring(cue.id or ""))
      gfx.x = 92
      gfx.drawstr(tostring(cue.character or ""))
      gfx.x = 214
      gfx.drawstr(ReaADR.format_timecode(cue.start_time, frame_rate))
      gfx.x = 340
      gfx.drawstr(ReaADR.format_timecode(cue.end_time, frame_rate))
      gfx.x = 466
      gfx.drawstr(tostring(cue.status or "Not Recorded"))
      gfx.x = 594
      local line = tostring(cue.line or "")
      if #line > 48 then
        line = line:sub(1, 45) .. "..."
      end
      gfx.drawstr(line)
    end
  end

  for _, rect in pairs(buttons) do
    if button(rect) and rect.hint then
      hover_hint = rect.hint
    end
  end

  local selected_cue = cues[state.selected]
  if selected_cue then
    gfx.setfont(1, "Arial", 14)
    gfx.set(0.78, 0.82, 0.86, 1)
    gfx.x = 22
    gfx.y = selected_y
    gfx.drawstr(("Selected Cue %s | %s | %.2fs"):format(tostring(selected_cue.id or ""), tostring(selected_cue.character or ""), ReaADR.cue_duration(selected_cue)))
  end

  if hover_hint ~= "" then
    gfx.set(0.08, 0.09, 0.10, 1)
    gfx.rect(22, state.height - 40, state.width - 44, 24, true)
    gfx.setfont(1, "Arial", 13)
    gfx.set(0.86, 0.90, 0.94, 1)
    gfx.x = 30
    gfx.y = state.height - 34
    gfx.drawstr(hover_hint)
  end

  gfx.update()

  local char = gfx.getchar()
  if char < 0 or char == 27 then
    gfx.quit()
    return
  elseif char == 30064 then
    state.selected = math.max(1, state.selected - 1)
    if state.selected <= state.scroll then
      state.scroll = math.max(0, state.scroll - 1)
    end
  elseif char == 1685026670 then
    state.selected = math.min(#cues, state.selected + 1)
    if state.selected > state.scroll + visible_rows then
      state.scroll = state.scroll + 1
    end
  end

  local wheel = gfx.mouse_wheel
  if wheel ~= 0 then
    state.scroll = math.max(0, math.min(max_scroll, state.scroll - (wheel > 0 and 1 or -1)))
    gfx.mouse_wheel = 0
  end

  local mouse = gfx.mouse_cap % 2
  if mouse == 1 and state.last_mouse == 0 then
    for row = 1, visible_rows do
      local cue_index = state.scroll + row
      local rect = { x = 22, y = list_y + ((row - 1) * row_h), w = 936, h = row_h - 2 }
      if cues[cue_index] and inside(rect, gfx.mouse_x, gfx.mouse_y) then
        state.selected = cue_index
      end
    end

    local cue = cues[state.selected]
    if cue and inside(buttons.jump, gfx.mouse_x, gfx.mouse_y) then
      ReaADR.jump_to_cue(cue)
      ReaADR.refresh_overlay_silent()
    elseif inside(buttons.prev, gfx.mouse_x, gfx.mouse_y) then
      state.selected = math.max(1, state.selected - 1)
    elseif inside(buttons.next, gfx.mouse_x, gfx.mouse_y) then
      state.selected = math.min(#cues, state.selected + 1)
    elseif cue and inside(buttons.status, gfx.mouse_x, gfx.mouse_y) then
      set_status_for_selected()
    elseif inside(buttons.overlay, gfx.mouse_x, gfx.mouse_y) then
      ReaADR.refresh_overlay_silent()
    elseif inside(buttons.info, gfx.mouse_x, gfx.mouse_y) then
      dofile(script_dir() .. "/ReaADR_Cue_Info_Panel.lua")
    end
  end
  state.last_mouse = mouse

  reaper.defer(frame)
end

gfx.init("ReaADR Cue Manager", state.width, state.height)
frame()
