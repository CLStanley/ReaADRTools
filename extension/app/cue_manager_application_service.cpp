#include "cue_manager_application_service.hpp"

namespace reaadr::reaper {

bool CueManagerApplicationService::synchronize(
  const std::vector<core::Fields>& cues,
  const char* last_operation,
  const char* snapshot_label,
  const char* undo_description,
  const char* event_type,
  const std::string& selected_cue_key,
  CueManagerApplicationResult& result)
{
  const core::OverlaySettingsLoadResult overlay = overlay_settings_.load();
  if (!overlay) {
    result.error = overlay.error;
    return false;
  }

  SessionRenderOptions render = render_options_;
  render.commit.replacement.last_operation = last_operation;
  render.commit.replacement.build.cues_modified = true;
  render.commit.replacement.build.tracks_modified = true;
  render.commit.replacement.build.regions_modified = true;
  render.commit.replacement.build.preroll_seconds = overlay.settings.preroll_seconds;
  render.commit.snapshot_label = snapshot_label;
  render.undo_description = undo_description;
  render.commit_event_type = event_type;
  if (api_.utc_timestamp) {
    render.commit.utc_timestamp = api_.utc_timestamp();
    render.event.utc_timestamp = render.commit.utc_timestamp;
  }
  if (render.event.source.empty()) render.event.source = "native_cue_manager";

  const core::CueSelectionLoadResult previous_selection = selections_.load();
  if (!previous_selection) {
    result.error = previous_selection.error;
    return false;
  }
  const auto refresh_overlay = render.refresh_overlay;
  bool selection_was_written = false;
  // Selection is written inside the same outer render transaction and before
  // overlay generation, so a renamed/newly selected cue is the overlay source.
  render.refresh_overlay = [this, selected_cue_key, refresh_overlay,
                            &selection_was_written](std::string* error) {
    const core::CueSelectionSaveResult saved = selections_.save_selected_cue(selected_cue_key);
    if (!saved) {
      if (error) *error = saved.error;
      return false;
    }
    selection_was_written = saved.changed;
    return !refresh_overlay || refresh_overlay(error);
  };

  result.synchronization = renderer_.commit_and_render(cues, render);
  if (!result.synchronization) {
    result.error = result.synchronization.error;
    if (selection_was_written) {
      const core::CueSelectionSaveResult restored = selections_.save_state(previous_selection.state);
      if (!restored) result.error += " " + restored.error;
    }
    return false;
  }
  result.revision = result.synchronization.commit.revision;
  return true;
}

CueManagerApplicationResult CueManagerApplicationService::edit(
  const core::CueManagerEditOptions& options)
{
  CueManagerApplicationResult result;
  const core::SessionLoadResult loaded = sessions_.load();
  if (!loaded) {
    result.error = core::session_load_error_message(loaded);
    return result;
  }

  // Validate before generating media or opening an Undo block. The renderer
  // will rebuild cue-derived model collections from this edited cue set.
  result.edit = core::edit_cue_manager_row(loaded.model, options);
  if (!result.edit) {
    result.error = result.edit.error;
    return result;
  }
  if (!result.edit.changed) {
    const core::RevisionResult revision = sessions_.revision();
    if (!revision) result.error = revision.error;
    else result.revision = revision.revision;
    return result;
  }

  synchronize(result.edit.model.cues, "edit_cue", "Edit Cue",
              "ReaADR: edit cue", "CueUpdated",
              options.new_cue_key.empty() ? options.cue_key : options.new_cue_key, result);
  return result;
}

CueManagerApplicationResult CueManagerApplicationService::add(
  const core::CueManagerAddOptions& options)
{
  CueManagerApplicationResult result;
  const core::SessionLoadResult loaded = sessions_.load();
  if (!loaded) {
    result.error = core::session_load_error_message(loaded);
    return result;
  }
  result.mutation = core::add_cue_manager_row(loaded.model, options);
  if (!result.mutation) {
    result.error = result.mutation.error;
    return result;
  }
  synchronize(result.mutation.model.cues, "add_cached_cue", "Add Cue From Cue Manager",
              "ReaADR: add cue", "CueCreated", result.mutation.selected_cue_key, result);
  return result;
}

CueManagerApplicationResult CueManagerApplicationService::remove(const std::string& cue_key)
{
  CueManagerApplicationResult result;
  const core::SessionLoadResult loaded = sessions_.load();
  if (!loaded) {
    result.error = core::session_load_error_message(loaded);
    return result;
  }
  result.mutation = core::remove_cue_manager_row(loaded.model, cue_key, true);
  if (!result.mutation) {
    result.error = result.mutation.error;
    return result;
  }
  synchronize(result.mutation.model.cues, "remove_cached_cue", "Remove Cue From Cue Manager",
              "ReaADR: remove cue", "CueDeleted", result.mutation.selected_cue_key, result);
  return result;
}

} // namespace reaadr::reaper
