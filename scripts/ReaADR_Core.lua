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
  preroll_seconds = 3,
}

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
  local required = {
    { label = "cue_id", aliases = { "cue_id", "cue_number", "cue_num", "cue_no", "cue", "id", "number" } },
    { label = "character", aliases = { "character", "char", "actor", "speaker", "performer", "talent" } },
    { label = "start", aliases = { "start", "start_time", "timecode", "tc", "in_time", "in" } },
    { label = "end", aliases = { "end", "end_time", "out_time", "out" } },
  }
  local missing = {}
  for _, spec in ipairs(required) do
    if not header_has_any(headers, spec.aliases) then
      missing[#missing + 1] = spec.label .. " (" .. table.concat(spec.aliases, ", ") .. ")"
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

local function csv_split(line)
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
    elseif char == "," and not in_quotes then
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

local function csv_escape(value)
  value = tostring(value or "")
  local needs_quotes = value:find('[,"\r\n]') ~= nil
  value = value:gsub('"', '""')
  if needs_quotes then
    return '"' .. value .. '"'
  end
  return value
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

function ReaADR.create_progress_window(title)
  local state = {
    title = title or "ReaADR",
    width = 460,
    height = 128,
    last_message = "",
    closed = false,
  }

  gfx.init(state.title, state.width, state.height)
  gfx.clear = 0x202020

  local function draw(message, current, total)
    if state.closed then
      return
    end

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
      fields[index] = encode_cache_field(cue[key])
    end
    lines[#lines + 1] = table.concat(fields, "\t")
  end

  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "last_import_cues_v1", table.concat(lines, "\n"))
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

function ReaADR.parse_csv(path, frame_rate)
  local content, read_error = read_file(path)
  if not content then
    return nil, read_error
  end

  content = content:gsub("\r\n", "\n"):gsub("\r", "\n")

  local headers
  local cues = {}
  local seen_cue_keys = {}
  local line_number = 0

  content = content .. "\n"
  for raw_line in content:gmatch("([^\n]*)\n") do
    line_number = line_number + 1
    local line = raw_line:gsub("^\239\187\191", "")

    if trim(line) ~= "" then
      local fields = csv_split(line)

      if not headers then
        headers = {}
        for index, header in ipairs(fields) do
          headers[index] = normalize_header(header)
        end
        local missing = missing_required_csv_columns(headers)
        if #missing > 0 then
          return nil, "Cue sheet is missing required column(s):\n\n" .. table.concat(missing, "\n")
        end
      else
        local row = {}
        for index, header in ipairs(headers) do
          row[header] = trim(fields[index])
        end

        local cue_id = first_nonempty(row.cue_id, row.cue_number, row.cue_num, row.cue_no, row.cue, row.id, row.number, tostring(#cues + 1))
        local character = first_nonempty(row.character, row.char, row.actor, row.speaker, row.performer, row.talent, "Unassigned")
        local start_value = first_nonempty(row.start, row.start_time, row.timecode, row.tc, row.in_time, row["in"])
        local end_value = first_nonempty(row["end"], row.end_time, row.out_time, row["out"])
        local cue_key = sanitize_token(cue_id)
        local start_seconds, start_error = ReaADR.parse_timecode(start_value, frame_rate)
        local end_seconds, end_error = ReaADR.parse_timecode(end_value, frame_rate)

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

        seen_cue_keys[cue_key] = true
        cues[#cues + 1] = {
          id = trim(cue_id),
          character = trim(character),
          start_time = start_seconds,
          end_time = end_seconds,
          line = first_nonempty(row.line, row.dialogue, row.dialog, row.text, row.script),
          notes = first_nonempty(row.notes, row.note),
          direction = first_nonempty(row.direction, row.performance_direction, row.perf_direction),
          cue_type = first_nonempty(row.cue_type, row.type, row.category),
          status = normalize_status(first_nonempty(row.status, row.cue_status)),
          source_line = line_number,
        }
      end
    end
  end

  if not headers then
    return nil, "Cue sheet is empty"
  end
  if #cues == 0 then
    return nil, "Cue sheet contains no cues"
  end

  return cues
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

function ReaADR.character_filter_key(character)
  return sanitize_token(character):lower()
end

function ReaADR.encode_character_filter(characters)
  local tokens = {}
  for _, character in ipairs(characters or {}) do
    local token = ReaADR.character_filter_key(character)
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
end

function ReaADR.character_filter_hides_regions()
  local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "character_filter_hide_regions")
  return value == "1"
end

function ReaADR.set_character_filter_hides_regions(enabled)
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "character_filter_hide_regions", enabled and "1" or "0")
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

function ReaADR.filter_cues_by_active_characters(cues)
  if not ReaADR.character_filter_enabled() then
    return cues or {}
  end

  local filtered = {}
  for _, cue in ipairs(cues or {}) do
    if ReaADR.character_is_active(cue.character) then
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

  local muted_tracks = 0
  local active_tracks = 0
  local hidden_regions = 0
  local visible_regions = 0
  for i = 0, reaper.CountTracks(project()) - 1 do
    local track = reaper.GetTrack(project(), i)
    local role = track_ext(track, "ReaADR.role")
    if role == "character" or role == "cue_character" then
      local key = track_ext(track, "ReaADR.key")
      local character_token = tostring(key or ""):match("^(.-)%.lane%d+$") or key
      local active = not ReaADR.character_filter_enabled() or ReaADR.character_is_active(character_token)
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
    local hide = hide_inactive_regions and not ReaADR.character_is_active(cue.character)
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
  return true
end

function ReaADR.active_overlay_cue_key()
  return project_key_value(ReaADR.EXT_NAMESPACE, "active_overlay_cue_key")
end

function ReaADR.set_active_overlay_cue(cue)
  local key = cue and ReaADR.cue_key(cue) or ""
  reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "active_overlay_cue_key", key)
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
  ReaADR.set_region_lane(cue, ruler_lanes[cue.character] or 0)
  local overlay_status = nil
  if ReaADR.refresh_overlay_fx_from_project then
    overlay_status = ReaADR.refresh_overlay_fx_from_project()
  end
  reaper.UpdateArrange()
  return cue, overlay_status
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

function ReaADR.generated_item_roles()
  return {
    cue_audio = true,
  }
end

function ReaADR.cleanup_generated_items()
  local removed_items = 0
  local generated_roles = ReaADR.generated_item_roles()

  reaper.Undo_BeginBlock()
  reaper.PreventUIRefresh(1)

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

  for _, cue in ipairs(display_cues) do
    local cue_start = tonumber(cue.start_time) or 0
    local cue_end = tonumber(cue.end_time) or cue_start
    lines[#lines + 1] = ("now >= %.6f && now <= %.6f ? (active_region_count += 1);"):format(cue_start, cue_end)
  end

  if settings.show_project_timer then
    lines[#lines + 1] = "project_total_frames = floor(max(0, now) * display_fps + 0.5); project_frames = project_total_frames - floor(project_total_frames / display_fps) * display_fps; project_total_seconds = floor(project_total_frames / display_fps); project_seconds = project_total_seconds - floor(project_total_seconds / 60) * 60; project_total_minutes = floor(project_total_seconds / 60); project_minutes = project_total_minutes - floor(project_total_minutes / 60) * 60; project_hours = floor(project_total_minutes / 60); sprintf(#project_tc, \"%02d:%02d:%02d:%02d\", project_hours, project_minutes, project_seconds, project_frames);"
    lines[#lines + 1] = "gfx_setfont(font_timer, \"Arial\"); gfx_set(1, 1, 1, 0.96); gfx_str_measure(#project_tc, ptw, pth); gfx_str_draw(#project_tc, (w - ptw) * 0.5, margin * 0.65);"
  end

  for _, cue in ipairs(display_cues) do
    local cue_start = tonumber(cue.start_time) or 0
    local cue_end = tonumber(cue.end_time) or cue_start
    local cue_timecode = format_timecode(cue_start, display_fps)
    local preroll = math.max(0, tonumber(settings.preroll_seconds) or 0)
    local item_start = math.max(0, cue_start - preroll)
    local direction = cue.direction
    if direction ~= "" and not direction:match("^%b()$") then
      direction = "(" .. direction .. ")"
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
      lines[#lines + 1] = "gfx_setfont(font_cue, \"Arial\");"
      lines[#lines + 1] = "gfx_set(1, 1, 1, 1);"
      lines[#lines + 1] = "gfx_str_draw(#cue_number, margin, margin * 0.55);"
    end

    if settings.show_character then
      lines[#lines + 1] = "#character = " .. eel_quote(cue.character) .. ";"
      lines[#lines + 1] = "gfx_setfont(font_meta, \"Arial\");"
      lines[#lines + 1] = "gfx_set(0.75, 0.92, 1, 1);"
      lines[#lines + 1] = "gfx_str_draw(#character, margin, margin * 0.55 + font_cue + pad * 0.4);"
    end

    if settings.show_cue_timecode then
      lines[#lines + 1] = "#cue_tc = " .. eel_quote(cue_timecode) .. ";"
      lines[#lines + 1] = "gfx_setfont(font_timer, \"Arial\"); gfx_set(1, 1, 1, 1); gfx_str_measure(#cue_tc, ctw, cth); gfx_str_draw(#cue_tc, w - margin - ctw, margin * 0.75);"
    else
      lines[#lines + 1] = "cth = 0;"
    end

    if settings.show_cue_type and cue.cue_type ~= "" then
      lines[#lines + 1] = "#cue_type = " .. eel_quote(cue.cue_type) .. ";"
      lines[#lines + 1] = "gfx_setfont(font_status, \"Arial\"); gfx_str_measure(#cue_type, typew, typeh); gfx_set(0.0, 0.50, 0.95, 0.82); gfx_fillrect(w - margin - typew - pad * 2, margin * 0.75 + cth + pad * 0.6, typew + pad * 2, typeh + pad); gfx_set(1, 1, 1, 1); gfx_str_draw(#cue_type, w - margin - typew - pad, margin * 0.75 + cth + pad * 1.1);"
    end

    if settings.show_status then
      lines[#lines + 1] = "#status = " .. eel_quote("Status: " .. cue_status) .. ";"
      lines[#lines + 1] = "gfx_setfont(font_status, \"Arial\");"
      lines[#lines + 1] = ("gfx_set(%.3f, %.3f, %.3f, 1);"):format(status_rgb_value[1] / 255, status_rgb_value[2] / 255, status_rgb_value[3] / 255)
      lines[#lines + 1] = "gfx_str_draw(#status, margin, margin * 0.55 + font_cue + font_meta + pad * 1.4);"
    end

    if settings.show_direction and direction ~= "" then
      lines[#lines + 1] = "#direction = " .. eel_quote(direction) .. ";"
      lines[#lines + 1] = "gfx_setfont(font_direction, \"Arial\"); gfx_set(1, 0.86, 0.35, 1); gfx_str_measure(#direction, dirw, dirh); gfx_str_draw(#direction, (w - dirw) * 0.5, h * 0.66);"
    end

    if settings.show_dialogue and cue.line ~= "" then
      lines[#lines + 1] = "#dialogue = " .. eel_quote(cue.line) .. ";"
      lines[#lines + 1] = "gfx_setfont(font_dialogue, \"Arial\");"
      lines[#lines + 1] = "gfx_str_measure(#dialogue, dw, dh);"
      lines[#lines + 1] = "dialogue_x = max(margin, (w - dw) * 0.5); dialogue_y = h * 0.80;"
      lines[#lines + 1] = "gfx_set(0, 0, 0, 0.62); gfx_fillrect(max(0, dialogue_x - pad * 1.5), dialogue_y - pad, min(w, dw + pad * 3), dh + pad * 2);"
      lines[#lines + 1] = "gfx_set(1, 1, 1, 1);"
      lines[#lines + 1] = "gfx_str_draw(#dialogue, dialogue_x, dialogue_y);"
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
  local lanes = {}
  local characters = ReaADR.collect_characters(cues or {})
  for index, character in ipairs(characters) do
    lanes[character] = index - 1
  end
  return lanes
end

function ReaADR.ensure_character_ruler_lanes(cues)
  if not reaper.GetSetProjectInfo then
    return false
  end

  local characters = ReaADR.collect_characters(cues or {})
  if #characters == 0 then
    return false
  end

  local existing_count = tonumber(reaper.GetSetProjectInfo(project(), "RULER_LANE_COUNT", 0, false)) or 0
  if existing_count < #characters then
    pcall(reaper.GetSetProjectInfo, project(), "RULER_LANE_COUNT", #characters, true)
  end

  for index, character in ipairs(characters) do
    local lane = index - 1
    if reaper.GetSetProjectInfo_String then
      pcall(reaper.GetSetProjectInfo_String, project(), "RULER_LANE_NAME:" .. tostring(lane), character, true)
    end
    pcall(reaper.GetSetProjectInfo, project(), "RULER_LANE_COLOR:" .. tostring(lane), character_color(character), true)
    pcall(reaper.GetSetProjectInfo, project(), "RULER_LANE_HIDDEN:" .. tostring(lane), 0, true)
  end

  return true
end

local function character_lane_key(character, lane)
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

local function setup_preroll_seconds(options)
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

local function assign_character_lanes(cues, preroll_seconds)
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
  for _, character in ipairs(characters) do
    lane_total = lane_total + math.max(1, tonumber(lane_counts[character]) or 1)
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
      ReaADR.set_region_lane(cue, ruler_lanes[cue.character] or 0)
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
