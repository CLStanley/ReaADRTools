-- Import an ADR cue sheet and set up tracks, cue start markers, and regions.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local core_path = script_dir() .. "/ReaADR_Core.lua"
local ReaADR = dofile(core_path)
local cue_audio_path = script_dir() .. "/../assets/cue.wav"
local overlay_settings = ReaADR.load_overlay_settings()

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

local summary, setup_error = ReaADR.setup_project(cues, {
  cue_audio_path = cue_audio_path,
  overlay_settings = overlay_settings,
})
if not summary then
  ReaADR.message("Project setup failed:\n\n" .. tostring(setup_error))
  return
end

ReaADR.message(
  ("Imported %d cue(s) for %d character(s).\n\nTracks: %d created, %d reused\nRegions: %d created, %d updated\nCue markers: %d created, %d updated\nCue audio: %d created, %d updated, %d skipped\nVideo overlays: %d created, %d updated, %d skipped"):format(
    summary.cue_count,
    summary.character_count,
    summary.tracks_created,
    summary.tracks_reused,
    summary.regions_created,
    summary.regions_updated,
    summary.markers_created,
    summary.markers_updated,
    summary.cue_audio_created,
    summary.cue_audio_updated,
    summary.cue_audio_skipped,
    summary.overlays_created,
    summary.overlays_updated,
    summary.overlays_skipped
  )
)
