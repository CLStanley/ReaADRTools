#include "recording_workflow_service.hpp"

namespace reaadr::reaper {

bool RecordingWorkflowService::has_pending(
  const PendingRecordingApplicationActions& pending)
{
  return pending.refresh_active_cue || pending.finalize_recorded_takes ||
    pending.persist_preroll_preference;
}

RecordingWorkflowStartResult RecordingWorkflowService::start(
  const RecordingWorkflowOptions& options)
{
  RecordingWorkflowStartResult result;
  if (active_) {
    result.error = "A recording workflow is already active.";
    return result;
  }

  const PreparedRecordingSetup prepared = setup_.prepare(options.setup);
  if (!prepared) {
    result.error = prepared.error;
    return result;
  }

  plan_ = prepared.plan;
  target_track_ = prepared.target_track;
  context_.record_start = plan_.record_start;
  context_.cue_start = plan_.cue_start;
  context_.cue_end = plan_.cue_end;
  state_ = options.initial_state;
  application_options_ = options.application;
  application_options_.cue_key = plan_.cue_key;
  pending_ = {};
  active_ = true;

  result.plan = plan_;
  result.context = context_;
  result.state = state_;
  result.target_track = target_track_;
  return result;
}

RecordingWorkflowDispatchResult RecordingWorkflowService::apply_pending()
{
  RecordingWorkflowDispatchResult result;
  result.state = state_;
  result.pending = pending_;
  if (!has_pending(pending_)) return result;

  result.application = application_.apply(pending_, application_options_);
  pending_ = result.application.remaining;
  result.pending = pending_;
  if (!result.application) result.error = result.application.error;
  return result;
}

RecordingWorkflowDispatchResult RecordingWorkflowService::dispatch(
  core::RecordingTransportEvent event,
  int play_state,
  double play_position)
{
  RecordingWorkflowDispatchResult result;
  result.state = state_;
  result.pending = pending_;
  if (!active_) {
    result.error = "No recording workflow is active.";
    return result;
  }
  if (has_pending(pending_)) {
    result.error = "Recording application work is still pending; retry it before advancing transport state.";
    return result;
  }

  core::RecordingTransportInput input;
  input.event = event;
  input.play_state = play_state;
  input.play_position = play_position;
  const core::RecordingTransportTransition transition =
    core::advance_recording_transport(state_, context_, input);
  if (!transition) {
    result.error = transition.error;
    return result;
  }

  const RecordingTransportExecutionResult executed =
    transport_.apply(transition, context_, target_track_);
  if (!executed) {
    result.error = executed.error;
    return result;
  }
  if (!executed.state_accepted) {
    result.error = "REAPER did not accept the recording transport transition.";
    return result;
  }

  state_ = transition.state;
  pending_ = executed.pending;
  result.state = state_;
  result.pending = pending_;
  result.state_changed = true;

  if (has_pending(pending_)) {
    RecordingWorkflowDispatchResult applied = apply_pending();
    applied.state_changed = true;
    return applied;
  }
  return result;
}

RecordingWorkflowDispatchResult RecordingWorkflowService::retry_pending()
{
  RecordingWorkflowDispatchResult result;
  result.state = state_;
  result.pending = pending_;
  if (!active_) {
    result.error = "No recording workflow is active.";
    return result;
  }
  if (!has_pending(pending_)) return result;
  return apply_pending();
}

} // namespace reaadr::reaper
