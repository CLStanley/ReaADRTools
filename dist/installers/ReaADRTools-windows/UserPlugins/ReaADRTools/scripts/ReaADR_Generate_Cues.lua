-- Generate ReaADR cue aids from existing project markers and regions.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local base_dir = script_dir()
local ReaADR = dofile(base_dir .. "/ReaADR_Core.lua")
local cue_audio_path = ReaADR.project_cue_audio_path()

local cues = ReaADR.collect_project_marker_cues({
  include_markers = true,
  include_regions = true,
  character = "ADR",
})

if #cues == 0 then
  ReaADR.message("No project markers or regions were found.")
  return
end

local answer = reaper.ShowMessageBox(
  ("Generate ReaADR cue aids from %d marker/region cue point(s)?"):format(#cues),
  "ReaADR",
  4
)
if answer ~= 6 then
  return
end

local progress = ReaADR.create_progress_window("Generating ADR Cues")
local overlay_settings = ReaADR.load_overlay_settings()
local frame_rate = reaper.TimeMap_curFrameRate(0)
if not frame_rate or frame_rate <= 0 then
  frame_rate = 24
end
local generated_cue_path, generated_cue_error = ReaADR.generate_project_cue_wav(cue_audio_path, frame_rate)
if not generated_cue_path then
  progress.close()
  ReaADR.message("Cue generation failed while creating project cue audio:\n\n" .. tostring(generated_cue_error))
  return
end
local summary, setup_error = ReaADR.setup_project(cues, {
  cue_audio_path = cue_audio_path,
  overlay_settings = overlay_settings,
  create_source_video_track = true,
  require_video_track = true,
  create_character_tracks = false,
  create_cues_track = true,
  on_progress = progress.update,
})

if not summary then
  progress.update("Cue generation failed.", 1, 1)
  progress.close()
  ReaADR.message("Cue generation failed:\n\n" .. tostring(setup_error))
  return
end

progress.close()
ReaADR.message(
  ("Generated %d cue(s) from project markers/regions.\n\nTracks: %d created, %d reused\nRegions: %d created, %d updated\nCue audio: %d created, %d updated, %d skipped"):format(
    summary.cue_count,
    summary.tracks_created,
    summary.tracks_reused,
    summary.regions_created,
    summary.regions_updated,
    summary.cue_audio_created,
    summary.cue_audio_updated,
    summary.cue_audio_skipped
  )
)
