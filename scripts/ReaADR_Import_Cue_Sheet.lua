-- Import an ADR cue sheet with script identity, selective character import,
-- duplicate protection, and revision-aware update behavior.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")
local cue_audio_path = ReaADR.project_cue_audio_path()
local overlay_settings = ReaADR.load_overlay_settings()
local import_preroll_seconds = 3

local function split_csv_list(value)
  local result = {}
  for token in tostring(value or ""):gmatch("([^,]+)") do
    token = tostring(token or ""):match("^%s*(.-)%s*$")
    if token ~= "" then
      result[#result + 1] = token
    end
  end
  return result
end

local function join_sorted_keys(map)
  local keys = {}
  for key in pairs(map or {}) do
    keys[#keys + 1] = key
  end
  table.sort(keys)
  return keys
end

local function parse_import_mode(value)
  value = tostring(value or ""):lower():match("^%s*(.-)%s*$")
  if value == "3" or value == "update" or value == "update existing import" then
    return "update"
  end
  if value == "2" or value == "selected" or value == "import selected characters" then
    return "selected"
  end
  return "all"
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

local function character_list_from_counts(counts)
  local names = {}
  for character in pairs(counts or {}) do
    names[#names + 1] = character
  end
  table.sort(names)
  return names
end

local function build_import_summary(script_info, cues, existing_counts, revision_diff)
  local lines, counts = ReaADR.character_summary_lines(cues, { imported_counts = existing_counts })
  local imported = join_sorted_keys(existing_counts or {})
  local available = {}
  for _, character in ipairs(character_list_from_counts(counts)) do
    if not existing_counts[character] then
      available[#available + 1] = character
    end
  end

  local summary = {
    "Select Characters To Import",
    "",
    "Script Name: " .. tostring(script_info.script_name or ""),
    "Script ID: " .. tostring(script_info.script_id or ""),
    "Cue Count: " .. tostring(script_info.cue_count or #cues),
  }

  if tostring(script_info.script_revision or "") ~= "" then
    summary[#summary + 1] = "Revision: " .. tostring(script_info.script_revision)
  end

  if #imported > 0 then
    summary[#summary + 1] = "Already Imported: " .. table.concat(imported, ", ")
  end
  if #available > 0 then
    summary[#summary + 1] = "Available: " .. table.concat(available, ", ")
  end

  if revision_diff and (
    revision_diff.new_cues > 0 or
    revision_diff.removed_cues > 0 or
    revision_diff.timing_changes > 0 or
    revision_diff.dialogue_changes > 0 or
    revision_diff.metadata_changes > 0
  ) then
    summary[#summary + 1] = ""
    summary[#summary + 1] = "Differences Detected:"
    summary[#summary + 1] = "+ New cues: " .. tostring(revision_diff.new_cues)
    summary[#summary + 1] = "+ Removed cues: " .. tostring(revision_diff.removed_cues)
    summary[#summary + 1] = "+ Timing changes: " .. tostring(revision_diff.timing_changes)
    summary[#summary + 1] = "+ Dialogue changes: " .. tostring(revision_diff.dialogue_changes)
    summary[#summary + 1] = "+ Metadata changes: " .. tostring(revision_diff.metadata_changes)
  end

  if #lines > 0 then
    summary[#summary + 1] = ""
    for _, line in ipairs(lines) do
      summary[#summary + 1] = line
    end
  end

  summary[#summary + 1] = ""
  summary[#summary + 1] = "Import Modes:"
  summary[#summary + 1] = "1 = Import Entire Script"
  summary[#summary + 1] = "2 = Import Selected Characters"
  summary[#summary + 1] = "3 = Update Existing Import"
  return table.concat(summary, "\n"), counts
end

local function prompt_import_plan(script_info, cues, existing_counts, revision_diff)
  local summary, counts = build_import_summary(script_info, cues, existing_counts, revision_diff)
  ReaADR.message(summary)

  local defaults_selected = {}
  for _, character in ipairs(character_list_from_counts(counts)) do
    if not existing_counts[character] then
      defaults_selected[#defaults_selected + 1] = character
    end
  end
  if #defaults_selected == 0 then
    defaults_selected = character_list_from_counts(counts)
  end

  local default_mode = next(existing_counts) and "3" or "2"
  local ok, values = reaper.GetUserInputs(
    "ADR Script Import Setup",
    3,
    "Script Name,Import Mode (1/2/3),Characters CSV",
    table.concat({
      tostring(script_info.script_name or ""),
      default_mode,
      table.concat(defaults_selected, ", "),
    }, ",")
  )
  if not ok then
    return nil
  end

  local fields = {}
  for value in (values .. ","):gmatch("([^,]*),") do
    fields[#fields + 1] = value
  end

  return {
    script_name = tostring(fields[1] or ""):match("^%s*(.-)%s*$"),
    mode = parse_import_mode(fields[2]),
    selected_characters = split_csv_list(fields[3]),
  }
end

local function warn_skipped_duplicates(skipped)
  if #skipped == 0 then
    return
  end
  ReaADR.message(
    "The following characters were already imported for this script and were skipped to prevent duplicates:\n\n" ..
    table.concat(skipped, "\n") ..
    "\n\nUse Import Mode 3 to update existing imported characters."
  )
end

local function apply_import_mode(existing_cues, imported_cues, script_id, selected_characters, mode)
  local selected_set = {}
  for _, character in ipairs(selected_characters or {}) do
    selected_set[character] = true
  end

  local final_cues = {}
  if mode == "update" then
    for _, cue in ipairs(existing_cues or {}) do
      if not (tostring(cue.script_id or "") == tostring(script_id or "") and selected_set[tostring(cue.character or "")]) then
        final_cues[#final_cues + 1] = cue
      end
    end
  else
    for _, cue in ipairs(existing_cues or {}) do
      final_cues[#final_cues + 1] = cue
    end
  end

  for _, cue in ipairs(imported_cues or {}) do
    final_cues[#final_cues + 1] = cue
  end

  table.sort(final_cues, function(a, b)
    local a_start = tonumber(a.start_time) or 0
    local b_start = tonumber(b.start_time) or 0
    if a_start == b_start then
      return tostring(a.id or "") < tostring(b.id or "")
    end
    return a_start < b_start
  end)
  return final_cues
end

local function stale_update_cues(existing_script_cues, imported_cues, selected_characters)
  local selected_set = {}
  for _, character in ipairs(selected_characters or {}) do
    selected_set[character] = true
  end

  local imported_keys = {}
  for _, cue in ipairs(imported_cues or {}) do
    imported_keys[tostring(cue.character or "") .. "\t" .. ReaADR.cue_key(cue)] = true
  end

  local stale = {}
  for _, cue in ipairs(existing_script_cues or {}) do
    local character = tostring(cue.character or "")
    if selected_set[character] and not imported_keys[character .. "\t" .. ReaADR.cue_key(cue)] then
      stale[#stale + 1] = cue
    end
  end
  return stale
end

local ok, path = reaper.GetUserFileNameForRead("", "Import ADR script", "csv;tsv;tab;txt;xlsx")
if not ok or not path or path == "" then
  return
end

local frame_rate = reaper.TimeMap_curFrameRate(0)
if not frame_rate or frame_rate <= 0 then
  frame_rate = 24
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

local existing_cues = ReaADR.load_last_import_cues() or {}
local initial_script_info = ReaADR.derive_script_identity(path, cues)
local existing_script_cues = ReaADR.script_cues(existing_cues, initial_script_info.script_id)
local existing_counts = ReaADR.character_counts(existing_script_cues)
local revision_diff = nil
if #existing_script_cues > 0 then
  revision_diff = ReaADR.compare_script_revisions(existing_script_cues, ReaADR.annotate_cues_with_script_info(cues, initial_script_info))
end

local plan = prompt_import_plan(initial_script_info, cues, existing_counts, revision_diff)
if not plan then
  return
end

local script_info = ReaADR.derive_script_identity(path, cues, {
  script_name = plan.script_name,
  script_id = initial_script_info.script_id,
  script_revision = initial_script_info.script_revision,
})
local annotated_cues = ReaADR.annotate_cues_with_script_info(cues, script_info)
local all_character_counts = ReaADR.character_counts(annotated_cues)
local all_characters = character_list_from_counts(all_character_counts)

local selected_characters = {}
if plan.mode == "all" then
  for _, character in ipairs(all_characters) do
    selected_characters[#selected_characters + 1] = character
  end
elseif #plan.selected_characters > 0 then
  selected_characters = plan.selected_characters
end

local selected_set = {}
for _, character in ipairs(selected_characters) do
  if all_character_counts[character] then
    selected_set[character] = true
  end
end

if next(selected_set) == nil then
  ReaADR.message("No valid characters were selected for import.")
  return
end

local skipped_duplicates = {}
if plan.mode ~= "update" then
  for character in pairs(selected_set) do
    if existing_counts[character] then
      skipped_duplicates[#skipped_duplicates + 1] = character
      selected_set[character] = nil
    end
  end
end
table.sort(skipped_duplicates)
warn_skipped_duplicates(skipped_duplicates)

selected_characters = join_sorted_keys(selected_set)
if #selected_characters == 0 then
  ReaADR.message("No characters remain to import after duplicate protection.\n\nUse Import Mode 3 to update existing imported characters.")
  return
end

local imported_cues = ReaADR.filter_cues_by_characters(annotated_cues, selected_characters)
local final_cues = apply_import_mode(existing_cues, imported_cues, script_info.script_id, selected_characters, plan.mode)

local validation = ReaADR.validate_cues(final_cues, { preroll_seconds = import_preroll_seconds })
local preview_lines = {
  ReaADR.validation_summary_text(validation):gsub("\nBuild this ADR session%?$", ""),
  "",
  "Script Import",
  "Name: " .. tostring(script_info.script_name),
  "ID: " .. tostring(script_info.script_id),
  "Mode: " .. ({ all = "Import Entire Script", selected = "Import Selected Characters", update = "Update Existing Import" })[plan.mode],
  "Characters: " .. table.concat(selected_characters, ", "),
  "",
  "Build/update this ADR session?",
}
local proceed = reaper.ShowMessageBox(table.concat(preview_lines, "\n"), "ReaADR Import Preview", 4)
if proceed ~= 6 then
  return
end

local snapshot = ReaADR.create_session_snapshot("Import Cue Sheet: " .. tostring(script_info.script_id))
ReaADR.log("INFO", "IMPORT", "Starting script import", {
  script_id = script_info.script_id,
  count = #imported_cues,
  detail = ({ all = "all", selected = "selected", update = "update" })[plan.mode],
})

overlay_settings.preroll_seconds = import_preroll_seconds
ReaADR.save_overlay_settings(overlay_settings)

local progress = ReaADR.create_progress_window("Importing ADR Cue Sheet")
local generated_cue_path, generated_cue_error = ReaADR.generate_project_cue_wav(cue_audio_path, frame_rate)
if not generated_cue_path then
  progress.close()
  ReaADR.message("Cue sheet import failed while creating project cue audio:\n\n" .. tostring(generated_cue_error))
  return
end

local preroll_status = ReaADR.configure_project_preroll(import_preroll_seconds)
local cleanup_summary = nil
local stale_cues = plan.mode == "update" and stale_update_cues(existing_script_cues, imported_cues, selected_characters) or {}
local summary, setup_error = ReaADR.setup_project(final_cues, {
  cue_audio_path = cue_audio_path,
  overlay_settings = overlay_settings,
  preroll_seconds = import_preroll_seconds,
  require_video_track = true,
  on_progress = progress.update,
})

if not summary then
  progress.update("Import failed.", 1, 1)
  progress.close()
  ReaADR.restore_session_snapshot(snapshot, "Import failed: " .. tostring(setup_error))
  ReaADR.log("ERROR", "IMPORT", "Cue sheet import failed during project setup", {
    script_id = script_info.script_id,
    detail = tostring(setup_error),
  })
  ReaADR.message("Cue sheet import failed while populating the project:\n\n" .. tostring(setup_error))
  return
end

if #stale_cues > 0 then
  cleanup_summary = ReaADR.remove_project_artifacts_for_cues(stale_cues)
  ReaADR.log("INFO", "IMPORT", "Removed stale cues after successful update", {
    script_id = script_info.script_id,
    count = #stale_cues,
  })
end

progress.close()
ReaADR.show_video_window()
ReaADR.log("INFO", "IMPORT", "Script import completed", {
  script_id = script_info.script_id,
  count = summary.cue_count,
  detail = table.concat(selected_characters, ", "),
})

ReaADR.message(
  ("Imported script %s.\n\nMode: %s\nScript ID: %s\nCharacters imported: %s\n\nProject cues: %d\nProject characters: %d\nTracks: %d created, %d reused\nRegions: %d created, %d updated\nOld cue markers removed: %d\nCue audio: %d created, %d updated, %d skipped\nOverlap splits: %d\nVideo overlay FX: %s\nPre-roll: %.1fs (%s)"):format(
    tostring(script_info.script_name),
    ({ all = "Import Entire Script", selected = "Import Selected Characters", update = "Update Existing Import" })[plan.mode],
    tostring(script_info.script_id),
    table.concat(selected_characters, ", "),
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
  .. (
    cleanup_summary and
    ("\nStale update cleanup: %d regions removed, %d cue items removed"):format(
      cleanup_summary.regions_removed or 0,
      cleanup_summary.cue_audio_removed or 0
    ) or
    ""
  )
)
