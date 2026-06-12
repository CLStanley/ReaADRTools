-- Import an ADR cue sheet and set up tracks, cue regions, and overlays.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local core_path = script_dir() .. "/ReaADR_Core.lua"
local ReaADR = dofile(core_path)
local cue_audio_path = script_dir() .. "/../assets/cue.wav"
local overlay_settings = ReaADR.load_overlay_settings()
local import_preroll_seconds = 3

local ok, path = reaper.GetUserFileNameForRead("", "Import ADR cue sheet", "csv")
if not ok or not path or path == "" then
  return
end

local frame_rate = reaper.TimeMap_curFrameRate(0)
if not frame_rate or frame_rate <= 0 then
  frame_rate = 24
end

local cues, parse_error = ReaADR.parse_csv(path, frame_rate)
if not cues then
  ReaADR.message("Cue sheet import failed:\n\n" .. tostring(parse_error))
  return
end

overlay_settings.preroll_seconds = import_preroll_seconds
ReaADR.save_overlay_settings(overlay_settings)

local progress = ReaADR.create_progress_window("Importing ADR Cue Sheet")
local preroll_status = ReaADR.configure_project_preroll(import_preroll_seconds)
local summary, setup_error = ReaADR.setup_project(cues, {
  cue_audio_path = cue_audio_path,
  overlay_settings = overlay_settings,
  preroll_seconds = import_preroll_seconds,
  on_progress = progress.update,
})

if not summary then
  progress.update("Import failed.", 1, 1)
  progress.close()
  ReaADR.message("Cue sheet import failed while populating the project:\n\n" .. tostring(setup_error))
  return
end

progress.close()

ReaADR.show_video_window()

ReaADR.message(
  ("Imported %d cue(s) for %d character(s).\n\nPlace the picture on the ADR Source Video track. ReaADR Video Overlay was installed/updated as a Video Processor FX on that track.\n\nTracks: %d created, %d reused\nRegions: %d created, %d updated\nOld cue markers removed: %d\nCue audio: %d created, %d updated, %d skipped\nVideo overlay FX: %s\nPre-roll: %.1fs (%s)"):format(
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
    summary.overlay_fx_status,
    preroll_status.seconds,
    preroll_status.status
  )
)
