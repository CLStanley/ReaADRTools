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

local mouse_was_down = false
local dirty = false
local saved_message_until = 0
local saved_message = "Saved"

local window_state = ReaADR.init_persistent_window("overlay_settings", "ReaADR Overlay Settings", {
  width = 760,
  height = 820,
})

local function save()
  ReaADR.save_overlay_settings(settings)
  local overlay_status, overlay_error = ReaADR.refresh_overlay_fx_from_project(settings)
  if overlay_status then
    saved_message = overlay_status == "disabled" and "Saved + disabled" or "Saved + refreshed"
  else
    saved_message = "Saved; import cues first"
  end
  dirty = false
  saved_message_until = reaper.time_precise() + 2.0
end

local function draw_checkbox(x, y, checked, label)
  local theme = ReaADR.ui_theme()
  ReaADR.set_gfx_color(theme.panel_alt)
  gfx.rect(x, y, 18, 18, 0)
  if checked then
    ReaADR.set_gfx_color(theme.accent_gold)
    gfx.rect(x + 4, y + 4, 10, 10, 1)
  end
  ReaADR.set_gfx_color(theme.text)
  gfx.x = x + 30
  gfx.y = y - 1
  gfx.drawstr(label)
end

local function hit(x, y, w, h)
  return gfx.mouse_x >= x and gfx.mouse_x <= x + w and gfx.mouse_y >= y and gfx.mouse_y <= y + h
end

local function draw_button(x, y, w, h, label)
  local theme = ReaADR.ui_theme()
  local hovered = hit(x, y, w, h)
  ReaADR.set_gfx_color(hovered and theme.accent_blue or theme.panel_alt)
  gfx.rect(x, y, w, h, 1)
  ReaADR.set_gfx_color(theme.text)
  local text_w = gfx.measurestr(label)
  gfx.x = x + math.floor((w - text_w) / 2)
  gfx.y = y + 8
  gfx.drawstr(label)
  return hovered
end

local metadata_field_labels = {
  "PGID",
  "MID",
  "Media Time",
  "Watermark Timestamp",
  "Asset Date Code",
  "Project Name",
}

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
      enabled = true,
      show_cue_id = true,
      show_character = true,
      show_dialogue = true,
      show_cue_timecode = true,
      show_project_timer = true,
      show_visual_cue = true,
      show_direction = true,
      show_cue_type = false,
      show_streamer = true,
      show_flash = true,
      show_status = true,
      show_metadata = false,
      bg_project_timer = false,
      bg_cue_id = false,
      bg_character = false,
      bg_cue_timecode = false,
      bg_dialogue = true,
      bg_direction = false,
      bg_cue_type = false,
      bg_status = false,
      bg_metadata = false,
    },
    engineer = {
      enabled = true,
      show_cue_id = true,
      show_character = true,
      show_dialogue = true,
      show_cue_timecode = true,
      show_project_timer = true,
      show_visual_cue = true,
      show_direction = true,
      show_cue_type = true,
      show_streamer = true,
      show_flash = true,
      show_status = true,
      show_metadata = true,
      bg_project_timer = true,
      bg_cue_id = false,
      bg_character = false,
      bg_cue_timecode = true,
      bg_dialogue = true,
      bg_direction = false,
      bg_cue_type = false,
      bg_status = false,
      bg_metadata = false,
    },
    studio = {
      enabled = true,
      show_cue_id = true,
      show_character = true,
      show_dialogue = true,
      show_cue_timecode = true,
      show_project_timer = true,
      show_visual_cue = true,
      show_direction = false,
      show_cue_type = true,
      show_streamer = true,
      show_flash = true,
      show_status = true,
      show_metadata = true,
      bg_project_timer = true,
      bg_cue_id = false,
      bg_character = false,
      bg_cue_timecode = true,
      bg_dialogue = true,
      bg_direction = false,
      bg_cue_type = false,
      bg_status = false,
      bg_metadata = false,
    },
    minimal = {
      enabled = true,
      show_cue_id = true,
      show_character = false,
      show_dialogue = true,
      show_cue_timecode = false,
      show_project_timer = true,
      show_visual_cue = true,
      show_direction = false,
      show_cue_type = false,
      show_streamer = true,
      show_flash = false,
      show_status = false,
      show_metadata = false,
      bg_project_timer = false,
      bg_cue_id = false,
      bg_character = false,
      bg_cue_timecode = false,
      bg_dialogue = true,
      bg_direction = false,
      bg_cue_type = false,
      bg_status = false,
      bg_metadata = false,
    },
  }

  for key, value in pairs(profiles[name] or {}) do
    settings[key] = value
  end
end

local function loop()
  local theme = ReaADR.ui_theme()
  ReaADR.set_gfx_color(theme.bg)
  gfx.rect(0, 0, gfx.w, gfx.h, 1)

  local header = ReaADR.draw_window_header(
    "ReaADR Video Overlay Settings",
    "Configure visible fields, backgrounds, and presets.",
    { x = 20, y = 18, width = gfx.w - 40, height = 74 }
  )

  gfx.setfont(1, "Arial", 16)
  local mouse_down = gfx.mouse_cap % 2 == 1
  local clicked = mouse_down and not mouse_was_down

  local profile_y = math.max(100, header.content_y)
  local profile_buttons = {
    { x = 24, y = profile_y, w = 92, h = 30, label = "Actor", key = "actor" },
    { x = 126, y = profile_y, w = 102, h = 30, label = "Engineer", key = "engineer" },
    { x = 238, y = profile_y, w = 92, h = 30, label = "Studio", key = "studio" },
    { x = 340, y = profile_y, w = 92, h = 30, label = "Minimal", key = "minimal" },
  }
  for _, profile in ipairs(profile_buttons) do
    local hovered = draw_button(profile.x, profile.y, profile.w, profile.h, profile.label)
    if clicked and hovered then
      apply_profile(profile.key)
      dirty = true
    end
  end

  local y = 102
  for _, row in ipairs(rows) do
    draw_checkbox(24, y, settings[row.key], row.label)
    if clicked and hit(20, y - 4, 360, 28) then
      settings[row.key] = not settings[row.key]
      dirty = true
    end
    y = y + 32
  end

  ReaADR.set_gfx_color(theme.muted)
  gfx.x = 24
  gfx.y = y + 4
  gfx.drawstr("Text backgrounds")
  local background_y = y + 34

  for index, row in ipairs(background_rows) do
    local x = index <= 5 and 24 or 386
    local row_y = background_y + (((index - 1) % 5) * 32)
    draw_checkbox(x, row_y, settings[row.key], row.label)
    if clicked and hit(x - 4, row_y - 4, 340, 28) then
      settings[row.key] = not settings[row.key]
      dirty = true
    end
  end

  y = background_y + (5 * 32)

  ReaADR.set_gfx_color(theme.muted)
  gfx.x = 24
  gfx.y = y + 2
  gfx.drawstr("Metadata fields")
  local metadata_hover = draw_button(182, y - 5, 118, 30, "Edit Fields")
  local white_rect = { x = 314, y = y - 5, w = 170, h = 24, value = "white", label = "White general text" }
  local yellow_rect = { x = 314, y = y + 25, w = 170, h = 24, value = "yellow", label = "Yellow general text" }
  ReaADR.set_gfx_color(theme.text)
  gfx.x = 516
  gfx.y = y + 2
  local fields = tostring(settings.metadata_fields or "")
  if #fields > 26 then
    fields = fields:sub(1, 23) .. "..."
  end
  gfx.drawstr(fields)
  gfx.x = 314
  gfx.y = y - 28
  ReaADR.set_gfx_color(theme.muted)
  gfx.drawstr("General overlay text color")
  gfx.setfont(1, "Arial", 14)
  for _, option in ipairs({ white_rect, yellow_rect }) do
    local selected_color = ReaADR.overlay_text_mode(settings) == option.value
    ReaADR.set_gfx_color(theme.panel_alt)
    gfx.circle(option.x + 10, option.y + 11, 8, false, true)
    if selected_color then
      ReaADR.set_gfx_color(theme.accent_gold)
      gfx.circle(option.x + 10, option.y + 11, 4, true, true)
    end
    ReaADR.set_gfx_color(theme.text)
    gfx.x = option.x + 24
    gfx.y = option.y + 2
    gfx.drawstr(option.label)
  end

  if clicked and metadata_hover then
    if edit_metadata_fields() then
      dirty = true
    end
  elseif hit(white_rect.x, white_rect.y, white_rect.w, white_rect.h) then
    if settings.text_color ~= "white" then
      settings.text_color = "white"
      dirty = true
    end
  elseif hit(yellow_rect.x, yellow_rect.y, yellow_rect.w, yellow_rect.h) then
    if settings.text_color ~= "yellow" then
      settings.text_color = "yellow"
      dirty = true
    end
  end

  local footer_y = gfx.h - 64
  ReaADR.set_gfx_color(theme.panel)
  gfx.rect(0, footer_y - 14, gfx.w, gfx.h - footer_y + 14, 1)

  local save_hover  = draw_button(24,  footer_y, 112, 36, "Save")
  local close_hover = draw_button(150, footer_y, 112, 36, "Close")

  -- State indicator sits immediately to the right of the Save button (§7.1)
  gfx.setfont(1, "Arial", 14)
  if dirty then
    ReaADR.set_gfx_color(theme.accent_gold)
    gfx.x = 150
    gfx.y = footer_y + 11
    gfx.drawstr(" \xe2\x80\x93 unsaved changes")
  elseif reaper.time_precise() < saved_message_until then
    ReaADR.set_gfx_color(theme.accent_green)
    gfx.x = 150
    gfx.y = footer_y + 11
    gfx.drawstr(" \xe2\x80\x93 " .. saved_message)
  end

  if clicked and save_hover then
    save()
  elseif clicked and close_hover then
    save()
    ReaADR.save_window_state("overlay_settings")
    gfx.quit()
    return
  end

  mouse_was_down = mouse_down
  if gfx.getchar() >= 0 then
    reaper.defer(loop)
  elseif dirty then
    save()
    ReaADR.save_window_state("overlay_settings")
  else
    ReaADR.save_window_state("overlay_settings")
  end
end

loop()
