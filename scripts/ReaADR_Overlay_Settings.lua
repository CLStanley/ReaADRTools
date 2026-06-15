-- Toggle ADR video overlay features for the current REAPER project.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")
local settings = ReaADR.load_overlay_settings()

local rows = {
  { key = "enabled", label = "Enable video overlays" },
  { key = "show_cue_id", label = "Cue ID" },
  { key = "show_character", label = "Character name" },
  { key = "show_cue_timecode", label = "Cue timecode" },
  { key = "show_project_timer", label = "Live project timer" },
  { key = "show_dialogue", label = "Dialogue line" },
  { key = "show_direction", label = "Notes" },
  { key = "show_cue_type", label = "Cue type" },
  { key = "show_visual_cue", label = "Visual cue indicator" },
  { key = "show_streamer", label = "Streamer bar" },
  { key = "show_flash", label = "Flash at cue start" },
  { key = "show_status", label = "Standby / take / clear status" },
  { key = "show_metadata", label = "Studio metadata" },
}

local background_rows = {
  { key = "bg_project_timer", label = "Timeline SMPTE background" },
  { key = "bg_cue_id", label = "Cue ID background" },
  { key = "bg_character", label = "Character background" },
  { key = "bg_cue_timecode", label = "Cue timecode background" },
  { key = "bg_dialogue", label = "Dialogue background" },
  { key = "bg_direction", label = "Notes background" },
  { key = "bg_cue_type", label = "Cue type background" },
  { key = "bg_status", label = "Status background" },
  { key = "bg_metadata", label = "Metadata background" },
}

local metadata_field_labels = {
  "PGID",
  "MID",
  "Media Time",
  "Watermark Timestamp",
  "Asset Date Code",
  "Project Name",
}

local state = {
  scroll = 0,
  content_h = 0,
  dirty = false,
  saved_message = "Saved",
  saved_message_until = 0,
  last_mouse = 0,
  dragging_scrollbar = false,
  scrollbar_drag_offset = 0,
}

ReaADR.init_persistent_window("overlay_settings", "ReaADR Overlay Settings", {
  width = 760,
  height = 820,
})

local function trim_to_width(text, max_w, font_size)
  text = tostring(text or "")
  gfx.setfont(1, "Arial", font_size or 14)
  if gfx.measurestr(text) <= max_w then
    return text
  end
  local trimmed = text
  while #trimmed > 1 and gfx.measurestr(trimmed .. "...") > max_w do
    trimmed = trimmed:sub(1, #trimmed - 1)
  end
  return trimmed .. "..."
end

local function split_metadata_fields(value)
  local fields = {}
  for field in tostring(value or ""):gmatch("([^,]+)") do
    field = field:match("^%s*(.-)%s*$")
    if field ~= "" then
      fields[#fields + 1] = field
    end
  end
  for index = 1, #metadata_field_labels do
    if fields[index] == nil then
      fields[index] = metadata_field_labels[index]
    end
  end
  return fields
end

local function edit_metadata_fields()
  local fields = split_metadata_fields(settings.metadata_fields)
  local ok, values = reaper.GetUserInputs(
    "Studio Metadata Overlay Fields",
    #metadata_field_labels,
    table.concat(metadata_field_labels, ","),
    table.concat(fields, ",")
  )
  if not ok then
    return false
  end

  local updated = {}
  for value in (values .. ","):gmatch("([^,]*),") do
    value = value:match("^%s*(.-)%s*$")
    if value ~= "" then
      updated[#updated + 1] = value
    end
  end
  settings.metadata_fields = table.concat(updated, ",")
  return true
end

local function apply_profile(name)
  local profiles = {
    actor = {
      enabled = true, show_cue_id = true, show_character = true, show_dialogue = true,
      show_cue_timecode = true, show_project_timer = true, show_visual_cue = true,
      show_direction = true, show_cue_type = false, show_streamer = true,
      show_flash = true, show_status = true, show_metadata = false,
      bg_project_timer = false, bg_cue_id = false, bg_character = false,
      bg_cue_timecode = false, bg_dialogue = true, bg_direction = false,
      bg_cue_type = false, bg_status = false, bg_metadata = false,
    },
    engineer = {
      enabled = true, show_cue_id = true, show_character = true, show_dialogue = true,
      show_cue_timecode = true, show_project_timer = true, show_visual_cue = true,
      show_direction = true, show_cue_type = true, show_streamer = true,
      show_flash = true, show_status = true, show_metadata = true,
      bg_project_timer = true, bg_cue_id = false, bg_character = false,
      bg_cue_timecode = true, bg_dialogue = true, bg_direction = false,
      bg_cue_type = false, bg_status = false, bg_metadata = false,
    },
    studio = {
      enabled = true, show_cue_id = true, show_character = true, show_dialogue = true,
      show_cue_timecode = true, show_project_timer = true, show_visual_cue = true,
      show_direction = false, show_cue_type = true, show_streamer = true,
      show_flash = true, show_status = true, show_metadata = true,
      bg_project_timer = true, bg_cue_id = false, bg_character = false,
      bg_cue_timecode = true, bg_dialogue = true, bg_direction = false,
      bg_cue_type = false, bg_status = false, bg_metadata = false,
    },
    minimal = {
      enabled = true, show_cue_id = true, show_character = false, show_dialogue = true,
      show_cue_timecode = false, show_project_timer = true, show_visual_cue = true,
      show_direction = false, show_cue_type = false, show_streamer = true,
      show_flash = false, show_status = false, show_metadata = false,
      bg_project_timer = false, bg_cue_id = false, bg_character = false,
      bg_cue_timecode = false, bg_dialogue = true, bg_direction = false,
      bg_cue_type = false, bg_status = false, bg_metadata = false,
    },
  }
  for key, value in pairs(profiles[name] or {}) do
    settings[key] = value
  end
end

local function save()
  ReaADR.save_overlay_settings(settings)
  local overlay_status = ReaADR.refresh_overlay_fx_from_project(settings)
  if overlay_status then
    state.saved_message = overlay_status == "disabled" and "Saved + disabled" or "Saved + refreshed"
  else
    state.saved_message = "Saved; import cues first"
  end
  state.dirty = false
  state.saved_message_until = reaper.time_precise() + 2.0
end

local function inside(rect, x, y)
  return x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
end

local function draw_button(rect)
  local theme = ReaADR.ui_theme()
  local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
  ReaADR.set_gfx_color(hover and theme.accent_blue or theme.panel_alt)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 14)
  ReaADR.set_gfx_color(theme.text)
  local label = trim_to_width(rect.label, rect.w - 16, 14)
  local tw = gfx.measurestr(label)
  gfx.x = rect.x + math.max(8, math.floor((rect.w - tw) * 0.5))
  gfx.y = rect.y + 7
  gfx.drawstr(label)
  return hover
end

local function draw_checkbox(entry, checked)
  local theme = ReaADR.ui_theme()
  ReaADR.set_gfx_color(theme.panel_alt)
  gfx.rect(entry.box_x, entry.box_y, 18, 18, false)
  if checked then
    ReaADR.set_gfx_color(theme.accent_gold)
    gfx.rect(entry.box_x + 4, entry.box_y + 4, 10, 10, true)
  end
  gfx.setfont(1, "Arial", 14)
  ReaADR.set_gfx_color(theme.text)
  gfx.x = entry.text_x
  gfx.y = entry.text_y
  gfx.drawstr(trim_to_width(entry.label, entry.text_w, 14))
end

local function layout_wrapped_buttons(start_x, start_y, available_w, specs)
  local buttons = {}
  local x = start_x
  local y = start_y
  local gap_x = 10
  local gap_y = 10
  local h = 30
  gfx.setfont(1, "Arial", 14)
  for _, spec in ipairs(specs or {}) do
    local w = math.max(spec.min_w or 92, math.ceil(gfx.measurestr(spec.label) + 26))
    if x > start_x and (x + w) > (start_x + available_w) then
      x = start_x
      y = y + h + gap_y
    end
    buttons[#buttons + 1] = { x = x, y = y, w = w, h = h, label = spec.label, key = spec.key }
    x = x + w + gap_x
  end
  return buttons, y + h
end

local function layout_checkbox_grid(start_x, start_y, available_w, source_rows)
  local min_col_w = 310
  local gap_x = 18
  local row_h = 32
  local cols = math.max(1, math.floor((available_w + gap_x) / (min_col_w + gap_x)))
  local col_w = math.floor((available_w - ((cols - 1) * gap_x)) / cols)
  local entries = {}
  for index, row in ipairs(source_rows or {}) do
    local col = (index - 1) % cols
    local row_index = math.floor((index - 1) / cols)
    local x = start_x + (col * (col_w + gap_x))
    local y = start_y + (row_index * row_h)
    entries[#entries + 1] = {
      key = row.key,
      label = row.label,
      rect = { x = x, y = y - 4, w = col_w, h = 26 },
      box_x = x,
      box_y = y,
      text_x = x + 30,
      text_y = y - 1,
      text_w = col_w - 34,
    }
  end
  local rows_used = math.max(1, math.ceil(#(source_rows or {}) / cols))
  return entries, start_y + (rows_used * row_h)
end

local function loop()
  local theme = ReaADR.ui_theme()
  local width = math.max(520, gfx.w or 760)
  local height = math.max(480, gfx.h or 820)
  local mouse = gfx.mouse_cap % 2
  local clicked = mouse == 1 and state.last_mouse == 0
  local viewport_top = 104
  local footer_h = 74
  local viewport_h = math.max(120, height - viewport_top - footer_h)
  local left = 24
  local content_w = width - 60
  local special_click = {}

  ReaADR.set_gfx_color(theme.bg)
  gfx.rect(0, 0, width, height, true)

  local header = ReaADR.draw_window_header(
    "ReaADR Video Overlay Settings",
    "Resize freely. Content wraps to width and scrolls when needed.",
    { x = 20, y = 18, width = width - 40, height = 74 }
  )

  local content_y = math.max(viewport_top, header.content_y + 4) - state.scroll

  gfx.setfont(1, "Arial", 14)
  ReaADR.set_gfx_color(theme.muted)
  gfx.x = left
  gfx.y = content_y
  gfx.drawstr("Profiles")
  local profile_buttons, after_profiles_y = layout_wrapped_buttons(left, content_y + 24, content_w, {
    { label = "Actor", key = "actor", min_w = 92 },
    { label = "Engineer", key = "engineer", min_w = 102 },
    { label = "Studio", key = "studio", min_w = 92 },
    { label = "Minimal", key = "minimal", min_w = 92 },
  })
  for _, rect in ipairs(profile_buttons) do
    special_click[#special_click + 1] = { rect = rect, kind = "profile", key = rect.key }
    draw_button(rect)
  end

  local overlay_label_y = after_profiles_y + 22
  gfx.setfont(1, "Arial", 14)
  ReaADR.set_gfx_color(theme.muted)
  gfx.x = left
  gfx.y = overlay_label_y
  gfx.drawstr("Overlay Elements")
  local overlay_entries, after_overlay_y = layout_checkbox_grid(left, overlay_label_y + 24, content_w, rows)
  for _, entry in ipairs(overlay_entries) do
    special_click[#special_click + 1] = { rect = entry.rect, kind = "toggle", key = entry.key }
    draw_checkbox(entry, settings[entry.key])
  end

  local backgrounds_label_y = after_overlay_y + 18
  gfx.setfont(1, "Arial", 14)
  ReaADR.set_gfx_color(theme.muted)
  gfx.x = left
  gfx.y = backgrounds_label_y
  gfx.drawstr("Text Backgrounds")
  local background_entries, after_backgrounds_y = layout_checkbox_grid(left, backgrounds_label_y + 24, content_w, background_rows)
  for _, entry in ipairs(background_entries) do
    special_click[#special_click + 1] = { rect = entry.rect, kind = "toggle", key = entry.key }
    draw_checkbox(entry, settings[entry.key])
  end

  local controls_y = after_backgrounds_y + 22
  local metadata_rect = { x = left, y = controls_y + 24, w = 132, h = 32, label = "Edit Fields" }
  local white_rect = { x = left, y = controls_y + 106, w = 220, h = 26, label = "White general text", value = "white" }
  local yellow_rect = { x = left, y = controls_y + 140, w = 220, h = 26, label = "Yellow general text", value = "yellow" }

  gfx.setfont(1, "Arial", 13)
  ReaADR.set_gfx_color(theme.muted)
  gfx.x = left
  gfx.y = controls_y
  gfx.drawstr("Metadata fields")
  draw_button(metadata_rect)
  gfx.x = left
  gfx.y = metadata_rect.y + 42
  gfx.drawstr("Current: " .. trim_to_width(settings.metadata_fields or "", math.max(180, content_w - 90), 13))

  gfx.x = left
  gfx.y = controls_y + 82
  gfx.drawstr("General overlay text color")
  for _, option in ipairs({ white_rect, yellow_rect }) do
    special_click[#special_click + 1] = { rect = option, kind = "text_color", value = option.value }
    ReaADR.set_gfx_color(theme.panel_alt)
    gfx.circle(option.x + 10, option.y + 13, 8, false, true)
    if ReaADR.overlay_text_mode(settings) == option.value then
      ReaADR.set_gfx_color(theme.accent_gold)
      gfx.circle(option.x + 10, option.y + 13, 4, true, true)
    end
    ReaADR.set_gfx_color(theme.text)
    gfx.x = option.x + 24
    gfx.y = option.y + 4
    gfx.drawstr(option.label)
  end
  special_click[#special_click + 1] = { rect = metadata_rect, kind = "metadata" }

  state.content_h = (yellow_rect.y + yellow_rect.h + 28) - math.max(viewport_top, header.content_y + 4)
  local max_scroll = math.max(0, state.content_h - viewport_h)
  state.scroll = math.max(0, math.min(state.scroll, max_scroll))

  local footer_y = height - 56
  ReaADR.set_gfx_color(theme.panel)
  gfx.rect(0, footer_y - 16, width, height - footer_y + 16, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(0, footer_y - 16, width, height - footer_y + 16, false)

  local save_rect = { x = 24, y = footer_y, w = 112, h = 36, label = "Save" }
  local close_rect = { x = 150, y = footer_y, w = 112, h = 36, label = "Close" }
  special_click[#special_click + 1] = { rect = save_rect, kind = "save" }
  special_click[#special_click + 1] = { rect = close_rect, kind = "close" }
  draw_button(save_rect)
  draw_button(close_rect)

  gfx.setfont(1, "Arial", 14)
  if state.dirty then
    ReaADR.set_gfx_color(theme.accent_gold)
    gfx.x = save_rect.x + save_rect.w + 16
    gfx.y = footer_y + 11
    gfx.drawstr("Unsaved changes")
  elseif reaper.time_precise() < state.saved_message_until then
    ReaADR.set_gfx_color(theme.accent_green)
    gfx.x = save_rect.x + save_rect.w + 16
    gfx.y = footer_y + 11
    gfx.drawstr(state.saved_message)
  end

  local scrollbar_rect = nil
  local scrollbar_thumb = nil
  if max_scroll > 0 then
    local track_x = width - 22
    local track_y = math.max(viewport_top, header.content_y + 4)
    local track_h = viewport_h
    local thumb_h = math.max(36, math.floor(track_h * math.max(0.12, viewport_h / math.max(viewport_h, state.content_h))))
    local travel = math.max(0, track_h - thumb_h)
    local thumb_y = track_y + math.floor((state.scroll / max_scroll) * travel)
    scrollbar_rect = { x = track_x, y = track_y, w = 10, h = track_h }
    scrollbar_thumb = { x = track_x + 1, y = thumb_y, w = 8, h = thumb_h }
    ReaADR.set_gfx_color(theme.panel_alt)
    gfx.rect(scrollbar_rect.x, scrollbar_rect.y, scrollbar_rect.w, scrollbar_rect.h, true)
    ReaADR.set_gfx_color(theme.border)
    gfx.rect(scrollbar_rect.x, scrollbar_rect.y, scrollbar_rect.w, scrollbar_rect.h, false)
    ReaADR.set_gfx_color((state.dragging_scrollbar and theme.accent_gold) or theme.highlight)
    gfx.rect(scrollbar_thumb.x, scrollbar_thumb.y, scrollbar_thumb.w, scrollbar_thumb.h, true)
  end

  gfx.update()

  local char = gfx.getchar()
  if char < 0 or char == 27 then
    if state.dirty then
      save()
    end
    ReaADR.save_window_state("overlay_settings")
    gfx.quit()
    return
  end

  local wheel = gfx.mouse_wheel
  if wheel ~= 0 then
    state.scroll = math.max(0, math.min(max_scroll, state.scroll - (wheel > 0 and 28 or -28)))
    gfx.mouse_wheel = 0
  end

  if state.dragging_scrollbar and mouse == 1 and scrollbar_rect and scrollbar_thumb then
    local travel = math.max(0, scrollbar_rect.h - scrollbar_thumb.h)
    if travel > 0 then
      local thumb_y = math.max(scrollbar_rect.y, math.min(gfx.mouse_y - state.scrollbar_drag_offset, scrollbar_rect.y + travel))
      local ratio = (thumb_y - scrollbar_rect.y) / travel
      state.scroll = math.max(0, math.min(max_scroll, math.floor(ratio * max_scroll + 0.5)))
    end
  elseif state.dragging_scrollbar and mouse == 0 then
    state.dragging_scrollbar = false
  end

  if clicked then
    if scrollbar_thumb and inside(scrollbar_thumb, gfx.mouse_x, gfx.mouse_y) then
      state.dragging_scrollbar = true
      state.scrollbar_drag_offset = gfx.mouse_y - scrollbar_thumb.y
      state.last_mouse = mouse
      reaper.defer(loop)
      return
    elseif scrollbar_rect and inside(scrollbar_rect, gfx.mouse_x, gfx.mouse_y) then
      local travel = math.max(0, scrollbar_rect.h - scrollbar_thumb.h)
      if travel > 0 then
        local thumb_y = math.max(scrollbar_rect.y, math.min(gfx.mouse_y - math.floor(scrollbar_thumb.h * 0.5), scrollbar_rect.y + travel))
        local ratio = (thumb_y - scrollbar_rect.y) / travel
        state.scroll = math.max(0, math.min(max_scroll, math.floor(ratio * max_scroll + 0.5)))
      end
    else
      for _, entry in ipairs(special_click) do
        if inside(entry.rect, gfx.mouse_x, gfx.mouse_y) then
          if entry.kind == "profile" then
            apply_profile(entry.key)
            state.dirty = true
          elseif entry.kind == "toggle" then
            settings[entry.key] = not settings[entry.key]
            state.dirty = true
          elseif entry.kind == "metadata" then
            if edit_metadata_fields() then
              state.dirty = true
            end
          elseif entry.kind == "text_color" then
            if settings.text_color ~= entry.value then
              settings.text_color = entry.value
              state.dirty = true
            end
          elseif entry.kind == "save" then
            save()
          elseif entry.kind == "close" then
            save()
            ReaADR.save_window_state("overlay_settings")
            gfx.quit()
            return
          end
          break
        end
      end
    end
  end

  state.last_mouse = mouse
  reaper.defer(loop)
end

loop()
