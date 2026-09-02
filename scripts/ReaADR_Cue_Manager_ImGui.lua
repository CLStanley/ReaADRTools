local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")
local theme = ReaADR.ui_theme()

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

local function imgui_const(name, fallback)
  local fn = reaper["ImGui_" .. name]
  if type(fn) == "function" then
    local ok, value = pcall(fn)
    if ok then
      return value
    end
  end
  return fallback or 0
end

local ctx = reaper.ImGui_CreateContext("ReaADR Cue Manager")
local cond_first_use = imgui_const("Cond_FirstUseEver", 4)
local table_flags =
  imgui_const("TableFlags_Borders", 0) +
  imgui_const("TableFlags_RowBg", 0) +
  imgui_const("TableFlags_Resizable", 0) +
  imgui_const("TableFlags_Reorderable", 0) +
  imgui_const("TableFlags_Sortable", 0) +
  imgui_const("TableFlags_ScrollX", 0) +
  imgui_const("TableFlags_ScrollY", 0) +
  imgui_const("TableFlags_SizingFixedFit", 0)
local table_col_fixed = imgui_const("TableColumnFlags_WidthFixed", 0)
local input_flags = imgui_const("InputTextFlags_EnterReturnsTrue", 0)
local key_space = imgui_const("Key_Space", 0)
local col_window_bg = imgui_const("Col_WindowBg", nil)
local col_header = imgui_const("Col_Header", nil)
local col_header_hovered = imgui_const("Col_HeaderHovered", nil)
local col_button = imgui_const("Col_Button", nil)
local col_button_hovered = imgui_const("Col_ButtonHovered", nil)
local col_frame_bg = imgui_const("Col_FrameBg", nil)
local col_popup_bg = imgui_const("Col_PopupBg", nil)
local col_border = imgui_const("Col_Border", nil)
local col_button_active = imgui_const("Col_ButtonActive", nil)

local state = {
  selected = 1,
  editing = nil,
  session_revision = ReaADR.session_revision and ReaADR.session_revision() or 0,
  filter_signature = select(2, ReaADR.active_character_filter()),
  selected_key = ReaADR.manager_selected_cue_key and ReaADR.manager_selected_cue_key() or "",
  restored = ReaADR.load_window_state("cue_manager", { width = 1180, height = 760, min_width = 900, min_height = 520 }),
  last_poll = 0,
  sort_key = "start_time",
  sort_ascending = true,
  show_column_controls = false,
  show_character_filter = false,
  character_filter_submenu_character = nil,
  close_requested = false,
  column_widths = {
    id = 48,
    character = 150,
    start_time = 112,
    end_time = 112,
    status = 112,
    cue_type = 84,
    line = 420,
    notes = 340,
  },
}

local default_column_widths = {
  id = 48,
  character = 150,
  start_time = 112,
  end_time = 112,
  status = 112,
  cue_type = 84,
  line = 420,
  notes = 340,
}

local logo_texture = nil
local logo_attempted = false

local function logo_path()
  return script_dir() .. "/../assets/logo.png"
end

local function ensure_logo_texture()
  if logo_attempted then
    return logo_texture
  end
  logo_attempted = true
  if type(reaper.ImGui_CreateImageFromFile) == "function" then
    local ok, image = pcall(reaper.ImGui_CreateImageFromFile, logo_path())
    if ok then
      logo_texture = image
      return logo_texture
    end
    local ok_ctx, image_ctx = pcall(reaper.ImGui_CreateImageFromFile, ctx, logo_path())
    if ok_ctx then
      logo_texture = image_ctx
    end
  end
  return logo_texture
end

local function push_theme()
  if type(reaper.ImGui_PushStyleColor) ~= "function" then
    return 0
  end
  local pushed = 0
  local function push(idx, rgba)
    if idx ~= nil then
      reaper.ImGui_PushStyleColor(ctx, idx, rgba[1], rgba[2], rgba[3], rgba[4] or 1)
      pushed = pushed + 1
    end
  end
  push(col_window_bg, theme.bg)
  push(col_header, theme.panel_alt)
  push(col_header_hovered, theme.highlight)
  push(col_button, theme.panel_alt)
  push(col_button_hovered, theme.highlight)
  push(col_frame_bg, theme.panel)
  push(col_popup_bg, theme.panel)
  push(col_border, theme.border)
  return pushed
end

local function pop_theme(count)
  if count and count > 0 and type(reaper.ImGui_PopStyleColor) == "function" then
    reaper.ImGui_PopStyleColor(ctx, count)
  end
end

local function setup_table_column(label, width)
  local ok = pcall(reaper.ImGui_TableSetupColumn, ctx, label, table_col_fixed, width)
  if not ok then
    reaper.ImGui_TableSetupColumn(ctx, label)
  end
end

local function column_width(key, fallback)
  return math.max(44, tonumber(state.column_widths[key]) or tonumber(fallback) or 100)
end

local function adjust_column_width(key, delta)
  state.column_widths[key] = math.max(44, math.min(900, column_width(key, default_column_widths[key]) + delta))
end

local function delayed_imgui_tooltip(text)
  if not text or text == "" or type(reaper.ImGui_SetTooltip) ~= "function" then
    state.tooltip = nil
    return
  end
  if ReaADR.tooltips_enabled and not ReaADR.tooltips_enabled() then
    state.tooltip = nil
    return
  end
  local now = reaper.time_precise and reaper.time_precise() or os.clock()
  text = tostring(text)
  if not state.tooltip or state.tooltip.text ~= text then
    state.tooltip = { text = text, started = now }
    return
  end
  if now - state.tooltip.started >= 2.0 then
    reaper.ImGui_SetTooltip(ctx, text)
  end
end

local function draw_column_controls()
  if not state.show_column_controls then
    return
  end
  local columns = {
    { key = "id", label = "Cue" },
    { key = "character", label = "Character" },
    { key = "start_time", label = "Start" },
    { key = "end_time", label = "End" },
    { key = "status", label = "Status" },
    { key = "cue_type", label = "Type" },
    { key = "line", label = "Line" },
    { key = "notes", label = "Notes" },
  }
  reaper.ImGui_Separator(ctx)
  reaper.ImGui_Text(ctx, "Column widths")
  reaper.ImGui_SameLine(ctx)
  if reaper.ImGui_Button(ctx, "Reset Columns") then
    for key, width in pairs(default_column_widths) do
      state.column_widths[key] = width
    end
  end
  for index, column in ipairs(columns) do
    if index > 1 then
      reaper.ImGui_SameLine(ctx)
    end
    reaper.ImGui_Text(ctx, column.label)
    reaper.ImGui_SameLine(ctx)
    if reaper.ImGui_Button(ctx, "-##col_" .. column.key) then
      adjust_column_width(column.key, -24)
    end
    reaper.ImGui_SameLine(ctx)
    reaper.ImGui_Text(ctx, tostring(math.floor(column_width(column.key, default_column_widths[column.key]))))
    reaper.ImGui_SameLine(ctx)
    if reaper.ImGui_Button(ctx, "+##col_" .. column.key) then
      adjust_column_width(column.key, 24)
    end
    if index == 4 then
      reaper.ImGui_NewLine(ctx)
    end
  end
  reaper.ImGui_Separator(ctx)
end

local set_sort

local function render_sort_header(label, key)
  local text = label
  if state.sort_key == key then
    text = text .. (state.sort_ascending and " ^" or " v")
  end
  reaper.ImGui_Selectable(ctx, text .. "##header_" .. key, false)
  if reaper.ImGui_IsItemHovered(ctx) and reaper.ImGui_IsMouseDoubleClicked(ctx, 0) then
    if set_sort then
      set_sort(key)
    end
  end
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

function set_sort(key)
  local selected = cues[state.selected]
  local selected_key = selected and ReaADR.cue_key(selected) or ""
  if state.sort_key == key then
    state.sort_ascending = not state.sort_ascending
  else
    state.sort_key = key
    state.sort_ascending = true
  end
  apply_sort()
  local index = cue_index_by_key(selected_key)
  if index then
    state.selected = index
  end
end

local selected_key = ReaADR.selected_region_cue_key()
if selected_key == "" then
  selected_key = ReaADR.manager_selected_cue_key()
end
apply_sort()
state.selected = cue_index_by_key(selected_key) or cue_index_at_position(ReaADR.current_timeline_position()) or 1

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
    end
    state.selected_key = selected_key
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
  local cue = cues[index]
  state.selected_key = ReaADR.cue_key(cue)
  ReaADR.set_manager_selected_cue(cue)
  if jump then
    ReaADR.jump_to_cue(cue)
  end
  ReaADR.refresh_overlay_silent()
end

local function set_status_for_selected(status)
  local cue = cues[state.selected]
  if not cue or not status then
    return
  end
  ReaADR.set_cue_status_at_position(status, cue.start_time)
  refresh_cues()
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
  local selected = cues[state.selected]
  ReaADR.set_manager_selected_cue(selected)
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

local function field_uses_dropdown(field_key)
  return field_key == "status" or field_key == "cue_type"
end

local function dropdown_choices_for_field(field_key)
  if field_key == "status" then
    return ReaADR.cue_statuses()
  elseif field_key == "cue_type" then
    return ReaADR.cue_types()
  end
  return {}
end

local function shorten_text(text, max_chars)
  text = tostring(text or "")
  if #text <= max_chars then
    return text
  end
  return text:sub(1, math.max(1, max_chars - 3)) .. "..."
end

local function begin_inline_edit(cue, field_key, frame_rate)
  state.editing = {
    cue_key = ReaADR.cue_key(cue),
    field_key = field_key,
    value = display_value_for_field(cue, field_key, frame_rate),
    focus = true,
  }
end

local function commit_inline_edit()
  local editing = state.editing
  if not editing then
    return true
  end

  local cue_index = cue_index_by_key(editing.cue_key)
  local cue = cue_index and cues[cue_index] or nil
  if not cue then
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
    if not frame_rate or frame_rate <= 0 then frame_rate = 24 end
    local parsed_start, start_error = ReaADR.parse_timecode(editing.value, frame_rate)
    if not parsed_start then
      ReaADR.message("Cue update failed:\n\nStart time is invalid: " .. tostring(start_error))
      return false
    end
    updated.start_time = parsed_start
  elseif editing.field_key == "end_time" then
    local frame_rate = reaper.TimeMap_curFrameRate(0)
    if not frame_rate or frame_rate <= 0 then frame_rate = 24 end
    local parsed_end, end_error = ReaADR.parse_timecode(editing.value, frame_rate)
    if not parsed_end then
      ReaADR.message("Cue update failed:\n\nEnd time is invalid: " .. tostring(end_error))
      return false
    end
    updated.end_time = parsed_end
  elseif editing.field_key == "cue_type" then
    updated.cue_type = editing.value
  elseif editing.field_key == "line" then
    updated.line = editing.value
  elseif editing.field_key == "notes" then
    updated.notes = editing.value
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
  end
  return true
end

local function render_text_editor(width)
  local editing = state.editing
  if not editing then return end
  if editing.focus then
    reaper.ImGui_SetKeyboardFocusHere(ctx)
    editing.focus = false
  end
  reaper.ImGui_PushItemWidth(ctx, width)
  local enter_pressed, new_value = reaper.ImGui_InputText(ctx, "##cell", tostring(editing.value or ""), input_flags)
  reaper.ImGui_PopItemWidth(ctx)
  editing.value = new_value
  if enter_pressed then
    commit_inline_edit()
    return
  end
  local deactivated = type(reaper.ImGui_IsItemDeactivatedAfterEdit) == "function"
    and reaper.ImGui_IsItemDeactivatedAfterEdit(ctx)
  if deactivated then
    commit_inline_edit()
  end
end

-- Render an inline dropdown (combo) for status or cue_type.
-- On first call for a given cell the combo is opened immediately; closing without
-- a selection dismisses the edit.
local function render_dropdown_cell(cue, field_key, current_value, width)
  local cue_key    = ReaADR.cue_key(cue)
  local is_editing = state.editing
    and state.editing.cue_key   == cue_key
    and state.editing.field_key == field_key

  if is_editing then
    reaper.ImGui_PushItemWidth(ctx, width)
    local combo_id = "##dd_" .. cue_key .. "." .. field_key
    if state.editing.open_next then
      reaper.ImGui_OpenPopup(ctx, combo_id)
      state.editing.open_next = false
    end
    local open, _ = reaper.ImGui_BeginCombo(ctx, combo_id, current_value)
    reaper.ImGui_PopItemWidth(ctx)
    if open then
      for _, choice in ipairs(dropdown_choices_for_field(field_key)) do
        local is_selected = choice == current_value
        if reaper.ImGui_Selectable(ctx, choice, is_selected) then
          -- Commit the new value
          state.editing.value = choice
          if field_key == "status" then
            set_status_for_selected(choice)
          else
            commit_inline_edit()
          end
          state.editing = nil
        end
        if is_selected and type(reaper.ImGui_SetItemDefaultFocus) == "function" then
          reaper.ImGui_SetItemDefaultFocus(ctx)
        end
      end
      reaper.ImGui_EndCombo(ctx)
    else
      -- Popup closed without a selection
      state.editing = nil
    end
    return
  end

  local is_row_selected = cues[state.selected] and ReaADR.cue_key(cues[state.selected]) == cue_key
  if reaper.ImGui_Selectable(ctx, current_value .. "##" .. cue_key .. "." .. field_key, is_row_selected) then
    select_cue(cue_index_by_key(cue_key) or state.selected, true)
  end
  local hovered = reaper.ImGui_IsItemHovered(ctx)
  if hovered then
    delayed_imgui_tooltip("Double-click to change " .. (field_key == "status" and "status." or "type."))
  end
  if hovered and reaper.ImGui_IsMouseDoubleClicked(ctx, 0) then
    state.editing = {
      cue_key   = cue_key,
      field_key = field_key,
      value     = current_value,
      open_next = true,
    }
  end
end

local function render_cell(cue, field_key, text, full_text, width, frame_rate)
  if field_uses_dropdown(field_key) then
    render_dropdown_cell(cue, field_key, text, width)
    return
  end

  local cue_key = ReaADR.cue_key(cue)
  local editing = state.editing and state.editing.cue_key == cue_key and state.editing.field_key == field_key
  if editing then
    render_text_editor(width)
    return
  end

  local selected = cues[state.selected] and ReaADR.cue_key(cues[state.selected]) == cue_key
  if reaper.ImGui_Selectable(ctx, text .. "##" .. cue_key .. "." .. field_key, selected) then
    select_cue(cue_index_by_key(cue_key) or state.selected, true)
  end
  local hovered = reaper.ImGui_IsItemHovered(ctx)
  if hovered and full_text ~= "" and full_text ~= text then
    delayed_imgui_tooltip(full_text)
  elseif hovered and (field_key == "line" or field_key == "notes") then
    delayed_imgui_tooltip("Double-click to edit " .. (field_key == "line" and "dialogue." or "notes."))
  elseif not hovered then
    state.tooltip = nil
  end
  if hovered and reaper.ImGui_IsMouseDoubleClicked(ctx, 0) then
    begin_inline_edit(cue, field_key, frame_rate)
  end
end

local function save_geometry_if_enabled()
  if not ReaADR.window_layout_enabled() then
    return
  end
  local x, y = reaper.ImGui_GetWindowPos(ctx)
  local width, height = reaper.ImGui_GetWindowSize(ctx)
  ReaADR.save_window_geometry("cue_manager", {
    x = x,
    y = y,
    width = width,
    height = height,
    dock = 0,
  })
end

local function launch_docked_cue_manager()
  local cue = cues[state.selected]
  if cue then
    ReaADR.set_manager_selected_cue(cue)
  end
  local dock_state, _, should_create_right = ReaADR.right_docker_state()
  ReaADR.save_window_geometry("cue_manager", {
    dock = dock_state,
    width = 1180,
    height = 760,
  })
  ReaADR.set_setting("cue_manager_dock_once", "1")
  ReaADR.set_setting("cue_manager_create_right_docker", should_create_right and "1" or "0")
  local path = script_dir() .. "/ReaADR_Cue_Manager_Gfx.lua"
  local command_id = reaper.AddRemoveReaScript(true, 0, path, true)
  if command_id and command_id > 0 then
    reaper.Main_OnCommand(command_id, 0)
  else
    dofile(path)
  end
  state.close_requested = true
end

local function handle_imgui_transport_key()
  if state.editing or (tonumber(key_space) or 0) <= 0 or type(reaper.ImGui_IsKeyPressed) ~= "function" then
    return false
  end
  local ok, pressed = pcall(reaper.ImGui_IsKeyPressed, ctx, key_space, false)
  if ok and pressed then
    reaper.Main_OnCommand(40044, 0)
    return true
  end
  return false
end

local function content_region_avail_height(fallback)
  if type(reaper.ImGui_GetContentRegionAvail) ~= "function" then
    return fallback
  end
  local ok, _, height = pcall(reaper.ImGui_GetContentRegionAvail, ctx)
  if ok and tonumber(height) then
    return height
  end
  return fallback
end

local function draw_manager_footer()
  local selected_cue = cues[state.selected]
  reaper.ImGui_Separator(ctx)
  if reaper.ImGui_Button(ctx, "Jump...") then
    prompt_jump_to_cue()
  end
  reaper.ImGui_SameLine(ctx)
  if reaper.ImGui_Button(ctx, "Previous") then
    select_cue(math.max(1, state.selected - 1), true)
  end
  reaper.ImGui_SameLine(ctx)
  if reaper.ImGui_Button(ctx, "Next") then
    select_cue(math.min(#cues, state.selected + 1), true)
  end
  local spaced = pcall(reaper.ImGui_SameLine, ctx, 0, 24)
  if not spaced then
    reaper.ImGui_SameLine(ctx)
  end
  if selected_cue then
    reaper.ImGui_Text(ctx, ("Cue %s  |  %s  |  %s  |  %.2fs"):format(
      tostring(selected_cue.id or ""),
      tostring(selected_cue.character or ""),
      tostring(selected_cue.status or "Not Recorded"),
      ReaADR.cue_duration(selected_cue)
    ))
  else
    reaper.ImGui_Text(ctx, "No cue selected")
  end
end

local function draw_action_bar()
  if col_button ~= nil then
    reaper.ImGui_PushStyleColor(ctx, col_button, 0.42, 0.20, 0.16, 1.0)
  end
  if col_button_hovered ~= nil then
    reaper.ImGui_PushStyleColor(ctx, col_button_hovered, 0.55, 0.24, 0.19, 1.0)
  end
  if col_button_active ~= nil then
    reaper.ImGui_PushStyleColor(ctx, col_button_active, 0.62, 0.28, 0.22, 1.0)
  end
  if reaper.ImGui_Button(ctx, "Record Current Cue") then
    launch_record_cue()
  end
  if col_button_active ~= nil then
    reaper.ImGui_PopStyleColor(ctx)
  end
  if col_button_hovered ~= nil then
    reaper.ImGui_PopStyleColor(ctx)
  end
  if col_button ~= nil then
    reaper.ImGui_PopStyleColor(ctx)
  end
  reaper.ImGui_SameLine(ctx)
  if reaper.ImGui_Button(ctx, "Add Cue") then
    add_cue_from_manager()
  end
  reaper.ImGui_SameLine(ctx)
  if reaper.ImGui_Button(ctx, "Remove Cue") then
    remove_selected_cue()
  end
  reaper.ImGui_SameLine(ctx)
  if reaper.ImGui_Button(ctx, "Character Filter") then
    state.show_character_filter = not state.show_character_filter
    if not state.show_character_filter then
      state.character_filter_submenu_character = nil
    end
  end
  reaper.ImGui_SameLine(ctx)
  if reaper.ImGui_Button(ctx, "Update Cues From Regions") then
    local progress = ReaADR.create_progress_window("Updating Cues From Regions")
    local summary, err = ReaADR.update_session_cues_from_regions({ on_progress = progress.update })
    progress.close()
    if not summary then
      ReaADR.message("Update failed:\n\n" .. tostring(err))
    else
      refresh_cues()
      ReaADR.message(("Updated %d cue(s) from current region timing."):format(summary.changed_cues or 0))
    end
  end
  reaper.ImGui_SameLine(ctx)
  if reaper.ImGui_Button(ctx, "Refresh Session") then
    local pre_selected = cues[state.selected]
    local pre_key = pre_selected and ReaADR.cue_key(pre_selected) or ""
    local drift = ReaADR.detect_session_drift and ReaADR.detect_session_drift({ cues = all_cues })
    local proceed = true
    if drift and (drift.modified_regions or 0) > 0 then
      proceed = reaper.ShowMessageBox(
        ("Detected %d cue region(s) whose timing differs from the saved session.\n\nRefresh Session will overwrite those moved regions. Use Update Cues From Regions first if the moved regions are correct.\n\nContinue with Refresh Session?"):format(drift.modified_regions or 0),
        "ReaADR Refresh Session",
        4
      ) == 6
    end
    local summary, err = nil, nil
    if proceed then
      summary, err = ReaADR.refresh_session()
    end
    if not summary then
      if proceed then
        ReaADR.message("Refresh failed:\n\n" .. tostring(err))
      end
    else
      refresh_cues()
      local index = cue_index_by_key(pre_key)
      if index then state.selected = index end
      ReaADR.refresh_overlay_silent()
      ReaADR.message(
        ("Session refreshed.\n\nCue regions refreshed: %d\nCue audio created: %d, updated: %d"):format(
          summary.sync.updated or 0,
          summary.rebuild.cue_audio_created or 0,
          summary.rebuild.cue_audio_updated or 0
        )
      )
    end
  end
  reaper.ImGui_SameLine(ctx)
  if reaper.ImGui_Button(ctx, "Info Panel") then
    launch_info_panel()
  end
  reaper.ImGui_SameLine(ctx)
  if reaper.ImGui_Button(ctx, state.show_column_controls and "Hide Columns" or "Columns") then
    state.show_column_controls = not state.show_column_controls
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

local function draw_character_filter_menu()
  if not state.show_character_filter then
    return
  end
  local targets, active = character_filter_options()
  local groups, by_character = grouped_filter_targets(targets)
  reaper.ImGui_Separator(ctx)
  local all_selected = not ReaADR.character_filter_enabled()
  if type(reaper.ImGui_BeginGroup) == "function" then
    reaper.ImGui_BeginGroup(ctx)
  end
  if reaper.ImGui_Selectable(ctx, (all_selected and "[x] " or "[ ] ") .. "Show All Character Cues##filter_all", all_selected) then
    ReaADR.set_active_character_filter({})
    if ReaADR.apply_character_filter then
      ReaADR.apply_character_filter()
    end
    refresh_cues()
  end
  for _, group in ipairs(groups) do
    local active_count = 0
    for _, target in ipairs(group.targets) do
      if active[target.key] then
        active_count = active_count + 1
      end
    end
    local checked = active_count == #group.targets
    local partial = active_count > 0 and not checked
    local prefix = checked and "[x] " or (partial and "[-] " or "[ ] ")
    local label = prefix .. tostring(group.character) .. (#group.targets > 1 and " >" or "") .. "##filter_character_" .. tostring(group.character)
    if reaper.ImGui_Selectable(ctx, label, checked) then
      for _, target in ipairs(group.targets) do
        active[target.key] = not checked
      end
      if #group.targets > 1 then
        state.character_filter_submenu_character = group.character
      end
      apply_character_filter_targets(targets, active)
    end
    if #group.targets > 1 and type(reaper.ImGui_IsItemHovered) == "function" and reaper.ImGui_IsItemHovered(ctx) then
      state.character_filter_submenu_character = group.character
    end
  end
  if type(reaper.ImGui_EndGroup) == "function" then
    reaper.ImGui_EndGroup(ctx)
  end

  local submenu_group = state.character_filter_submenu_character and by_character[state.character_filter_submenu_character]
  if submenu_group and #submenu_group.targets > 1 then
    local spaced = pcall(reaper.ImGui_SameLine, ctx, 0, 24)
    if not spaced then
      reaper.ImGui_SameLine(ctx)
    end
    if type(reaper.ImGui_BeginGroup) == "function" then
      reaper.ImGui_BeginGroup(ctx)
    end
    reaper.ImGui_Text(ctx, "Character Instances")
    for _, target in ipairs(submenu_group.targets) do
      local checked = active[target.key] == true
      if reaper.ImGui_Selectable(ctx, (checked and "[x] " or "[ ] ") .. tostring(target.label) .. "##filter_target_" .. tostring(target.key), checked) then
        active[target.key] = not checked
        apply_character_filter_targets(targets, active)
      end
    end
    if type(reaper.ImGui_EndGroup) == "function" then
      reaper.ImGui_EndGroup(ctx)
    end
  end
  reaper.ImGui_Separator(ctx)
end

local function loop()
  maybe_refresh_external_changes()
  local style_count = push_theme()

  reaper.ImGui_SetNextWindowSize(ctx, state.restored.width, state.restored.height, cond_first_use)
  if type(reaper.ImGui_SetNextWindowSizeConstraints) == "function" then
    reaper.ImGui_SetNextWindowSizeConstraints(ctx, 900, 520, 8192, 8192)
  end
  if state.restored.x and state.restored.y then
    reaper.ImGui_SetNextWindowPos(ctx, state.restored.x, state.restored.y, cond_first_use)
  end

  local visible, open = reaper.ImGui_Begin(ctx, "ReaADR Cue Manager", true)
  if visible then
    handle_imgui_transport_key()
    save_geometry_if_enabled()

    local frame_rate = reaper.TimeMap_curFrameRate(0)
    if not frame_rate or frame_rate <= 0 then
      frame_rate = 24
    end

    local texture = ensure_logo_texture()
    if texture and type(reaper.ImGui_Image) == "function" then
      pcall(reaper.ImGui_Image, ctx, texture, 52, 52)
      reaper.ImGui_SameLine(ctx)
    end
    reaper.ImGui_Text(ctx, ("Source: %s"):format(tostring(source or "session")))
    reaper.ImGui_SameLine(ctx)
    reaper.ImGui_Text(ctx, ("Cues: %d"):format(#cues))
    reaper.ImGui_SameLine(ctx)
    if reaper.ImGui_Button(ctx, "Dock") then
      launch_docked_cue_manager()
    end

    draw_column_controls()
    draw_action_bar()
    draw_character_filter_menu()

    local table_height = math.max(48, content_region_avail_height(520) - 92)
    if reaper.ImGui_BeginTable(ctx, "cue_table", 8, table_flags, 0, table_height) then
      setup_table_column("Cue", column_width("id", 48))
      setup_table_column("Character", column_width("character", 150))
      setup_table_column("Start SMPTE", column_width("start_time", 112))
      setup_table_column("End SMPTE", column_width("end_time", 112))
      setup_table_column("Status", column_width("status", 112))
      setup_table_column("Type", column_width("cue_type", 84))
      setup_table_column("Line", column_width("line", 420))
      setup_table_column("Notes", column_width("notes", 340))
      reaper.ImGui_TableNextRow(ctx)
      reaper.ImGui_TableSetColumnIndex(ctx, 0)
      render_sort_header("Cue", "id")
      reaper.ImGui_TableSetColumnIndex(ctx, 1)
      render_sort_header("Character", "character")
      reaper.ImGui_TableSetColumnIndex(ctx, 2)
      render_sort_header("Start SMPTE", "start_time")
      reaper.ImGui_TableSetColumnIndex(ctx, 3)
      render_sort_header("End SMPTE", "end_time")
      reaper.ImGui_TableSetColumnIndex(ctx, 4)
      render_sort_header("Status", "status")
      reaper.ImGui_TableSetColumnIndex(ctx, 5)
      render_sort_header("Type", "cue_type")
      reaper.ImGui_TableSetColumnIndex(ctx, 6)
      render_sort_header("Line", "line")
      reaper.ImGui_TableSetColumnIndex(ctx, 7)
      render_sort_header("Notes", "notes")

      for index, cue in ipairs(cues) do
        reaper.ImGui_TableNextRow(ctx)
        reaper.ImGui_TableSetColumnIndex(ctx, 0)
        render_cell(cue, "id",         display_value_for_field(cue, "id",         frame_rate), display_value_for_field(cue, "id",         frame_rate), column_width("id", 48) - 8,  frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 1)
        render_cell(cue, "character",  display_value_for_field(cue, "character",  frame_rate), display_value_for_field(cue, "character",  frame_rate), column_width("character", 150) - 8, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 2)
        render_cell(cue, "start_time", display_value_for_field(cue, "start_time", frame_rate), display_value_for_field(cue, "start_time", frame_rate), column_width("start_time", 112) - 8, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 3)
        render_cell(cue, "end_time",   display_value_for_field(cue, "end_time",   frame_rate), display_value_for_field(cue, "end_time",   frame_rate), column_width("end_time", 112) - 8, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 4)
        render_cell(cue, "status",     display_value_for_field(cue, "status",     frame_rate), display_value_for_field(cue, "status",     frame_rate), column_width("status", 112) - 8, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 5)
        render_cell(cue, "cue_type",   display_value_for_field(cue, "cue_type",   frame_rate), display_value_for_field(cue, "cue_type",   frame_rate), column_width("cue_type", 84) - 8, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 6)
        render_cell(cue, "line",  shorten_text(display_value_for_field(cue, "line",  frame_rate), 72), display_value_for_field(cue, "line",  frame_rate), column_width("line", 420) - 8, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 7)
        render_cell(cue, "notes", shorten_text(display_value_for_field(cue, "notes", frame_rate), 60), display_value_for_field(cue, "notes", frame_rate), column_width("notes", 340) - 8, frame_rate)

        if index == state.selected then
          ReaADR.set_manager_selected_cue(cue)
        end
      end
      reaper.ImGui_EndTable(ctx)
    end
    draw_manager_footer()
  end
  reaper.ImGui_End(ctx)
  pop_theme(style_count)

  if open and not state.close_requested then
    reaper.defer(loop)
  else
    if not state.close_requested then
      save_geometry_if_enabled()
    end
    reaper.ImGui_DestroyContext(ctx)
  end
end

reaper.defer(loop)
