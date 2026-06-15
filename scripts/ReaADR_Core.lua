-- ReaADR core helpers shared by the import/generation scripts.
-- The module is intentionally conservative: every generated object is tagged
-- by name and/or project metadata so scripts can be re-run without duplicating
-- tracks, markers, or regions.

local ReaADR = {}

ReaADR.VERSION = "0.1.0"
ReaADR.EXT_NAMESPACE = "ReaADRTools"
ReaADR.NAME_PREFIX = "[ReaADR]"
ReaADR.TRACK_PREFIX = "ADR"

ReaADR.DEFAULT_OVERLAY_SETTINGS = {
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
  show_metadata = false,
  bg_cue_id = false,
  bg_character = false,
  bg_cue_timecode = false,
  bg_project_timer = false,
  bg_dialogue = true,
  bg_direction = false,
  bg_cue_type = false,
  bg_status = false,
  bg_metadata = false,
  text_color = "white",
  metadata_fields = "PGID,MID,Media Time,Watermark Timestamp,Asset Date Code,Project Name",
  preroll_seconds = 3,
}

ReaADR.UI_THEME = {
  bg = { 0.09, 0.08, 0.07, 1.0 },
  panel = { 0.13, 0.12, 0.11, 1.0 },
  panel_alt = { 0.17, 0.16, 0.15, 1.0 },
  border = { 0.42, 0.35, 0.28, 1.0 },
  text = { 0.93, 0.88, 0.78, 1.0 },
  muted = { 0.74, 0.68, 0.60, 1.0 },
  accent_blue = { 0.57, 0.50, 0.39, 1.0 },
  accent_green = { 0.22, 0.32, 0.25, 1.0 },
  accent_red = { 0.42, 0.20, 0.16, 1.0 },
  accent_gold = { 0.76, 0.67, 0.52, 1.0 },
  highlight = { 0.57, 0.50, 0.39, 1.0 },
}

local logo_image_slot = 987
local logo_image_loaded = false
local logo_image_available = false

local function script_file_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local function logo_image_paths()
  local base = script_file_dir()
  local candidates = {
    base .. "/../assets/logo.png",
    base .. "\\..\\assets\\logo.png",
    base .. "/../assets/logo.bmp",
    base .. "\\..\\assets\\logo.bmp",
  }
  local paths = {}
  for _, path in ipairs(candidates) do
    local file = io.open(path, "rb")
    if file then
      file:close()
      paths[#paths + 1] = path
    end
  end
  return paths
end

function ReaADR.ui_theme()
  return ReaADR.UI_THEME
end

function ReaADR.set_gfx_color(rgba)
  if not rgba then
    return
  end
  gfx.set(rgba[1] or 0, rgba[2] or 0, rgba[3] or 0, rgba[4] or 1)
end

function ReaADR.ensure_logo_image()
  if logo_image_loaded then
    return logo_image_available, logo_image_slot
  end
  logo_image_loaded = true
  if not gfx or type(gfx.loadimg) ~= "function" or type(gfx.getimgdim) ~= "function" then
    return false, nil
  end

  for _, path in ipairs(logo_image_paths()) do
    local ok, loaded = pcall(function()
      return gfx.loadimg(logo_image_slot, path)
    end)
    if ok and loaded ~= nil and loaded >= 0 then
      logo_image_slot = tonumber(loaded) or logo_image_slot
      local dim_ok, width, height = pcall(function()
        return gfx.getimgdim(logo_image_slot)
      end)
      if dim_ok and (width or 0) > 0 and (height or 0) > 0 then
        logo_image_available = true
        return true, logo_image_slot
      end
    end
  end
  return logo_image_available, logo_image_slot
end

function ReaADR.draw_logo(x, y, size)
  local ok, slot = ReaADR.ensure_logo_image()
  local theme = ReaADR.ui_theme()
  if ok then
    local dim_ok, width, height = pcall(function()
      return gfx.getimgdim(slot)
    end)
    if dim_ok and width and height and width > 0 and height > 0 then
      size = math.max(16, tonumber(size) or 32)
      local scale = math.min(size / width, size / height)
      local draw_w = width * scale
      local draw_h = height * scale
      gfx.blit(slot, 1, 0, 0, 0, width, height, x, y, draw_w, draw_h)
      return draw_w
    end
  end

  size = math.max(16, tonumber(size) or 32)
  local mid_x = x + (size * 0.5)
  local top_y = y
  local bot_y = y + size
  local left_x = x
  local right_x = x + size
  local shoulder_y = y + (size * 0.28)

  ReaADR.set_gfx_color(theme.panel_alt)
  gfx.triangle(mid_x, top_y, right_x, shoulder_y, right_x - (size * 0.10), bot_y - (size * 0.08), mid_x, bot_y)
  gfx.triangle(mid_x, top_y, left_x, shoulder_y, left_x + (size * 0.10), bot_y - (size * 0.08), mid_x, bot_y)
  ReaADR.set_gfx_color(theme.border)
  gfx.line(mid_x, top_y, right_x, shoulder_y)
  gfx.line(right_x, shoulder_y, right_x - (size * 0.10), bot_y - (size * 0.08))
  gfx.line(right_x - (size * 0.10), bot_y - (size * 0.08), mid_x, bot_y)
  gfx.line(mid_x, bot_y, left_x + (size * 0.10), bot_y - (size * 0.08))
  gfx.line(left_x + (size * 0.10), bot_y - (size * 0.08), left_x, shoulder_y)
  gfx.line(left_x, shoulder_y, mid_x, top_y)

  ReaADR.set_gfx_color(theme.accent_green)
  gfx.triangle(mid_x, y + (size * 0.10), left_x + (size * 0.12), shoulder_y + (size * 0.04), mid_x - (size * 0.04), bot_y - (size * 0.12))
  ReaADR.set_gfx_color(theme.accent_blue)
  gfx.triangle(mid_x, y + (size * 0.10), right_x - (size * 0.12), shoulder_y + (size * 0.04), mid_x + (size * 0.04), bot_y - (size * 0.12))
  ReaADR.set_gfx_color(theme.accent_red)
  gfx.triangle(mid_x - (size * 0.02), y + (size * 0.34), right_x - (size * 0.16), bot_y - (size * 0.14), mid_x + (size * 0.02), bot_y - (size * 0.10))

  ReaADR.set_gfx_color(theme.border)
  gfx.setfont(1, "Arial", math.max(10, math.floor(size * 0.24)))
  ReaADR.set_gfx_color(theme.text)
  gfx.x = x + math.floor(size * 0.18)
  gfx.y = y + math.floor(size * 0.34)
  gfx.drawstr("ADR")

  ReaADR.set_gfx_color(theme.text)
  local wave_y = y + (size * 0.76)
  gfx.line(x + (size * 0.16), wave_y, x + (size * 0.30), wave_y - (size * 0.04))
  gfx.line(x + (size * 0.30), wave_y - (size * 0.04), x + (size * 0.42), wave_y + (size * 0.03))
  gfx.line(x + (size * 0.42), wave_y + (size * 0.03), x + (size * 0.54), wave_y - (size * 0.08))
  gfx.line(x + (size * 0.54), wave_y - (size * 0.08), x + (size * 0.68), wave_y + (size * 0.02))
  gfx.line(x + (size * 0.68), wave_y + (size * 0.02), x + (size * 0.82), wave_y - (size * 0.02))

  return size
end

function ReaADR.draw_window_header(title, subtitle, options)
  options = options or {}
  local theme = ReaADR.ui_theme()
  local x = tonumber(options.x) or 20
  local y = tonumber(options.y) or 16
  local width = tonumber(options.width) or (gfx.w or 800) - (x * 2)
  local height = tonumber(options.height) or 74

  ReaADR.set_gfx_color(theme.panel)
  gfx.rect(x - 6, y - 8, width, height, true)

  local logo_w = ReaADR.draw_logo(x, y, height - 10)
  gfx.setfont(1, "Arial", options.title_size or 22)
  ReaADR.set_gfx_color(theme.text)
  gfx.x = x + logo_w + 14
  gfx.y = y + 2
  gfx.drawstr(title or "")

  if subtitle and subtitle ~= "" then
    gfx.setfont(1, "Arial", options.subtitle_size or 13)
    ReaADR.set_gfx_color(theme.muted)
    gfx.x = x + logo_w + 14
    gfx.y = y + 30
    gfx.drawstr(subtitle)
  end

  return {
    content_y = y + height + 10,
    logo_w = logo_w,
  }
end

local ROLE_COLORS = {
  cues = { 235, 198, 80 },
  source_video = { 93, 173, 226 },
  inactive = { 85, 85, 85 },
}

local STATUS_COLORS = {
  ["not recorded"] = { 120, 120, 120 },
  ["recorded"] = { 0, 174, 239 },
  ["approved"] = { 0, 210, 90 },
  ["needs retake"] = { 255, 0, 0 },
}

local CHARACTER_COLORS = {
  { 236, 112, 99 },
  { 175, 122, 197 },
  { 72, 201, 176 },
  { 245, 176, 65 },
  { 84, 153, 199 },
  { 220, 118, 51 },
  { 127, 179, 213 },
  { 130, 224, 170 },
}

local function trim(value)
  return tostring(value or ""):match("^%s*(.-)%s*$")
end

local function sanitize_token(value)
  value = trim(value)
  value = value:gsub("%s+", "_")
  value = value:gsub("[^%w_%-%.]", "")
  return value
end

local function normalize_header(value)
  value = trim(value):lower()
  value = value:gsub("[^%w]+", "_")
  value = value:gsub("_+", "_")
  value = value:gsub("^_+", ""):gsub("_+$", "")
  return value
end

local function first_nonempty(...)
  for i = 1, select("#", ...) do
    local value = trim(select(i, ...))
    if value ~= "" then
      return value
    end
  end
  return ""
end

local ADR_FIELD_ALIASES = {
  cue_id = { "cue_id", "cue_number", "cue_num", "cue_no", "cue", "id", "number" },
  character = { "character", "char", "actor", "speaker", "performer", "talent", "role" },
  start = { "start", "start_time", "timecode", "tc", "in_time", "in" },
  ["end"] = { "end", "end_time", "out_time", "out" },
  line = { "line", "dialogue", "dialog", "text", "script" },
  notes = { "notes", "note" },
  direction = { "direction", "performance_direction", "perf_direction" },
  cue_type = { "cue_type", "type", "category" },
  status = { "status", "cue_status" },
}

local REQUIRED_ADR_FIELDS = { "cue_id", "character", "start", "end" }

local function header_has_any(headers, aliases)
  local present = {}
  for _, header in ipairs(headers or {}) do
    present[header] = true
  end
  for _, alias in ipairs(aliases or {}) do
    if present[alias] then
      return true
    end
  end
  return false
end

local function missing_required_csv_columns(headers)
  local missing = {}
  for _, field in ipairs(REQUIRED_ADR_FIELDS) do
    local aliases = ADR_FIELD_ALIASES[field] or {}
    if not header_has_any(headers, aliases) then
      missing[#missing + 1] = field .. " (" .. table.concat(aliases, ", ") .. ")"
    end
  end
  return missing
end

local function native_color(rgb)
  return reaper.ColorToNative(rgb[1], rgb[2], rgb[3]) + 0x1000000
end

local function character_color(character)
  character = trim(character)
  local hash = 0
  for i = 1, #character do
    hash = hash + string.byte(character, i)
  end
  return native_color(CHARACTER_COLORS[(hash % #CHARACTER_COLORS) + 1])
end

local function normalize_status(status)
  status = trim(status)
  if status == "" then
    return "Not Recorded"
  end
  local lowered = status:lower():gsub("[%s_%-]+", " ")
  if lowered == "not recorded" or lowered == "notrecorded" or lowered == "pending" then
    return "Not Recorded"
  end
  if lowered == "recorded" then
    return "Recorded"
  end
  if lowered == "approved" then
    return "Approved"
  end
  if lowered == "needs retake" or lowered == "retake" or lowered == "needsretake" then
    return "Needs Retake"
  end
  return status
end

local function status_color(status, fallback)
  local normalized = normalize_status(status):lower()
  local rgb = STATUS_COLORS[normalized]
  if rgb then
    return native_color(rgb)
  end
  return fallback
end

local function status_rgb(status)
  return STATUS_COLORS[normalize_status(status):lower()] or STATUS_COLORS["not recorded"]
end

local function format_timecode(seconds, frame_rate)
  frame_rate = math.max(1, math.floor((tonumber(frame_rate) or 24) + 0.5))
  local total_frames = math.floor(math.max(0, tonumber(seconds) or 0) * frame_rate + 0.5)
  local frames = total_frames % frame_rate
  local total_seconds = math.floor(total_frames / frame_rate)
  local display_seconds = total_seconds % 60
  local total_minutes = math.floor(total_seconds / 60)
  local display_minutes = total_minutes % 60
  local hours = math.floor(total_minutes / 60)
  return ("%02d:%02d:%02d:%02d"):format(hours, display_minutes, display_seconds, frames)
end

local function delimited_split(line, delimiter)
  delimiter = delimiter or ","
  local fields = {}
  local field = {}
  local in_quotes = false
  local i = 1

  while i <= #line do
    local char = line:sub(i, i)
    local next_char = line:sub(i + 1, i + 1)

    if char == '"' then
      if in_quotes and next_char == '"' then
        field[#field + 1] = '"'
        i = i + 1
      else
        in_quotes = not in_quotes
      end
    elseif char == delimiter and not in_quotes then
      fields[#fields + 1] = table.concat(field)
      field = {}
    else
      field[#field + 1] = char
    end

    i = i + 1
  end

  fields[#fields + 1] = table.concat(field)
  return fields
end

local function csv_split(line)
  return delimited_split(line, ",")
end

local function delimiter_for_content(content, path)
  local extension = tostring(path or ""):lower():match("%.([^%.\\/]+)$")
  if extension == "tsv" or extension == "tab" then
    return "\t"
  end

  local first_line = tostring(content or ""):match("([^\n\r]+)") or ""
  local tabs = select(2, first_line:gsub("\t", ""))
  local commas = select(2, first_line:gsub(",", ""))
  if tabs > commas then
    return "\t"
  end
  return ","
end

local function delimiter_name(delimiter)
  return delimiter == "\t" and "TSV" or "CSV"
end

local function csv_escape(value)
  value = tostring(value or "")
  local needs_quotes = value:find('[,"\r\n]') ~= nil
  value = value:gsub('"', '""')
  if needs_quotes then
    return '"' .. value .. '"'
  end
  return value
end

local function split_list(value)
  local list = {}
  for item in tostring(value or ""):gmatch("([^,]+)") do
    item = trim(item)
    if item ~= "" then
      list[#list + 1] = item
    end
  end
  return list
end

local function read_file(path)
  local file = io.open(path, "r")
  if not file then
    return nil, "Could not open file: " .. tostring(path)
  end

  local content = file:read("*a")
  file:close()
  return content
end

local function file_exists(path)
  local file = io.open(path, "rb")
  if file then
    file:close()
    return true
  end
  return false
end

local CUE_CACHE_FIELDS = {
  "id",
  "character",
  "start_time",
  "end_time",
  "line",
  "notes",
  "direction",
  "cue_type",
  "source_line",
  "status",
  "metadata",
}

local function encode_cache_field(value)
  return tostring(value or ""):gsub("([^%w%-%_%. ])", function(char)
    return ("%%%02X"):format(string.byte(char))
  end)
end

local function decode_cache_field(value)
  return tostring(value or ""):gsub("%%(%x%x)", function(hex)
    return string.char(tonumber(hex, 16))
  end)
end

local function serialize_metadata(metadata)
  if type(metadata) ~= "table" then
    return ""
  end
  local keys = {}
  for key, value in pairs(metadata) do
    if trim(value) ~= "" then
      keys[#keys + 1] = key
    end
  end
  table.sort(keys)

  local fields = {}
  for _, key in ipairs(keys) do
    fields[#fields + 1] = encode_cache_field(key) .. "=" .. encode_cache_field(metadata[key])
  end
  return table.concat(fields, "&")
end

local function deserialize_metadata(value)
  local metadata = {}
  for pair in tostring(value or ""):gmatch("([^&]+)") do
    local key, raw_value = pair:match("^([^=]*)=(.*)$")
    key = decode_cache_field(key or "")
    if key ~= "" then
      metadata[key] = decode_cache_field(raw_value or "")
    end
  end
  return metadata
end

local function bool_to_string(value)
  return value and "1" or "0"
end

local function string_to_bool(value, default)
  if value == nil or value == "" then
    return default
  end
  return value == "1" or value == "true" or value == "yes"
end

local function project()
  return 0
end

local function call_progress(callback, message, current, total)
  if callback then
    pcall(callback, message, current, total)
  end
end

function ReaADR.message(text)
  reaper.ShowMessageBox(tostring(text), "ReaADR", 0)
end

function ReaADR.bump_session_revision()
  local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "session_revision")
  local revision = (tonumber(value) or 0) + 1
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "session_revision", tostring(revision))
  return revision
end

function ReaADR.session_revision()
  local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "session_revision")
  return tonumber(value) or 0
end

function ReaADR.set_cue_info_launch_options(options)
  options = options or {}
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "cue_info_open_edit", options.edit and "1" or "")
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "cue_info_close_on_save", options.close_on_save and "1" or "")
end

function ReaADR.consume_cue_info_launch_options()
  local _, edit = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "cue_info_open_edit")
  local _, close_on_save = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "cue_info_close_on_save")
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "cue_info_open_edit", "")
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "cue_info_close_on_save", "")
  return {
    edit = edit == "1",
    close_on_save = close_on_save == "1",
  }
end

function ReaADR.create_progress_window(title)
  local state = {
    title = title or "ReaADR",
    width = 460,
    height = 128,
    min_width = 360,
    min_height = 110,
    last_message = "",
    closed = false,
  }

  gfx.init(state.title, state.width, state.height)
  gfx.clear = 0x202020

  local function draw(message, current, total)
    if state.closed then
      return
    end
    state.width = math.max(state.min_width, gfx.w or state.width)
    state.height = math.max(state.min_height, gfx.h or state.height)

    current = math.max(0, tonumber(current) or 0)
    total = math.max(1, tonumber(total) or 1)
    local progress = math.max(0, math.min(1, current / total))
    state.last_message = tostring(message or state.last_message or "")

    gfx.set(0.12, 0.12, 0.12, 1)
    gfx.rect(0, 0, state.width, state.height, true)

    gfx.setfont(1, "Arial", 20)
    gfx.set(1, 1, 1, 1)
    gfx.x = 22
    gfx.y = 18
    gfx.drawstr(state.title)

    gfx.setfont(1, "Arial", 15)
    gfx.set(0.84, 0.84, 0.84, 1)
    gfx.x = 22
    gfx.y = 50
    gfx.drawstr(state.last_message)

    local bar_x = 22
    local bar_y = 82
    local bar_w = state.width - 44
    local bar_h = 20
    gfx.set(0.28, 0.28, 0.28, 1)
    gfx.rect(bar_x, bar_y, bar_w, bar_h, true)
    gfx.set(0.25, 0.68, 0.92, 1)
    gfx.rect(bar_x, bar_y, math.floor(bar_w * progress), bar_h, true)
    gfx.set(0.9, 0.9, 0.9, 1)
    gfx.rect(bar_x, bar_y, bar_w, bar_h, false)

    gfx.setfont(1, "Arial", 13)
    gfx.x = bar_x
    gfx.y = bar_y + 26
    gfx.drawstr(("%d%%"):format(math.floor(progress * 100 + 0.5)))
    gfx.update()
  end

  draw("Starting...", 0, 1)

  return {
    update = draw,
    close = function()
      state.closed = true
      if gfx.quit then
        gfx.quit()
      end
    end,
  }
end

function ReaADR.get_setting(key, default)
  local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "overlay." .. key)
  if value == "" then
    return default
  end
  return value
end

function ReaADR.set_setting(key, value)
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "overlay." .. key, tostring(value))
end

function ReaADR.overlay_text_mode(settings)
  local value = trim(settings and settings.text_color or ReaADR.get_setting("text_color", ReaADR.DEFAULT_OVERLAY_SETTINGS.text_color))
  value = value:lower()
  if value ~= "yellow" then
    value = "white"
  end
  return value
end

function ReaADR.overlay_text_rgba_expr(settings)
  if ReaADR.overlay_text_mode(settings) == "yellow" then
    return "1, 0.93, 0.48, 1"
  end
  return "1, 1, 1, 1"
end

function ReaADR.window_layout_enabled()
  local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "ui.remember_window_layout")
  if value == "" then
    return false
  end
  return value == "1" or value == "true" or value == "yes"
end

function ReaADR.set_window_layout_enabled(enabled)
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "ui.remember_window_layout", enabled and "1" or "0")
end

function ReaADR.cue_hover_preview_enabled()
  local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "ui.cue_hover_preview")
  if value == "" then
    return true
  end
  return value == "1" or value == "true" or value == "yes"
end

function ReaADR.set_cue_hover_preview_enabled(enabled)
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "ui.cue_hover_preview", enabled and "1" or "0")
end

local function ui_window_key(window_id, field)
  return ("ui.window.%s.%s"):format(sanitize_token(window_id), field)
end

function ReaADR.load_window_state(window_id, defaults)
  defaults = defaults or {}
  local state = {
    width = tonumber(defaults.width) or 800,
    height = tonumber(defaults.height) or 600,
    dock = tonumber(defaults.dock) or 0,
    x = tonumber(defaults.x),
    y = tonumber(defaults.y),
  }
  if not ReaADR.window_layout_enabled() then
    return state
  end

  local _, width = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "width"))
  local _, height = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "height"))
  local _, dock = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "dock"))
  local _, x = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "x"))
  local _, y = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "y"))

  state.width = tonumber(width) or state.width
  state.height = tonumber(height) or state.height
  state.dock = tonumber(dock) or state.dock
  state.x = tonumber(x) or state.x
  state.y = tonumber(y) or state.y
  return state
end

function ReaADR.init_persistent_window(window_id, title, defaults)
  local state = ReaADR.load_window_state(window_id, defaults)
  gfx.init(title, state.width, state.height, state.dock, state.x, state.y)
  return state
end

function ReaADR.save_window_geometry(window_id, geometry)
  geometry = geometry or {}
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "dock"), tostring(tonumber(geometry.dock) or 0))
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "x"), tostring(tonumber(geometry.x) or 0))
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "y"), tostring(tonumber(geometry.y) or 0))
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "width"), tostring(tonumber(geometry.width) or 0))
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "height"), tostring(tonumber(geometry.height) or 0))
  return true
end

function ReaADR.save_window_state(window_id)
  if not ReaADR.window_layout_enabled() then
    return false
  end
  if not gfx or not gfx.w or not gfx.h or not gfx.dock then
    return false
  end

  local ok, dock, x, y, width, height = pcall(gfx.dock, -1, 0, 0, 0, 0)
  if not ok then
    dock = 0
    x = 0
    y = 0
    width = gfx.w
    height = gfx.h
  end

  return ReaADR.save_window_geometry(window_id, {
    dock = dock,
    x = x,
    y = y,
    width = tonumber(width) or gfx.w or 0,
    height = tonumber(height) or gfx.h or 0,
  })
end

function ReaADR.load_overlay_settings()
  local settings = {}
  for key, default in pairs(ReaADR.DEFAULT_OVERLAY_SETTINGS) do
    local value = ReaADR.get_setting(key, "")
    if type(default) == "boolean" then
      settings[key] = string_to_bool(value, default)
    elseif type(default) == "number" then
      settings[key] = tonumber(value) or default
    else
      settings[key] = value ~= "" and value or default
    end
  end
  return settings
end

function ReaADR.save_overlay_settings(settings)
  for key, default in pairs(ReaADR.DEFAULT_OVERLAY_SETTINGS) do
    local value = settings[key]
    if value == nil then
      value = default
    end

    if type(default) == "boolean" then
      ReaADR.set_setting(key, bool_to_string(value))
    else
      ReaADR.set_setting(key, value)
    end
  end
end

function ReaADR.save_column_mapping_preset(name, mapping)
  name = sanitize_token(name)
  if name == "" then
    name = "last"
  end
  local fields = {}
  for key, value in pairs(mapping or {}) do
    if trim(value) ~= "" then
      fields[#fields + 1] = encode_cache_field(key) .. "=" .. encode_cache_field(value)
    end
  end
  table.sort(fields)
  reaper.SetExtState(ReaADR.EXT_NAMESPACE, "column_mapping." .. name, table.concat(fields, "&"), true)
end

function ReaADR.load_column_mapping_preset(name)
  name = sanitize_token(name)
  if name == "" then
    name = "last"
  end
  local value = reaper.GetExtState(ReaADR.EXT_NAMESPACE, "column_mapping." .. name)
  if value == "" then
    return {}
  end
  local mapping = {}
  for pair in value:gmatch("([^&]+)") do
    local key, raw_value = pair:match("^([^=]*)=(.*)$")
    key = decode_cache_field(key or "")
    if key ~= "" then
      mapping[key] = decode_cache_field(raw_value or "")
    end
  end
  return mapping
end

function ReaADR.configure_project_preroll(seconds)
  seconds = math.max(0, tonumber(seconds) or ReaADR.DEFAULT_OVERLAY_SETTINGS.preroll_seconds)
  local attempts = 0
  local applied = 0

  local function mark(result)
    attempts = attempts + 1
    if result ~= false and result ~= nil then
      applied = applied + 1
    end
  end

  local function set_config_string(name, value)
    if type(reaper.set_config_var_string) ~= "function" then
      return
    end
    local ok, result = pcall(reaper.set_config_var_string, name, tostring(value))
    if ok then
      mark(result)
    else
      attempts = attempts + 1
    end
  end

  local function set_sws_double(name, value)
    if type(reaper.SNM_SetDoubleConfigVar) ~= "function" then
      return
    end
    local ok, result = pcall(reaper.SNM_SetDoubleConfigVar, name, value)
    if ok then
      mark(result)
    else
      attempts = attempts + 1
    end
  end

  local function set_sws_int(name, value)
    if type(reaper.SNM_SetIntConfigVar) ~= "function" then
      return
    end
    local ok, result = pcall(reaper.SNM_SetIntConfigVar, name, value)
    if ok then
      mark(result)
    else
      attempts = attempts + 1
    end
  end

  local seconds_text = ("%.6f"):format(seconds)

  -- REAPER does not currently document pre-roll fields through GetSetProjectInfo.
  -- These config variables are applied only when the host exposes them.
  set_config_string("preroll", seconds_text)
  set_sws_double("preroll", seconds)
  set_config_string("prerollmeas", "0")
  set_sws_int("prerollmeas", 0)

  for _, key in ipairs({
    "prerollplay",
    "preroll_play",
    "prerollplayback",
    "preroll_playback",
    "prerollrec",
    "preroll_record",
    "prerollrecord",
    "preroll_recording",
  }) do
    set_config_string(key, "1")
    set_sws_int(key, 1)
  end

  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "project_preroll_seconds", seconds_text)
  if applied > 0 then
    reaper.MarkProjectDirty(project())
  end

  return {
    seconds = seconds,
    attempts = attempts,
    applied = applied,
    status = applied > 0 and "configured" or "not_available",
  }
end

function ReaADR.save_last_import_cues(cues)
  local lines = {}
  for _, cue in ipairs(cues or {}) do
    local fields = {}
    for index, key in ipairs(CUE_CACHE_FIELDS) do
      local value = key == "metadata" and serialize_metadata(cue.metadata) or cue[key]
      fields[index] = encode_cache_field(value)
    end
    lines[#lines + 1] = table.concat(fields, "\t")
  end

  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "last_import_cues_v1", table.concat(lines, "\n"))
  ReaADR.bump_session_revision()
end

function ReaADR.load_last_import_cues()
  local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "last_import_cues_v1")
  if value == "" then
    return nil, "No cached cue sheet data found. Run Import Cue Sheet once first."
  end

  local cues = {}
  value = value .. "\n"
  for line in value:gmatch("([^\n]*)\n") do
    if line ~= "" then
      local cue = {}
      local field_index = 1
      for raw_field in (line .. "\t"):gmatch("([^\t]*)\t") do
        local key = CUE_CACHE_FIELDS[field_index]
        if key then
          cue[key] = decode_cache_field(raw_field)
        end
        field_index = field_index + 1
      end

      cue.start_time = tonumber(cue.start_time) or 0
      cue.end_time = tonumber(cue.end_time) or cue.start_time
      cue.source_line = tonumber(cue.source_line) or 0
      cue.id = cue.id or ""
      cue.character = cue.character or ""
      cue.line = cue.line or ""
      cue.notes = cue.notes or ""
      cue.direction = cue.direction or ""
      cue.cue_type = cue.cue_type or ""
      cue.status = normalize_status(cue.status)
      cue.metadata = deserialize_metadata(cue.metadata)
      cues[#cues + 1] = cue
    end
  end

  if #cues == 0 then
    return nil, "Cached cue sheet data is empty. Run Import Cue Sheet again."
  end

  return cues
end

function ReaADR.format_timecode(seconds, frame_rate)
  return format_timecode(seconds, frame_rate)
end

function ReaADR.parse_timecode(value, frame_rate)
  value = trim(value)
  if value == "" then
    return nil, "Missing time value"
  end

  local seconds = tonumber(value)
  if seconds then
    return seconds
  end

  local hh, mm, ss, ff = value:match("^(%d+):(%d+):(%d+):(%d+)$")
  if hh then
    frame_rate = tonumber(frame_rate) or 24
    return (tonumber(hh) * 3600) + (tonumber(mm) * 60) + tonumber(ss) + (tonumber(ff) / frame_rate)
  end

  hh, mm, ss = value:match("^(%d+):(%d+):(%d+%.?%d*)$")
  if hh then
    return (tonumber(hh) * 3600) + (tonumber(mm) * 60) + tonumber(ss)
  end

  mm, ss = value:match("^(%d+):(%d+%.?%d*)$")
  if mm then
    return (tonumber(mm) * 60) + tonumber(ss)
  end

  return nil, "Unsupported time format: " .. value
end

local function normalized_header_map(headers)
  local map = {}
  for _, header in ipairs(headers or {}) do
    local normalized = normalize_header(header)
    if normalized ~= "" and not map[normalized] then
      map[normalized] = normalized
    end
  end
  return map
end

local function mapping_from_aliases(headers)
  local present = normalized_header_map(headers)
  local mapping = {}
  for field, aliases in pairs(ADR_FIELD_ALIASES) do
    for _, alias in ipairs(aliases) do
      if present[alias] then
        mapping[field] = alias
        break
      end
    end
  end
  return mapping
end

local function normalize_mapping(mapping)
  local normalized = {}
  for field, source in pairs(mapping or {}) do
    local normalized_source = normalize_header(source)
    if normalized_source ~= "" then
      normalized[field] = normalized_source
    end
  end
  return normalized
end

local function missing_required_mapping(mapping)
  local missing = {}
  for _, field in ipairs(REQUIRED_ADR_FIELDS) do
    if not mapping[field] or mapping[field] == "" then
      missing[#missing + 1] = field
    end
  end
  return missing
end

local function parse_delimited_content(content, path)
  content = tostring(content or ""):gsub("\r\n", "\n"):gsub("\r", "\n")
  local delimiter = delimiter_for_content(content, path)
  local headers
  local rows = {}
  local raw_headers = {}
  local line_number = 0

  content = content .. "\n"
  for raw_line in content:gmatch("([^\n]*)\n") do
    line_number = line_number + 1
    local line = raw_line:gsub("^\239\187\191", "")

    if trim(line) ~= "" then
      local fields = delimited_split(line, delimiter)

      if not headers then
        headers = {}
        for index, header in ipairs(fields) do
          raw_headers[index] = trim(header)
          headers[index] = normalize_header(header)
        end
      else
        local row = { _line_number = line_number, _raw = {} }
        for index, header in ipairs(headers) do
          local value = trim(fields[index])
          row[header] = value
          row._raw[header] = {
            value = value,
            label = raw_headers[index] ~= "" and raw_headers[index] or header,
          }
        end
        rows[#rows + 1] = row
      end
    end
  end

  if not headers then
    return nil, "Cue sheet is empty"
  end

  return {
    headers = headers,
    raw_headers = raw_headers,
    rows = rows,
    delimiter = delimiter,
    delimiter_name = delimiter_name(delimiter),
  }
end

function ReaADR.inspect_script_file(path)
  local content, read_error = read_file(path)
  if not content then
    return nil, read_error
  end
  return parse_delimited_content(content, path)
end

function ReaADR.default_column_mapping(headers)
  return mapping_from_aliases(headers or {})
end

function ReaADR.required_mapping_missing(mapping)
  return missing_required_mapping(normalize_mapping(mapping or {}))
end

function ReaADR.parse_script_file(path, frame_rate, mapping)
  local table_data, read_error = ReaADR.inspect_script_file(path)
  if not table_data then
    return nil, read_error
  end

  mapping = normalize_mapping(mapping or mapping_from_aliases(table_data.headers))
  local missing_mapping = missing_required_mapping(mapping)
  if #missing_mapping > 0 then
    local missing = missing_required_csv_columns(table_data.headers)
    if #missing > 0 then
      return nil, "Cue sheet is missing required column mapping(s):\n\n" .. table.concat(missing, "\n"), {
        code = "missing_required_mapping",
        headers = table_data.raw_headers,
        normalized_headers = table_data.headers,
        mapping = mapping,
        delimiter_name = table_data.delimiter_name,
      }
    end
    return nil, "Cue sheet is missing required column mapping(s):\n\n" .. table.concat(missing_mapping, "\n"), {
      code = "missing_required_mapping",
      headers = table_data.raw_headers,
      normalized_headers = table_data.headers,
      mapping = mapping,
      delimiter_name = table_data.delimiter_name,
    }
  end

  local cues = {}
  local seen_cue_keys = {}

  for _, row in ipairs(table_data.rows) do
    local cue_id = first_nonempty(row[mapping.cue_id], tostring(#cues + 1))
    local character = first_nonempty(row[mapping.character], "Unassigned")
    local start_value = row[mapping.start]
    local end_value = row[mapping["end"]]
    local cue_key = sanitize_token(cue_id)
    local start_seconds, start_error = ReaADR.parse_timecode(start_value, frame_rate)
    local end_seconds, end_error = ReaADR.parse_timecode(end_value, frame_rate)
    local line_number = row._line_number or 0

    if cue_key == "" then
      return nil, ("Line %d: cue_id is required"):format(line_number)
    end
    if seen_cue_keys[cue_key] then
      return nil, ("Line %d cue %s: duplicate cue_id"):format(line_number, cue_id)
    end

    if not start_seconds then
      return nil, ("Line %d cue %s: %s"):format(line_number, cue_id, start_error)
    end
    if not end_seconds then
      return nil, ("Line %d cue %s: %s"):format(line_number, cue_id, end_error)
    end
    if end_seconds <= start_seconds then
      return nil, ("Line %d cue %s: end must be after start"):format(line_number, cue_id)
    end

    local used_headers = {}
    for _, source in pairs(mapping) do
      used_headers[source] = true
    end

    local metadata = {}
    for header, raw in pairs(row._raw or {}) do
      if not used_headers[header] and raw.value ~= "" then
        metadata[raw.label or header] = raw.value
      end
    end

    seen_cue_keys[cue_key] = true
    cues[#cues + 1] = {
      id = trim(cue_id),
      character = trim(character),
      start_time = start_seconds,
      end_time = end_seconds,
      line = mapping.line and row[mapping.line] or "",
      notes = mapping.notes and row[mapping.notes] or "",
      direction = mapping.direction and row[mapping.direction] or "",
      cue_type = mapping.cue_type and row[mapping.cue_type] or "",
      status = normalize_status(mapping.status and row[mapping.status] or ""),
      source_line = line_number,
      metadata = metadata,
    }
  end

  if #cues == 0 then
    return nil, "Cue sheet contains no cues"
  end

  return cues
end

function ReaADR.parse_csv(path, frame_rate)
  return ReaADR.parse_script_file(path, frame_rate)
end

local function track_ext(track, key)
  local ok, value = reaper.GetSetMediaTrackInfo_String(track, "P_EXT:" .. key, "", false)
  if ok then
    return value
  end
  return ""
end

local function set_track_ext(track, key, value)
  reaper.GetSetMediaTrackInfo_String(track, "P_EXT:" .. key, tostring(value or ""), true)
end

local function set_track_name(track, name)
  reaper.GetSetMediaTrackInfo_String(track, "P_NAME", name, true)
end

local function set_track_color(track, color)
  if color then
    reaper.SetMediaTrackInfo_Value(track, "I_CUSTOMCOLOR", color)
  end
end

local function set_track_muted(track, muted)
  reaper.SetMediaTrackInfo_Value(track, "B_MUTE", muted and 1 or 0)
end

local function get_track_name(track)
  local ok, value = reaper.GetSetMediaTrackInfo_String(track, "P_NAME", "", false)
  if ok then
    return value
  end
  return ""
end

local function source_looks_like_video(source)
  if not source then
    return false
  end

  local source_type = ""
  if reaper.GetMediaSourceType then
    source_type = tostring(reaper.GetMediaSourceType(source, "") or ""):lower()
    if source_type:find("video", 1, true) or source_type:find("ffmpeg", 1, true) then
      return true
    end
  end

  local filename = ""
  if reaper.GetMediaSourceFileName then
    filename = tostring(reaper.GetMediaSourceFileName(source, "") or ""):lower()
  end
  return filename:match("%.mov$") or filename:match("%.mp4$") or filename:match("%.m4v$") or
    filename:match("%.avi$") or filename:match("%.mkv$") or filename:match("%.webm$") or
    filename:match("%.mpeg$") or filename:match("%.mpg$")
end

local function track_has_video_media(track)
  for item_index = 0, reaper.CountTrackMediaItems(track) - 1 do
    local item = reaper.GetTrackMediaItem(track, item_index)
    for take_index = 0, reaper.CountTakes(item) - 1 do
      local take = reaper.GetTake(item, take_index)
      if take then
        local source = reaper.GetMediaItemTake_Source(take)
        if source_looks_like_video(source) then
          return true
        end
      end
    end
  end
  return false
end

function ReaADR.find_existing_video_track()
  for track_index = 0, reaper.CountTracks(project()) - 1 do
    local track = reaper.GetTrack(project(), track_index)
    if track_has_video_media(track) then
      return track
    end
  end
  return nil
end

function ReaADR.mark_source_video_track(track, color)
  if not track then
    return
  end
  set_track_ext(track, "ReaADR.role", "source_video")
  set_track_ext(track, "ReaADR.key", "source_video")
  set_track_ext(track, "ReaADR.version", ReaADR.VERSION)
  set_track_color(track, color or native_color(ROLE_COLORS.source_video))
end

function ReaADR.require_existing_video_track()
  local track = ReaADR.find_existing_video_track()
  if not track then
    return nil, "No video item was found in the project. Import or place the video in the timeline before building ReaADR cues."
  end
  ReaADR.mark_source_video_track(track, native_color(ROLE_COLORS.source_video))
  return track
end

local function le16(value)
  value = math.floor(value or 0)
  return string.char(value % 256, math.floor(value / 256) % 256)
end

local function le32(value)
  value = math.floor(value or 0)
  return string.char(
    value % 256,
    math.floor(value / 256) % 256,
    math.floor(value / 65536) % 256,
    math.floor(value / 16777216) % 256
  )
end

function ReaADR.project_cue_audio_path()
  local project_path = reaper.GetProjectPath("")
  if not project_path or project_path == "" then
    project_path = reaper.GetResourcePath() or "."
  end
  return project_path .. "/reaadr_cue.wav"
end

local function file_exists(path)
  local file = path and io.open(path, "rb")
  if file then
    file:close()
    return true
  end
  return false
end

local function read_le32(blob, offset)
  local b1, b2, b3, b4 = blob:byte(offset, offset + 3)
  if not b1 or not b2 or not b3 or not b4 then
    return nil
  end
  return b1 + (b2 * 256) + (b3 * 65536) + (b4 * 16777216)
end

local function wav_duration_seconds(path)
  local file = path and io.open(path, "rb")
  if not file then
    return nil
  end
  local header = file:read(44)
  file:close()
  if not header or #header < 44 or header:sub(1, 4) ~= "RIFF" or header:sub(9, 12) ~= "WAVE" then
    return nil
  end
  local channels = header:byte(23) + (header:byte(24) * 256)
  local sample_rate = read_le32(header, 25)
  local bits = header:byte(35) + (header:byte(36) * 256)
  local data_size = read_le32(header, 41)
  if not channels or not sample_rate or not bits or not data_size or channels <= 0 or sample_rate <= 0 or bits <= 0 then
    return nil
  end
  return data_size / (sample_rate * channels * (bits / 8))
end

function ReaADR.generate_project_cue_wav(path, frame_rate)
  path = path or ReaADR.project_cue_audio_path()
  frame_rate = tonumber(frame_rate)
  if not frame_rate or frame_rate <= 0 then
    frame_rate = reaper.TimeMap_curFrameRate(project())
  end
  if not frame_rate or frame_rate <= 0 then
    frame_rate = 24
  end

  local sample_rate = 48000
  local channels = 1
  local bits = 16
  local beep_seconds = 1 / frame_rate
  local interval_seconds = 1.0
  local total_seconds = 3.0
  local starts = { 0, interval_seconds, interval_seconds * 2 }
  if file_exists(path) then
    local duration = wav_duration_seconds(path)
    if duration and math.abs(duration - total_seconds) <= 0.02 then
      return path, {
        status = "skipped",
        reason = "exists",
        total_seconds = duration,
      }
    end
  end

  local total_samples = math.max(1, math.floor(total_seconds * sample_rate + 0.5))
  local beep_samples = math.max(1, math.floor(beep_seconds * sample_rate + 0.5))
  local amplitude = 0.72
  local frequency = 1000
  local sample_values = {}

  for i = 1, total_samples do
    sample_values[i] = 0
  end
  for _, start_seconds in ipairs(starts) do
    local start_sample = math.floor(start_seconds * sample_rate + 0.5) + 1
    local end_sample = math.min(total_samples, start_sample + beep_samples - 1)
    for sample = start_sample, end_sample do
      local t = (sample - start_sample) / sample_rate
      sample_values[sample] = math.sin(2 * math.pi * frequency * t) * amplitude
    end
  end

  local data = {}
  for i = 1, total_samples do
    local value = math.max(-1, math.min(1, sample_values[i] or 0))
    local int_value = math.floor(value * 32767)
    if int_value < 0 then
      int_value = int_value + 65536
    end
    data[#data + 1] = le16(int_value)
  end
  local data_blob = table.concat(data)
  local byte_rate = sample_rate * channels * bits / 8
  local block_align = channels * bits / 8
  local file = io.open(path, "wb")
  if not file then
    return nil, "Could not write cue WAV: " .. tostring(path)
  end

  file:write("RIFF")
  file:write(le32(36 + #data_blob))
  file:write("WAVEfmt ")
  file:write(le32(16))
  file:write(le16(1))
  file:write(le16(channels))
  file:write(le32(sample_rate))
  file:write(le32(byte_rate))
  file:write(le16(block_align))
  file:write(le16(bits))
  file:write("data")
  file:write(le32(#data_blob))
  file:write(data_blob)
  file:close()
  return path, {
    status = "created",
    frame_rate = frame_rate,
    beep_seconds = beep_seconds,
    interval_seconds = interval_seconds,
    total_seconds = total_seconds,
  }
end

function ReaADR.find_track_by_ext(role, key)
  key = key or ""
  for i = 0, reaper.CountTracks(project()) - 1 do
    local track = reaper.GetTrack(project(), i)
    if track_ext(track, "ReaADR.role") == role and track_ext(track, "ReaADR.key") == key then
      return track
    end
  end
  return nil
end

function ReaADR.ensure_track(role, name, key, color)
  key = key or ""

  local existing = ReaADR.find_track_by_ext(role, key)
  if existing then
    set_track_color(existing, color)
    return existing, false
  end

  for i = 0, reaper.CountTracks(project()) - 1 do
    local track = reaper.GetTrack(project(), i)
    if get_track_name(track) == name then
      set_track_ext(track, "ReaADR.role", role)
      set_track_ext(track, "ReaADR.key", key)
      set_track_ext(track, "ReaADR.version", ReaADR.VERSION)
      set_track_color(track, color)
      return track, false
    end
  end

  local index = reaper.CountTracks(project())
  reaper.InsertTrackAtIndex(index, true)
  local track = reaper.GetTrack(project(), index)
  set_track_name(track, name)
  set_track_ext(track, "ReaADR.role", role)
  set_track_ext(track, "ReaADR.key", key)
  set_track_ext(track, "ReaADR.version", ReaADR.VERSION)
  set_track_color(track, color)
  return track, true
end

function ReaADR.cue_key(cue)
  local id = sanitize_token(cue.id)
  if id == "" then
    id = tostring(cue.source_line or "unknown")
  end
  return id
end

function ReaADR.cue_tag(cue)
  return ("%s:id=%s"):format(ReaADR.NAME_PREFIX, ReaADR.cue_key(cue))
end

local character_lane_key
local setup_preroll_seconds
local assign_character_lanes

function ReaADR.character_filter_key(character)
  return sanitize_token(character):lower()
end

function ReaADR.character_filter_target_key(character, lane)
  return character_lane_key(first_nonempty(character, "Unassigned"), tonumber(lane) or 1):lower()
end

function ReaADR.encode_character_filter(characters)
  local tokens = {}
  for _, character in ipairs(characters or {}) do
    local token
    if type(character) == "table" then
      token = character.key or ReaADR.character_filter_target_key(character.character, character.lane)
    else
      token = ReaADR.character_filter_key(character)
    end
    if token ~= "" then
      tokens[#tokens + 1] = token
    end
  end
  table.sort(tokens)
  return table.concat(tokens, ",")
end

function ReaADR.active_character_filter()
  local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "active_character_filter")
  local active = {}
  for token in tostring(value or ""):gmatch("([^,]+)") do
    active[token] = true
  end
  return active, value
end

function ReaADR.set_active_character_filter(characters)
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "active_character_filter", ReaADR.encode_character_filter(characters))
  ReaADR.bump_session_revision()
end

function ReaADR.character_filter_hides_regions()
  local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "character_filter_hide_regions")
  return value == "1"
end

function ReaADR.set_character_filter_hides_regions(enabled)
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "character_filter_hide_regions", enabled and "1" or "0")
  ReaADR.bump_session_revision()
end

function ReaADR.character_filter_enabled()
  local _, value = ReaADR.active_character_filter()
  return value ~= nil and value ~= ""
end

function ReaADR.character_is_active(character)
  local active, value = ReaADR.active_character_filter()
  if value == nil or value == "" then
    return true
  end
  return active[ReaADR.character_filter_key(character)] == true
end

function ReaADR.character_lane_is_active(character, lane)
  local active, value = ReaADR.active_character_filter()
  if value == nil or value == "" then
    return true
  end
  local lane_key = ReaADR.character_filter_target_key(character, lane)
  local character_key = ReaADR.character_filter_key(character)
  return active[lane_key] == true or active[character_key] == true
end

local function assign_filter_lanes(cues)
  local preroll_seconds = setup_preroll_seconds({ overlay_settings = ReaADR.load_overlay_settings() })
  assign_character_lanes(cues or {}, preroll_seconds)
end

function ReaADR.filter_cues_by_active_characters(cues)
  if not ReaADR.character_filter_enabled() then
    return cues or {}
  end

  assign_filter_lanes(cues)
  local filtered = {}
  for _, cue in ipairs(cues or {}) do
    if ReaADR.character_lane_is_active(cue.character, cue._reaadr_lane) then
      filtered[#filtered + 1] = cue
    end
  end
  return filtered
end

function ReaADR.available_filter_characters()
  local cues = ReaADR.load_last_import_cues()
  if not cues then
    cues = ReaADR.collect_project_marker_cues({
      include_markers = true,
      include_regions = true,
      character = "",
      cue_type = "",
      flexible_export = true,
    })
  end
  return ReaADR.collect_characters(cues or {})
end

function ReaADR.available_filter_targets()
  local cues = ReaADR.load_last_import_cues()
  if not cues then
    cues = ReaADR.collect_project_marker_cues({
      include_markers = true,
      include_regions = true,
      character = "",
      cue_type = "",
      flexible_export = true,
    })
  end
  cues = cues or {}
  assign_filter_lanes(cues)

  local characters = ReaADR.collect_characters(cues)
  local max_lanes = {}
  for _, cue in ipairs(cues) do
    local character = first_nonempty(cue.character, "Unassigned")
    max_lanes[character] = math.max(max_lanes[character] or 1, tonumber(cue._reaadr_lane) or 1)
  end

  local targets = {}
  for _, character in ipairs(characters) do
    for lane = 1, math.max(1, max_lanes[character] or 1) do
      targets[#targets + 1] = {
        character = character,
        lane = lane,
        key = ReaADR.character_filter_target_key(character, lane),
        label = lane <= 1 and character or ("%s #%d"):format(character, lane),
      }
    end
  end
  return targets
end

function ReaADR.current_active_characters(characters)
  characters = characters or ReaADR.available_filter_characters()
  if not ReaADR.character_filter_enabled() then
    local all = {}
    for _, character in ipairs(characters) do
      all[#all + 1] = character
    end
    return all
  end

  local active_characters = {}
  for _, character in ipairs(characters) do
    if ReaADR.character_is_active(character) then
      active_characters[#active_characters + 1] = character
    end
  end
  return active_characters
end

function ReaADR.current_active_filter_targets(targets)
  targets = targets or ReaADR.available_filter_targets()
  if not ReaADR.character_filter_enabled() then
    local all = {}
    for _, target in ipairs(targets) do
      all[#all + 1] = target
    end
    return all
  end

  local active_targets = {}
  for _, target in ipairs(targets) do
    if ReaADR.character_lane_is_active(target.character, target.lane) then
      active_targets[#active_targets + 1] = target
    end
  end
  return active_targets
end

function ReaADR.apply_character_filter()
  local cues = ReaADR.load_last_import_cues()
  if not cues then
    cues = ReaADR.collect_project_marker_cues({
      include_markers = true,
      include_regions = true,
      character = "",
      cue_type = "",
      flexible_export = true,
    })
  end
  cues = cues or {}
  assign_filter_lanes(cues)

  local muted_tracks = 0
  local active_tracks = 0
  local hidden_regions = 0
  local visible_regions = 0
  for i = 0, reaper.CountTracks(project()) - 1 do
    local track = reaper.GetTrack(project(), i)
    local role = track_ext(track, "ReaADR.role")
    if role == "character" or role == "cue_character" then
      local key = track_ext(track, "ReaADR.key")
      local character_token, lane = tostring(key or ""):match("^(.-)%.lane(%d+)$")
      local active = not ReaADR.character_filter_enabled() or ReaADR.character_lane_is_active(character_token or key, tonumber(lane) or 1)
      set_track_muted(track, not active)
      if active then
        active_tracks = active_tracks + 1
      else
        muted_tracks = muted_tracks + 1
      end
    end
  end

  local hide_inactive_regions = ReaADR.character_filter_hides_regions()
  for _, cue in ipairs(cues) do
    local hide = hide_inactive_regions and not ReaADR.character_lane_is_active(cue.character, cue._reaadr_lane)
    if ReaADR.set_region_hidden(cue, hide) then
      if hide then
        hidden_regions = hidden_regions + 1
      else
        visible_regions = visible_regions + 1
      end
    end
  end

  if hide_inactive_regions then
    ReaADR.refresh_overlay_silent()
  end
  ReaADR.bump_session_revision()
  reaper.UpdateArrange()
  return {
    active_tracks = active_tracks,
    muted_tracks = muted_tracks,
    cue_count = #cues,
    hidden_regions = hidden_regions,
    visible_regions = visible_regions,
  }
end

function ReaADR.region_name(cue)
  return ("%s ADR Cue %s - %s"):format(ReaADR.cue_tag(cue), cue.id, cue.character)
end

function ReaADR.cue_item_name(cue)
  return ("%s Cue Audio %s - %s"):format(ReaADR.cue_tag(cue), cue.id, cue.character)
end

local function find_project_marker(name, is_region)
  local _, marker_count, region_count = reaper.CountProjectMarkers(project())
  local total = marker_count + region_count

  for i = 0, total - 1 do
    local ok, marker_is_region, pos, region_end, marker_name, marker_id, color =
      reaper.EnumProjectMarkers3(project(), i)
    if ok and marker_is_region == is_region and marker_name == name then
      return {
        enum_index = i,
        id = marker_id,
        pos = pos,
        region_end = region_end,
        color = color,
      }
    end
  end

  return nil
end

local function project_markers_by_name(is_region)
  local markers = {}
  local _, marker_count, region_count = reaper.CountProjectMarkers(project())
  local total = marker_count + region_count

  for i = 0, total - 1 do
    local ok, marker_is_region, pos, region_end, marker_name, marker_id, color =
      reaper.EnumProjectMarkers3(project(), i)
    if ok and marker_is_region == is_region then
      markers[marker_name] = {
        enum_index = i,
        id = marker_id,
        pos = pos,
        region_end = region_end,
        color = color,
      }
    end
  end

  return markers
end

local function delete_project_marker_by_name(name, is_region)
  local existing = find_project_marker(name, is_region)
  if not existing then
    return false
  end
  return reaper.DeleteProjectMarker(project(), existing.id, is_region) == true
end

local function upsert_project_marker(is_region, start_time, end_time, name, color)
  local existing = find_project_marker(name, is_region)
  if existing then
    reaper.SetProjectMarker4(project(), existing.id, is_region, start_time, end_time or 0, name, color or existing.color, 0)
    return existing.id, false
  end

  local id = reaper.AddProjectMarker2(project(), is_region, start_time, end_time or 0, name, -1, color or 0)
  return id, true
end

local function upsert_project_marker_from_index(index, is_region, start_time, end_time, name, color)
  local existing = index[name]
  if existing then
    reaper.SetProjectMarker4(project(), existing.id, is_region, start_time, end_time or 0, name, color or existing.color, 0)
    existing.pos = start_time
    existing.region_end = end_time
    existing.color = color or existing.color
    return existing.id, false
  end

  local id = reaper.AddProjectMarker2(project(), is_region, start_time, end_time or 0, name, -1, color or 0)
  index[name] = {
    id = id,
    pos = start_time,
    region_end = end_time,
    color = color or 0,
  }
  return id, true
end

function ReaADR.ensure_region(cue, color)
  return upsert_project_marker(true, cue.start_time, cue.end_time, ReaADR.region_name(cue), color)
end

local function ensure_region_with_index(index, cue, color)
  return upsert_project_marker_from_index(index, true, cue.start_time, cue.end_time, ReaADR.region_name(cue), color)
end

local function set_region_marker_value_by_name(region_name, parameter_name, value)
  if not reaper.GetRegionOrMarker or not reaper.SetRegionOrMarkerInfo_Value then
    return false
  end

  local existing = find_project_marker(region_name, true)
  if not existing then
    return false
  end

  local marker = reaper.GetRegionOrMarker(project(), existing.enum_index, "")
  if not marker then
    return false
  end

  local ok = pcall(reaper.SetRegionOrMarkerInfo_Value, project(), marker, parameter_name, tonumber(value) or 0)
  return ok
end

local function set_region_lane_by_name(region_name, lane_number)
  return set_region_marker_value_by_name(region_name, "I_LANENUMBER", lane_number)
end

function ReaADR.set_region_lane(cue, lane_number)
  return set_region_lane_by_name(ReaADR.region_name(cue), lane_number)
end

function ReaADR.set_region_hidden(cue, hidden)
  return set_region_marker_value_by_name(ReaADR.region_name(cue), "B_HIDDEN", hidden and 1 or 0)
end

function ReaADR.remove_start_markers()
  local marker_ids = {}
  local _, marker_count, region_count = reaper.CountProjectMarkers(project())
  local total = marker_count + region_count

  for i = 0, total - 1 do
    local ok, is_region, _, _, name, marker_id = reaper.EnumProjectMarkers3(project(), i)
    if ok and not is_region and name:find(ReaADR.NAME_PREFIX, 1, true) and name:find("Cue Start", 1, true) then
      marker_ids[#marker_ids + 1] = marker_id
    end
  end

  local removed = 0
  for _, marker_id in ipairs(marker_ids) do
    if reaper.DeleteProjectMarker(project(), marker_id, false) then
      removed = removed + 1
    end
  end
  return removed
end

function ReaADR.delete_generated_cue_regions()
  local region_ids = {}
  local _, marker_count, region_count = reaper.CountProjectMarkers(project())
  local total = marker_count + region_count

  for i = 0, total - 1 do
    local ok, is_region, _, _, name, marker_id = reaper.EnumProjectMarkers3(project(), i)
    if ok and is_region and name:find(ReaADR.NAME_PREFIX, 1, true) and name:find("ADR Cue", 1, true) then
      region_ids[#region_ids + 1] = marker_id
    end
  end

  local removed = 0
  for _, marker_id in ipairs(region_ids) do
    if reaper.DeleteProjectMarker(project(), marker_id, true) then
      removed = removed + 1
    end
  end
  return removed
end

local function media_item_ext(item, key)
  local ok, value = reaper.GetSetMediaItemInfo_String(item, "P_EXT:" .. key, "", false)
  if ok then
    return value
  end
  return ""
end

local function set_media_item_ext(item, key, value)
  reaper.GetSetMediaItemInfo_String(item, "P_EXT:" .. key, tostring(value or ""), true)
end

local function project_key_value(namespace, key)
  local _, value = reaper.GetProjExtState(project(), namespace, key)
  return value
end

local function set_take_name(take, name)
  if take then
    reaper.GetSetMediaItemTakeInfo_String(take, "P_NAME", name, true)
  end
end

function ReaADR.find_cue_audio_item(track, cue)
  local cue_key = ReaADR.cue_key(cue)
  for i = 0, reaper.CountTrackMediaItems(track) - 1 do
    local item = reaper.GetTrackMediaItem(track, i)
    if media_item_ext(item, "ReaADR.role") == "cue_audio" and media_item_ext(item, "ReaADR.cue_key") == cue_key then
      return item
    end
  end
  return nil
end

local function cue_audio_items_by_key(track)
  local items = {}
  for i = 0, reaper.CountTrackMediaItems(track) - 1 do
    local item = reaper.GetTrackMediaItem(track, i)
    if media_item_ext(item, "ReaADR.role") == "cue_audio" then
      local cue_key = media_item_ext(item, "ReaADR.cue_key")
      if cue_key ~= "" then
        items[cue_key] = item
      end
    end
  end
  return items
end

function ReaADR.ensure_cue_audio_item(track, cue, audio_path, item_index)
  if not audio_path or audio_path == "" then
    return nil, "skipped", "missing_path"
  end
  if not file_exists(audio_path) then
    return nil, "skipped", "missing_file"
  end

  local source = reaper.PCM_Source_CreateFromFile(audio_path)
  if not source then
    return nil, "skipped", "source_failed"
  end

  local source_length = ({ reaper.GetMediaSourceLength(source) })[1] or 0
  if source_length <= 0 then
    reaper.PCM_Source_Destroy(source)
    return nil, "skipped", "invalid_source_length"
  end

  local cue_key = ReaADR.cue_key(cue)
  local item = item_index and item_index[cue_key] or ReaADR.find_cue_audio_item(track, cue)
  local status = "updated"
  if not item then
    item = reaper.AddMediaItemToTrack(track)
    status = "created"
    if item_index then
      item_index[cue_key] = item
    end
  end

  local take = reaper.GetActiveTake(item)
  if not take then
    take = reaper.AddTakeToMediaItem(item)
  end

  local position = math.max(0, cue.start_time - source_length)
  reaper.SetMediaItemInfo_Value(item, "D_POSITION", position)
  reaper.SetMediaItemInfo_Value(item, "D_LENGTH", source_length)
  reaper.SetMediaItemInfo_Value(item, "B_LOOPSRC", 0)
  reaper.SetMediaItemTake_Source(take, source)
  set_take_name(take, ReaADR.cue_item_name(cue))

  set_media_item_ext(item, "ReaADR.role", "cue_audio")
  set_media_item_ext(item, "ReaADR.cue_key", cue_key)
  set_media_item_ext(item, "ReaADR.version", ReaADR.VERSION)
  set_media_item_ext(item, "ReaADR.source_path", audio_path)

  return item, status
end

local function parse_spotting_label(label)
  label = trim(label)
  if label == "" then
    return "", ""
  end

  local character, line = label:match("^([^:]+):%s*(.-)$")
  if character then
    return trim(character), trim(line)
  end

  character, line = label:match("^(.+)%s+%-%s+(.-)$")
  if character then
    return trim(character), trim(line)
  end

  return label, ""
end

function ReaADR.collect_project_marker_cues(options)
  options = options or {}
  local default_duration = math.max(0.1, tonumber(options.default_duration) or 2.0)
  local include_markers = options.include_markers ~= false
  local include_regions = options.include_regions ~= false
  local flexible_export = options.flexible_export == true
  local cues = {}
  local _, marker_count, region_count = reaper.CountProjectMarkers(project())
  local total = marker_count + region_count

  for i = 0, total - 1 do
    local ok, is_region, pos, region_end, name, marker_id = reaper.EnumProjectMarkers3(project(), i)
    if ok and ((is_region and include_regions) or ((not is_region) and include_markers)) then
      local label = trim(name)
      if label == "" and not flexible_export then
        label = is_region and ("Region " .. tostring(marker_id)) or ("Marker " .. tostring(marker_id))
      end

      local character = first_nonempty(options.character, "ADR")
      local line = label
      local cue_type = is_region and "Region" or "Marker"
      if flexible_export then
        local parsed_character, parsed_line = parse_spotting_label(label)
        character = first_nonempty(parsed_character, options.character, "")
        line = parsed_line
        cue_type = first_nonempty(options.cue_type, "")
      end

      local end_time = is_region and region_end or (pos + default_duration)
      if end_time <= pos then
        end_time = pos + default_duration
      end

      cues[#cues + 1] = {
        id = tostring(marker_id),
        character = character,
        start_time = pos,
        end_time = end_time,
        line = line,
        notes = "",
        direction = "",
        cue_type = cue_type,
        status = "Not Recorded",
        source_line = i + 1,
      }
    end
  end

  table.sort(cues, function(a, b)
    if a.start_time == b.start_time then
      return tostring(a.id) < tostring(b.id)
    end
    return a.start_time < b.start_time
  end)

  return cues
end

function ReaADR.navigation_cues()
  local cues = ReaADR.collect_project_marker_cues({
    include_markers = false,
    include_regions = true,
    character = "",
    cue_type = "",
    flexible_export = true,
  })
  local source = "project regions"

  if #cues == 0 then
    local cached = ReaADR.load_last_import_cues()
    if cached and #cached > 0 then
      cues = cached
      source = "cached ReaADR cues"
    end
  end

  if #cues == 0 then
    cues = ReaADR.collect_project_marker_cues({
      include_markers = true,
      include_regions = false,
      character = "",
      cue_type = "",
      flexible_export = true,
    })
    source = "project markers"
  end

  table.sort(cues, function(a, b)
    if a.start_time == b.start_time then
      return tostring(a.id) < tostring(b.id)
    end
    return a.start_time < b.start_time
  end)

  return cues, source
end

function ReaADR.current_timeline_position()
  local play_state = reaper.GetPlayState()
  if play_state % 2 == 1 then
    return reaper.GetPlayPosition()
  end
  return reaper.GetCursorPosition()
end

function ReaADR.jump_to_cue(cue)
  if not cue or not cue.start_time then
    return false
  end
  reaper.SetEditCurPos(tonumber(cue.start_time) or 0, true, false)
  ReaADR.set_active_overlay_cue(cue)
  ReaADR.set_manager_selected_cue(cue)
  return true
end

function ReaADR.active_overlay_cue_key()
  return project_key_value(ReaADR.EXT_NAMESPACE, "active_overlay_cue_key")
end

function ReaADR.set_active_overlay_cue(cue)
  local key = cue and ReaADR.cue_key(cue) or ""
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "active_overlay_cue_key", key)
end

function ReaADR.manager_selected_cue_key()
  return project_key_value(ReaADR.EXT_NAMESPACE, "manager_selected_cue_key")
end

function ReaADR.set_manager_selected_cue(cue)
  local key = cue and ReaADR.cue_key(cue) or ""
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "manager_selected_cue_key", key)
  ReaADR.set_active_overlay_cue(cue)
end

function ReaADR.refresh_overlay_silent()
  if ReaADR.refresh_overlay_fx_from_project then
    pcall(ReaADR.refresh_overlay_fx_from_project)
  end
end

function ReaADR.find_next_cue(cues, position)
  local epsilon = 0.0001
  position = tonumber(position) or 0
  for _, cue in ipairs(cues or {}) do
    if (tonumber(cue.start_time) or 0) > position + epsilon then
      return cue
    end
  end
  return (cues and cues[1]) or nil
end

function ReaADR.find_previous_cue(cues, position)
  local epsilon = 0.0001
  position = tonumber(position) or 0
  local previous = nil
  for _, cue in ipairs(cues or {}) do
    if (tonumber(cue.start_time) or 0) < position - epsilon then
      previous = cue
    else
      break
    end
  end
  return previous or (cues and cues[#cues]) or nil
end

function ReaADR.find_cue_by_id(cues, cue_id)
  cue_id = trim(cue_id)
  if cue_id == "" then
    return nil
  end

  for _, cue in ipairs(cues or {}) do
    if trim(cue.id) == cue_id then
      return cue
    end
  end

  local lowered = cue_id:lower()
  for _, cue in ipairs(cues or {}) do
    local id = trim(cue.id):lower()
    if id:find(lowered, 1, true) then
      return cue
    end
  end

  return nil
end

function ReaADR.cue_duration(cue)
  if not cue then
    return 0
  end
  return math.max(0, (tonumber(cue.end_time) or 0) - (tonumber(cue.start_time) or 0))
end

function ReaADR.cue_metadata_value(cue, key)
  if not cue or type(cue.metadata) ~= "table" then
    return ""
  end
  local wanted = normalize_header(key)
  for metadata_key, value in pairs(cue.metadata) do
    if normalize_header(metadata_key) == wanted then
      return tostring(value or "")
    end
  end
  return ""
end

function ReaADR.visible_metadata_pairs(cue, settings)
  settings = settings or ReaADR.load_overlay_settings()
  local pairs = {}
  for _, key in ipairs(split_list(settings.metadata_fields)) do
    local value = ReaADR.cue_metadata_value(cue, key)
    if value ~= "" then
      pairs[#pairs + 1] = { key = key, value = value }
    end
  end
  return pairs
end

local function renumber_cue_id(cue, index, width)
  local padded = tostring(index)
  if width and width > 1 then
    padded = ("%0" .. tostring(width) .. "d"):format(index)
  end
  cue.id = padded
end

local function renumber_cues_in_order(cues)
  local numeric_only = true
  local width = 1
  for _, cue in ipairs(cues or {}) do
    local id = trim(cue.id)
    if id == "" or not id:match("^%d+$") then
      numeric_only = false
      break
    end
    width = math.max(width, #id)
  end
  if not numeric_only then
    width = 0
  end
  for index, cue in ipairs(cues or {}) do
    renumber_cue_id(cue, index, width)
    cue.source_line = index
  end
end

function ReaADR.count_recorded_takes_for_cue(cue)
  if not cue then
    return 0
  end
  if not reaper or not reaper.CountTracks then
    return 0
  end
  local cue_start = tonumber(cue.start_time) or 0
  local cue_end = tonumber(cue.end_time) or cue_start
  local character = sanitize_token(cue.character):lower()
  local count = 0

  for track_index = 0, reaper.CountTracks(project()) - 1 do
    local track = reaper.GetTrack(project(), track_index)
    local role = track_ext(track, "ReaADR.role")
    local key = track_ext(track, "ReaADR.key")
    local track_character = tostring(key or ""):match("^(.-)%.lane%d+$") or key
    if role == "character" and (character == "" or tostring(track_character):lower() == character) then
      for item_index = 0, reaper.CountTrackMediaItems(track) - 1 do
        local item = reaper.GetTrackMediaItem(track, item_index)
        local item_start = reaper.GetMediaItemInfo_Value(item, "D_POSITION")
        local item_end = item_start + reaper.GetMediaItemInfo_Value(item, "D_LENGTH")
        if item_end > cue_start and item_start < cue_end then
          count = count + math.max(1, reaper.CountTakes(item))
        end
      end
    end
  end

  return count
end

function ReaADR.active_cue()
  local cues = ReaADR.load_last_import_cues()
  if not cues then
    cues = ReaADR.navigation_cues()
  end
  cues = cues or {}
  cues = ReaADR.filter_cues_by_active_characters(cues)

  local selected_key = ReaADR.manager_selected_cue_key()
  if selected_key ~= "" then
    for _, cue in ipairs(cues) do
      if ReaADR.cue_key(cue) == selected_key then
        return cue, cues
      end
    end
  end

  selected_key = ReaADR.selected_region_cue_key()
  if selected_key ~= "" then
    for _, cue in ipairs(cues) do
      if ReaADR.cue_key(cue) == selected_key then
        ReaADR.set_manager_selected_cue(cue)
        return cue, cues
      end
    end
  end

  local position = ReaADR.current_timeline_position()
  return ReaADR.find_cue_at_position(cues, position) or ReaADR.find_next_cue(cues, position), cues
end

function ReaADR.session_cues()
  local cues, source = ReaADR.load_last_import_cues()
  if cues then
    return ReaADR.filter_cues_by_active_characters(cues), "cached ReaADR session"
  end
  cues, source = ReaADR.navigation_cues()
  return ReaADR.filter_cues_by_active_characters(cues or {}), source or "project"
end

function ReaADR.validate_cues(cues, options)
  options = options or {}
  local preroll = math.max(0, tonumber(options.preroll_seconds) or ReaADR.DEFAULT_OVERLAY_SETTINGS.preroll_seconds)
  local result = {
    cue_count = #(cues or {}),
    character_count = 0,
    missing_character = 0,
    missing_dialogue = 0,
    invalid_time = 0,
    overlap_conflicts = 0,
    metadata_fields = 0,
    characters = {},
    warnings = {},
  }

  local characters = {}
  local metadata_keys = {}
  local active_windows = {}
  local sorted = {}
  for index, cue in ipairs(cues or {}) do
    sorted[index] = cue
  end
  table.sort(sorted, function(a, b)
    return (tonumber(a.start_time) or 0) < (tonumber(b.start_time) or 0)
  end)

  for _, cue in ipairs(sorted) do
    local character = trim(cue.character)
    if character == "" or character == "Unassigned" then
      result.missing_character = result.missing_character + 1
    else
      characters[character] = true
    end
    if trim(cue.line) == "" then
      result.missing_dialogue = result.missing_dialogue + 1
    end
    if (tonumber(cue.end_time) or 0) <= (tonumber(cue.start_time) or 0) then
      result.invalid_time = result.invalid_time + 1
    end
    for key, value in pairs(cue.metadata or {}) do
      if trim(value) ~= "" then
        metadata_keys[key] = true
      end
    end

    character = first_nonempty(character, "Unassigned")
    active_windows[character] = active_windows[character] or {}
    local cue_start = tonumber(cue.start_time) or 0
    local cue_end = tonumber(cue.end_time) or cue_start
    local window_start = math.max(0, cue_start - preroll)
    for _, window_end in ipairs(active_windows[character]) do
      if window_start < window_end then
        result.overlap_conflicts = result.overlap_conflicts + 1
        break
      end
    end
    active_windows[character][#active_windows[character] + 1] = cue_end
  end

  for character in pairs(characters) do
    result.characters[#result.characters + 1] = character
  end
  table.sort(result.characters)
  result.character_count = #result.characters

  for _ in pairs(metadata_keys) do
    result.metadata_fields = result.metadata_fields + 1
  end

  if result.missing_character > 0 then
    result.warnings[#result.warnings + 1] = tostring(result.missing_character) .. " cue(s) have no character."
  end
  if result.missing_dialogue > 0 then
    result.warnings[#result.warnings + 1] = tostring(result.missing_dialogue) .. " cue(s) have blank dialogue."
  end
  if result.invalid_time > 0 then
    result.warnings[#result.warnings + 1] = tostring(result.invalid_time) .. " cue(s) have invalid timing."
  end
  if result.overlap_conflicts > 0 then
    result.warnings[#result.warnings + 1] = tostring(result.overlap_conflicts) .. " cue(s) need overlap lane splitting."
  end

  return result
end

function ReaADR.validation_summary_text(validation)
  validation = validation or {}
  local lines = {
    "Import validation",
    "",
    "Cues: " .. tostring(validation.cue_count or 0),
    "Characters: " .. tostring(validation.character_count or 0),
    "Overlap splits: " .. tostring(validation.overlap_conflicts or 0),
    "Blank dialogue: " .. tostring(validation.missing_dialogue or 0),
    "Missing character: " .. tostring(validation.missing_character or 0),
    "Metadata fields: " .. tostring(validation.metadata_fields or 0),
  }

  if validation.characters and #validation.characters > 0 then
    lines[#lines + 1] = ""
    lines[#lines + 1] = "Characters: " .. table.concat(validation.characters, ", ")
  end
  if validation.warnings and #validation.warnings > 0 then
    lines[#lines + 1] = ""
    lines[#lines + 1] = "Warnings:"
    for _, warning in ipairs(validation.warnings) do
      lines[#lines + 1] = "- " .. warning
    end
  end

  lines[#lines + 1] = ""
  lines[#lines + 1] = "Build this ADR session?"
  return table.concat(lines, "\n")
end

function ReaADR.cue_statuses()
  return {
    "Not Recorded",
    "Recorded",
    "Approved",
    "Needs Retake",
  }
end

function ReaADR.normalize_status(status)
  return normalize_status(status)
end

function ReaADR.find_cue_at_position(cues, position)
  position = tonumber(position) or 0
  for _, cue in ipairs(cues or {}) do
    local start_time = tonumber(cue.start_time) or 0
    local end_time = tonumber(cue.end_time) or start_time
    if position >= start_time and position <= end_time then
      return cue
    end
  end
  return nil
end

function ReaADR.set_cue_status_at_position(status, position)
  status = normalize_status(status)
  position = tonumber(position) or ReaADR.current_timeline_position()

  local cues, cue_error = ReaADR.load_last_import_cues()
  if not cues then
    cues = ReaADR.navigation_cues()
  end
  if not cues or #cues == 0 then
    return nil, cue_error or "No cues were found."
  end

  local cue = ReaADR.find_cue_at_position(cues, position)
  if not cue then
    return nil, "Place the edit cursor inside a cue region before setting cue status."
  end

  cue.status = status
  ReaADR.set_active_overlay_cue(cue)
  ReaADR.save_last_import_cues(cues)
  ReaADR.ensure_region(cue, character_color(cue.character))
  ReaADR.ensure_character_ruler_lanes(cues)
  local ruler_lanes = ReaADR.character_region_lanes(cues)
  ReaADR.set_region_lane(cue, ReaADR.region_lane_for_cue(cue, ruler_lanes))
  local overlay_status = nil
  if ReaADR.refresh_overlay_fx_from_project then
    overlay_status = ReaADR.refresh_overlay_fx_from_project()
  end
  reaper.UpdateArrange()
  return cue, overlay_status
end

function ReaADR.update_cached_cue(updated_cue)
  if not updated_cue then
    return nil, "No cue was provided."
  end

  local cues, cue_error = ReaADR.load_last_import_cues()
  if not cues then
    return nil, cue_error or "No cached ReaADR session was found."
  end

  local key = updated_cue._original_key or ReaADR.cue_key(updated_cue)
  local updated = nil
  for _, cue in ipairs(cues) do
    if ReaADR.cue_key(cue) == key then
      local old_region_name = ReaADR.region_name(cue)
      cue.id = first_nonempty(updated_cue.id, cue.id)
      cue.character = first_nonempty(updated_cue.character, cue.character)
      cue.line = updated_cue.line or cue.line or ""
      cue.direction = updated_cue.direction or cue.direction or ""
      cue.cue_type = updated_cue.cue_type or cue.cue_type or ""
      cue.status = normalize_status(updated_cue.status or cue.status)
      cue.notes = updated_cue.notes or cue.notes or ""
      cue.start_time = tonumber(updated_cue.start_time) or cue.start_time
      cue.end_time = tonumber(updated_cue.end_time) or cue.end_time
      if cue.end_time <= cue.start_time then
        cue.end_time = cue.start_time + 0.1
      end
      local existing_region = find_project_marker(old_region_name, true)
      if existing_region then
        reaper.SetProjectMarker4(
          project(),
          existing_region.id,
          true,
          tonumber(cue.start_time) or existing_region.pos,
          tonumber(cue.end_time) or existing_region.region_end,
          ReaADR.region_name(cue),
          character_color(cue.character),
          0
        )
      end
      updated = cue
      break
    end
  end

  if not updated then
    return nil, "Cue was not found in the cached ReaADR session."
  end

  ReaADR.save_last_import_cues(cues)
  ReaADR.ensure_region(updated, character_color(updated.character))
  ReaADR.ensure_character_ruler_lanes(cues)
  local ruler_lanes = ReaADR.character_region_lanes(cues)
  ReaADR.set_region_lane(updated, ReaADR.region_lane_for_cue(updated, ruler_lanes))
  ReaADR.set_manager_selected_cue(updated)
  ReaADR.refresh_overlay_silent()
  reaper.UpdateArrange()
  return updated
end

function ReaADR.sync_cached_cues_from_project_regions()
  local cues, cue_error = ReaADR.load_last_import_cues()
  if not cues then
    return nil, cue_error or "No cached ReaADR session was found."
  end

  local updated = 0
  local missing = 0
  for _, cue in ipairs(cues) do
    local region = find_project_marker(ReaADR.region_name(cue), true)
    if region then
      local start_time = tonumber(region.pos) or tonumber(cue.start_time) or 0
      local end_time = tonumber(region.region_end) or tonumber(cue.end_time) or start_time
      if end_time <= start_time then
        end_time = start_time + 0.1
      end
      if math.abs((tonumber(cue.start_time) or 0) - start_time) > 0.0005 or math.abs((tonumber(cue.end_time) or 0) - end_time) > 0.0005 then
        cue.start_time = start_time
        cue.end_time = end_time
        updated = updated + 1
      end
    else
      missing = missing + 1
    end
  end

  ReaADR.save_last_import_cues(cues)
  ReaADR.ensure_character_ruler_lanes(cues)
  local ruler_lanes = ReaADR.character_region_lanes(cues)
  for _, cue in ipairs(cues) do
    ReaADR.set_region_lane(cue, ReaADR.region_lane_for_cue(cue, ruler_lanes))
  end
  return {
    cues = cues,
    updated = updated,
    missing = missing,
    cue_count = #cues,
  }
end

function ReaADR.add_cached_cue(cue)
  cue = cue or {}
  local cues = ReaADR.load_last_import_cues()
  if not cues then
    cues = ReaADR.navigation_cues()
  end
  cues = cues or {}

  cue.id = first_nonempty(cue.id, tostring(#cues + 1))
  cue.character = first_nonempty(cue.character, "ADR")
  cue.start_time = tonumber(cue.start_time) or ReaADR.current_timeline_position()
  cue.end_time = tonumber(cue.end_time) or (cue.start_time + 2.0)
  if cue.end_time <= cue.start_time then
    cue.end_time = cue.start_time + 2.0
  end
  cue.line = cue.line or ""
  cue.direction = cue.direction or ""
  cue.cue_type = cue.cue_type or "Dialogue"
  cue.status = normalize_status(cue.status)
  cue.notes = cue.notes or ""
  cue.source_line = cue.source_line or (#cues + 1)

  cues[#cues + 1] = cue
  table.sort(cues, function(a, b)
    if a.start_time == b.start_time then
      return tostring(a.id) < tostring(b.id)
    end
    return (tonumber(a.start_time) or 0) < (tonumber(b.start_time) or 0)
  end)
  ReaADR.save_last_import_cues(cues)
  ReaADR.set_manager_selected_cue(cue)
  return cue, cues
end

function ReaADR.remove_cached_cue(target_cue, options)
  options = options or {}
  if not target_cue then
    return nil, "No cue was provided."
  end

  local cues, cue_error = ReaADR.load_last_import_cues()
  if not cues then
    return nil, cue_error or "No cached ReaADR session was found."
  end

  local key = target_cue._original_key or ReaADR.cue_key(target_cue)
  local removed = nil
  for index, cue in ipairs(cues) do
    if ReaADR.cue_key(cue) == key then
      removed = cue
      table.remove(cues, index)
      break
    end
  end

  if not removed then
    return nil, "Cue was not found in the cached ReaADR session."
  end

  delete_project_marker_by_name(ReaADR.region_name(removed), true)
  if options.renumber ~= false then
    renumber_cues_in_order(cues)
  end

  ReaADR.save_last_import_cues(cues)

  local selected = cues[math.min(#cues, math.max(1, tonumber(options.select_index) or 1))]
  ReaADR.set_manager_selected_cue(selected)
  ReaADR.ensure_character_ruler_lanes(cues)
  ReaADR.refresh_overlay_silent()
  reaper.UpdateArrange()

  return {
    removed = removed,
    cues = cues,
    selected = selected,
  }
end

function ReaADR.rebuild_cached_session(options)
  options = options or {}
  local cues, err = ReaADR.load_last_import_cues()
  if not cues then
    return nil, err or "No cached ReaADR session was found."
  end

  local frame_rate = reaper.TimeMap_curFrameRate(project())
  if not frame_rate or frame_rate <= 0 then
    frame_rate = 24
  end
  local cue_audio_path = options.cue_audio_path or ReaADR.project_cue_audio_path()
  local generated, generated_error = ReaADR.generate_project_cue_wav(cue_audio_path, frame_rate)
  if not generated then
    return nil, generated_error
  end

  if options.clear_generated_items then
    ReaADR.delete_generated_cue_audio_items()
  end
  if options.clear_generated_regions then
    ReaADR.delete_generated_cue_regions()
  end

  return ReaADR.setup_project(cues, {
    cue_audio_path = cue_audio_path,
    overlay_settings = options.overlay_settings or ReaADR.load_overlay_settings(),
    require_video_track = options.require_video_track ~= false,
    create_source_video_track = options.create_source_video_track ~= false,
    create_character_tracks = options.create_character_tracks ~= false,
    create_cues_track = options.create_cues_track ~= false,
    on_progress = options.on_progress,
  })
end

function ReaADR.detect_dialogue_cues_from_selected_media(options)
  options = options or {}
  local selected_count = reaper.CountSelectedMediaItems(project())
  if selected_count <= 0 then
    return nil, "Select one audio or video media item to analyze."
  end

  local item = reaper.GetSelectedMediaItem(project(), 0)
  local take = item and reaper.GetActiveTake(item)
  if not take then
    return nil, "The selected media item does not have an active take."
  end

  if not reaper.CreateTakeAudioAccessor or not reaper.GetAudioAccessorSamples then
    return nil, "This REAPER build does not expose the audio accessor APIs needed for dialogue detection."
  end

  local accessor = reaper.CreateTakeAudioAccessor(take)
  if not accessor then
    return nil, "Could not create an audio accessor for the selected media."
  end

  local sample_rate = math.floor(tonumber(options.sample_rate) or 12000)
  local channels = 1
  local block_seconds = tonumber(options.block_seconds) or 0.025
  local block_samples = math.max(64, math.floor(sample_rate * block_seconds + 0.5))
  local threshold_db = tonumber(options.threshold_db) or -42
  local threshold = 10 ^ (threshold_db / 20)
  local min_speech = tonumber(options.min_speech_seconds) or 0.25
  local min_silence = tonumber(options.min_silence_seconds) or 0.35
  local pad = tonumber(options.pad_seconds) or 0.05
  local character = first_nonempty(options.character, "ADR")
  local cue_type = first_nonempty(options.cue_type, "Dialogue")
  local item_position = tonumber(reaper.GetMediaItemInfo_Value(item, "D_POSITION")) or 0
  local item_length = tonumber(reaper.GetMediaItemInfo_Value(item, "D_LENGTH")) or 0
  local item_end = item_position + item_length
  local start_time = reaper.GetAudioAccessorStartTime(accessor)
  local end_time = reaper.GetAudioAccessorEndTime(accessor)
  local buffer = reaper.new_array(block_samples * channels)
  local raw_segments = {}
  local active_start = nil
  local last_loud_end = nil
  local t = start_time

  local function item_timeline_time(accessor_time)
    return item_position + math.max(0, (tonumber(accessor_time) or start_time) - start_time)
  end

  while t < end_time do
    buffer.clear()
    local ok = reaper.GetAudioAccessorSamples(accessor, sample_rate, channels, t, block_samples, buffer)
    local loud = false
    if ok == 1 then
      local samples = buffer.table()
      local sum = 0
      local count = #samples
      for _, sample in ipairs(samples) do
        sum = sum + (sample * sample)
      end
      local rms = count > 0 and math.sqrt(sum / count) or 0
      loud = rms >= threshold
    end

    local block_end = math.min(end_time, t + (block_samples / sample_rate))
    if loud then
      if not active_start then
        active_start = t
      end
      last_loud_end = block_end
    elseif active_start and last_loud_end and (t - last_loud_end) >= min_silence then
      if (last_loud_end - active_start) >= min_speech then
        local detected_start = item_timeline_time(active_start - pad)
        local detected_end = item_timeline_time(last_loud_end + pad)
        raw_segments[#raw_segments + 1] = {
          start_time = math.max(item_position, detected_start),
          end_time = math.min(item_end, detected_end),
        }
      end
      active_start = nil
      last_loud_end = nil
    end

    t = block_end
  end

  if active_start and last_loud_end and (last_loud_end - active_start) >= min_speech then
    local detected_start = item_timeline_time(active_start - pad)
    local detected_end = item_timeline_time(last_loud_end + pad)
    raw_segments[#raw_segments + 1] = {
      start_time = math.max(item_position, detected_start),
      end_time = math.min(item_end, detected_end),
    }
  end

  reaper.DestroyAudioAccessor(accessor)

  if #raw_segments == 0 then
    return {}, "No dialogue regions were detected. Try a lower threshold such as -48 dB."
  end

  local cues = {}
  for index, segment in ipairs(raw_segments) do
    cues[#cues + 1] = {
      id = ("%03d"):format(index),
      character = character,
      start_time = segment.start_time,
      end_time = segment.end_time,
      line = "",
      notes = "Detected from selected media",
      direction = "",
      cue_type = cue_type,
      status = "Not Recorded",
      source_line = index,
    }
  end

  return cues
end

function ReaADR.selected_cue_key()
  local item_count = reaper.CountSelectedMediaItems(project())
  for i = 0, item_count - 1 do
    local item = reaper.GetSelectedMediaItem(project(), i)
    local ok, key = reaper.GetSetMediaItemInfo_String(item, "P_EXT:ReaADR.cue_key", "", false)
    if ok and key and key ~= "" then
      return key
    end
  end
  return ""
end

function ReaADR.selected_region_cue_key()
  if not reaper.GetRegionOrMarker or not reaper.GetRegionOrMarkerInfo_Value then
    return ""
  end

  local _, marker_count, region_count = reaper.CountProjectMarkers(project())
  local total = marker_count + region_count
  for i = 0, total - 1 do
    local ok, is_region, _, _, name = reaper.EnumProjectMarkers3(project(), i)
    if ok and is_region then
      local marker = reaper.GetRegionOrMarker(project(), i, "")
      local selected = marker and reaper.GetRegionOrMarkerInfo_Value(project(), marker, "B_UISEL") or 0
      if selected and selected ~= 0 then
        return tostring(name or ""):match("%[ReaADR%]:id=([^%s]+)") or ""
      end
    end
  end

  return ""
end

function ReaADR.export_cues_to_csv(cues, path, options)
  options = options or {}
  local frame_rate = tonumber(options.frame_rate)
  if not frame_rate or frame_rate <= 0 then
    frame_rate = reaper.TimeMap_curFrameRate(project())
  end
  if not frame_rate or frame_rate <= 0 then
    frame_rate = 24
  end

  local file = io.open(path, "w")
  if not file then
    return nil, "Could not write file: " .. tostring(path)
  end

  local headers = { "cue_id", "character", "start", "end", "line", "direction", "cue_type", "status", "notes" }
  file:write(table.concat(headers, ","), "\n")

  for index, cue in ipairs(cues or {}) do
    local row = {
      cue.id ~= "" and cue.id or tostring(index),
      cue.character or "",
      format_timecode(cue.start_time, frame_rate),
      format_timecode(cue.end_time, frame_rate),
      cue.line or "",
      cue.direction or "",
      cue.cue_type or "",
      cue.status or "Not Recorded",
      cue.notes or "",
    }

    for field_index, value in ipairs(row) do
      if field_index > 1 then
        file:write(",")
      end
      file:write(csv_escape(value))
    end
    file:write("\n")
  end

  file:close()
  return true
end

function ReaADR.export_timing_report(cues, path, options)
  options = options or {}
  local frame_rate = tonumber(options.frame_rate)
  if not frame_rate and reaper and reaper.TimeMap_curFrameRate then
    frame_rate = reaper.TimeMap_curFrameRate(project())
  end
  frame_rate = frame_rate or 24
  local file = io.open(path, "w")
  if not file then
    return nil, "Could not write file: " .. tostring(path)
  end

  file:write("cue_id,character,start_smpte,end_smpte,length_seconds,line,status\n")
  for _, cue in ipairs(cues or {}) do
    local row = {
      cue.id or "",
      cue.character or "",
      format_timecode(cue.start_time, frame_rate),
      format_timecode(cue.end_time, frame_rate),
      ("%.3f"):format(ReaADR.cue_duration(cue)),
      cue.line or "",
      cue.status or "Not Recorded",
    }
    for index, value in ipairs(row) do
      if index > 1 then
        file:write(",")
      end
      file:write(csv_escape(value))
    end
    file:write("\n")
  end

  file:close()
  return true
end

function ReaADR.export_recording_report(cues, path, options)
  options = options or {}
  local frame_rate = tonumber(options.frame_rate)
  if not frame_rate and reaper and reaper.TimeMap_curFrameRate then
    frame_rate = reaper.TimeMap_curFrameRate(project())
  end
  frame_rate = frame_rate or 24
  local file = io.open(path, "w")
  if not file then
    return nil, "Could not write file: " .. tostring(path)
  end

  file:write("cue_id,character,start_smpte,end_smpte,status,take_count,line,notes\n")
  for _, cue in ipairs(cues or {}) do
    local row = {
      cue.id or "",
      cue.character or "",
      format_timecode(cue.start_time, frame_rate),
      format_timecode(cue.end_time, frame_rate),
      cue.status or "Not Recorded",
      tostring(ReaADR.count_recorded_takes_for_cue(cue)),
      cue.line or "",
      cue.notes or "",
    }
    for index, value in ipairs(row) do
      if index > 1 then
        file:write(",")
      end
      file:write(csv_escape(value))
    end
    file:write("\n")
  end

  file:close()
  return true
end

function ReaADR.export_session_metadata_report(cues, path)
  local metadata_keys = {}
  local seen = {}
  for _, cue in ipairs(cues or {}) do
    for key, value in pairs(cue.metadata or {}) do
      if trim(value) ~= "" and not seen[key] then
        seen[key] = true
        metadata_keys[#metadata_keys + 1] = key
      end
    end
  end
  table.sort(metadata_keys)

  local file = io.open(path, "w")
  if not file then
    return nil, "Could not write file: " .. tostring(path)
  end

  file:write("cue_id,character")
  for _, key in ipairs(metadata_keys) do
    file:write(",", csv_escape(key))
  end
  file:write("\n")

  for _, cue in ipairs(cues or {}) do
    file:write(csv_escape(cue.id or ""), ",", csv_escape(cue.character or ""))
    for _, key in ipairs(metadata_keys) do
      file:write(",", csv_escape(ReaADR.cue_metadata_value(cue, key)))
    end
    file:write("\n")
  end

  file:close()
  return true
end

-- JSON session export (SRS Addendum B §5)

local function json_str_escape(s)
  s = tostring(s or "")
  s = s:gsub('\\', '\\\\')
  s = s:gsub('"',  '\\"')
  s = s:gsub('\n', '\\n')
  s = s:gsub('\r', '\\r')
  s = s:gsub('\t', '\\t')
  return '"' .. s .. '"'
end

local function json_encode(value, depth)
  depth = depth or 0
  local vt = type(value)
  if value == nil then
    return "null"
  elseif vt == "boolean" then
    return value and "true" or "false"
  elseif vt == "number" then
    if value ~= value or value == math.huge or value == -math.huge then
      return "null"
    end
    if value == math.floor(value) and math.abs(value) < 1e15 then
      return tostring(math.floor(value))
    end
    return ("%.10g"):format(value)
  elseif vt == "string" then
    return json_str_escape(value)
  elseif vt == "table" then
    local pad   = string.rep("  ", depth)
    local inner = string.rep("  ", depth + 1)

    -- Detect sequential array (no holes, keys 1..n).
    local n = #value
    local is_array = n > 0
    if is_array then
      for k in pairs(value) do
        if type(k) ~= "number" or k < 1 or k > n or k ~= math.floor(k) then
          is_array = false
          break
        end
      end
    end

    if is_array then
      local parts = {}
      for i = 1, n do
        parts[i] = inner .. json_encode(value[i], depth + 1)
      end
      return "[\n" .. table.concat(parts, ",\n") .. "\n" .. pad .. "]"
    else
      local keys = {}
      for k in pairs(value) do
        if type(k) == "string" then keys[#keys + 1] = k end
      end
      table.sort(keys)
      if #keys == 0 then return "{}" end
      local parts = {}
      for _, k in ipairs(keys) do
        parts[#parts + 1] = inner .. json_str_escape(k) .. ": " .. json_encode(value[k], depth + 1)
      end
      return "{\n" .. table.concat(parts, ",\n") .. "\n" .. pad .. "}"
    end
  end
  return "null"
end

function ReaADR.export_session_json(cues, path, options)
  options = options or {}
  cues = cues or {}

  local frame_rate = tonumber(options.frame_rate)
  if not frame_rate or frame_rate <= 0 then
    if reaper and reaper.TimeMap_curFrameRate then
      frame_rate = reaper.TimeMap_curFrameRate(project())
    end
  end
  if not frame_rate or frame_rate <= 0 then frame_rate = 24 end

  local project_name = ""
  if reaper and reaper.GetProjectName then
    project_name = reaper.GetProjectName(project(), "") or ""
  end

  local timestamp = (os and os.date) and tostring(os.date("%Y-%m-%dT%H:%M:%S")) or ""

  local status_counts = {}
  local cue_list = {}
  for _, cue in ipairs(cues) do
    local s = normalize_status(cue.status)
    status_counts[s] = (status_counts[s] or 0) + 1

    local takes = (reaper and ReaADR.count_recorded_takes_for_cue(cue)) or 0
    cue_list[#cue_list + 1] = {
      character   = cue.character or "",
      cue_id      = cue.id or "",
      cue_type    = cue.cue_type or "",
      dialogue    = cue.line or "",
      direction   = cue.direction or "",
      duration    = ReaADR.cue_duration(cue),
      end_smpte   = format_timecode(cue.end_time, frame_rate),
      end_time    = cue.end_time,
      metadata    = type(cue.metadata) == "table" and cue.metadata or {},
      notes       = cue.notes or "",
      start_smpte = format_timecode(cue.start_time, frame_rate),
      start_time  = cue.start_time,
      status      = cue.status or "Not Recorded",
      take_count  = takes,
    }
  end

  local session = {
    characters      = ReaADR.collect_characters(cues),
    cue_count       = #cues,
    cues            = cue_list,
    export_timestamp = timestamp,
    project         = { frame_rate = frame_rate, name = project_name },
    reaadr_version  = ReaADR.VERSION,
    status_summary  = status_counts,
  }

  local file = io.open(path, "w")
  if not file then
    return nil, "Could not write file: " .. tostring(path)
  end
  file:write(json_encode(session) .. "\n")
  file:close()
  return true
end

-- EDL export — CMX 3600 format (SRS Addendum B §6)

function ReaADR.export_cues_to_edl(cues, path, options)
  options = options or {}

  local frame_rate = tonumber(options.frame_rate)
  if not frame_rate or frame_rate <= 0 then
    if reaper and reaper.TimeMap_curFrameRate then
      frame_rate = reaper.TimeMap_curFrameRate(project())
    end
  end
  if not frame_rate or frame_rate <= 0 then
    frame_rate = 24
  end
  local fr_int = math.max(1, math.floor(frame_rate + 0.5))

  local project_name = options.project_name or ""
  if project_name == "" and reaper and reaper.GetProjectName then
    project_name = reaper.GetProjectName(project(), "") or ""
  end
  if project_name == "" then
    project_name = "ADR Session"
  end

  -- Drop-frame only applies to 29.97 (30000/1001)
  local is_drop = math.abs(frame_rate - 29.97) < 0.02
  local fcm = is_drop and "DROP FRAME" or "NON-DROP FRAME"

  -- Record track offset: place cues on a 01:00:00:00 programme start
  local rec_offset = 3600.0

  local file = io.open(path, "w")
  if not file then
    return nil, "Could not write file: " .. tostring(path)
  end

  file:write("TITLE: " .. project_name .. "\n")
  file:write("FCM: " .. fcm .. "\n")
  file:write("\n")

  for edit_num, cue in ipairs(cues or {}) do
    local src_in  = format_timecode(cue.start_time, fr_int)
    local src_out = format_timecode(cue.end_time,   fr_int)
    local rec_in  = format_timecode(cue.start_time + rec_offset, fr_int)
    local rec_out = format_timecode(cue.end_time   + rec_offset, fr_int)

    -- Reel name: 8 chars, left-justified, padded with spaces
    local reel = ("%-8s"):format(("ADR%03d"):format(edit_num):sub(1, 8))

    file:write(("%03d  %s AA    C        %s %s %s %s\n"):format(
      edit_num, reel, src_in, src_out, rec_in, rec_out
    ))

    local clip_name = (cue.id or tostring(edit_num)) .. " - " .. (cue.character or "")
    file:write("* FROM CLIP NAME: " .. clip_name .. "\n")

    if trim(cue.line or "") ~= "" then
      file:write("* DIALOGUE: " .. cue.line .. "\n")
    end

    if trim(cue.status or "") ~= "" and cue.status ~= "Not Recorded" then
      file:write("* STATUS: " .. cue.status .. "\n")
    end

    if trim(cue.cue_type or "") ~= "" then
      file:write("* CUE TYPE: " .. cue.cue_type .. "\n")
    end

    file:write("\n")
  end

  file:close()
  return true
end

function ReaADR.generated_item_roles()
  return {
    cue_audio = true,
  }
end

function ReaADR.delete_generated_cue_audio_items()
  local removed_items = 0
  local generated_roles = ReaADR.generated_item_roles()

  for track_index = 0, reaper.CountTracks(project()) - 1 do
    local track = reaper.GetTrack(project(), track_index)
    for item_index = reaper.CountTrackMediaItems(track) - 1, 0, -1 do
      local item = reaper.GetTrackMediaItem(track, item_index)
      local role = media_item_ext(item, "ReaADR.role")
      if generated_roles[role] then
        reaper.DeleteTrackMediaItem(track, item)
        removed_items = removed_items + 1
      end
    end
  end

  return removed_items
end

function ReaADR.cleanup_generated_items()
  local removed_items = 0

  reaper.Undo_BeginBlock()
  reaper.PreventUIRefresh(1)

  removed_items = ReaADR.delete_generated_cue_audio_items()

  reaper.PreventUIRefresh(-1)
  reaper.TrackList_AdjustWindows(false)
  reaper.UpdateArrange()
  reaper.Undo_EndBlock("ReaADR: clean generated cue items", -1)

  return {
    removed_items = removed_items,
  }
end

function ReaADR.marker_decision_key(is_region, marker_id)
  return ("%s:%s"):format(is_region and "region" or "marker", tostring(marker_id))
end

function ReaADR.get_marker_decision(is_region, marker_id)
  return project_key_value(ReaADR.EXT_NAMESPACE, "marker_decision." .. ReaADR.marker_decision_key(is_region, marker_id))
end

function ReaADR.set_marker_decision(is_region, marker_id, decision)
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "marker_decision." .. ReaADR.marker_decision_key(is_region, marker_id), tostring(decision or ""))
end

local function eel_quote(value)
  value = tostring(value or "")
  value = value:gsub("\\", "\\\\")
  value = value:gsub('"', '\\"')
  value = value:gsub("\r\n", " ")
  value = value:gsub("\r", " ")
  value = value:gsub("\n", " ")
  return '"' .. value .. '"'
end

local function overlay_fx_code(cues, settings)
  local frame_rate = reaper.TimeMap_curFrameRate(project())
  if not frame_rate or frame_rate <= 0 then
    frame_rate = 24
  end
  local selected_key = ReaADR.selected_region_cue_key()
  if selected_key == "" then
    selected_key = ReaADR.selected_cue_key()
  end
  if selected_key == "" then
    selected_key = ReaADR.active_overlay_cue_key()
  end
  local display_cues = {}
  for index, cue in ipairs(cues or {}) do
    display_cues[index] = cue
  end
  table.sort(display_cues, function(a, b)
    local a_selected = selected_key ~= "" and ReaADR.cue_key(a) == selected_key
    local b_selected = selected_key ~= "" and ReaADR.cue_key(b) == selected_key
    if a_selected ~= b_selected then
      return a_selected
    end
    if a.start_time == b.start_time then
      return tostring(a.id) < tostring(b.id)
    end
    return (tonumber(a.start_time) or 0) < (tonumber(b.start_time) or 0)
  end)
  local display_fps = math.max(1, math.floor(frame_rate + 0.5))
  local lines = {
    "// ReaADR generated source-track video overlay",
    "gfx_blit(0);",
    "w = project_w > 0 ? project_w : 1920;",
    "h = project_h > 0 ? project_h : 1080;",
    "now = project_time;",
    "font_cue = max(42, h * 0.070);",
    "font_meta = max(30, h * 0.045);",
    "font_timer = max(38, h * 0.058);",
    "font_direction = max(34, h * 0.050);",
    "font_dialogue = max(42, h * 0.060);",
    "font_status = max(24, h * 0.034);",
    "margin = max(24, w * 0.035);",
    "pad = max(12, h * 0.018);",
    "overlay_drawn = 0;",
    "active_region_count = 0;",
    ("display_fps = %d;"):format(display_fps),
  }
  local base_text_rgba = ReaADR.overlay_text_rgba_expr(settings)

  local function wrap_overlay_text(text, max_chars)
    text = trim(text)
    max_chars = math.max(16, tonumber(max_chars) or 48)
    if text == "" then
      return {}
    end

    local wrapped = {}
    local current = ""
    for word in text:gmatch("%S+") do
      local candidate = current == "" and word or (current .. " " .. word)
      if #candidate <= max_chars then
        current = candidate
      else
        if current ~= "" then
          wrapped[#wrapped + 1] = current
        end
        while #word > max_chars do
          wrapped[#wrapped + 1] = word:sub(1, max_chars)
          word = word:sub(max_chars + 1)
        end
        current = word
      end
    end
    if current ~= "" then
      wrapped[#wrapped + 1] = current
    end
    return wrapped
  end

  local function append_text_draw(text_var, font_var, color_expr, x_expr, y_expr, measure_w_var, measure_h_var, bg_enabled, pad_x_expr, pad_y_expr)
    lines[#lines + 1] = ("gfx_setfont(%s, \"Arial\");"):format(font_var)
    lines[#lines + 1] = ("gfx_str_measure(%s, %s, %s);"):format(text_var, measure_w_var, measure_h_var)
    if bg_enabled then
      lines[#lines + 1] = ("gfx_set(0, 0, 0, 0.62); gfx_fillrect(max(0, (%s) - (%s)), (%s) - (%s), min(w, %s + ((%s) * 2)), %s + ((%s) * 2));"):format(
        x_expr, pad_x_expr, y_expr, pad_y_expr, measure_w_var, pad_x_expr, measure_h_var, pad_y_expr
      )
    end
    lines[#lines + 1] = ("gfx_set(%s); gfx_str_draw(%s, %s, %s);"):format(color_expr, text_var, x_expr, y_expr)
  end

  for _, cue in ipairs(display_cues) do
    local cue_start = tonumber(cue.start_time) or 0
    local cue_end = tonumber(cue.end_time) or cue_start
    lines[#lines + 1] = ("now >= %.6f && now <= %.6f ? (active_region_count += 1);"):format(cue_start, cue_end)
  end

  if settings.show_project_timer then
    lines[#lines + 1] = "project_total_frames = floor(max(0, now) * display_fps + 0.5); project_frames = project_total_frames - floor(project_total_frames / display_fps) * display_fps; project_total_seconds = floor(project_total_frames / display_fps); project_seconds = project_total_seconds - floor(project_total_seconds / 60) * 60; project_total_minutes = floor(project_total_seconds / 60); project_minutes = project_total_minutes - floor(project_total_minutes / 60) * 60; project_hours = floor(project_total_minutes / 60); sprintf(#project_tc, \"%02d:%02d:%02d:%02d\", project_hours, project_minutes, project_seconds, project_frames);"
    lines[#lines + 1] = "#timeline_label = \"Timeline SMPTE\";"
    append_text_draw("#timeline_label", "font_status", base_text_rgba, "(w - tlw) * 0.5", "margin * 0.40", "tlw", "tlh", settings.bg_project_timer, "pad * 0.9", "pad * 0.45")
    append_text_draw("#project_tc", "font_timer", base_text_rgba, "(w - ptw) * 0.5", "margin * 0.40 + font_status", "ptw", "pth", settings.bg_project_timer, "pad * 1.1", "pad * 0.55")
  end

  for _, cue in ipairs(display_cues) do
    local cue_start = tonumber(cue.start_time) or 0
    local cue_end = tonumber(cue.end_time) or cue_start
    local cue_timecode = format_timecode(cue_start, display_fps)
    local preroll = math.max(0, tonumber(settings.preroll_seconds) or 0)
    local item_start = math.max(0, cue_start - preroll)
    local note_text = trim(first_nonempty(cue.notes, cue.direction))
    if note_text ~= "" and not note_text:match("^%b()$") then
      note_text = "(" .. note_text .. ")"
    end
    local cue_status = normalize_status(cue.status)
    local status_rgb_value = status_rgb(cue_status)
    local cue_key = ReaADR.cue_key(cue)
    local condition
    if selected_key ~= "" and cue_key == selected_key then
      condition = ("overlay_drawn == 0 && now >= %.6f && now <= %.6f"):format(item_start, cue_end)
    else
      condition = ("overlay_drawn == 0 && now >= %.6f && now <= %.6f && (now >= %.6f || active_region_count == 0)"):format(item_start, cue_end, cue_start)
    end

    lines[#lines + 1] = condition .. " ? ("
    lines[#lines + 1] = "overlay_drawn = 1;"
    lines[#lines + 1] = ("cue_start = %.6f; cue_end = %.6f;"):format(cue_start, cue_end)
    lines[#lines + 1] = "rel = now - cue_start; until_cue = cue_start - now;"
    lines[#lines + 1] = ("pre = max(0.001, %.6f);"):format(math.max(0.001, cue_start - item_start))
    if settings.show_flash then
      lines[#lines + 1] = "flash = abs(rel) < 0.10 ? 1 : 0;"
      lines[#lines + 1] = "flash ? (gfx_set(1, 1, 1, 0.32); gfx_fillrect(0, 0, w, h));"
    end

    if settings.show_streamer then
      lines[#lines + 1] = "until_cue > 0 && until_cue <= pre ? (progress = 1 - (until_cue / pre); x = w * progress; gfx_set(0.0, 0.65, 1, 0.96); gfx_fillrect(0, h * 0.48 - 9, x, 18); gfx_set(1, 1, 1, 0.96); gfx_fillrect(x - 4, h * 0.34, 8, h * 0.28));"
    end

    if settings.show_visual_cue then
      lines[#lines + 1] = "abs(rel) < 0.05 ? (gfx_set(1, 1, 1, 0.98); gfx_fillrect(w * 0.5 - 8, h * 0.30, 16, h * 0.40); gfx_fillrect(w * 0.35, h * 0.50 - 8, w * 0.30, 16));"
      lines[#lines + 1] = "until_cue > 0 && until_cue <= pre ? (pulse = 1 - until_cue / pre; cue_x = w * 0.5; cue_y = h * 0.50; cue_size = max(34, h * 0.065) + pulse * max(28, h * 0.05); gfx_set(1, 0.86, 0.15, 0.90); gfx_fillrect(cue_x - cue_size * 0.5, cue_y - 5, cue_size, 10); gfx_fillrect(cue_x - 5, cue_y - cue_size * 0.5, 10, cue_size));"
    end

    if settings.show_cue_id then
      lines[#lines + 1] = "#cue_number = " .. eel_quote("Cue #" .. cue.id) .. ";"
      append_text_draw("#cue_number", "font_cue", base_text_rgba, "margin", "margin * 0.55", "cuew", "cueh", settings.bg_cue_id, "pad * 1.2", "pad * 0.6")
    end

    if settings.show_character then
      lines[#lines + 1] = "#character = " .. eel_quote(cue.character) .. ";"
      append_text_draw("#character", "font_meta", "0.75, 0.92, 1, 1", "margin", "margin * 0.55 + font_cue + pad * 0.4", "charw", "charh", settings.bg_character, "pad * 1.0", "pad * 0.45")
    end

    if settings.show_cue_timecode then
      lines[#lines + 1] = "#cue_tc = " .. eel_quote(cue_timecode) .. ";"
      lines[#lines + 1] = "#cue_tc_label = \"Cue SMPTE\";"
      append_text_draw("#cue_tc_label", "font_status", base_text_rgba, "w - margin - ctlw", "margin * 0.50", "ctlw", "ctlh", settings.bg_cue_timecode, "pad * 0.9", "pad * 0.45")
      append_text_draw("#cue_tc", "font_timer", base_text_rgba, "w - margin - ctw", "margin * 0.50 + font_status", "ctw", "cth", settings.bg_cue_timecode, "pad * 1.1", "pad * 0.55")
    else
      lines[#lines + 1] = "cth = 0;"
    end

    local media_time = ReaADR.cue_metadata_value(cue, "Media Time")
    if media_time ~= "" then
      lines[#lines + 1] = "#media_time_label = \"Media Time\";"
      lines[#lines + 1] = "#media_time = " .. eel_quote(media_time) .. ";"
      append_text_draw("#media_time_label", "font_status", "0.72, 0.78, 0.84, 1", "w - margin - mtlabelw", "margin * 0.50 + font_status + font_timer + pad * 0.70", "mtlabelw", "mtlabelh", settings.bg_metadata, "pad * 0.9", "pad * 0.45")
      append_text_draw("#media_time", "font_meta", base_text_rgba, "w - margin - mtw", "margin * 0.50 + font_status + font_timer + font_status + pad", "mtw", "mth", settings.bg_metadata, "pad * 1.0", "pad * 0.45")
    end

    if settings.show_cue_type and cue.cue_type ~= "" then
      lines[#lines + 1] = "#cue_type = " .. eel_quote(cue.cue_type) .. ";"
      if settings.bg_cue_type then
        lines[#lines + 1] = "gfx_setfont(font_status, \"Arial\"); gfx_str_measure(#cue_type, typew, typeh); type_y = margin * 0.50 + font_status + font_timer + pad * 1.65; gfx_set(0, 0, 0, 0.62); gfx_fillrect(max(0, w - margin - typew - pad * 1.8), type_y - pad * 0.30, min(w, typew + pad * 2.2), typeh + pad * 0.75); gfx_set(1, 1, 1, 1); gfx_str_draw(#cue_type, w - margin - typew - pad * 0.9, type_y);"
      else
        lines[#lines + 1] = "gfx_setfont(font_status, \"Arial\"); gfx_str_measure(#cue_type, typew, typeh); type_y = margin * 0.50 + font_status + font_timer + pad * 1.65; gfx_set(0.0, 0.50, 0.95, 0.82); gfx_fillrect(w - margin - typew - pad * 1.6, type_y - pad * 0.25, typew + pad * 1.8, typeh + pad * 0.65); gfx_set(1, 1, 1, 1); gfx_str_draw(#cue_type, w - margin - typew - pad * 0.8, type_y);"
      end
    end

    if settings.show_metadata then
      local metadata_pairs = ReaADR.visible_metadata_pairs(cue, settings)
      for index, pair in ipairs(metadata_pairs) do
        if index <= 5 then
          lines[#lines + 1] = ("#metadata_%d = %s;"):format(index, eel_quote(pair.key .. ": " .. pair.value))
          append_text_draw(("#metadata_%d"):format(index), "font_status", base_text_rgba, "w - margin - mdw", ("margin * 0.75 + cth + pad * %.1f"):format(2.0 + (index * 1.25)), "mdw", "mdh", settings.bg_metadata, "pad * 0.9", "pad * 0.40")
        end
      end
    end

    if settings.show_status then
      lines[#lines + 1] = "#status = " .. eel_quote("Status: " .. cue_status) .. ";"
      append_text_draw("#status", "font_status", ("%.3f, %.3f, %.3f, 1"):format(status_rgb_value[1] / 255, status_rgb_value[2] / 255, status_rgb_value[3] / 255), "margin", "margin * 0.55 + font_cue + font_meta + pad * 1.4", "statusw", "statush", settings.bg_status, "pad * 0.9", "pad * 0.40")
    end

    if settings.show_direction and note_text ~= "" then
      lines[#lines + 1] = "#direction = " .. eel_quote(note_text) .. ";"
      append_text_draw("#direction", "font_direction", base_text_rgba, "(w - dirw) * 0.5", "h * 0.66", "dirw", "dirh", settings.bg_direction, "pad * 1.1", "pad * 0.50")
    end

    if settings.show_dialogue and cue.line ~= "" then
      local dialogue_lines = wrap_overlay_text(cue.line, 52)
      lines[#lines + 1] = ("dialogue_base_y = h * 0.80 - ((%d - 1) * (font_dialogue * 0.46));"):format(#dialogue_lines)
      lines[#lines + 1] = "dialogue_box_w = 0; dialogue_box_h = 0;"
      for index, dialogue_line in ipairs(dialogue_lines) do
        lines[#lines + 1] = ("#dialogue_%d = %s;"):format(index, eel_quote(dialogue_line))
        lines[#lines + 1] = ("gfx_setfont(font_dialogue, \"Arial\"); gfx_str_measure(#dialogue_%d, dlgw_%d, dlgh_%d); dialogue_box_w = max(dialogue_box_w, dlgw_%d); dialogue_box_h = max(dialogue_box_h, ((%d - 1) * (font_dialogue * 0.92)) + dlgh_%d);"):format(
          index, index, index, index, index, index
        )
      end
      if settings.bg_dialogue then
        lines[#lines + 1] = "dialogue_box_x = max(0, max(margin, (w - dialogue_box_w) * 0.5) - pad * 1.5);"
        lines[#lines + 1] = "dialogue_box_y = dialogue_base_y - pad;"
        lines[#lines + 1] = "dialogue_box_draw_w = min(w - dialogue_box_x, dialogue_box_w + pad * 3);"
        lines[#lines + 1] = "dialogue_box_draw_h = dialogue_box_h + pad * 2;"
        lines[#lines + 1] = "gfx_set(0, 0, 0, 0.62); gfx_fillrect(dialogue_box_x, dialogue_box_y, dialogue_box_draw_w, dialogue_box_draw_h);"
      end
      for index, _ in ipairs(dialogue_lines) do
        append_text_draw(
          ("#dialogue_%d"):format(index),
          "font_dialogue",
          base_text_rgba,
          ("max(margin, (w - dw) * 0.5)"),
          ("dialogue_base_y + ((%d - 1) * (font_dialogue * 0.92))"):format(index),
          "dw",
          "dh",
          false,
          "pad * 1.5",
          "pad"
        )
      end
    end

    lines[#lines + 1] = ");"
  end

  return table.concat(lines, "\n")
end

local function find_source_video_overlay_fx(track)
  for i = 0, reaper.TrackFX_GetCount(track) - 1 do
    local ok, name = reaper.TrackFX_GetFXName(track, i)
    if ok and name:find("ReaADR Video Overlay", 1, true) then
      return i
    end
    local code_ok, code = reaper.TrackFX_GetNamedConfigParm(track, i, "VIDEO_CODE")
    if code_ok and code:find("ReaADR generated source-track video overlay", 1, true) then
      return i
    end
  end
  return -1
end

local function delete_source_video_overlay_fx(track)
  local deleted = false
  while true do
    local fx_index = find_source_video_overlay_fx(track)
    if fx_index < 0 then
      return deleted
    end
    reaper.TrackFX_Delete(track, fx_index)
    deleted = true
  end
end

function ReaADR.ensure_source_video_overlay_fx(track, cues, settings)
  if not settings.enabled then
    delete_source_video_overlay_fx(track)
    return "disabled"
  end

  delete_source_video_overlay_fx(track)

  local fx_index = reaper.TrackFX_AddByName(track, "Video processor", false, 1)
  if fx_index < 0 then
    return "failed"
  end

  reaper.TrackFX_SetNamedConfigParm(track, fx_index, "renamed_name", "ReaADR Video Overlay")
  if not reaper.TrackFX_SetNamedConfigParm(track, fx_index, "VIDEO_CODE", overlay_fx_code(cues, settings)) then
    return "failed"
  end
  reaper.TrackFX_SetNamedConfigParm(track, fx_index, "DONE", "")

  reaper.TrackFX_SetEnabled(track, fx_index, true)
  return "updated"
end

function ReaADR.refresh_overlay_fx_from_project(settings)
  settings = settings or ReaADR.load_overlay_settings()

  local cues, cue_error = ReaADR.load_last_import_cues()
  if not cues then
    return nil, cue_error
  end
  if ReaADR.character_filter_hides_regions() then
    cues = ReaADR.filter_cues_by_active_characters(cues)
  end

  local source_video_track = ReaADR.find_track_by_ext("source_video", "source_video")
  if not source_video_track then
    return nil, "ADR Source Video track was not found. Run Import Cue Sheet once first."
  end

  local status = ReaADR.ensure_source_video_overlay_fx(source_video_track, cues, settings)
  reaper.TrackList_AdjustWindows(false)
  reaper.UpdateArrange()
  return status
end

local function add_unique(list, seen, value)
  value = trim(value)
  if value ~= "" and not seen[value] then
    seen[value] = true
    list[#list + 1] = value
  end
end

function ReaADR.collect_characters(cues)
  local characters = {}
  local seen = {}
  for _, cue in ipairs(cues) do
    add_unique(characters, seen, first_nonempty(cue.character, "Unassigned"))
  end
  table.sort(characters)
  return characters
end

function ReaADR.character_region_lanes(cues)
  cues = cues or {}
  if assign_character_lanes then
    assign_character_lanes(cues, setup_preroll_seconds({ overlay_settings = ReaADR.load_overlay_settings() }))
  end
  local lanes = {}
  local characters = ReaADR.collect_characters(cues)
  local max_lanes = {}
  for _, cue in ipairs(cues) do
    local character = first_nonempty(cue.character, "Unassigned")
    max_lanes[character] = math.max(max_lanes[character] or 1, tonumber(cue._reaadr_lane) or 1)
  end
  local lane_index = 0
  for _, character in ipairs(characters) do
    local lane_count = math.max(1, tonumber(max_lanes[character]) or 1)
    for lane = 1, lane_count do
      local key = character_lane_key(character, lane)
      lanes[key] = lane_index
      if lane == 1 then
        lanes[character] = lane_index
      end
      lane_index = lane_index + 1
    end
  end
  return lanes
end

function ReaADR.ensure_character_ruler_lanes(cues)
  if not reaper.GetSetProjectInfo then
    return false
  end

  cues = cues or {}
  if assign_character_lanes then
    assign_character_lanes(cues, setup_preroll_seconds({ overlay_settings = ReaADR.load_overlay_settings() }))
  end
  local characters = ReaADR.collect_characters(cues)
  if #characters == 0 then
    return false
  end
  local max_lanes = {}
  for _, cue in ipairs(cues) do
    local character = first_nonempty(cue.character, "Unassigned")
    max_lanes[character] = math.max(max_lanes[character] or 1, tonumber(cue._reaadr_lane) or 1)
  end

  local lane_specs = {}
  for _, character in ipairs(characters) do
    local lane_count = math.max(1, tonumber(max_lanes[character]) or 1)
    for lane = 1, lane_count do
      lane_specs[#lane_specs + 1] = {
        character = character,
        lane = lane,
        label = lane <= 1 and character or ("%s #%d"):format(character, lane),
      }
    end
  end

  local existing_count = tonumber(reaper.GetSetProjectInfo(project(), "RULER_LANE_COUNT", 0, false)) or 0
  if existing_count < #lane_specs then
    pcall(reaper.GetSetProjectInfo, project(), "RULER_LANE_COUNT", #lane_specs, true)
  end

  for index, spec in ipairs(lane_specs) do
    local lane = index - 1
    if reaper.GetSetProjectInfo_String then
      pcall(reaper.GetSetProjectInfo_String, project(), "RULER_LANE_NAME:" .. tostring(lane), spec.label, true)
    end
    pcall(reaper.GetSetProjectInfo, project(), "RULER_LANE_COLOR:" .. tostring(lane), character_color(spec.character), true)
    pcall(reaper.GetSetProjectInfo, project(), "RULER_LANE_HIDDEN:" .. tostring(lane), 0, true)
  end

  return true
end

function ReaADR.region_lane_for_cue(cue, lanes)
  local character = first_nonempty(cue and cue.character, "Unassigned")
  local lane = tonumber(cue and cue._reaadr_lane) or 1
  local key = character_lane_key(character, lane)
  return (lanes or {})[key] or (lanes or {})[character] or 0
end

function character_lane_key(character, lane)
  return sanitize_token(character) .. ".lane" .. tostring(lane)
end

local function character_lane_name(character, lane)
  if lane <= 1 then
    return character
  end
  return ("%s %d"):format(character, lane)
end

local function cue_track_name(character, lane)
  if lane <= 1 then
    return "Cue - " .. character
  end
  return ("Cue - %s %d"):format(character, lane)
end

function setup_preroll_seconds(options)
  options = options or {}
  local preroll = tonumber(options.preroll_seconds)
  if not preroll and options.overlay_settings then
    preroll = tonumber(options.overlay_settings.preroll_seconds)
  end
  if not preroll then
    preroll = tonumber(ReaADR.DEFAULT_OVERLAY_SETTINGS.preroll_seconds)
  end
  return math.max(0, preroll or 0)
end

function assign_character_lanes(cues, preroll_seconds)
  preroll_seconds = math.max(0, tonumber(preroll_seconds) or 0)
  local sorted = {}
  for index, cue in ipairs(cues or {}) do
    sorted[index] = cue
  end
  table.sort(sorted, function(a, b)
    if a.start_time == b.start_time then
      return tostring(a.id) < tostring(b.id)
    end
    return (tonumber(a.start_time) or 0) < (tonumber(b.start_time) or 0)
  end)

  local lane_ends = {}
  local lane_counts = {}
  for _, cue in ipairs(sorted) do
    local character = first_nonempty(cue.character, "Unassigned")
    cue.character = character
    lane_ends[character] = lane_ends[character] or {}
    local lanes = lane_ends[character]
    local start_time = tonumber(cue.start_time) or 0
    local end_time = tonumber(cue.end_time) or start_time
    local cue_window_start = math.max(0, start_time - preroll_seconds)
    local assigned_lane = nil

    for lane = 1, #lanes do
      if cue_window_start >= lanes[lane] then
        assigned_lane = lane
        break
      end
    end

    if not assigned_lane then
      assigned_lane = #lanes + 1
    end

    lanes[assigned_lane] = math.max(lanes[assigned_lane] or 0, end_time)
    lane_counts[character] = math.max(lane_counts[character] or 0, assigned_lane)
    cue._reaadr_lane = assigned_lane
  end

  return lane_counts
end

function ReaADR.setup_project(cues, options)
  options = options or {}
  local create_source_video_track = options.create_source_video_track ~= false
  local use_existing_video_track = options.use_existing_video_track ~= false
  local create_character_tracks = options.create_character_tracks ~= false
  local create_cues_track = options.create_cues_track ~= false and options.cue_audio_path ~= nil
  local preroll_seconds = setup_preroll_seconds(options)
  local lane_counts = assign_character_lanes(cues, preroll_seconds)
  ReaADR.ensure_character_ruler_lanes(cues)
  local ruler_lanes = ReaADR.character_region_lanes(cues)
  local characters = ReaADR.collect_characters(cues)
  local lane_total = 0
  local overlap_conflicts = 0
  for _, character in ipairs(characters) do
    lane_total = lane_total + math.max(1, tonumber(lane_counts[character]) or 1)
  end
  for _, cue in ipairs(cues or {}) do
    if (tonumber(cue._reaadr_lane) or 1) > 1 then
      overlap_conflicts = overlap_conflicts + 1
    end
  end
  local total_steps = 5 + #cues
  if create_source_video_track then
    total_steps = total_steps + 1
  end
  if create_cues_track then
    total_steps = total_steps + lane_total
  end
  if create_character_tracks then
    total_steps = total_steps + lane_total
  end
  if options.overlay_settings and create_source_video_track then
    total_steps = total_steps + 1
  end
  local current_step = 0

  local function progress(message, amount)
    current_step = math.min(total_steps, current_step + (amount or 1))
    call_progress(options.on_progress, message, current_step, total_steps)
  end

  call_progress(options.on_progress, "Preparing project...", 0, total_steps)

  local summary = {
    tracks_created = 0,
    tracks_reused = 0,
    regions_created = 0,
    regions_updated = 0,
    markers_removed = 0,
    cue_audio_created = 0,
    cue_audio_updated = 0,
    cue_audio_skipped = 0,
    overlay_fx_status = "not_configured",
    cue_count = #cues,
    character_count = #characters,
    cue_track_count = create_cues_track and lane_total or 0,
    character_track_count = create_character_tracks and lane_total or 0,
    overlap_conflicts = overlap_conflicts,
  }

  reaper.Undo_BeginBlock()
  reaper.PreventUIRefresh(1)

  local ok, err = pcall(function()
    progress("Preparing ADR tracks...")
    local tracks = {}
    local source_video_track = nil
    if create_source_video_track and use_existing_video_track then
      source_video_track = ReaADR.find_existing_video_track()
      if source_video_track then
        ReaADR.mark_source_video_track(source_video_track, native_color(ROLE_COLORS.source_video))
        summary.tracks_reused = summary.tracks_reused + 1
        progress("Using existing video track...")
      end
    end

    if create_source_video_track and options.require_video_track and not source_video_track then
      error("No video item was found in the project. Place the video in the timeline before building ReaADR cues.")
    end

    if create_source_video_track and not source_video_track then
      tracks[#tracks + 1] = { role = "source_video", name = "ADR Source Video", key = "source_video", color = native_color(ROLE_COLORS.source_video) }
    end

    for _, spec in ipairs(tracks) do
      local track, created = ReaADR.ensure_track(spec.role, spec.name, spec.key, spec.color)
      if spec.role == "source_video" then
        source_video_track = track
      end

      if created then
        summary.tracks_created = summary.tracks_created + 1
      else
        summary.tracks_reused = summary.tracks_reused + 1
      end
      progress("Preparing ADR tracks...")
    end

    summary.markers_removed = ReaADR.remove_start_markers()
    progress("Removing old cue markers...")
    local existing_regions = project_markers_by_name(true)
    progress("Scanning existing regions...")

    local cue_tracks = {}
    local cue_audio_items = {}
    if create_cues_track or create_character_tracks then
      for _, character in ipairs(characters) do
        local lane_count = math.max(1, tonumber(lane_counts[character]) or 1)
        for lane = 1, lane_count do
          local key = character_lane_key(character, lane)

          if create_cues_track then
            local track, created = ReaADR.ensure_track("cue_character", cue_track_name(character, lane), key, character_color(character))
            cue_tracks[key] = track
            cue_audio_items[key] = cue_audio_items_by_key(track)
            if created then
              summary.tracks_created = summary.tracks_created + 1
            else
              summary.tracks_reused = summary.tracks_reused + 1
            end
            progress("Creating cue track: " .. cue_track_name(character, lane))
          end

          if create_character_tracks then
            local name = character_lane_name(character, lane)
            local _, created = ReaADR.ensure_track("character", name, key, character_color(character))
            if created then
              summary.tracks_created = summary.tracks_created + 1
            else
              summary.tracks_reused = summary.tracks_reused + 1
            end
            progress("Creating character track: " .. name)
          end
        end
      end
    end

    for cue_index, cue in ipairs(cues) do
      local cue_color = character_color(cue.character)
      local _, region_created = ensure_region_with_index(existing_regions, cue, options.region_color or cue_color)
      ReaADR.set_region_lane(cue, ReaADR.region_lane_for_cue(cue, ruler_lanes))
      if region_created then
        summary.regions_created = summary.regions_created + 1
      else
        summary.regions_updated = summary.regions_updated + 1
      end

      if options.cue_audio_path and create_cues_track then
        local lane = tonumber(cue._reaadr_lane) or 1
        local key = character_lane_key(first_nonempty(cue.character, "Unassigned"), lane)
        local cue_track = cue_tracks[key]
        local existing_cue_audio_items = cue_audio_items[key]
        local _, cue_audio_status = ReaADR.ensure_cue_audio_item(cue_track, cue, options.cue_audio_path, existing_cue_audio_items)
        if cue_audio_status == "created" then
          summary.cue_audio_created = summary.cue_audio_created + 1
        elseif cue_audio_status == "updated" then
          summary.cue_audio_updated = summary.cue_audio_updated + 1
        else
          summary.cue_audio_skipped = summary.cue_audio_skipped + 1
        end
      end

      progress(("Populating cue %d of %d"):format(cue_index, #cues))
    end

    if options.overlay_settings and source_video_track then
      progress("Installing video overlay...")
      summary.overlay_fx_status = ReaADR.ensure_source_video_overlay_fx(source_video_track, cues, options.overlay_settings)
    else
      progress("Skipping video overlay...")
    end

    progress("Saving ReaADR project state...")
    ReaADR.save_last_import_cues(cues)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "version", ReaADR.VERSION)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "last_import_cue_count", tostring(#cues))
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "last_import_character_count", tostring(#characters))
    if ReaADR.character_filter_enabled() then
      progress("Applying character filter...")
      ReaADR.apply_character_filter()
    end
    call_progress(options.on_progress, "Finalizing...", total_steps, total_steps)
  end)

  reaper.PreventUIRefresh(-1)
  reaper.TrackList_AdjustWindows(false)
  reaper.UpdateArrange()

  if ok then
    reaper.Undo_EndBlock("ReaADR: import cue sheet and setup ADR project", -1)
    return summary
  end

  reaper.Undo_EndBlock("ReaADR: failed cue sheet import", -1)
  return nil, err
end

function ReaADR.show_video_window()
  local command_id = 50125 -- View: Show video window
  if reaper.GetToggleCommandState(command_id) ~= 1 then
    reaper.Main_OnCommand(command_id, 0)
  end
end

return ReaADR
