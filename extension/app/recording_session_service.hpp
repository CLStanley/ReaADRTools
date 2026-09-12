#pragma once

#include "recording_target_application_service.hpp"
#include "recording_workflow_service.hpp"
#include "../reaadr_core/recording_preferences.hpp"

namespace reaadr::reaper {

struct RecordingSessionOptions {
  core::CueStatusCommitOptions status_commit;
  core::EventPublishOptions event;
  std::string undo_description = "ReaADR: record cue";
};

struct RecordingSessionStartResult {
  RecordingTargetApplicationResult target;
  core::RecordingPreferenceLoadResult preferences;
  RecordingWorkflowStartResult workflow;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Reproduces the startup contract of ReaADR_Record_Cue.lua without owning UI:
// resolve active cue -> load loop preference -> prepare owned recording track ->
// initialize the deterministic transport/application workflow.
class RecordingSessionService final {
public:
  RecordingSessionService(RecordingTargetApplicationService& target,
                          core::RecordingPreferenceRepository& preferences,
                          RecordingWorkflowService& workflow)
    : target_(target), preferences_(preferences), workflow_(workflow) {}

  RecordingSessionStartResult begin(double timeline_position,
                                    const RecordingSessionOptions& options);

  RecordingWorkflowDispatchResult dispatch(core::RecordingTransportEvent event,
                                            int play_state,
                                            double play_position)
  {
    return workflow_.dispatch(event, play_state, play_position);
  }

  RecordingWorkflowDispatchResult retry_pending() { return workflow_.retry_pending(); }
  RecordingWorkflowDispatchResult shutdown(int play_state, double play_position)
  {
    return workflow_.shutdown(play_state, play_position);
  }

  bool active() const { return workflow_.active(); }
  const core::RecordingTransportState& state() const { return workflow_.state(); }
  const core::RecordingSetupPlan& plan() const { return workflow_.plan(); }

private:
  RecordingTargetApplicationService& target_;
  core::RecordingPreferenceRepository& preferences_;
  RecordingWorkflowService& workflow_;
};

} // namespace reaadr::reaper
