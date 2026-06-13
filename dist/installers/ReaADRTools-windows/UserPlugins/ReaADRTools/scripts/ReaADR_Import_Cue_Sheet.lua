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

local ok, path = reaper.GetUserFileNameForRead("", "Import ADR script", "csv;tsv")
if not ok or not path or path == "" then
  return
end

local frame_rate = reaper.TimeMap_curFrameRate(0)
if not frame_rate or frame_rate <= 0 then
  frame_rate = 24
end

local function mapping_value(mapping, field)
  return tostring((mapping or {})[field] or "")
end

local function merge_mapping(primary, fallback)
  local merged = {}
  for key, value in pairs(fallback or {}) do
    merged[key] = value
  end
  for key, value in pairs(primary or {}) do
    if tostring(value or "") ~= "" then
      merged[key] = value
    end
  end
  return merged
end

local function prompt_column_mapping(details)
  local headers = table.concat(details.headers or {}, ", ")
  local default_mapping = merge_mapping(details.mapping, ReaADR.load_column_mapping_preset("last"))
  local defaults = table.concat({
    mapping_value(default_mapping, "cue_id"),
    mapping_value(default_mapping, "character"),
    mapping_value(default_mapping, "start"),
    mapping_value(default_mapping, "end"),
    mapping_value(default_mapping, "line"),
  }, ",")

  ReaADR.message(
    ("Column mapping is required for this %s file.\n\nAvailable columns:\n%s\n\nEnter source column names for the required ADR fields. Optional Dialogue may be blank."):format(
      tostring(details.delimiter_name or "script"),
      headers
    )
  )

  local ok, values = reaper.GetUserInputs(
    "Map ADR Script Columns",
    5,
    "Cue ID,Character,Start,End,Dialogue(optional)",
    defaults
  )
  if not ok then
    return nil
  end

  local fields = {}
  for value in (values .. ","):gmatch("([^,]*),") do
    fields[#fields + 1] = value
  end

  return {
    cue_id = fields[1],
    character = fields[2],
    start = fields[3],
    ["end"] = fields[4],
    line = fields[5],
  }
end

local cues, parse_error, parse_details = ReaADR.parse_script_file(path, frame_rate)
if not cues and parse_details and parse_details.code == "missing_required_mapping" then
  local mapping = prompt_column_mapping(parse_details)
  if mapping then
    cues, parse_error = ReaADR.parse_script_file(path, frame_rate, mapping)
    if cues then
      ReaADR.save_column_mapping_preset("last", mapping)
    end
  end
end

if not cues then
  ReaADR.message("Cue sheet import failed:\n\n" .. tostring(parse_error))
  return
end

local validation = ReaADR.validate_cues(cues, { preroll_seconds = import_preroll_seconds })
local proceed = reaper.ShowMessageBox(ReaADR.validation_summary_text(validation), "ReaADR Import Preview", 4)
if proceed ~= 6 then
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
  ("Imported %d cue(s) for %d character(s).\n\nPlace the picture on the ADR Source Video track. ReaADR Video Overlay was installed/updated as a Video Processor FX on that track.\n\nTracks: %d created, %d reused\nRegions: %d created, %d updated\nOld cue markers removed: %d\nCue audio: %d created, %d updated, %d skipped\nOverlap splits: %d\nVideo overlay FX: %s\nPre-roll: %.1fs (%s)"):format(
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
    summary.overlap_conflicts or 0,
    summary.overlay_fx_status,
    preroll_status.seconds,
    preroll_status.status
  )
)
