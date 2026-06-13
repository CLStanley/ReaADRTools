-- Cue list manager for ReaADR sessions.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local all_cues, source = ReaADR.session_cues()
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
  status_dropdown = false,
  session_revision = ReaADR.session_revision and ReaADR.session_revision() or 0,
  filter_signature = select(2, ReaADR.active_character_filter()),
  closed = false,
}

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

local function refresh_cues()
  local selected = cues[state.selected]
  local selected_key = selected and ReaADR.cue_key(selected) or ReaADR.manager_selected_cue_key()
  all_cues, source = ReaADR.session_cues()
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
  local revision = ReaADR.session_revision and ReaADR.session_revision() or 0
  local _, filter_signature = ReaADR.active_character_filter()
  filter_signature = tostring(filter_signature or "")
  if revision ~= state.session_revision or filter_signature ~= tostring(state.filter_signature or "") then
    state.session_revision = revision
    state.filter_signature = filter_signature
    refresh_cues()
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
  state.status_dropdown = false
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

local function sync_regions_from_manager()
  local selected = cues[state.selected]
  local selected_key = selected and ReaADR.cue_key(selected) or ""
  local sync_summary, sync_err = ReaADR.sync_cached_cues_from_project_regions()
  if not sync_summary then
    ReaADR.message("Region sync failed:\n\n" .. tostring(sync_err))
    return
  end

  local summary, rebuild_err = ReaADR.rebuild_cached_session({ clear_generated_items = true })
  if not summary then
    ReaADR.message("Region positions were synced, but cue rebuild failed:\n\n" .. tostring(rebuild_err))
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
    ("Synced %d moved region(s).\n\nCue audio items rebuilt: %d created, %d updated, %d skipped\nOverlap splits: %d\nMissing generated regions: %d"):format(
      sync_summary.updated or 0,
      summary.cue_audio_created or 0,
      summary.cue_audio_updated or 0,
      summary.cue_audio_skipped or 0,
      summary.overlap_conflicts or 0,
      sync_summary.missing or 0
    )
  )
end

local function launch_info_panel(edit, close_on_save)
  local cue = cues[state.selected]
  if not cue then
    return
  end
  ReaADR.set_manager_selected_cue(cue)
  if ReaADR.set_cue_info_launch_options then
    ReaADR.set_cue_info_launch_options({ edit = edit == true, close_on_save = close_on_save == true })
  end
  local path = script_dir() .. "/ReaADR_Cue_Info_Panel.lua"
  local command_id = reaper.AddRemoveReaScript(true, 0, path, true)
  if command_id and command_id > 0 then
    reaper.Main_OnCommand(command_id, 0)
  else
    ReaADR.message("Could not open Cue Information Panel.")
  end
end

local function draw_text_input(rect, value, label)
  gfx.setfont(1, "Arial", 12)
  gfx.set(0.62, 0.66, 0.70, 1)
  gfx.x = rect.x
  gfx.y = rect.y - 16
  gfx.drawstr(label)
  gfx.set(0.05, 0.06, 0.07, 1)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  gfx.set(0.40, 0.44, 0.48, 1)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 13)
  gfx.set(1, 1, 1, 1)
  gfx.x = rect.x + 6
  gfx.y = rect.y + 7
  local display = tostring(value or "")
  if #display > math.floor(rect.w / 7) then
    display = display:sub(1, math.max(1, math.floor(rect.w / 7) - 3)) .. "..."
  end
  gfx.drawstr(display)
end

local function draw_status_dropdown(anchor, selected_status)
  local statuses = ReaADR.cue_statuses()
  local option_h = 24
  local menu_h = #statuses * option_h
  local y = anchor.y - menu_h - 4
  if y < 78 then
    y = anchor.y + anchor.h + 4
  end
  local options = {}
  gfx.set(0.07, 0.08, 0.09, 1)
  gfx.rect(anchor.x, y, anchor.w, menu_h, true)
  gfx.set(0.45, 0.50, 0.55, 1)
  gfx.rect(anchor.x, y, anchor.w, menu_h, false)
  for index, status in ipairs(statuses) do
    local rect = { x = anchor.x, y = y + ((index - 1) * option_h), w = anchor.w, h = option_h, status = status }
    options[#options + 1] = rect
    local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
    local active = tostring(status) == tostring(selected_status)
    gfx.set(active and 0.22 or (hover and 0.18 or 0.10), active and 0.34 or (hover and 0.24 or 0.11), active and 0.42 or (hover and 0.30 or 0.12), 1)
    gfx.rect(rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, true)
    gfx.setfont(1, "Arial", 13)
    gfx.set(1, 1, 1, 1)
    gfx.x = rect.x + 8
    gfx.y = rect.y + 5
    gfx.drawstr(status)
  end
  return options
end

local button_specs = {
  { key = "jump", label = "Jump...", min_w = 92, hint = "Type a cue number and jump to its region start." },
  { key = "prev", label = "Previous", min_w = 92, hint = "Select the previous cue in the list." },
  { key = "next", label = "Next", min_w = 92, hint = "Select the next cue in the list." },
  { key = "add", label = "Add Cue", min_w = 92, hint = "Create a cue at the current timeline position." },
  { key = "filter", label = "Character Filter", min_w = 132, hint = "Enable or disable character tracks for focused recording passes." },
  { key = "sync", label = "Sync Regions", min_w = 124, hint = "Apply manually moved region positions back to cues, cue beeps, lanes, and overlays." },
  { key = "overlay", label = "Refresh Ovl", min_w = 116, hint = "Refresh the video overlay for the selected/current cue state." },
  { key = "info", label = "Info Panel", min_w = 112, hint = "Open the large cue information panel for the selected cue." },
}

local function layout_buttons(content_w)
  local gap = 8
  local button_h = 32
  local rows = {}
  local current = {}
  local current_w = 0

  for _, spec in ipairs(button_specs) do
    local next_w = current_w + (current_w > 0 and gap or 0) + spec.min_w
    if #current > 0 and next_w > content_w then
      rows[#rows + 1] = current
      current = { spec }
      current_w = spec.min_w
    else
      current[#current + 1] = spec
      current_w = next_w
    end
  end
  if #current > 0 then
    rows[#rows + 1] = current
  end

  local toolbar_h = (#rows * button_h) + ((#rows - 1) * gap)
  local toolbar_y = state.height - 52 - toolbar_h
  local buttons = {}

  for row_index, row in ipairs(rows) do
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

  local content_w = state.width - 44
  local buttons, toolbar_y = layout_buttons(content_w)
  local selected_y = toolbar_y - 38

  local header_y = 78
  gfx.set(0.15, 0.16, 0.18, 1)
  gfx.rect(22, header_y, content_w, 28, true)
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
  local visible_rows = math.max(1, math.floor((selected_y - list_y - 8) / row_h))
  local max_scroll = math.max(0, #cues - visible_rows)
  state.scroll = math.max(0, math.min(state.scroll, max_scroll))

  for row = 1, visible_rows do
    local cue_index = state.scroll + row
    local cue = cues[cue_index]
    if cue then
      local y = list_y + ((row - 1) * row_h)
      local selected = cue_index == state.selected
      gfx.set(selected and 0.24 or 0.13, selected and 0.28 or 0.14, selected and 0.32 or 0.15, 1)
      gfx.rect(22, y, content_w, row_h - 2, true)
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
      local line_chars = math.max(20, math.floor((state.width - 620) / 7))
      if #line > line_chars then
        line = line:sub(1, math.max(1, line_chars - 3)) .. "..."
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
  local status_rect = nil
  local status_options = {}
  if selected_cue then
    gfx.setfont(1, "Arial", 14)
    gfx.set(0.78, 0.82, 0.86, 1)
    gfx.x = 22
    gfx.y = selected_y
    gfx.drawstr(("Selected Cue %s | %s | %.2fs"):format(tostring(selected_cue.id or ""), tostring(selected_cue.character or ""), ReaADR.cue_duration(selected_cue)))
    status_rect = { x = math.min(state.width - 280, 700), y = selected_y - 6, w = 150, h = 28 }
    draw_text_input(status_rect, selected_cue.status or "Not Recorded", "Status")
  end

  if selected_cue and state.status_dropdown and status_rect then
    status_options = draw_status_dropdown(status_rect, selected_cue.status or "Not Recorded")
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
    select_cue(math.max(1, state.selected - 1), true)
    sync_scroll_to_selection(visible_rows)
  elseif char == 1685026670 then
    select_cue(math.min(#cues, state.selected + 1), true)
    sync_scroll_to_selection(visible_rows)
  end

  local wheel = gfx.mouse_wheel
  if wheel ~= 0 then
    state.scroll = math.max(0, math.min(max_scroll, state.scroll - (wheel > 0 and 1 or -1)))
    gfx.mouse_wheel = 0
  end

  local mouse = gfx.mouse_cap % 2
  if mouse == 1 and state.last_mouse == 0 then
    local clicked_status_dropdown = false
    if state.status_dropdown then
      for _, option in ipairs(status_options or {}) do
        if inside(option, gfx.mouse_x, gfx.mouse_y) then
          set_status_for_selected(option.status)
          clicked_status_dropdown = true
          break
        end
      end
      if not clicked_status_dropdown and status_rect and not inside(status_rect, gfx.mouse_x, gfx.mouse_y) then
        state.status_dropdown = false
      end
    end

    if not clicked_status_dropdown then
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
            launch_info_panel(true, true)
            break
          end
        end
      end
    end

    local cue = cues[state.selected]
    if clicked_status_dropdown then
      -- Status selection already handled.
    elseif cue and inside(buttons.jump, gfx.mouse_x, gfx.mouse_y) then
      prompt_jump_to_cue()
    elseif inside(buttons.prev, gfx.mouse_x, gfx.mouse_y) then
      select_cue(math.max(1, state.selected - 1), true)
    elseif inside(buttons.next, gfx.mouse_x, gfx.mouse_y) then
      select_cue(math.min(#cues, state.selected + 1), true)
    elseif inside(buttons.add, gfx.mouse_x, gfx.mouse_y) then
      add_cue_from_manager()
    elseif inside(buttons.filter, gfx.mouse_x, gfx.mouse_y) then
      launch_character_filter()
    elseif inside(buttons.sync, gfx.mouse_x, gfx.mouse_y) then
      sync_regions_from_manager()
    elseif inside(buttons.overlay, gfx.mouse_x, gfx.mouse_y) then
      ReaADR.refresh_overlay_silent()
    elseif cue and inside(buttons.info, gfx.mouse_x, gfx.mouse_y) then
      launch_info_panel(false, false)
    end

    if cue and status_rect and inside(status_rect, gfx.mouse_x, gfx.mouse_y) then
      state.status_dropdown = not state.status_dropdown
    end
  end
  state.last_mouse = mouse

  reaper.defer(frame)
end

gfx.init("ReaADR Cue Manager", state.width, state.height)
frame()
