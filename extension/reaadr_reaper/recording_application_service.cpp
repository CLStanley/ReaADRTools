#include "recording_application_service.hpp"

#include "reaadr_core/render_plan.hpp"

namespace reaadr::reaper {
namespace {

std::string field(const core::Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

} // namespace

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
  core::CueStatusCommitOptions commit_options = options.status_commit;
  commit_options.update.cue_key = options.cue_key;
  commit_options.update.status = "Recorded";
  commit_options.update.last_operation = "record_cue";

  bool restore_model = false;
  {
    ProjectTransaction transaction(
      project_, transaction_api_, options.undo_description, -1, true);
    result.status = core::commit_cue_status(model_repository_, commit_options);
    if (!result.status) {
      result.error = result.status.error;
      transaction.mark_failed();
    } else if (!api_.refresh_overlay || !api_.refresh_overlay()) {
      result.error = "REAPER could not refresh the overlay after recording.";
      restore_model = result.status.update.changed;
      transaction.mark_failed();
    } else {
      ++result.overlay_refreshes;
    }
  }

  if (restore_model) {
    const core::RevisionResult restored =
      model_repository_.restore_snapshot(result.status.snapshot);
    result.model_rolled_back = static_cast<bool>(restored);
    if (!restored) result.error += " Cue status rollback also failed: " + restored.error;
  }
  if (!result.error.empty()) return false;

  if (result.status.update.changed) {
    core::EventPublishOptions event_options = options.event;
    if (event_options.utc_timestamp.empty()) {
      event_options.utc_timestamp = commit_options.utc_timestamp;
    }
    event_options.session_id = result.status.update.model.session_id();
    if (event_options.source.empty()) event_options.source = "native_recording";
    const core::Fields payload = {
      {"cue_id", field(result.status.update.cue, "id")},
      {"cue_key", options.cue_key},
      {"revision", std::to_string(result.status.revision)},
      {"status", result.status.update.normalized_status},
    };
    result.event = event_log_.publish("CueUpdated", payload, event_options);
    if (!result.event) {
      result.event_warning =
        "CueUpdated event publication failed: " + result.event.error;
    }
  }
  return true;
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
