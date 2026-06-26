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
  imgui_const("TableFlags_ScrollY", 0) +
  imgui_const("TableFlags_SizingStretchProp", 0)
local input_flags = imgui_const("InputTextFlags_EnterReturnsTrue", 0)
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
  restored = ReaADR.load_window_state("cue_manager", { width = 1180, height = 760 }),
  last_poll = 0,
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

local selected_key = ReaADR.selected_region_cue_key()
if selected_key == "" then
  selected_key = ReaADR.manager_selected_cue_key()
end
state.selected = cue_index_by_key(selected_key) or cue_index_at_position(ReaADR.current_timeline_position()) or 1

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
  local cue = cues[index]
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
  local snapshot = ReaADR.create_session_snapshot("Add Cue From Cue Manager")
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
  local cue = ReaADR.add_cached_cue({
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

  local summary, err = ReaADR.sync_full({})
  if not summary then
    ReaADR.restore_session_snapshot(snapshot, "Add cue rebuild failed: " .. tostring(err))
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

  local snapshot = ReaADR.create_session_snapshot("Remove Cue From Cue Manager")
  local removed, err = ReaADR.remove_cached_cue(cue, { select_index = state.selected, renumber = true })
  if not removed then
    ReaADR.message("Cue remove failed:\n\n" .. tostring(err))
    return
  end

  local summary, rebuild_err = ReaADR.sync_full({})
  if not summary then
    ReaADR.restore_session_snapshot(snapshot, "Remove cue rebuild failed: " .. tostring(rebuild_err))
    ReaADR.message("Cue was removed, but the session refresh failed:\n\n" .. tostring(rebuild_err))
    refresh_cues()
    return
  end
  ReaADR.remove_project_artifacts_for_cues({ removed.removed })

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
  local path = script_dir() .. "/ReaADR_Cue_Info_Panel.lua"
  local command_id = reaper.AddRemoveReaScript(true, 0, path, true)
  if command_id and command_id > 0 then
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
  if reaper.ImGui_IsItemHovered(ctx) and reaper.ImGui_IsMouseDoubleClicked(ctx, 0) then
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
  if hovered and full_text ~= "" and full_text ~= text and type(reaper.ImGui_SetTooltip) == "function" then
    reaper.ImGui_SetTooltip(ctx, full_text)
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

local function loop()
  maybe_refresh_external_changes()
  local style_count = push_theme()

  reaper.ImGui_SetNextWindowSize(ctx, state.restored.width, state.restored.height, cond_first_use)
  if state.restored.x and state.restored.y then
    reaper.ImGui_SetNextWindowPos(ctx, state.restored.x, state.restored.y, cond_first_use)
  end

  local visible, open = reaper.ImGui_Begin(ctx, "ReaADR Cue Manager", true)
  if visible then
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
    reaper.ImGui_TextDisabled(ctx, "Double-click any cell to edit inline.")

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
    reaper.ImGui_SameLine(ctx)
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
      launch_character_filter()
    end
    reaper.ImGui_SameLine(ctx)
    if reaper.ImGui_Button(ctx, "Refresh Session") then
      local pre_selected = cues[state.selected]
      local pre_key = pre_selected and ReaADR.cue_key(pre_selected) or ""
      local summary, err = ReaADR.refresh_session()
      if not summary then
        ReaADR.message("Refresh failed:\n\n" .. tostring(err))
      else
        refresh_cues()
        local index = cue_index_by_key(pre_key)
        if index then state.selected = index end
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
    if reaper.ImGui_Button(ctx, "Refresh Overlay") then
      ReaADR.refresh_overlay_silent()
    end
    reaper.ImGui_SameLine(ctx)
    if reaper.ImGui_Button(ctx, "Info Panel") then
      launch_info_panel()
    end

    local selected_cue = cues[state.selected]
    if selected_cue then
      reaper.ImGui_Separator(ctx)
      reaper.ImGui_Text(ctx, ("Cue %s  |  %s  |  %s  |  %.2fs  |  Double-click cells to edit"):format(
        tostring(selected_cue.id or ""),
        tostring(selected_cue.character or ""),
        tostring(selected_cue.status or "Not Recorded"),
        ReaADR.cue_duration(selected_cue)
      ))
    end

    local table_height = -40
    if reaper.ImGui_BeginTable(ctx, "cue_table", 8, table_flags, 0, table_height) then
      reaper.ImGui_TableSetupColumn(ctx, "Cue")
      reaper.ImGui_TableSetupColumn(ctx, "Character")
      reaper.ImGui_TableSetupColumn(ctx, "Start SMPTE")
      reaper.ImGui_TableSetupColumn(ctx, "End SMPTE")
      reaper.ImGui_TableSetupColumn(ctx, "Status")
      reaper.ImGui_TableSetupColumn(ctx, "Type")
      reaper.ImGui_TableSetupColumn(ctx, "Line")
      reaper.ImGui_TableSetupColumn(ctx, "Notes")
      reaper.ImGui_TableHeadersRow(ctx)

      for index, cue in ipairs(cues) do
        reaper.ImGui_TableNextRow(ctx)
        reaper.ImGui_TableSetColumnIndex(ctx, 0)
        render_cell(cue, "id",         display_value_for_field(cue, "id",         frame_rate), display_value_for_field(cue, "id",         frame_rate), 56,  frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 1)
        render_cell(cue, "character",  display_value_for_field(cue, "character",  frame_rate), display_value_for_field(cue, "character",  frame_rate), 120, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 2)
        render_cell(cue, "start_time", display_value_for_field(cue, "start_time", frame_rate), display_value_for_field(cue, "start_time", frame_rate), 118, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 3)
        render_cell(cue, "end_time",   display_value_for_field(cue, "end_time",   frame_rate), display_value_for_field(cue, "end_time",   frame_rate), 118, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 4)
        render_cell(cue, "status",     display_value_for_field(cue, "status",     frame_rate), display_value_for_field(cue, "status",     frame_rate), 126, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 5)
        render_cell(cue, "cue_type",   display_value_for_field(cue, "cue_type",   frame_rate), display_value_for_field(cue, "cue_type",   frame_rate), 104, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 6)
        render_cell(cue, "line",  shorten_text(display_value_for_field(cue, "line",  frame_rate), 72), display_value_for_field(cue, "line",  frame_rate), 300, frame_rate)
        reaper.ImGui_TableSetColumnIndex(ctx, 7)
        render_cell(cue, "notes", shorten_text(display_value_for_field(cue, "notes", frame_rate), 60), display_value_for_field(cue, "notes", frame_rate), 260, frame_rate)

        if index == state.selected then
          ReaADR.set_manager_selected_cue(cue)
        end
      end
      reaper.ImGui_EndTable(ctx)
    end
  end
  reaper.ImGui_End(ctx)
  pop_theme(style_count)

  if open then
    reaper.defer(loop)
  else
    save_geometry_if_enabled()
    reaper.ImGui_DestroyContext(ctx)
  end
end

reaper.defer(loop)
