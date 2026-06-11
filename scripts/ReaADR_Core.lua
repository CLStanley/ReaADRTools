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
  show_countdown = true,
  show_streamer = true,
  show_flash = true,
  show_status = true,
  preroll_seconds = 3,
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
  value = value:gsub("^%s+", ""):gsub("%s+$", "")
  value = value:gsub("[%s%-]+", "_")
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

function ReaADR.message(text)
  reaper.ShowMessageBox(tostring(text), "ReaADR", 0)
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
      else
        local row = {}
        for index, header in ipairs(headers) do
          row[header] = trim(fields[index])
        end

        local cue_id = first_nonempty(row.cue_id, row.id, row.cue, tostring(#cues + 1))
        local character = first_nonempty(row.character, row.char, row.actor, "Unassigned")
        local start_value = first_nonempty(row.start, row.start_time, row.in_time, row["in"])
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
          line = first_nonempty(row.line, row.dialogue, row.dialog, row.text),
          notes = first_nonempty(row.notes, row.note),
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

local function get_track_name(track)
  local ok, value = reaper.GetSetMediaTrackInfo_String(track, "P_NAME", "", false)
  if ok then
    return value
  end
  return ""
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

function ReaADR.ensure_track(role, name, key)
  key = key or ""

  local existing = ReaADR.find_track_by_ext(role, key)
  if existing then
    return existing, false
  end

  for i = 0, reaper.CountTracks(project()) - 1 do
    local track = reaper.GetTrack(project(), i)
    if get_track_name(track) == name then
      set_track_ext(track, "ReaADR.role", role)
      set_track_ext(track, "ReaADR.key", key)
      set_track_ext(track, "ReaADR.version", ReaADR.VERSION)
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

function ReaADR.region_name(cue)
  return ("%s ADR Cue %s - %s"):format(ReaADR.cue_tag(cue), cue.id, cue.character)
end

function ReaADR.marker_name(cue)
  return ("%s Cue Start %s - %s"):format(ReaADR.cue_tag(cue), cue.id, cue.character)
end

function ReaADR.cue_item_name(cue)
  return ("%s Cue Audio %s - %s"):format(ReaADR.cue_tag(cue), cue.id, cue.character)
end

function ReaADR.overlay_item_name(cue)
  return ("%s Video Overlay %s - %s"):format(ReaADR.cue_tag(cue), cue.id, cue.character)
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

local function upsert_project_marker(is_region, start_time, end_time, name, color)
  local existing = find_project_marker(name, is_region)
  if existing then
    reaper.SetProjectMarker4(project(), existing.id, is_region, start_time, end_time or 0, name, color or existing.color, 0)
    return existing.id, false
  end

  local id = reaper.AddProjectMarker2(project(), is_region, start_time, end_time or 0, name, -1, color or 0)
  return id, true
end

function ReaADR.ensure_region(cue, color)
  return upsert_project_marker(true, cue.start_time, cue.end_time, ReaADR.region_name(cue), color)
end

function ReaADR.ensure_start_marker(cue, color)
  return upsert_project_marker(false, cue.start_time, 0, ReaADR.marker_name(cue), color)
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

function ReaADR.ensure_cue_audio_item(track, cue, audio_path)
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

  local item = ReaADR.find_cue_audio_item(track, cue)
  local status = "updated"
  if not item then
    item = reaper.AddMediaItemToTrack(track)
    status = "created"
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
  set_media_item_ext(item, "ReaADR.cue_key", ReaADR.cue_key(cue))
  set_media_item_ext(item, "ReaADR.version", ReaADR.VERSION)
  set_media_item_ext(item, "ReaADR.source_path", audio_path)

  return item, status
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

local function chunk_quote(value)
  value = tostring(value or "")
  value = value:gsub("[\r\n]", " ")
  value = value:gsub('"', "'")
  return '"' .. value .. '"'
end

local function video_code(settings, cue, item_start, item_length)
  local cue_start = cue.start_time
  local cue_end = cue.end_time
  local lines = {
    "// ReaADR generated video overlay",
    "gfx_blit(input_track(0));",
    "w = project_w; h = project_h;",
    "now = project_time;",
    ("cue_start = %.6f;"):format(cue_start),
    ("cue_end = %.6f;"):format(cue_end),
    ("item_start = %.6f;"):format(item_start),
    ("item_end = %.6f;"):format(item_start + item_length),
    "rel = now - cue_start;",
    "until_cue = cue_start - now;",
    "dur = max(0.001, cue_end - cue_start);",
    "pre = max(0.001, cue_start - item_start);",
    "font_big = max(34, h * 0.055);",
    "font_med = max(24, h * 0.038);",
    "font_small = max(18, h * 0.028);",
    "margin = max(24, w * 0.035);",
    "gfx_set(0, 0, 0, 0.45);",
    "gfx_fillrect(0, 0, w, h * 0.18);",
    "gfx_fillrect(0, h * 0.78, w, h * 0.22);",
  }

  if settings.show_flash then
    lines[#lines + 1] = "flash = abs(rel) < 0.10 ? 1 : 0;"
    lines[#lines + 1] = "flash ? (gfx_set(1, 1, 1, 0.45); gfx_fillrect(0, 0, w, h));"
  end

  if settings.show_streamer then
    lines[#lines + 1] = "until_cue > 0 && until_cue <= pre ? (progress = 1 - (until_cue / pre); gfx_set(0.1, 0.75, 1, 0.95); gfx_fillrect(0, h * 0.50 - 8, w * progress, 16); gfx_set(1, 1, 1, 0.95); gfx_fillrect(w * progress - 3, h * 0.38, 6, h * 0.24));"
  end

  if settings.show_countdown then
    lines[#lines + 1] = "until_cue > 0 ? (gfx_setfont(font_big, \"Arial\"); gfx_set(1, 0.86, 0.15, 1); sprintf(#countdown, \"%.1f\", until_cue); gfx_str_measure(#countdown, tw, th); gfx_str_draw(#countdown, w - margin - tw, margin));"
  end

  if settings.show_cue_id or settings.show_character then
    local heading_parts = {}
    if settings.show_cue_id then
      heading_parts[#heading_parts + 1] = "Cue " .. cue.id
    end
    if settings.show_character then
      heading_parts[#heading_parts + 1] = cue.character
    end
    lines[#lines + 1] = "#heading = " .. eel_quote(table.concat(heading_parts, " - ")) .. ";"
    lines[#lines + 1] = "gfx_setfont(font_big, \"Arial\");"
    lines[#lines + 1] = "gfx_set(1, 1, 1, 1);"
    lines[#lines + 1] = "gfx_str_draw(#heading, margin, margin);"
  end

  if settings.show_dialogue and cue.line ~= "" then
    lines[#lines + 1] = "#dialogue = " .. eel_quote(cue.line) .. ";"
    lines[#lines + 1] = "gfx_setfont(font_med, \"Arial\");"
    lines[#lines + 1] = "gfx_set(1, 1, 1, 1);"
    lines[#lines + 1] = "gfx_str_measure(#dialogue, dw, dh);"
    lines[#lines + 1] = "gfx_str_draw(#dialogue, max(margin, (w - dw) * 0.5), h * 0.84);"
  end

  if settings.show_status then
    lines[#lines + 1] = "rel < 0 ? #status = \"STANDBY\" : (now <= cue_end ? #status = \"TAKE\" : #status = \"CLEAR\");"
    lines[#lines + 1] = "gfx_setfont(font_small, \"Arial\");"
    lines[#lines + 1] = "gfx_set(rel >= 0 && now <= cue_end ? 1 : 0.5, rel < 0 ? 0.75 : 0.1, rel < 0 ? 0.15 : 0.1, 1);"
    lines[#lines + 1] = "gfx_str_measure(#status, sw, sh);"
    lines[#lines + 1] = "gfx_str_draw(#status, w - margin - sw, h * 0.84);"
  end

  return table.concat(lines, "\n")
end

local function set_video_processor_chunk(item, name, code)
  local ok, chunk = reaper.GetItemStateChunk(item, "", false)
  if not ok then
    return false
  end

  local prefix = chunk:match("(.+)<SOURCE EMPTY")
  if not prefix then
    prefix = chunk:match("(.+)<SOURCE")
  end
  if not prefix then
    return false
  end

  local source_chunk = 'NAME ' .. chunk_quote(name) .. [[
FADEFLAG 1
VOLPAN 1 0 1 -1
SOFFS 0
PLAYRATE 1 1 0 -1 0 0.0025
CHANMODE 0
GUID ]] .. reaper.genGuid() .. [[

<SOURCE VIDEOEFFECT
<CODE
]] .. code .. [[

>
CODEPARM 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
>
>]]

  return reaper.SetItemStateChunk(item, prefix .. source_chunk, false)
end

function ReaADR.find_overlay_item(track, cue)
  local cue_key = ReaADR.cue_key(cue)
  for i = 0, reaper.CountTrackMediaItems(track) - 1 do
    local item = reaper.GetTrackMediaItem(track, i)
    if media_item_ext(item, "ReaADR.role") == "video_overlay" and media_item_ext(item, "ReaADR.cue_key") == cue_key then
      return item
    end
  end
  return nil
end

function ReaADR.ensure_overlay_item(track, cue, settings)
  if not settings.enabled then
    local existing = ReaADR.find_overlay_item(track, cue)
    if existing then
      reaper.DeleteTrackMediaItem(track, existing)
    end
    return nil, "skipped"
  end

  local preroll = math.max(0, tonumber(settings.preroll_seconds) or 0)
  local item_start = math.max(0, cue.start_time - preroll)
  local item_end = cue.end_time
  local item_length = math.max(0.001, item_end - item_start)
  local item = ReaADR.find_overlay_item(track, cue)
  local status = "updated"

  if not item then
    item = reaper.AddMediaItemToTrack(track)
    status = "created"
  end

  local name = ReaADR.overlay_item_name(cue)
  reaper.SetMediaItemInfo_Value(item, "D_POSITION", item_start)
  reaper.SetMediaItemInfo_Value(item, "D_LENGTH", item_length)
  reaper.GetSetMediaItemInfo_String(item, "P_NOTES", name, true)
  set_video_processor_chunk(item, name, video_code(settings, cue, item_start, item_length))

  set_media_item_ext(item, "ReaADR.role", "video_overlay")
  set_media_item_ext(item, "ReaADR.cue_key", ReaADR.cue_key(cue))
  set_media_item_ext(item, "ReaADR.version", ReaADR.VERSION)

  return item, status
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
    add_unique(characters, seen, cue.character)
  end
  table.sort(characters)
  return characters
end

function ReaADR.setup_project(cues, options)
  options = options or {}

  local summary = {
    tracks_created = 0,
    tracks_reused = 0,
    regions_created = 0,
    regions_updated = 0,
    markers_created = 0,
    markers_updated = 0,
    cue_audio_created = 0,
    cue_audio_updated = 0,
    cue_audio_skipped = 0,
    overlays_created = 0,
    overlays_updated = 0,
    overlays_skipped = 0,
    cue_count = #cues,
    character_count = 0,
  }

  reaper.Undo_BeginBlock()
  reaper.PreventUIRefresh(1)

  local ok, err = pcall(function()
    local tracks = {
      { role = "folder", name = "ADR", key = "root" },
      { role = "cues", name = "ADR Cues", key = "cues" },
      { role = "streamers", name = "ADR Streamers", key = "streamers" },
      { role = "overlays", name = "ADR Video Overlays", key = "overlays" },
    }

    local cues_track
    local overlays_track
    for _, spec in ipairs(tracks) do
      local track, created = ReaADR.ensure_track(spec.role, spec.name, spec.key)
      if spec.role == "cues" then
        cues_track = track
      elseif spec.role == "overlays" then
        overlays_track = track
      end

      if created then
        summary.tracks_created = summary.tracks_created + 1
      else
        summary.tracks_reused = summary.tracks_reused + 1
      end
    end

    local characters = ReaADR.collect_characters(cues)
    summary.character_count = #characters

    for _, character in ipairs(characters) do
      local key = sanitize_token(character)
      local _, created = ReaADR.ensure_track("character", "ADR Character - " .. character, key)
      if created then
        summary.tracks_created = summary.tracks_created + 1
      else
        summary.tracks_reused = summary.tracks_reused + 1
      end
    end

    for _, cue in ipairs(cues) do
      local _, region_created = ReaADR.ensure_region(cue, options.region_color)
      if region_created then
        summary.regions_created = summary.regions_created + 1
      else
        summary.regions_updated = summary.regions_updated + 1
      end

      local _, marker_created = ReaADR.ensure_start_marker(cue, options.marker_color)
      if marker_created then
        summary.markers_created = summary.markers_created + 1
      else
        summary.markers_updated = summary.markers_updated + 1
      end

      if options.cue_audio_path then
        local _, cue_audio_status = ReaADR.ensure_cue_audio_item(cues_track, cue, options.cue_audio_path)
        if cue_audio_status == "created" then
          summary.cue_audio_created = summary.cue_audio_created + 1
        elseif cue_audio_status == "updated" then
          summary.cue_audio_updated = summary.cue_audio_updated + 1
        else
          summary.cue_audio_skipped = summary.cue_audio_skipped + 1
        end
      end

      if options.overlay_settings then
        local _, overlay_status = ReaADR.ensure_overlay_item(overlays_track, cue, options.overlay_settings)
        if overlay_status == "created" then
          summary.overlays_created = summary.overlays_created + 1
        elseif overlay_status == "updated" then
          summary.overlays_updated = summary.overlays_updated + 1
        else
          summary.overlays_skipped = summary.overlays_skipped + 1
        end
      end
    end

    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "version", ReaADR.VERSION)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "last_import_cue_count", tostring(#cues))
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "last_import_character_count", tostring(#characters))
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

return ReaADR
