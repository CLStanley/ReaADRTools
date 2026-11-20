-- Detect dialogue regions from selected audio/video media and build editable ADR cues.
-- Prefers the native extension scan path and falls back to a defer-based Lua scan.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local src_item = reaper.GetSelectedMediaItem(0, 0)
local src_take = src_item and reaper.GetActiveTake(src_item)
local item_position = src_item and (reaper.GetMediaItemInfo_Value(src_item, "D_POSITION") or 0) or 0

if not src_item or not src_take then
  ReaADR.message("Select one audio or video media item to analyze.")
  return
end

if not reaper.CreateTakeAudioAccessor or not reaper.GetAudioAccessorSamples then
  ReaADR.message("This REAPER build does not expose the audio accessor APIs needed for dialogue detection.")
  return
end

local ok, values = reaper.GetUserInputs(
  "Detect Dialogue From Selected Media",
  5,
  "Character/Placeholder,Threshold dB,Min speech sec,Min silence sec,Pad sec",
  table.concat({ "ADR", "-42", "0.25", "0.35", "0.05" }, ",")
)
if not ok then
  return
end

local parts = {}
for value in (values .. ","):gmatch("([^,]*),") do
  parts[#parts + 1] = value
end

local character = (parts[1] ~= "" and parts[1]) or "Unknown"
local threshold_db = tonumber(parts[2]) or -42
local min_speech = tonumber(parts[3]) or 0.25
local min_silence = tonumber(parts[4]) or 0.35
local pad = tonumber(parts[5]) or 0.05

local SAMPLE_RATE = 12000
local CHANNELS = 1
local BLOCK_SAMPLES = 300
local BLOCK_DUR = BLOCK_SAMPLES / SAMPLE_RATE
local BLOCKS_PER_FRAME = 200
local threshold = 10 ^ (threshold_db / 20)

local item_length = tonumber(reaper.GetMediaItemInfo_Value(src_item, "D_LENGTH")) or 0
local item_end = item_position + item_length
local function build_and_import_cues(raw_segments)
  if #raw_segments == 0 then
    ReaADR.message("No dialogue regions were detected with the current settings.\n\nTry lowering the threshold dB value.")
    return
  end

  if not ReaADR.confirm_replace_active_cues("Detect Dialogue From Selected Media") then
    return
  end

  local cues = {}
  local cue_id_width = math.max(3, #tostring(#raw_segments))
  for index, segment in ipairs(raw_segments) do
    cues[index] = {
      id = ("%0" .. tostring(cue_id_width) .. "d"):format(index),
      character = character ~= "" and character or "Unknown",
      cue_type = "Dialogue",
      start_time = segment.start_time,
      end_time = segment.end_time,
      line = "",
      status = "Not Recorded",
      notes = "Detected from selected media",
      source_line = index,
    }
  end

  local preview_summary, preview_error = ReaADR.sync_validate(
    { cues = cues },
    { preroll_seconds = ReaADR.load_overlay_settings().preroll_seconds }
  )
  if not preview_summary then
    ReaADR.message("Dialogue detection preview failed:\n\n" .. tostring(preview_error))
    return
  end
  local preview = preview_summary.validation
  local answer = reaper.ShowMessageBox(
    ReaADR.validation_summary_text(preview):gsub("^Import validation", "Dialogue detection preview"),
    "ReaADR Dialogue Detection",
    4
  )
  if answer ~= 6 then
    return
  end

  ReaADR.log("INFO", "DETECT", "Saving detected dialogue cues", { count = #cues })

  local progress = ReaADR.create_progress_window("Building Detected ADR Cues")
  local sync_summary, setup_error = ReaADR.commit_session_cues(cues, {
    snapshot_label = "Detect Dialogue From Selected Media",
    undo_description = "ReaADR: build detected dialogue session",
    save_options = {
      event_type = "BulkCueCreated", source = "detect_dialogue",
      last_operation = "detect_dialogue",
    },
    sync_options = { on_progress = progress.update, source = "detect_dialogue" },
  })
  local summary = sync_summary and sync_summary.rebuild
  progress.close()

  if not summary then
    ReaADR.log("ERROR", "DETECT", "Detected cue setup failed", { detail = tostring(setup_error) })
    ReaADR.message("Detected cue setup failed and was rolled back:\n\n" .. tostring(setup_error))
    return
  end

  ReaADR.show_video_window()
  ReaADR.message(
    ("%s and created %d editable cue(s).\n\nTracks: %d created, %d reused\nRegions: %d created, %d updated\nCue audio: %d created, %d updated, %d skipped\nOverlap splits: %d\nOverlay: %s"):format(
      "Detected",
      summary.cue_count,
      summary.tracks_created,
      summary.tracks_reused,
      summary.regions_created,
      summary.regions_updated,
      summary.cue_audio_created,
      summary.cue_audio_updated,
      summary.cue_audio_skipped,
      summary.overlap_conflicts or 0,
      summary.overlay_fx_status
    )
  )
end

local function parse_segment_blob(blob)
  local segments = {}
  for line in tostring(blob or ""):gmatch("([^\n]+)") do
    local start_time, end_time = line:match("^([^\t]+)\t([^\t]+)$")
    start_time = tonumber(start_time)
    end_time = tonumber(end_time)
    if start_time and end_time and end_time > start_time then
      segments[#segments + 1] = {
        start_time = start_time,
        end_time = end_time,
      }
    end
  end
  return segments
end

local function detect_segments_native()
  local native_fn = reaper.ReaADR_DetectDialogueSegments
  if type(native_fn) ~= "function" then
    return nil, "unavailable"
  end

  local ok_call, ok_detected, segments_blob, error_text = pcall(
    native_fn,
    threshold_db,
    min_speech,
    min_silence,
    pad,
    SAMPLE_RATE,
    "",
    8 * 1024 * 1024,
    "",
    4096
  )
  if not ok_call then
    return nil, tostring(ok_detected)
  end
  if not ok_detected then
    return nil, tostring(error_text or "Native dialogue detection failed.")
  end
  return parse_segment_blob(segments_blob)
end

local native_segments, native_error = detect_segments_native()
if native_segments then
  build_and_import_cues(native_segments)
  return
end

if native_error and native_error ~= "unavailable" then
  reaper.ShowConsoleMsg("[ReaADR] Native dialogue detection unavailable, falling back to Lua scan: " .. tostring(native_error) .. "\n")
end

local accessor = reaper.CreateTakeAudioAccessor(src_take)
if not accessor then
  ReaADR.message("Could not create an audio accessor for the selected item.")
  return
end

local start_t = reaper.GetAudioAccessorStartTime(accessor)
local scan_end = math.min(reaper.GetAudioAccessorEndTime(accessor), start_t + item_length)
local total_dur = math.max(0.001, scan_end - start_t)
local buffer = reaper.new_array(BLOCK_SAMPLES)
local raw_segments = {}
local active_start = nil
local last_loud = nil
local t = start_t
local cancelled = false
local progress = ReaADR.create_progress_window("Detecting Dialogue")

progress.update("Starting scan...", 0, total_dur)

local function timeline_time(accessor_time)
  return item_position + math.max(0, accessor_time - start_t)
end

local function flush_segment()
  if active_start and last_loud and (last_loud - active_start) >= min_speech then
    raw_segments[#raw_segments + 1] = {
      start_time = math.max(item_position, timeline_time(active_start - pad)),
      end_time = math.min(item_end, timeline_time(last_loud + pad)),
    }
  end
  active_start = nil
  last_loud = nil
end

local function finalize_scan()
  reaper.DestroyAudioAccessor(accessor)
  progress.close()
  if cancelled then
    return
  end
  build_and_import_cues(raw_segments)
end

local function scan_frame()
  if gfx.getchar() < 0 then
    cancelled = true
    finalize_scan()
    return
  end

  for _ = 1, BLOCKS_PER_FRAME do
    if t >= scan_end then
      break
    end

    local block_end = math.min(scan_end, t + BLOCK_DUR)
    buffer.clear()
    local got = reaper.GetAudioAccessorSamples(accessor, SAMPLE_RATE, CHANNELS, t, BLOCK_SAMPLES, buffer)

    if got == 1 then
      local sum = 0
      for index = 1, BLOCK_SAMPLES do
        local sample = buffer[index]
        sum = sum + sample * sample
      end
      local rms = math.sqrt(sum / BLOCK_SAMPLES)
      local loud = rms >= threshold

      if loud then
        if not active_start then
          active_start = t
        end
        last_loud = block_end
      elseif active_start and last_loud and (t - last_loud) >= min_silence then
        flush_segment()
      end
    end

    t = block_end
  end

  progress.update(
    ("Scanning %.1f / %.1f s  (%d region(s) found)"):format(t - start_t, total_dur, #raw_segments),
    t - start_t,
    total_dur
  )

  if t < scan_end then
    reaper.defer(scan_frame)
    return
  end

  flush_segment()
  finalize_scan()
end

reaper.defer(scan_frame)
