-- Detect dialogue regions from selected audio/video media and build editable ADR cues.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local defaults = table.concat({
  "ADR",
  "-42",
  "0.25",
  "0.35",
  "0.05",
}, ",")

local ok, values = reaper.GetUserInputs(
  "Detect Dialogue From Selected Media",
  5,
  "Character,Threshold dB,Min speech sec,Min silence sec,Pad sec",
  defaults
)
if not ok then
  return
end

local parts = {}
for value in (values .. ","):gmatch("([^,]*),") do
  parts[#parts + 1] = value
end

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
