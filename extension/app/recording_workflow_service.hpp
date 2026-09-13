#pragma once

#include "recording_application_service.hpp"
#include "../reaadr_reaper/recording_setup_adapter.hpp"
#include "../reaadr_reaper/recording_transport_executor.hpp"

#include <string>

struct MediaTrack;

namespace reaadr::reaper {

struct RecordingWorkflowOptions {
  core::RecordingSetupOptions setup;
  core::RecordingTransportState initial_state;
  RecordingApplicationOptions application;
};

struct RecordingWorkflowStartResult {
  core::RecordingSetupPlan plan;
  core::RecordingTransportContext context;
  core::RecordingTransportState state;
  MediaTrack* target_track = nullptr;
  std::string target_track_name;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct RecordingWorkflowDispatchResult {
  core::RecordingTransportState state;
  PendingRecordingApplicationActions pending;
  RecordingApplicationResult application;
  bool state_changed = false;
  bool workflow_closed = false;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Coordinates the already-native recording layers. UI code supplies semantic
// events and current transport observations; this service owns transition ->
// host execution -> model/application ordering and preserves retryable pending
// application work without duplicating transport behavior in a window loop.
class RecordingWorkflowService final {
public:
  RecordingWorkflowService(RecordingSetupService& setup,
                           RecordingTransportExecutor& transport,
                           RecordingApplicationService& application)
    : setup_(setup), transport_(transport), application_(application) {}

  RecordingWorkflowStartResult start(const RecordingWorkflowOptions& options);

  RecordingWorkflowDispatchResult dispatch(
    core::RecordingTransportEvent event,
    int play_state,
    double play_position);

  RecordingWorkflowDispatchResult retry_pending();

  // Used by every native window exit route. Abort first restores transport,
  // loop range, and record-arm state; the workflow is released only after any
  // resulting canonical status work also succeeds.
  RecordingWorkflowDispatchResult shutdown(int play_state, double play_position);

  bool active() const { return active_; }
  const core::RecordingTransportState& state() const { return state_; }
  const core::RecordingTransportContext& context() const { return context_; }
  const core::RecordingSetupPlan& plan() const { return plan_; }
  const PendingRecordingApplicationActions& pending() const { return pending_; }

private:
  static bool has_pending(const PendingRecordingApplicationActions& pending);
  RecordingWorkflowDispatchResult apply_pending();
  void release();

  RecordingSetupService& setup_;
  RecordingTransportExecutor& transport_;
  RecordingApplicationService& application_;
  RecordingApplicationOptions application_options_;
  core::RecordingSetupPlan plan_;
  core::RecordingTransportContext context_;
  core::RecordingTransportState state_;
  MediaTrack* target_track_ = nullptr;
  PendingRecordingApplicationActions pending_;
  bool active_ = false;
};

} // namespace reaadr::reaper
