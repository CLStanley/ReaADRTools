-- Cue list manager for ReaADR sessions.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local function manager_source_cues()
  local loaded = ReaADR.load_session_cues()
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
  min_width = 900,
  min_height = 180,
  selected = 1,
  scroll = 0,
  last_mouse = 0,
  last_click_time = 0,
  last_click_index = nil,
  dropdown_field = nil,
  character_filter_open = false,
  character_filter_options = nil,
  session_revision = ReaADR.session_revision and ReaADR.session_revision() or 0,
  filter_signature = select(2, ReaADR.active_character_filter()),
  selected_key = ReaADR.manager_selected_cue_key and ReaADR.manager_selected_cue_key() or "",
  closed = false,
  editing = nil,
  dropdown_rect = nil,
  character_filter_rects = {},
  character_filter_submenu_character = nil,
  last_poll = 0,
  dragging_scrollbar = false,
  scrollbar_drag_offset = 0,
  sort_key = "start_time",
  sort_ascending = true,
  last_header_click_key = nil,
  last_header_click_time = 0,
  move_docker_right_after_update = false,
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

local function sort_value(cue, key)
  if key == "start_time" or key == "end_time" then
    return tonumber(cue[key]) or 0
  end
  return tostring(cue[key] or ""):lower()
end

local function apply_sort()
  local key = state.sort_key
  if not key or key == "" then
    return
  end
  local ascending = state.sort_ascending ~= false
  table.sort(cues, function(a, b)
    local av = sort_value(a, key)
    local bv = sort_value(b, key)
    if av == bv then
      local as = tonumber(a.start_time) or 0
      local bs = tonumber(b.start_time) or 0
      if as == bs then
        return tostring(a.id or "") < tostring(b.id or "")
      end
      return as < bs
    end
    if ascending then
      return av < bv
    end
    return av > bv
  end)
end

local selected_key = ReaADR.selected_region_cue_key()
if selected_key == "" then
  selected_key = ReaADR.manager_selected_cue_key()
end
apply_sort()
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
  local label = tostring(rect.label or "")
  local tw, th = gfx.measurestr(label)
  gfx.x = rect.x + math.floor((rect.w - tw) / 2)
  gfx.y = rect.y + math.floor((rect.h - th) / 2) - 1
  gfx.drawstr(label)
  return hover
end

local function wrap_index(index, delta, count)
  count = tonumber(count) or 0
  if count <= 0 then
    return nil
  end
  index = tonumber(index) or 1
  delta = tonumber(delta) or 0
  local next_index = index + delta
  if ReaADR.navigation_wrap_enabled and ReaADR.navigation_wrap_enabled() then
    while next_index < 1 do
      next_index = next_index + count
    end
    while next_index > count do
      next_index = next_index - count
    end
    return next_index
  end
  return math.max(1, math.min(count, next_index))
end

local select_cue

local function select_relative_cue(delta, jump)
  local count = #cues
  local next_index = wrap_index(state.selected, delta, count)
  if next_index then
    select_cue(next_index, jump)
  end
end

local function refresh_cues()
  local selected = cues[state.selected]
  local selected_key = selected and ReaADR.cue_key(selected) or ReaADR.manager_selected_cue_key()
  all_cues, source = manager_source_cues()
  all_cues = all_cues or {}
  cues = ReaADR.filter_cues_by_active_characters(all_cues)
  apply_sort()
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
  local selected_key = ReaADR.manager_selected_cue_key and ReaADR.manager_selected_cue_key() or ""
  if revision ~= state.session_revision or filter_signature ~= tostring(state.filter_signature or "") then
    state.session_revision = revision
    state.filter_signature = filter_signature
    state.selected_key = selected_key
    refresh_cues()
    state.last_poll = now
  elseif selected_key ~= "" and selected_key ~= tostring(state.selected_key or "") then
    local index = cue_index_by_key(selected_key)
    if index then
      state.selected = index
      sync_scroll_to_selection()
    end
    state.selected_key = selected_key
    state.last_poll = now
  elseif not state.editing and (now - (state.last_poll or 0)) >= 0.25 then
    refresh_cues()
    state.last_poll = now
  end
end

select_cue = function(index, jump)
  if not cues[index] then
    return
  end
  state.selected = index
  local cue = cues[state.selected]
  state.selected_key = ReaADR.cue_key(cue)
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

local function character_filter_options()
  local targets = ReaADR.available_filter_targets and ReaADR.available_filter_targets() or {}
  local active = {}
  if ReaADR.current_active_filter_targets then
    for _, target in ipairs(ReaADR.current_active_filter_targets(targets)) do
      active[target.key] = true
    end
  end
  return targets, active
end

local function grouped_filter_targets(targets)
  local groups = {}
  local by_character = {}
  for _, target in ipairs(targets or {}) do
    local character = target.character or "Unassigned"
    local group = by_character[character]
    if not group then
      group = { character = character, targets = {} }
      groups[#groups + 1] = group
      by_character[character] = group
    end
    group.targets[#group.targets + 1] = target
  end
  return groups, by_character
end

local function apply_character_filter_targets(targets, selected)
  local active = {}
  for _, target in ipairs(targets or {}) do
    if selected[target.key] then
      active[#active + 1] = target
    end
  end
  if #active == #targets then
    ReaADR.set_active_character_filter({})
  elseif #active == 0 then
    ReaADR.set_active_character_filter({ "__none__" })
  else
    ReaADR.set_active_character_filter(active)
  end
  if ReaADR.apply_character_filter then
    ReaADR.apply_character_filter()
  end
  refresh_cues()
end

local function fit_filter_text(value, max_w)
  local text = tostring(value or "")
  max_w = tonumber(max_w) or 0
  if max_w <= 0 then
    return text
  end
  gfx.setfont(1, "Arial", 13)
  if gfx.measurestr(text) <= max_w then
    return text
  end
  local trimmed = text
  while #trimmed > 1 and gfx.measurestr(trimmed .. "...") > max_w do
    trimmed = trimmed:sub(1, #trimmed - 1)
  end
  return trimmed .. "..."
end

local function draw_character_filter_menu(anchor)
  local theme = ReaADR.ui_theme()
  local targets, active = character_filter_options()
  local groups, by_character = grouped_filter_targets(targets)
  local option_h = 26
  local width = math.max(anchor.w, 260)
  local submenu_width = 220
  local height = option_h * (#groups + 1)
  local x = anchor.x
  local y = anchor.y + anchor.h + 4
  if y + height > state.height - 58 then
    y = anchor.y - height - 4
  end
  y = math.max(8, y)
  state.character_filter_rects = {}
  ReaADR.set_gfx_color(theme.panel)
  gfx.rect(x, y, width, height, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(x, y, width, height, false)

  local all_checked = not ReaADR.character_filter_enabled()
  local all_rect = { x = x, y = y, w = width, h = option_h, key = "__all__" }
  state.character_filter_rects[#state.character_filter_rects + 1] = all_rect
  ReaADR.set_gfx_color(inside(all_rect, gfx.mouse_x, gfx.mouse_y) and theme.highlight or theme.panel_alt)
  gfx.rect(all_rect.x + 1, all_rect.y + 1, all_rect.w - 2, all_rect.h - 2, true)
  ReaADR.set_gfx_color(all_checked and theme.accent_gold or theme.border)
  gfx.rect(all_rect.x + 8, all_rect.y + 6, 14, 14, all_checked)
  gfx.rect(all_rect.x + 8, all_rect.y + 6, 14, 14, false)
  gfx.setfont(1, "Arial", 13)
  ReaADR.set_gfx_color(theme.text)
  gfx.x = all_rect.x + 30
  gfx.y = all_rect.y + 6
  gfx.drawstr("Show All Character Cues")

  local hovered_submenu_character = nil
  local hovered_submenu_group = nil
  local hovered_group_rect = nil
  for index, group in ipairs(groups) do
    local rect = { x = x, y = y + (index * option_h), w = width, h = option_h, key = "character:" .. group.character, kind = "character", group = group }
    state.character_filter_rects[#state.character_filter_rects + 1] = rect
    local active_count = 0
    for _, target in ipairs(group.targets) do
      if active[target.key] then
        active_count = active_count + 1
      end
    end
    local checked = active_count == #group.targets
    local partial = active_count > 0 and not checked
    local hovered = inside(rect, gfx.mouse_x, gfx.mouse_y)
    if hovered and #group.targets > 1 then
      hovered_submenu_character = group.character
      hovered_submenu_group = group
      hovered_group_rect = rect
    end
    ReaADR.set_gfx_color(inside(rect, gfx.mouse_x, gfx.mouse_y) and theme.highlight or theme.panel_alt)
    gfx.rect(rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, true)
    ReaADR.set_gfx_color((checked or partial) and theme.accent_gold or theme.border)
    gfx.rect(rect.x + 8, rect.y + 6, 14, 14, checked)
    gfx.rect(rect.x + 8, rect.y + 6, 14, 14, false)
    gfx.setfont(1, "Arial", 13)
    ReaADR.set_gfx_color(theme.text)
    gfx.x = rect.x + 30
    gfx.y = rect.y + 6
    gfx.drawstr(fit_filter_text(group.character, rect.w - 38))
  end

  local submenu_group = hovered_submenu_group or (state.character_filter_submenu_character and by_character[state.character_filter_submenu_character])
  local keep_open = false
  if submenu_group and #submenu_group.targets > 1 then
    local submenu_h = option_h * #submenu_group.targets
    local submenu_x = x + width + 6
    local submenu_y = math.max(8, math.min(y + option_h, state.height - submenu_h - 58))
    if submenu_x + submenu_width > state.width - 12 then
      submenu_x = math.max(12, x - submenu_width - 6)
    end
    local corridor_left = math.min(hovered_group_rect and (hovered_group_rect.x + hovered_group_rect.w) or submenu_x, submenu_x)
    local corridor_right = math.max(hovered_group_rect and (hovered_group_rect.x + hovered_group_rect.w) or submenu_x, submenu_x)
    local corridor_top = math.min(hovered_group_rect and hovered_group_rect.y or submenu_y, submenu_y)
    local corridor_bottom = math.max((hovered_group_rect and hovered_group_rect.y or submenu_y) + option_h, submenu_y + submenu_h)
    local mouse_in_corridor = gfx.mouse_x >= corridor_left - 4 and gfx.mouse_x <= corridor_right + 4 and gfx.mouse_y >= corridor_top - 4 and gfx.mouse_y <= corridor_bottom + 4
    keep_open = hovered_submenu_character ~= nil or inside({ x = submenu_x, y = submenu_y, w = submenu_width, h = submenu_h }, gfx.mouse_x, gfx.mouse_y) or mouse_in_corridor
    state.character_filter_submenu_character = keep_open and (hovered_submenu_character or state.character_filter_submenu_character) or nil
    ReaADR.set_gfx_color(theme.panel)
    gfx.rect(submenu_x, submenu_y, submenu_width, submenu_h, true)
    ReaADR.set_gfx_color(theme.border)
    gfx.rect(submenu_x, submenu_y, submenu_width, submenu_h, false)
    for index, target in ipairs(submenu_group.targets) do
      local rect = { x = submenu_x, y = submenu_y + ((index - 1) * option_h), w = submenu_width, h = option_h, key = target.key, kind = "target", target = target }
      state.character_filter_rects[#state.character_filter_rects + 1] = rect
      local checked = active[target.key] == true
      ReaADR.set_gfx_color(inside(rect, gfx.mouse_x, gfx.mouse_y) and theme.highlight or theme.panel_alt)
      gfx.rect(rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, true)
      ReaADR.set_gfx_color(checked and theme.accent_gold or theme.border)
      gfx.rect(rect.x + 8, rect.y + 6, 14, 14, checked)
      gfx.rect(rect.x + 8, rect.y + 6, 14, 14, false)
      gfx.setfont(1, "Arial", 13)
      ReaADR.set_gfx_color(theme.text)
      gfx.x = rect.x + 30
      gfx.y = rect.y + 6
      gfx.drawstr(fit_filter_text(target.label, rect.w - 38))
    end
  else
    state.character_filter_submenu_character = nil
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
  }, { save = false })
  if not cue then
    ReaADR.message("Cue could not be added.")
    return
  end

  local summary, err = ReaADR.commit_session_cues(all_cues, {
    snapshot_label = "Add Cue From Cue Manager",
    undo_description = "ReaADR: add cue",
    save_options = { event_type = "CueCreated", source = "cue_manager", last_operation = "add_cached_cue" },
  })
  if not summary then
    ReaADR.message("Cue was added, but the session refresh failed:\n\n" .. tostring(err))
    refresh_cues()
    return
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
  local gap = 4
  local x = 30
  local fixed = {
    { key = "id", label = "Cue", w = 44 },
    { key = "character", label = "Character", w = 128 },
    { key = "start_time", label = "Start SMPTE", w = 98 },
    { key = "end_time", label = "End SMPTE", w = 98 },
    { key = "status", label = "Status", w = 96 },
    { key = "cue_type", label = "Type", w = 72 },
  }
  local columns = {}
  for _, column in ipairs(fixed) do
    column.x = x
    columns[#columns + 1] = column
    x = x + column.w + gap
  end
  local trailing_w = math.max(360, (22 + content_w - 16) - x)
  local line_w = math.max(190, math.floor(trailing_w * 0.58))
  local notes_w = math.max(150, trailing_w - line_w - gap)
  columns[#columns + 1] = { key = "line", label = "Line", x = x, w = line_w }
  columns[#columns + 1] = { key = "notes", label = "Notes", x = x + line_w + gap, w = notes_w }
  return columns
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

  local removed, err = ReaADR.remove_cached_cue(cue, { select_index = state.selected, renumber = true, save = false })
  if not removed then
    ReaADR.message("Cue remove failed:\n\n" .. tostring(err))
    return
  end

  local summary, rebuild_err = ReaADR.commit_session_cues(removed.cues, {
    snapshot_label = "Remove Cue From Cue Manager",
    undo_description = "ReaADR: remove cue",
    save_options = { event_type = "CueDeleted", source = "cue_manager", last_operation = "remove_cached_cue" },
    after_sync = function()
      return ReaADR.remove_project_artifacts_for_cues({ removed.removed }, { manage_undo = false })
    end,
  })
  if not summary then
    ReaADR.message("Cue was removed, but the session refresh failed:\n\n" .. tostring(rebuild_err))
    refresh_cues()
    return
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
	    ("Session refreshed.\n\nCue regions refreshed: %d\nCue audio created: %d, updated: %d, skipped: %d\nOverlap lanes: %d\nCompleted characters skipped: %d"):format(
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
  if ReaADR.cue_info_panel_is_active and ReaADR.cue_info_panel_is_active() then
    ReaADR.message("Cue Information Panel is already open.\n\nThe open panel has been updated to the selected cue.")
    return
  end
  local path = script_dir() .. "/ReaADR_Cue_Info_Panel.lua"
  local command_id = reaper.AddRemoveReaScript(true, 0, path, true)
  if command_id and command_id > 0 then
    ReaADR.mark_cue_info_panel_active()
    reaper.Main_OnCommand(command_id, 0)
  else
    ReaADR.message("Could not open Cue Information Panel.")
  end
end

local function dock_cue_manager()
  if not gfx or not gfx.dock then
    return
  end
  local ok, dock, x, y = pcall(gfx.dock, -1, 0, 0, 0, 0)
  dock = ok and tonumber(dock) or 0
  if dock == 0 then
    local dock_state, _, should_create_right = ReaADR.right_docker_state()
    gfx.dock(dock_state)
    state.move_docker_right_after_update = should_create_right
  end
  ReaADR.save_window_state("cue_manager")
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
  { key = "sync", label = "Refresh Session", min_w = 134, hint = "Repair generated tracks, cue regions, cue audio, filters, and overlay state." },
  { key = "info", label = "Info Panel", min_w = 112, hint = "Open the large cue information panel for the selected cue." },
}

local function layout_button_row(row_keys, content_w, y)
  local gap = 8
  local button_h = 32
  local by_key = {}
  for _, spec in ipairs(button_specs) do
    by_key[spec.key] = spec
  end
  local buttons = {}
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
  local grow = #row > 0 and math.floor(extra / #row) or 0
  local remainder = extra - (grow * #row)
  local x = 22
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
  return buttons
end

local function layout_buttons(content_w, action_y)
  local buttons = layout_button_row({ "record", "add", "remove", "filter", "sync", "info" }, content_w, action_y)
  local nav = layout_button_row({ "jump", "prev", "next" }, math.min(content_w, 300), state.height - 43)
  for key, rect in pairs(nav) do
    buttons[key] = rect
  end
  return buttons
end

local function draw_footer_background()
  local theme = ReaADR.ui_theme()
  local footer_h = 54
  local y = state.height - footer_h
  ReaADR.set_gfx_color(theme.panel)
  gfx.rect(0, y, state.width, footer_h, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(0, y, state.width, 1, true)
  return y, footer_h
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
    ("Source: %s | Cues: %d"):format(tostring(source or "session"), #cues),
    { x = 22, y = 18, width = state.width - 44, height = 76, right_padding = 100 }
  )
  local dock_button = { x = state.width - 126, y = 32, w = 86, h = 32, label = "Dock", hint = "Dock this Cue Manager window in REAPER." }

  local content_w = state.width - 44
  local action_y = math.max(108, header.content_y)
  local buttons = layout_buttons(content_w, action_y)
  local columns = cell_columns(content_w)
  local footer_y = state.height - 54
  local selected_y = footer_y + 20

  local header_y = action_y + 42
  ReaADR.set_gfx_color(theme.panel_alt)
  gfx.rect(22, header_y, content_w, 28, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(22, header_y, content_w, 28, false)
  gfx.setfont(1, "Arial", 13)
  ReaADR.set_gfx_color(theme.text)
  local header_cells = {}
  for _, column in ipairs(columns) do
    local label = column.label or column.key
    if state.sort_key == column.key then
      label = label .. (state.sort_ascending and " ^" or " v")
    end
    local rect = { x = column.x, y = header_y, w = column.w, h = 28, key = column.key }
    header_cells[#header_cells + 1] = rect
    if inside(rect, gfx.mouse_x, gfx.mouse_y) then
      ReaADR.set_gfx_color(theme.highlight)
      gfx.rect(rect.x, rect.y + 1, rect.w, rect.h - 2, true)
      ReaADR.set_gfx_color(theme.text)
      if column.key == "status" or column.key == "cue_type" then
        hover_hint = "Double-click to change " .. (column.key == "status" and "status." or "type.")
      elseif column.key == "line" then
        hover_hint = "Double-click to edit dialogue."
      elseif column.key == "notes" then
        hover_hint = "Double-click to edit notes."
      end
    end
    gfx.x = column.x
    gfx.y = header_y + 7
    gfx.drawstr(trim_to_width(label, column.w - 4, 13))
  end

  local row_h = 30
  local list_y = header_y + 30
  local visible_rows = math.max(1, math.floor((footer_y - list_y - 8) / row_h))
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
      for _, column in ipairs(columns) do
        local value = trim_cell_text(display_value_for_field(cue, column.key, frame_rate))
        local limit = math.max(8, math.floor((column.w - 8) / 7))
        if #value > limit then
          value = value:sub(1, math.max(1, limit - 3)) .. "..."
        end
        gfx.x = column.x
        gfx.y = y + 7
        gfx.drawstr(value)
      end

      local row_rect = { x = 22, y = y, w = content_w, h = row_h - 2 }
      if inside(row_rect, gfx.mouse_x, gfx.mouse_y) then
        for _, column in ipairs(columns) do
          local cell_rect = { x = column.x, y = row_rect.y, w = column.w, h = row_rect.h }
          if inside(cell_rect, gfx.mouse_x, gfx.mouse_y) then
            local full_value = trim_cell_text(display_value_for_field(cue, column.key, frame_rate))
            if column.key == "status" or column.key == "cue_type" then
              hover_hint = "Double-click to change " .. (column.key == "status" and "status." or "type.")
            elseif column.key == "line" then
              hover_hint = "Double-click to edit dialogue."
            elseif column.key == "notes" then
              hover_hint = "Double-click to edit notes."
            end
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

  draw_footer_background()

  for _, rect in pairs(buttons) do
    if button(rect) and rect.hint then
      hover_hint = rect.hint
    end
  end
  if state.character_filter_open and buttons.filter then
    draw_character_filter_menu(buttons.filter)
  else
    state.character_filter_rects = {}
  end
  if button(dock_button) and dock_button.hint then
    hover_hint = dock_button.hint
  end

  local selected_cue = cues[state.selected]
  local dropdown_options = {}
  if selected_cue then
    local selected_x = math.max(320, (buttons.next and (buttons.next.x + buttons.next.w + 20)) or 320)
    gfx.setfont(1, "Arial", 14)
    ReaADR.set_gfx_color(theme.muted)
    gfx.x = selected_x
    gfx.y = selected_y
    gfx.drawstr(trim_to_width(
      ("Cue %s | %s | %s | %s | %.2fs"):format(
        tostring(selected_cue.id or ""),
        tostring(selected_cue.character or ""),
        tostring(selected_cue.status or "Not Recorded"),
        tostring(selected_cue.cue_type or "Dialogue"),
        ReaADR.cue_duration(selected_cue)
      ),
      math.max(180, state.width - selected_x - 24),
      14
    ))
  end

  if selected_cue and state.dropdown_field and state.dropdown_rect then
    local current_value = state.dropdown_field == "cue_type" and (selected_cue.cue_type or "Dialogue") or (selected_cue.status or "Not Recorded")
    dropdown_options = draw_dropdown(state.dropdown_rect, current_value, state.dropdown_field)
  end

  ReaADR.draw_gfx_tooltip(hover_hint)

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
  if state.move_docker_right_after_update then
    state.move_docker_right_after_update = false
    ReaADR.move_current_docker_right()
  end

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
  elseif ReaADR.handle_gfx_transport_key(char, state.editing ~= nil) then
    reaper.defer(frame)
    return
  elseif state.editing and handle_inline_input(char) then
    reaper.defer(frame)
    return
  elseif char == 30064 then
    if state.editing and not commit_inline_edit() then
      reaper.defer(frame)
      return
    end
    select_relative_cue(-1, true)
    sync_scroll_to_selection(visible_rows)
  elseif char == 1685026670 then
    if state.editing and not commit_inline_edit() then
      reaper.defer(frame)
      return
    end
    select_relative_cue(1, true)
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

    if state.character_filter_open then
      local clicked_filter_option = false
      for _, rect in ipairs(state.character_filter_rects or {}) do
        if inside(rect, gfx.mouse_x, gfx.mouse_y) then
          clicked_filter_option = true
          if rect.key == "__all__" then
            ReaADR.set_active_character_filter({})
            if ReaADR.apply_character_filter then
              ReaADR.apply_character_filter()
            end
            refresh_cues()
          elseif rect.kind == "character" and rect.group then
            local targets, selected = character_filter_options()
            local all_selected = true
            for _, target in ipairs(rect.group.targets or {}) do
              if not selected[target.key] then
                all_selected = false
                break
              end
            end
            for _, target in ipairs(rect.group.targets or {}) do
              selected[target.key] = not all_selected
            end
            if #(rect.group.targets or {}) > 1 then
              state.character_filter_submenu_character = rect.group.character
            end
            apply_character_filter_targets(targets, selected)
          else
            local targets, selected = character_filter_options()
            selected[rect.key] = not selected[rect.key]
            apply_character_filter_targets(targets, selected)
          end
          break
        end
      end
      if clicked_filter_option then
        state.last_mouse = mouse
        reaper.defer(frame)
        return
      elseif buttons.filter and not inside(buttons.filter, gfx.mouse_x, gfx.mouse_y) then
        local inside_menu = false
        for _, rect in ipairs(state.character_filter_rects or {}) do
          if inside(rect, gfx.mouse_x, gfx.mouse_y) then
            inside_menu = true
            break
          end
        end
        if not inside_menu then
          state.character_filter_open = false
          state.character_filter_submenu_character = nil
        end
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

    for _, header_cell in ipairs(header_cells or {}) do
      if inside(header_cell, gfx.mouse_x, gfx.mouse_y) then
        local now = reaper.time_precise()
        local double_click = state.last_header_click_key == header_cell.key and (now - state.last_header_click_time) <= 0.35
        state.last_header_click_key = header_cell.key
        state.last_header_click_time = now
        if double_click then
          local selected = cues[state.selected]
          local selected_key = selected and ReaADR.cue_key(selected) or ""
          if state.sort_key == header_cell.key then
            state.sort_ascending = not state.sort_ascending
          else
            state.sort_key = header_cell.key
            state.sort_ascending = true
          end
          apply_sort()
          local selected_index = cue_index_by_key(selected_key)
          if selected_index then
            state.selected = selected_index
            sync_scroll_to_selection(visible_rows)
          end
        end
        state.last_mouse = mouse
        reaper.defer(frame)
        return
      end
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
      select_relative_cue(-1, true)
    elseif inside(buttons.next, gfx.mouse_x, gfx.mouse_y) then
      select_relative_cue(1, true)
    elseif inside(buttons.add, gfx.mouse_x, gfx.mouse_y) then
      add_cue_from_manager()
    elseif cue and inside(buttons.remove, gfx.mouse_x, gfx.mouse_y) then
      remove_selected_cue()
    elseif inside(buttons.filter, gfx.mouse_x, gfx.mouse_y) then
      state.character_filter_open = not state.character_filter_open
      if not state.character_filter_open then
        state.character_filter_submenu_character = nil
      end
    elseif cue and inside(buttons.record, gfx.mouse_x, gfx.mouse_y) then
      launch_record_cue()
    elseif inside(buttons.sync, gfx.mouse_x, gfx.mouse_y) then
      refresh_session_from_manager()
    elseif cue and inside(buttons.info, gfx.mouse_x, gfx.mouse_y) then
      launch_info_panel()
    elseif inside(dock_button, gfx.mouse_x, gfx.mouse_y) then
      dock_cue_manager()
    end

  end
  state.last_mouse = mouse

  reaper.defer(frame)
end

local auto_dock = ReaADR.cue_manager_auto_dock_enabled()
local dock_once = ReaADR.get_setting("cue_manager_dock_once", "0") == "1"
local should_dock = auto_dock or dock_once
local initial_dock, should_create_right = 0, false
if should_dock then
  initial_dock, _, should_create_right = ReaADR.right_docker_state()
end
local restored = ReaADR.init_persistent_window("cue_manager", "ReaADR Cue Manager", {
  width = state.width,
  height = state.height,
  min_width = state.min_width,
  min_height = state.min_height,
  dock = initial_dock,
  force_dock = should_dock,
  force_float = not should_dock,
})
if should_create_right or ReaADR.get_setting("cue_manager_create_right_docker", "0") == "1" then
  state.move_docker_right_after_update = true
end
ReaADR.set_setting("cue_manager_dock_once", "0")
ReaADR.set_setting("cue_manager_create_right_docker", "0")
state.width = restored.width
state.height = restored.height
frame()
