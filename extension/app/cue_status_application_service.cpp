#include "cue_status_application_service.hpp"

namespace reaadr::reaper {
namespace {

std::string field(const core::Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

} // namespace

CueStatusApplicationResult CueStatusApplicationService::apply(
  const std::string& cue_key,
  const std::string& status,
  const CueStatusApplicationOptions& options)
{
  CueStatusApplicationResult result;
  core::CueStatusCommitOptions commit_options = options.commit;
  commit_options.update.cue_key = cue_key;
  commit_options.update.status = status;
  if (commit_options.update.last_operation.empty())
    commit_options.update.last_operation = "set_cue_status";

  bool restore_model = false;
  {
    ProjectTransaction transaction(
      project_, transaction_api_, options.undo_description, -1, true);
    result.status = core::commit_cue_status(repository_, commit_options);
    if (!result.status) {
      result.error = result.status.error;
      transaction.mark_failed();
    } else if (options.refresh_overlay) {
      std::string overlay_error;
      if (!options.refresh_overlay(&overlay_error)) {
        result.error = overlay_error.empty()
          ? "REAPER could not refresh the overlay after updating cue status."
          : overlay_error;
        restore_model = result.status.update.changed;
        transaction.mark_failed();
      }
    }
  }

  if (restore_model) {
    const core::RevisionResult restored =
      repository_.restore_snapshot(result.status.snapshot);
    result.model_rolled_back = static_cast<bool>(restored);
    if (!restored) {
      if (!result.error.empty()) result.error += " ";
      result.error += "Cue status rollback also failed: " + restored.error;
    }
  }
  if (!result.error.empty()) return result;

  if (result.status.update.changed) {
    core::EventPublishOptions event_options = options.event;
    if (event_options.utc_timestamp.empty())
      event_options.utc_timestamp = commit_options.utc_timestamp;
    event_options.session_id = result.status.update.model.session_id();
    if (event_options.source.empty()) event_options.source = "native_set_cue_status";
    const core::Fields payload = {
      {"cue_id", field(result.status.update.cue, "id")},
      {"cue_key", cue_key},
      {"revision", std::to_string(result.status.revision)},
      {"status", result.status.update.normalized_status},
    };
    result.event = event_log_.publish("CueUpdated", payload, event_options);
    if (!result.event)
      result.event_warning = "CueUpdated event publication failed: " + result.event.error;
  }
  return result;
}

} // namespace reaadr::reaper
