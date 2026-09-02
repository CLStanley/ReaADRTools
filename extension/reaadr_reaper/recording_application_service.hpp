#pragma once

#include "reaadr_core/cue_navigation.hpp"
#include "reaadr_core/cue_status.hpp"
#include "reaadr_core/event_log.hpp"
#include "reaadr_core/recording_preferences.hpp"
#include "project_transaction.hpp"
#include "recording_transport_executor.hpp"

#include <string>

struct ReaProject;

namespace reaadr::reaper {

struct RecordingApplicationApi {
  // Rebuilds the current generated overlay from canonical model and selection
  // state. The adapter owns exact FX identification and mutation safety.
  bool (*refresh_overlay)() = nullptr;
};

struct RecordingApplicationOptions {
  std::string cue_key;
  bool include_preroll_each_loop = true;
  core::CueStatusCommitOptions status_commit;
  core::EventPublishOptions event;
  std::string undo_description = "ReaADR: update recording state";
};

struct RecordingApplicationResult {
  PendingRecordingApplicationActions remaining;
  core::CueSelectionSaveResult selection;
  core::CueStatusCommitResult status;
  core::RecordingPreferenceSaveResult preference;
  core::EventPublishResult event;
  int overlay_refreshes = 0;
  bool selection_rolled_back = false;
  bool model_rolled_back = false;
  std::string event_warning;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Consumes application actions only after transport execution succeeds. Failed
// work remains explicit and retryable; successful idempotent retries do not
// create extra model revisions or CueUpdated events.
class RecordingApplicationService {
public:
  RecordingApplicationService(core::SessionModelRepository& model_repository,
                              core::CueSelectionRepository& selection_repository,
                              core::RecordingPreferenceRepository& preference_repository,
                              core::EventLogRepository& event_log,
                              ReaProject* project,
                              TransactionApi transaction_api,
                              RecordingApplicationApi api)
    : model_repository_(model_repository),
      selection_repository_(selection_repository),
      preference_repository_(preference_repository),
      event_log_(event_log),
      project_(project),
      transaction_api_(transaction_api),
      api_(api)
  {
  }

  RecordingApplicationResult apply(
    const PendingRecordingApplicationActions& pending,
    const RecordingApplicationOptions& options);

private:
  bool validate_cue_key(const std::string& cue_key, std::string& error) const;
  bool refresh_selection(const RecordingApplicationOptions& options,
                         RecordingApplicationResult& result);
  bool finalize_takes(const RecordingApplicationOptions& options,
                      RecordingApplicationResult& result);

  core::SessionModelRepository& model_repository_;
  core::CueSelectionRepository& selection_repository_;
  core::RecordingPreferenceRepository& preference_repository_;
  core::EventLogRepository& event_log_;
  ReaProject* project_ = nullptr;
  TransactionApi transaction_api_;
  RecordingApplicationApi api_;
};

} // namespace reaadr::reaper
