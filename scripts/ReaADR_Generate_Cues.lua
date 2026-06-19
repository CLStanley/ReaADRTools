-- ReaADR_Generate_Cues.lua  (Step 1 of 2)
-- Detect existing project markers/regions and generate cue audio items on the
-- ReaADR cue track.  Character tracks, overlay, and lane assignment are NOT
-- built here — use "Rebuild Session From Cache" (Step 2) for the full structure.

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

local answer = reaper.ShowMessageBox(
  ("Generate ReaADR cue items from %d marker/region cue point(s)?\n\n" ..
   "Step 1 of 2: creates cue audio items only.\n" ..
   "Use \xe2\x80\x9cRebuild Session From Cache\xe2\x80\x9d afterward to build\n" ..
   "character tracks, overlay, and lane assignments."):format(#cues),
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

-- Step 1: cue audio items only — no character tracks, no overlay, no video track required.
local snapshot = ReaADR.create_session_snapshot("Generate Cues from Markers/Regions")
local summary, setup_error = ReaADR.setup_project(cues, {
  cue_audio_path         = cue_audio_path,
  overlay_settings       = ReaADR.load_overlay_settings(),
  create_source_video_track = false,
  require_video_track    = false,
  create_character_tracks = false,
  create_cues_track      = true,
  on_progress            = progress.update,
})

progress.close()

if not summary then
  ReaADR.restore_session_snapshot(snapshot, "Generate cues failed: " .. tostring(setup_error))
  ReaADR.log("ERROR", "GENERATE", "Cue generation failed", { detail = tostring(setup_error) })
  ReaADR.message("Cue generation failed:\n\n" .. tostring(setup_error))
  return
end

ReaADR.message(
  ("Step 1 complete: %d cue item(s) generated.\n\n" ..
   "Cue audio created: %d  updated: %d  skipped: %d\n\n" ..
   "Next: use \xe2\x80\x9cRebuild Session From Cache\xe2\x80\x9d to add character tracks,\n" ..
   "lane assignments, and video overlay."):format(
    summary.cue_count or 0,
    summary.cue_audio_created or 0,
    summary.cue_audio_updated or 0,
    summary.cue_audio_skipped or 0
  )
)
