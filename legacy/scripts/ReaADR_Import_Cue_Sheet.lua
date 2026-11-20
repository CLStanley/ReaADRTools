-- Import an ADR cue sheet with script identity, selective character import,
-- duplicate protection, and revision-aware update behavior.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")
local overlay_settings = ReaADR.load_overlay_settings()
local import_preroll_seconds = 3

local function split_character_list(value)
  local result = {}
  value = tostring(value or "")
  local separator = value:find(";", 1, true) and ";" or ","
  local pattern = separator == ";" and "([^;]+)" or "([^,]+)"
  for token in value:gmatch(pattern) do
    token = tostring(token or ""):match("^%s*(.-)%s*$")
    if token ~= "" then
      result[#result + 1] = token
    end
  end
  return result
end

local function join_sorted_keys(map)
  local keys = {}
  for key in pairs(map or {}) do
    keys[#keys + 1] = key
  end
  table.sort(keys)
  return keys
end

local function parse_import_mode(value)
  value = tostring(value or ""):lower():match("^%s*(.-)%s*$")
  if value == "3" or value == "update" or value == "update existing import" or value == "update already imported characters" then
    return "update"
  end
  if value == "2" or value == "selected" or value == "import selected characters" or value == "add selected characters" then
    return "selected"
  end
  return "all"
end

local function mapping_value(mapping, field)
  return tostring((mapping or {})[field] or "")
end

local function merge_mapping(primary, fallback)
  local merged = {}
  for key, value in pairs(fallback or {}) do
    merged[key] = value
  end
  for key, value in pairs(primary or {}) do
    if tostring(value or "") ~= "" then
      merged[key] = value
    end
  end
  return merged
end

local function prompt_column_mapping(details)
  local headers = table.concat(details.headers or {}, ", ")
  local default_mapping = merge_mapping(details.mapping, ReaADR.load_column_mapping_preset("last"))
  local defaults = table.concat({
    mapping_value(default_mapping, "cue_id"),
    mapping_value(default_mapping, "character"),
    mapping_value(default_mapping, "start"),
    mapping_value(default_mapping, "end"),
    mapping_value(default_mapping, "line"),
  }, ",")

  ReaADR.message(
    ("Column mapping is required for this %s file.\n\nAvailable columns:\n%s\n\nEnter source column names for the required ADR fields. Optional Dialogue may be blank."):format(
      tostring(details.delimiter_name or "script"),
      headers
    )
  )

  local ok, values = reaper.GetUserInputs(
    "Map ADR Script Columns",
    5,
    "Cue ID,Character,Start,End,Dialogue(optional)",
    defaults
  )
  if not ok then
    return nil
  end

  local fields = {}
  for value in (values .. ","):gmatch("([^,]*),") do
    fields[#fields + 1] = value
  end

  return {
    cue_id = fields[1],
    character = fields[2],
    start = fields[3],
    ["end"] = fields[4],
    line = fields[5],
  }
end

local function character_list_from_counts(counts)
  local names = {}
  for character in pairs(counts or {}) do
    names[#names + 1] = character
  end
  table.sort(names)
  return names
end

local function build_import_summary(script_info, cues, existing_counts, revision_diff)
  local lines, counts = ReaADR.character_summary_lines(cues, { imported_counts = existing_counts })
  local imported = join_sorted_keys(existing_counts or {})
  local available = {}
  for _, character in ipairs(character_list_from_counts(counts)) do
    if not existing_counts[character] then
      available[#available + 1] = character
    end
  end

  local summary = {
    "Select Characters To Import",
    "",
    "Script Name: " .. tostring(script_info.script_name or ""),
    "Script ID: " .. tostring(script_info.script_id or ""),
    "Cue Count: " .. tostring(script_info.cue_count or #cues),
  }

  if tostring(script_info.script_revision or "") ~= "" then
    summary[#summary + 1] = "Revision: " .. tostring(script_info.script_revision)
  end

  if #imported > 0 then
    summary[#summary + 1] = "Already Imported: " .. table.concat(imported, ", ")
  end
  if #available > 0 then
    summary[#summary + 1] = "Available: " .. table.concat(available, ", ")
  end

  if revision_diff and (
    revision_diff.new_cues > 0 or
    revision_diff.removed_cues > 0 or
    revision_diff.timing_changes > 0 or
    revision_diff.dialogue_changes > 0 or
    revision_diff.metadata_changes > 0
  ) then
    summary[#summary + 1] = ""
    summary[#summary + 1] = "Differences Detected:"
    summary[#summary + 1] = "+ New cues: " .. tostring(revision_diff.new_cues)
    summary[#summary + 1] = "+ Removed cues: " .. tostring(revision_diff.removed_cues)
    summary[#summary + 1] = "+ Timing changes: " .. tostring(revision_diff.timing_changes)
    summary[#summary + 1] = "+ Dialogue changes: " .. tostring(revision_diff.dialogue_changes)
    summary[#summary + 1] = "+ Metadata changes: " .. tostring(revision_diff.metadata_changes)
  end

  if #lines > 0 then
    summary[#summary + 1] = ""
    for _, line in ipairs(lines) do
      summary[#summary + 1] = line
    end
  end

  summary[#summary + 1] = ""
  summary[#summary + 1] = "Import Actions:"
  summary[#summary + 1] = "Import Selected Characters"
  summary[#summary + 1] = "Update Existing Import"
  summary[#summary + 1] = ""
  summary[#summary + 1] = "Select the characters you want to bring into the project. Use Select All for the whole script."
  return table.concat(summary, "\n"), counts
end

local function prompt_import_plan_text(script_info, cues, existing_counts, revision_diff)
  local summary, counts = build_import_summary(script_info, cues, existing_counts, revision_diff)
  ReaADR.message(summary)

  local defaults_selected = {}
  for _, character in ipairs(character_list_from_counts(counts)) do
    if not existing_counts[character] then
      defaults_selected[#defaults_selected + 1] = character
    end
  end
  if #defaults_selected == 0 then
    defaults_selected = character_list_from_counts(counts)
  end

  local default_mode = next(existing_counts) and "Update Existing Import" or "Import Selected Characters"
  local ok, values = reaper.GetUserInputs(
    "ADR Script Import Setup",
    3,
    "Script Name,Import Action,Characters to Import",
    table.concat({
      tostring(script_info.script_name or ""),
      default_mode,
      table.concat(defaults_selected, "; "),
    }, ",")
  )
  if not ok then
    return nil
  end

  local fields = {}
  for value in (values .. ","):gmatch("([^,]*),") do
    fields[#fields + 1] = value
  end

  return {
    script_name = tostring(fields[1] or ""):match("^%s*(.-)%s*$"),
    mode = parse_import_mode(fields[2]),
    selected_characters = split_character_list(fields[3]),
  }
end

local function inside(rect, x, y)
  return rect and x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
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

local function prompt_import_plan(script_info, cues, existing_counts, revision_diff, callback)
  if not gfx or not reaper.defer then
    callback(prompt_import_plan_text(script_info, cues, existing_counts, revision_diff))
    return
  end

  local _, counts = build_import_summary(script_info, cues, existing_counts, revision_diff)
  local characters = character_list_from_counts(counts)
  local selected = {}
  for _, character in ipairs(characters) do
    if not existing_counts[character] then
      selected[character] = true
    end
  end
  if next(selected) == nil then
    for _, character in ipairs(characters) do
      selected[character] = true
    end
  end

  local state = {
    script_name = tostring(script_info.script_name or ""),
    script_name_original = tostring(script_info.script_name or ""),
    mode = "selected",
    width = 620,
    height = math.min(840, math.max(540, 285 + (#characters * 32))),
    min_width = 560,
    min_height = 480,
    scroll = 0,
    last_mouse = 0,
    scrollbar_drag_offset = 0,
    dragging_scrollbar = false,
    last_script_click = 0,
    editing_script_name = false,
    script_name_cursor = #tostring(script_info.script_name or ""),
    done = false,
  }
  local rows, action_buttons = {}, {}
  local script_name_rect, btn_all, btn_none, btn_continue, btn_cancel
  local last_layout_w, last_layout_h = 0, 0

  local function selected_count()
    local count = 0
    for _, character in ipairs(characters) do
      if selected[character] then
        count = count + 1
      end
    end
    return count
  end

  local function selected_characters()
    local result = {}
    for _, character in ipairs(characters) do
      if selected[character] then
        result[#result + 1] = character
      end
    end
    return result
  end

  local function set_all(value)
    for _, character in ipairs(characters) do
      selected[character] = value and true or nil
    end
  end

  local function layout()
    local w = math.max(state.min_width, gfx.w or state.width)
    local h = math.max(state.min_height, gfx.h or state.height)
    if w == last_layout_w and h == last_layout_h then
      return
    end
    last_layout_w, last_layout_h = w, h
    state.width, state.height = w, h

    action_buttons = {
      { x = 24, y = 128, w = 220, h = 30, label = "Import Selected Characters", mode = "selected" },
      { x = 254, y = 128, w = 190, h = 30, label = "Update Existing Import", mode = "update" },
    }

    script_name_rect = { x = 24, y = 82, w = w - 48, h = 34 }
    local by = h - 50
    btn_all = { x = 24, y = by, w = 78, h = 28, label = "Select All" }
    btn_none = { x = 112, y = by, w = 66, h = 28, label = "Clear" }
    btn_continue = { x = w - 190, y = by, w = 96, h = 28, label = "Continue" }
    btn_cancel = { x = w - 84, y = by, w = 60, h = 28, label = "Cancel" }
  end

  local function draw_button(rect, active)
    local theme = ReaADR.ui_theme()
    local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
    ReaADR.set_gfx_color(active and theme.accent_green or (hover and theme.highlight or theme.panel_alt))
    gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
    ReaADR.set_gfx_color(theme.border)
    gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
    gfx.setfont(1, "Arial", 13)
    ReaADR.set_gfx_color(theme.text)
    local tw = gfx.measurestr(rect.label)
    gfx.x = rect.x + math.max(6, (rect.w - tw) * 0.5)
    gfx.y = rect.y + 7
    gfx.drawstr(rect.label)
  end

  local function finish()
    local chosen = selected_characters()
    if #chosen == 0 then
      ReaADR.message("Select at least one character to import.")
      return
    end
    state.done = true
    ReaADR.save_window_state("import_character_selection")
    gfx.quit()
    callback({
      script_name = state.script_name,
      mode = state.mode,
      selected_characters = chosen,
    })
  end

  local function cancel()
    state.done = true
    ReaADR.save_window_state("import_character_selection")
    gfx.quit()
    callback(nil)
  end

  local function begin_script_name_edit()
    state.editing_script_name = true
    state.script_name_original = state.script_name
    state.script_name_cursor = math.max(0, math.min(#state.script_name, tonumber(state.script_name_cursor) or #state.script_name))
  end

  local function finish_script_name_edit(commit)
    if not state.editing_script_name then
      return
    end
    if not commit then
      state.script_name = state.script_name_original or state.script_name
    end
    state.editing_script_name = false
    state.last_script_click = 0
  end

  local function set_script_name_cursor_from_mouse()
    if not script_name_rect then
      return
    end
    local x = gfx.mouse_x - (script_name_rect.x + 8)
    if x <= 0 then
      state.script_name_cursor = 0
      return
    end
    local best = #state.script_name
    for pos = 0, #state.script_name do
      local width = gfx.measurestr(state.script_name:sub(1, pos))
      if width >= x then
        best = pos
        break
      end
    end
    state.script_name_cursor = math.max(0, math.min(#state.script_name, best))
  end

  local function insert_script_name_text(text)
    if text == "" then
      return
    end
    local cursor = math.max(0, math.min(#state.script_name, tonumber(state.script_name_cursor) or #state.script_name))
    state.script_name = state.script_name:sub(1, cursor) .. text .. state.script_name:sub(cursor + 1)
    state.script_name_cursor = cursor + #text
  end

  local function frame()
    local theme = ReaADR.ui_theme()
    layout()
    ReaADR.set_gfx_color(theme.bg)
    gfx.rect(0, 0, state.width, state.height, true)

    ReaADR.draw_window_header(
      "Import Characters",
      ("%d character(s) found  -  %d selected"):format(#characters, selected_count()),
      { x = 20, y = 14, width = state.width - 40, height = 58 }
    )

    gfx.setfont(1, "Arial", 14)
    ReaADR.set_gfx_color(theme.muted)
    gfx.x = script_name_rect.x
    gfx.y = script_name_rect.y
    gfx.drawstr("Script Name")
    ReaADR.set_gfx_color((state.editing_script_name or inside(script_name_rect, gfx.mouse_x, gfx.mouse_y)) and theme.panel_alt or theme.panel)
    gfx.rect(script_name_rect.x, script_name_rect.y + 16, script_name_rect.w, 18, true)
    ReaADR.set_gfx_color(theme.border)
    gfx.rect(script_name_rect.x, script_name_rect.y + 16, script_name_rect.w, 18, false)
    ReaADR.set_gfx_color(theme.text)
    gfx.x = script_name_rect.x + 8
    gfx.y = script_name_rect.y + 18
    gfx.drawstr(state.script_name)
    if state.editing_script_name then
      local prefix_w = gfx.measurestr(state.script_name:sub(1, math.max(0, math.min(#state.script_name, state.script_name_cursor or 0))))
      local cursor_x = script_name_rect.x + 8 + prefix_w + 2
      gfx.line(cursor_x, script_name_rect.y + 18, cursor_x, script_name_rect.y + 32)
    end
    ReaADR.set_gfx_color(theme.muted)
    local edit_hint = "double-click to edit"
    local edit_hint_w = gfx.measurestr(edit_hint)
    gfx.x = script_name_rect.x + script_name_rect.w - edit_hint_w - 8
    gfx.y = script_name_rect.y + 18
    gfx.drawstr(edit_hint)

    for _, rect in ipairs(action_buttons) do
      draw_button(rect, state.mode == rect.mode)
    end

    local list_top = 198
    local list_bottom = state.height - 66
    local row_h = 30
    local visible_rows = math.max(1, math.floor((list_bottom - list_top) / row_h))
    local max_scroll = math.max(0, #characters - visible_rows)
    state.scroll = math.max(0, math.min(max_scroll, state.scroll or 0))
    local scrollbar_rect = nil
    local scrollbar_thumb = nil
    if max_scroll > 0 then
      local track_w = 10
      local track_x = state.width - 24
      local track_h = list_bottom - list_top
      local thumb_h = math.max(24, math.floor(track_h * (visible_rows / #characters)))
      local travel = math.max(1, track_h - thumb_h)
      local thumb_y = list_top + math.floor(travel * (state.scroll / max_scroll))
      scrollbar_rect = { x = track_x, y = list_top, w = track_w, h = track_h, travel = travel, thumb_h = thumb_h }
      scrollbar_thumb = { x = track_x, y = thumb_y, w = track_w, h = thumb_h }
    end
    rows = {}

    gfx.setfont(1, "Arial", 13)
    ReaADR.set_gfx_color(theme.muted)
    gfx.x = 24
    gfx.y = list_top - 20
    gfx.drawstr("Characters")

    local row_width = scrollbar_rect and (state.width - 60) or (state.width - 48)
    for visible = 1, visible_rows do
      local index = visible + state.scroll
      local character = characters[index]
      if character then
        local row = { x = 24, y = list_top + (visible - 1) * row_h, w = row_width, h = 25, character = character }
        rows[#rows + 1] = row
        local checked = selected[character] == true
        local already = existing_counts[character] ~= nil
        ReaADR.set_gfx_color(theme.panel)
        gfx.rect(row.x, row.y, row.w, row.h, true)
        ReaADR.set_gfx_color(theme.border)
        gfx.rect(row.x, row.y, row.w, row.h, false)
        ReaADR.set_gfx_color(checked and theme.accent_gold or theme.panel_alt)
        gfx.rect(row.x + 7, row.y + 5, 14, 14, true)
        ReaADR.set_gfx_color(theme.border)
        gfx.rect(row.x + 7, row.y + 5, 14, 14, false)
        ReaADR.set_gfx_color(theme.text)
        gfx.x = row.x + 30
        gfx.y = row.y + 5
        gfx.drawstr(character)
        ReaADR.set_gfx_color(already and theme.accent_gold or theme.muted)
        local note = ("%d cue(s)%s"):format(tonumber(counts[character]) or 0, already and " - already imported" or "")
        local tw = gfx.measurestr(note)
        gfx.x = row.x + row.w - tw - 10
        gfx.y = row.y + 5
        gfx.drawstr(note)
      end
    end

    if max_scroll > 0 then
      ReaADR.set_gfx_color(theme.panel_alt)
      gfx.rect(scrollbar_rect.x, scrollbar_rect.y, scrollbar_rect.w, scrollbar_rect.h, true)
      ReaADR.set_gfx_color(theme.border)
      gfx.rect(scrollbar_rect.x, scrollbar_rect.y, scrollbar_rect.w, scrollbar_rect.h, false)
      ReaADR.set_gfx_color(state.dragging_scrollbar and theme.accent_green or theme.highlight)
      gfx.rect(scrollbar_thumb.x, scrollbar_thumb.y, scrollbar_thumb.w, scrollbar_thumb.h, true)
      ReaADR.set_gfx_color(theme.border)
      gfx.rect(scrollbar_thumb.x, scrollbar_thumb.y, scrollbar_thumb.w, scrollbar_thumb.h, false)
      ReaADR.set_gfx_color(theme.muted)
      gfx.x = state.width - 120
      gfx.y = list_bottom - 18
      gfx.drawstr(("Scroll %d/%d"):format(state.scroll + 1, max_scroll + 1))
    end

    draw_button(btn_all)
    draw_button(btn_none)
    draw_button(btn_continue, selected_count() > 0)
    draw_button(btn_cancel)
    gfx.update()

    local char = gfx.getchar()
    if char < 0 or char == 27 then
      if state.editing_script_name then
        finish_script_name_edit(false)
        reaper.defer(frame)
        return
      end
      cancel()
      return
    elseif ReaADR.handle_gfx_transport_key(char, state.editing_script_name) then
      reaper.defer(frame)
      return
    elseif char == 13 then
      if state.editing_script_name then
        finish_script_name_edit(true)
        reaper.defer(frame)
        return
      end
      finish()
      return
    end

    if state.editing_script_name then
      if char == KEY_LEFT then
        state.script_name_cursor = math.max(0, (state.script_name_cursor or 0) - 1)
      elseif char == KEY_RIGHT then
        state.script_name_cursor = math.min(#state.script_name, (state.script_name_cursor or 0) + 1)
      elseif char == KEY_HOME then
        state.script_name_cursor = 0
      elseif char == KEY_END then
        state.script_name_cursor = #state.script_name
      elseif char == KEY_DELETE then
        local cursor = math.max(0, math.min(#state.script_name, state.script_name_cursor or 0))
        if cursor < #state.script_name then
          state.script_name = state.script_name:sub(1, cursor) .. state.script_name:sub(cursor + 2)
        end
      elseif char == 8 or char == 177 or char == 127 then
        local cursor = math.max(0, math.min(#state.script_name, state.script_name_cursor or 0))
        if cursor > 0 then
          state.script_name = state.script_name:sub(1, cursor - 1) .. state.script_name:sub(cursor + 1)
          state.script_name_cursor = cursor - 1
        end
      elseif char >= 32 and char < 256 then
        local cursor = math.max(0, math.min(#state.script_name, state.script_name_cursor or 0))
        local insert = string.char(char)
        state.script_name = state.script_name:sub(1, cursor) .. insert .. state.script_name:sub(cursor + 1)
        state.script_name_cursor = cursor + 1
      end
    end

    if gfx.mouse_wheel ~= 0 then
      state.scroll = math.max(0, math.min(max_scroll, state.scroll - (gfx.mouse_wheel > 0 and 1 or -1)))
      gfx.mouse_wheel = 0
    end

    local mouse = gfx.mouse_cap % 2
    if state.dragging_scrollbar and mouse == 1 and scrollbar_rect then
      local thumb_y = math.max(scrollbar_rect.y, math.min(gfx.mouse_y - state.scrollbar_drag_offset, scrollbar_rect.y + scrollbar_rect.travel))
      local ratio = scrollbar_rect.travel > 0 and ((thumb_y - scrollbar_rect.y) / scrollbar_rect.travel) or 0
      state.scroll = math.max(0, math.min(max_scroll, math.floor(ratio * max_scroll + 0.5)))
    end
    if state.dragging_scrollbar and mouse == 0 then
      state.dragging_scrollbar = false
    end
    if mouse == 1 and state.last_mouse == 0 then
      if scrollbar_thumb and inside(scrollbar_thumb, gfx.mouse_x, gfx.mouse_y) then
        state.dragging_scrollbar = true
        state.scrollbar_drag_offset = gfx.mouse_y - scrollbar_thumb.y
      elseif scrollbar_rect and inside(scrollbar_rect, gfx.mouse_x, gfx.mouse_y) then
        local relative = math.max(0, math.min(scrollbar_rect.travel, gfx.mouse_y - scrollbar_rect.y - math.floor(scrollbar_thumb.h * 0.5)))
        local ratio = scrollbar_rect.travel > 0 and (relative / scrollbar_rect.travel) or 0
        state.scroll = math.max(0, math.min(max_scroll, math.floor(ratio * max_scroll + 0.5)))
        state.dragging_scrollbar = true
        state.scrollbar_drag_offset = math.floor(scrollbar_thumb.h * 0.5)
      else
        if state.editing_script_name and not inside(script_name_rect, gfx.mouse_x, gfx.mouse_y) then
          finish_script_name_edit(true)
        elseif state.editing_script_name and inside(script_name_rect, gfx.mouse_x, gfx.mouse_y) then
          set_script_name_cursor_from_mouse()
        end
        for _, rect in ipairs(action_buttons) do
          if inside(rect, gfx.mouse_x, gfx.mouse_y) then
            state.mode = rect.mode
          end
        end
        for _, row in ipairs(rows) do
          if inside(row, gfx.mouse_x, gfx.mouse_y) then
            selected[row.character] = not selected[row.character]
          end
        end
        if inside(script_name_rect, gfx.mouse_x, gfx.mouse_y) then
          local now = reaper.time_precise and reaper.time_precise() or os.clock()
          if now - (state.last_script_click or 0) <= 0.35 then
            begin_script_name_edit()
            state.last_script_click = 0
          else
            state.last_script_click = now
          end
          if state.editing_script_name then
            set_script_name_cursor_from_mouse()
          end
        elseif state.editing_script_name then
          set_script_name_cursor_from_mouse()
        elseif inside(btn_all, gfx.mouse_x, gfx.mouse_y) then
          set_all(true)
          state.mode = "selected"
        elseif inside(btn_none, gfx.mouse_x, gfx.mouse_y) then
          set_all(false)
        elseif inside(btn_continue, gfx.mouse_x, gfx.mouse_y) then
          finish()
          return
        elseif inside(btn_cancel, gfx.mouse_x, gfx.mouse_y) then
          cancel()
          return
        end
      end
    end
    state.last_mouse = mouse

    reaper.defer(frame)
  end

  ReaADR.init_persistent_window("import_character_selection", "ReaADR - Import Characters", {
    width = state.width,
    height = state.height,
  })
  frame()
end

local function warn_skipped_duplicates(skipped)
  if #skipped == 0 then
    return
  end
  ReaADR.message(
    "The following characters were already imported for this script and were skipped to prevent duplicates:\n\n" ..
    table.concat(skipped, "\n") ..
    "\n\nUse Update Existing Import to update already imported characters."
  )
end

local function apply_import_mode(existing_cues, imported_cues, script_id, selected_characters, mode)
  local selected_set = {}
  for _, character in ipairs(selected_characters or {}) do
    selected_set[character] = true
  end

  local final_cues = {}
  if mode == "update" then
    for _, cue in ipairs(existing_cues or {}) do
      if not (tostring(cue.script_id or "") == tostring(script_id or "") and selected_set[tostring(cue.character or "")]) then
        final_cues[#final_cues + 1] = cue
      end
    end
  else
    for _, cue in ipairs(existing_cues or {}) do
      final_cues[#final_cues + 1] = cue
    end
  end

  for _, cue in ipairs(imported_cues or {}) do
    final_cues[#final_cues + 1] = cue
  end

  table.sort(final_cues, function(a, b)
    local a_start = tonumber(a.start_time) or 0
    local b_start = tonumber(b.start_time) or 0
    if a_start == b_start then
      return tostring(a.id or "") < tostring(b.id or "")
    end
    return a_start < b_start
  end)
  return final_cues
end

local function stale_update_cues(existing_script_cues, imported_cues, selected_characters)
  local selected_set = {}
  for _, character in ipairs(selected_characters or {}) do
    selected_set[character] = true
  end

  local imported_keys = {}
  for _, cue in ipairs(imported_cues or {}) do
    imported_keys[tostring(cue.character or "") .. "\t" .. ReaADR.cue_key(cue)] = true
  end

  local stale = {}
  for _, cue in ipairs(existing_script_cues or {}) do
    local character = tostring(cue.character or "")
    if selected_set[character] and not imported_keys[character .. "\t" .. ReaADR.cue_key(cue)] then
      stale[#stale + 1] = cue
    end
  end
  return stale
end

local ok, path = reaper.GetUserFileNameForRead("", "Import ADR script", "csv;tsv;tab;txt;xlsx")
if not ok or not path or path == "" then
  return
end

local frame_rate = reaper.TimeMap_curFrameRate(0)
if not frame_rate or frame_rate <= 0 then
  frame_rate = 24
end

local cues, parse_error, parse_details = ReaADR.parse_script_file(path, frame_rate)
if not cues and parse_details and parse_details.code == "missing_required_mapping" then
  local mapping = prompt_column_mapping(parse_details)
  if mapping then
    cues, parse_error = ReaADR.parse_script_file(path, frame_rate, mapping)
    if cues then
      ReaADR.save_column_mapping_preset("last", mapping)
    end
  end
end

if not cues then
  ReaADR.message("Cue sheet import failed:\n\n" .. tostring(parse_error))
  return
end

local existing_cues = ReaADR.load_session_cues() or {}
local initial_script_info = ReaADR.derive_script_identity(path, cues)
local existing_script_cues = ReaADR.script_cues(existing_cues, initial_script_info.script_id)
local existing_counts = ReaADR.character_counts(existing_script_cues)
local revision_diff = nil
if #existing_script_cues > 0 then
  revision_diff = ReaADR.compare_script_revisions(existing_script_cues, ReaADR.annotate_cues_with_script_info(cues, initial_script_info))
end

local function continue_import(plan)
if not plan then
  return
end

local script_info = ReaADR.derive_script_identity(path, cues, {
  script_name = plan.script_name,
  script_id = initial_script_info.script_id,
  script_revision = initial_script_info.script_revision,
})
local annotated_cues = ReaADR.annotate_cues_with_script_info(cues, script_info)
local all_character_counts = ReaADR.character_counts(annotated_cues)
local all_characters = character_list_from_counts(all_character_counts)

local selected_characters = {}
if plan.mode == "all" then
  for _, character in ipairs(all_characters) do
    selected_characters[#selected_characters + 1] = character
  end
elseif #plan.selected_characters > 0 then
  selected_characters = plan.selected_characters
end

local selected_set = {}
for _, character in ipairs(selected_characters) do
  if all_character_counts[character] then
    selected_set[character] = true
  end
end

if next(selected_set) == nil then
  ReaADR.message("No valid characters were selected for import.")
  return
end

local skipped_duplicates = {}
if plan.mode ~= "update" then
  for character in pairs(selected_set) do
    if existing_counts[character] then
      skipped_duplicates[#skipped_duplicates + 1] = character
      selected_set[character] = nil
    end
  end
end
table.sort(skipped_duplicates)
warn_skipped_duplicates(skipped_duplicates)

selected_characters = join_sorted_keys(selected_set)
if #selected_characters == 0 then
  ReaADR.message("No characters remain to import after duplicate protection.\n\nUse Update Existing Import to update already imported characters.")
  return
end

if plan.mode == "update" then
  local update_existing = {}
  for _, character in ipairs(selected_characters) do
    if existing_counts[character] then
      update_existing[#update_existing + 1] = character
    end
  end
  if #update_existing > 0 then
    local answer = reaper.ShowMessageBox(
      ("Update existing imported character data?\n\n%s\n\nThis replaces the saved ReaADR cues for these characters with the version from the imported script. A safety snapshot will be created first."):format(
        table.concat(update_existing, ", ")
      ),
      "ReaADR Import Update",
      4
    )
    if answer ~= 6 then
      return
    end
  end
end

local imported_cues = ReaADR.filter_cues_by_characters(annotated_cues, selected_characters)
local final_cues = apply_import_mode(existing_cues, imported_cues, script_info.script_id, selected_characters, plan.mode)

local validation_summary, validation_error = ReaADR.sync_validate({ cues = final_cues }, { preroll_seconds = import_preroll_seconds })
if not validation_summary then
  ReaADR.message("Cue sheet import check failed:\n\n" .. tostring(validation_error))
  return
end
local validation = validation_summary.validation
local preview_lines = {
  ReaADR.validation_summary_text(validation):gsub("\nBuild this ADR session%?$", ""),
  "",
  "Script Import",
  "Name: " .. tostring(script_info.script_name),
  "ID: " .. tostring(script_info.script_id),
  "Mode: " .. ({ all = "Import Entire Script", selected = "Import Selected Characters", update = "Update Existing Import" })[plan.mode],
  "Characters: " .. table.concat(selected_characters, ", "),
  "",
  "Create/update this ReaADR session?",
}
local proceed = reaper.ShowMessageBox(table.concat(preview_lines, "\n"), "ReaADR Import Preview", 4)
if proceed ~= 6 then
  return
end

ReaADR.log("INFO", "IMPORT", "Starting script import", {
  script_id = script_info.script_id,
  count = #imported_cues,
  detail = ({ all = "all", selected = "selected", update = "update" })[plan.mode],
})

overlay_settings.preroll_seconds = import_preroll_seconds
ReaADR.save_overlay_settings(overlay_settings)

local progress = ReaADR.create_progress_window("Importing ADR Cue Sheet")
local preroll_status = ReaADR.configure_project_preroll(import_preroll_seconds)
local cleanup_summary = nil
local stale_cues = plan.mode == "update" and stale_update_cues(existing_script_cues, imported_cues, selected_characters) or {}
local sync_summary, setup_error, committed_cleanup = ReaADR.commit_session_cues(final_cues, {
  snapshot_label = "Import Cue Sheet: " .. tostring(script_info.script_id),
  undo_description = "ReaADR: import cue sheet and setup ADR project",
  save_options = {
    event_type = "ScriptImported", source = "import_cue_sheet",
    last_operation = "import_cue_sheet", batch_id = script_info.script_id,
  },
  sync_options = {
    overlay_settings = overlay_settings, require_video_track = true,
    on_progress = progress.update, source = "import_cue_sheet",
    batch_id = script_info.script_id,
  },
  after_sync = function()
    if #stale_cues > 0 then
      return ReaADR.remove_project_artifacts_for_cues(stale_cues, { manage_undo = false })
    end
  end,
})
local summary = sync_summary and sync_summary.rebuild

if not summary then
  progress.update("Import failed.", 1, 1)
  progress.close()
  ReaADR.log("ERROR", "IMPORT", "Cue sheet import failed during project setup", {
    script_id = script_info.script_id,
    detail = tostring(setup_error),
  })
  ReaADR.message("Cue sheet import failed while populating the project:\n\n" .. tostring(setup_error))
  return
end

cleanup_summary = committed_cleanup
if #stale_cues > 0 then
  ReaADR.log("INFO", "IMPORT", "Removed stale cues after successful update", {
    script_id = script_info.script_id,
    count = #stale_cues,
  })
end

progress.close()
ReaADR.show_video_window()
ReaADR.log("INFO", "IMPORT", "Script import completed", {
  script_id = script_info.script_id,
  count = summary.cue_count,
  detail = table.concat(selected_characters, ", "),
})

ReaADR.message(
  ("Imported script %s.\n\nMode: %s\nScript ID: %s\nCharacters imported: %s\n\nProject cues: %d\nProject characters: %d\nTracks: %d created, %d reused\nRegions: %d created, %d updated\nOld cue markers removed: %d\nCue audio: %d created, %d updated, %d skipped\nOverlap splits: %d\nVideo overlay FX: %s\nPre-roll: %.1fs (%s)"):format(
    tostring(script_info.script_name),
    ({ all = "Import Entire Script", selected = "Import Selected Characters", update = "Update Existing Import" })[plan.mode],
    tostring(script_info.script_id),
    table.concat(selected_characters, ", "),
    summary.cue_count,
    summary.character_count,
    summary.tracks_created,
    summary.tracks_reused,
    summary.regions_created,
    summary.regions_updated,
    summary.markers_removed,
    summary.cue_audio_created,
    summary.cue_audio_updated,
    summary.cue_audio_skipped,
    summary.overlap_conflicts or 0,
    summary.overlay_fx_status,
    preroll_status.seconds,
    preroll_status.status
  )
  .. (
    cleanup_summary and
    ("\nStale update cleanup: %d regions removed, %d cue items removed"):format(
      cleanup_summary.regions_removed or 0,
      cleanup_summary.cue_audio_removed or 0
    ) or
    ""
  )
)
end

prompt_import_plan(initial_script_info, cues, existing_counts, revision_diff, continue_import)
