#include "recording_application_service.hpp"

#include "cue_status_application_service.hpp"
#include "reaadr_core/render_plan.hpp"

namespace reaadr::reaper {

bool RecordingApplicationService::validate_cue_key(
  const std::string& cue_key,
  std::string& error) const
{
  if (cue_key.empty()) {
    error = "A recording cue key is required before applying recording state.";
    return false;
  }
  const core::SessionLoadResult loaded = model_repository_.load();
  if (!loaded) {
    error = core::session_load_error_message(loaded);
    return false;
  }
  int matches = 0;
  for (const core::Fields& cue : loaded.model.cues) {
    if (core::render_cue_key(cue) == cue_key) ++matches;
  }
  if (matches == 0) {
    error = "The recording cue is no longer present in the canonical session.";
    return false;
  }
  if (matches > 1) {
    error = "Multiple canonical cues match the recording cue key: " + cue_key;
    return false;
  }
  return true;
}

bool RecordingApplicationService::refresh_selection(
  const RecordingApplicationOptions& options,
  RecordingApplicationResult& result)
{
  if (!validate_cue_key(options.cue_key, result.error)) return false;
  const core::CueSelectionLoadResult previous = selection_repository_.load();
  if (!previous) {
    result.error = previous.error;
    return false;
  }

  bool restore_selection = false;
  {
    ProjectTransaction transaction(
      project_, transaction_api_, options.undo_description, -1, true);
    result.selection = selection_repository_.save_selected_cue(options.cue_key);
    if (!result.selection) {
      result.error = result.selection.error;
      restore_selection = true;
      transaction.mark_failed();
    } else if (!api_.refresh_overlay || !api_.refresh_overlay()) {
      result.error = "REAPER could not refresh the recording cue overlay.";
      restore_selection = result.selection.changed;
      transaction.mark_failed();
    } else {
      ++result.overlay_refreshes;
    }
  }
  if (restore_selection) {
    const core::CueSelectionSaveResult restored =
      selection_repository_.save_state(previous.state);
    result.selection_rolled_back = static_cast<bool>(restored);
    if (!restored) result.error += " Cue selection rollback also failed: " + restored.error;
  }
  return result.error.empty();
}

bool RecordingApplicationService::finalize_takes(
  const RecordingApplicationOptions& options,
  RecordingApplicationResult& result)
{
  CueStatusApplicationService status_service(
    model_repository_, event_log_, project_, transaction_api_);
  CueStatusApplicationOptions status_options;
  status_options.commit = options.status_commit;
  status_options.commit.update.last_operation = "record_cue";
  status_options.event = options.event;
  if (status_options.event.source.empty())
    status_options.event.source = "native_recording";
  status_options.undo_description = options.undo_description;
  status_options.refresh_overlay = [this, &result](std::string* error) {
    if (api_.refresh_overlay && api_.refresh_overlay()) {
      ++result.overlay_refreshes;
      return true;
    }
    if (error) *error = "REAPER could not refresh the overlay after recording.";
    return false;
  };

  const CueStatusApplicationResult applied =
    status_service.apply(options.cue_key, "Recorded", status_options);
  result.status = applied.status;
  result.event = applied.event;
  result.model_rolled_back = applied.model_rolled_back;
  result.event_warning = applied.event_warning;
  result.error = applied.error;
  return static_cast<bool>(applied);
}

RecordingApplicationResult RecordingApplicationService::apply(
  const PendingRecordingApplicationActions& pending,
  const RecordingApplicationOptions& options)
{
  RecordingApplicationResult result;
  result.remaining = pending;

  if (pending.refresh_active_cue) {
    if (!refresh_selection(options, result)) return result;
    result.remaining.refresh_active_cue = false;
  }
  if (pending.finalize_recorded_takes) {
    if (!finalize_takes(options, result)) return result;
    result.remaining.finalize_recorded_takes = false;
  }
  if (pending.persist_preroll_preference) {
    result.preference = preference_repository_.save_include_preroll_each_loop(
      options.include_preroll_each_loop);
    if (!result.preference) {
      result.error = result.preference.error;
      return result;
    }
    result.remaining.persist_preroll_preference = false;
  }
  return result;
}

} // namespace reaadr::reaper
