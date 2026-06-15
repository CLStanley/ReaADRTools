-- Detect dialogue regions from selected audio/video media and build editable ADR cues.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

-- Capture source item info before the dialog so it's available if transcription is requested.
local src_item       = reaper.GetSelectedMediaItem(0, 0)
local src_take       = src_item  and reaper.GetActiveTake(src_item)
local src_source     = src_take  and reaper.GetMediaItemTake_Source(src_take)
local source_path    = src_source and reaper.GetMediaSourceFileName(src_source, "") or ""
local item_position  = src_item  and (reaper.GetMediaItemInfo_Value(src_item, "D_POSITION")  or 0) or 0
local item_startoffs = src_item  and (reaper.GetMediaItemInfo_Value(src_item, "D_STARTOFFS") or 0) or 0
-- item_offset converts Whisper's source-relative times to project-absolute times.
local item_offset    = item_position - item_startoffs

local defaults = table.concat({
  "ADR",
  "-42",
  "0.25",
  "0.35",
  "0.05",
  "n",
}, ",")

local ok, values = reaper.GetUserInputs(
  "Detect Dialogue From Selected Media",
  6,
  "Character,Threshold dB,Min speech sec,Min silence sec,Pad sec,Transcribe (y/n)",
  defaults
)
if not ok then
  return
end

local parts = {}
for value in (values .. ","):gmatch("([^,]*),") do
  parts[#parts + 1] = value
end

local do_transcribe = (parts[6] or ""):lower():sub(1, 1) == "y"

local cues, detect_error = ReaADR.detect_dialogue_cues_from_selected_media({
  character = parts[1],
  threshold_db = tonumber(parts[2]) or -42,
  min_speech_seconds = tonumber(parts[3]) or 0.25,
  min_silence_seconds = tonumber(parts[4]) or 0.35,
  pad_seconds = tonumber(parts[5]) or 0.05,
  cue_type = "Dialogue",
})

if not cues then
  ReaADR.message("Dialogue detection failed:\n\n" .. tostring(detect_error))
  return
end

if #cues == 0 then
  ReaADR.message(tostring(detect_error or "No dialogue regions were detected."))
  return
end

-- Transcription pass: overlay Whisper text onto detected cue time ranges.
if do_transcribe then
  if source_path == "" then
    ReaADR.message(
      "Transcription skipped: the source file path is not accessible for the selected item.\n\n" ..
      "Detection results will continue without dialogue text."
    )
  else
    reaper.ShowConsoleMsg("[ReaADR] Transcribing with Whisper (this may take several minutes for long media)...\n")
    local segs, whisper_err = ReaADR.transcribe_media_segments(source_path, { model = "base" })
    if not segs then
      ReaADR.message(
        "Transcription failed:\n\n" .. tostring(whisper_err) ..
        "\n\nDialogue detection results will continue without transcription text."
      )
    else
      reaper.ShowConsoleMsg(("[ReaADR] Transcription complete: %d segment(s).\n"):format(#segs))
      for _, c in ipairs(cues) do
        local cs = tonumber(c.start_time) or 0
        local ce = tonumber(c.end_time)   or cs
        local best_text    = ""
        local best_overlap = 0
        for _, seg in ipairs(segs) do
          local ss = seg.start_time + item_offset
          local se = seg.end_time   + item_offset
          local overlap = math.min(ce, se) - math.max(cs, ss)
          if overlap > best_overlap then
            best_overlap = overlap
            best_text    = seg.text or ""
          end
        end
        if best_text ~= "" then
          c.line  = best_text
          c.notes = (tostring(c.notes or "") ~= "") and (c.notes .. " [transcribed]") or "[transcribed]"
        end
      end
    end
  end
end

local preview = ReaADR.validate_cues(cues, { preroll_seconds = ReaADR.load_overlay_settings().preroll_seconds })
local answer = reaper.ShowMessageBox(
  ReaADR.validation_summary_text(preview):gsub("^Import validation", "Dialogue detection preview"),
  "ReaADR Dialogue Detection",
  4
)
if answer ~= 6 then
  return
end

ReaADR.save_last_import_cues(cues)

local progress = ReaADR.create_progress_window("Building Detected ADR Cues")
local summary, setup_error = ReaADR.rebuild_cached_session({
  on_progress = progress.update,
})
progress.close()

if not summary then
  ReaADR.message("Detected cues were cached, but project setup failed:\n\n" .. tostring(setup_error))
  return
end

ReaADR.show_video_window()
ReaADR.message(
  ("Detected and created %d editable cue(s).\n\nTracks: %d created, %d reused\nRegions: %d created, %d updated\nCue audio: %d created, %d updated, %d skipped\nOverlap splits: %d\nOverlay: %s"):format(
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
