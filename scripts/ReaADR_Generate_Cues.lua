-- ReaADR_Generate_Cues.lua
-- Detect existing project markers/regions and build a complete editable ReaADR
-- session from them.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local cues = ReaADR.collect_project_marker_cues({
  include_markers = true,
  include_regions = true,
  character       = "ADR",
})

if #cues == 0 then
  ReaADR.message("No project markers or regions were found.")
  return
end

if not ReaADR.confirm_replace_active_cues("Generate Cues from Markers/Regions") then
  return
end

local answer = reaper.ShowMessageBox(
  ("Generate a complete editable ReaADR session from %d marker/region cue point(s)?\n\n" ..
	   "This creates or updates saved cues, cue regions, cue audio, character tracks, lane assignments, and video overlay data."):format(#cues),
  "ReaADR \xe2\x80\x93 Generate Cues",
  4
)
if answer ~= 6 then return end

local progress     = ReaADR.create_progress_window("Generating ADR Cue Items")
local cue_audio_path = ReaADR.project_cue_audio_path()

local frame_rate = reaper.TimeMap_curFrameRate(0)
if not frame_rate or frame_rate <= 0 then frame_rate = 24 end

local generated_cue_path, generated_cue_error =
  ReaADR.generate_project_cue_wav(cue_audio_path, frame_rate)
if not generated_cue_path then
  progress.close()
  ReaADR.message("Cue generation failed while creating cue audio:\n\n" .. tostring(generated_cue_error))
  return
end

local sync_summary, setup_error = ReaADR.commit_session_cues(cues, {
  snapshot_label = "Generate Cues from Markers/Regions",
  undo_description = "ReaADR: generate cues from markers and regions",
  save_options = {
    event_type = "BulkCueCreated", source = "generate_cues_from_selection",
    last_operation = "generate_cues_from_selection",
  },
  sync_options = {
    overlay_settings = ReaADR.load_overlay_settings(),
    create_source_video_track = true, require_video_track = false,
    create_character_tracks = true, create_cues_track = true,
    on_progress = progress.update, source = "generate_cues_from_selection",
  },
})
local summary = sync_summary and sync_summary.rebuild

progress.close()

if not summary then
  ReaADR.log("ERROR", "GENERATE", "Cue generation failed", { detail = tostring(setup_error) })
  ReaADR.message("Cue generation failed:\n\n" .. tostring(setup_error))
  return
end

ReaADR.message(
  ("Generated a complete ReaADR session from %d marker/region cue point(s).\n\n" ..
	   "Tracks: %d created, %d reused\nRegions: %d created, %d updated\nCue audio: %d created, %d updated, %d skipped\nOverlay: %s"):format(
    summary.cue_count or 0,
    summary.tracks_created or 0,
    summary.tracks_reused or 0,
    summary.regions_created or 0,
    summary.regions_updated or 0,
    summary.cue_audio_created or 0,
    summary.cue_audio_updated or 0,
    summary.cue_audio_skipped or 0,
    summary.overlay_fx_status or "not_configured"
  )
)
