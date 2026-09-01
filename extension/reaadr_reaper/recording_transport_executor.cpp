#include "recording_transport_executor.hpp"

#include <cmath>

namespace reaadr::reaper {
namespace {

constexpr int kPlayCommand = 1007;
constexpr int kRecordCommand = 1013;
constexpr int kStopCommand = 1016;

void append_cleanup_error(std::string& error, const std::string& cleanup_error)
{
  if (cleanup_error.empty()) return;
  if (!error.empty()) error += " ";
  error += cleanup_error;
}

} // namespace

bool RecordingTransportExecutor::configure_loop_range(
  const core::RecordingTransportContext& context,
  std::string& error)
{
  if (!api_.get_loop_time_range || !api_.set_loop_time_range) {
    error = "The REAPER loop-range API is incomplete.";
    return false;
  }
  if (!loop_range_saved_) {
    double start = 0.0;
    double end = 0.0;
    if (!api_.get_loop_time_range(&start, &end) || !std::isfinite(start) ||
        !std::isfinite(end)) {
      error = "REAPER could not capture the existing loop range.";
      return false;
    }
    saved_loop_start_ = start;
    saved_loop_end_ = end;
    loop_range_saved_ = true;
  }
  if (!api_.set_loop_time_range(context.record_start, context.cue_end)) {
    error = "REAPER could not configure the ADR recording loop range.";
    return false;
  }
  loop_range_active_ = true;
  return true;
}

bool RecordingTransportExecutor::restore_loop_range(std::string& error)
{
  if (!loop_range_active_) return true;
  if (!loop_range_saved_ || !api_.set_loop_time_range) {
    error = "The saved REAPER loop range is unavailable.";
    return false;
  }
  if (!api_.set_loop_time_range(saved_loop_start_, saved_loop_end_)) {
    error = "REAPER could not restore the prior loop range.";
    return false;
  }
  loop_range_active_ = false;
  return true;
}

void RecordingTransportExecutor::compensate_start_failure(
  bool restore_arm,
  bool restore_loop,
  std::string& error)
{
  if (restore_arm) {
    const RecordArmApplyResult restored = record_arm_.restore();
    if (!restored) append_cleanup_error(error, restored.error);
  }
  if (restore_loop) {
    std::string loop_error;
    if (!restore_loop_range(loop_error)) append_cleanup_error(error, loop_error);
  }
}

RecordingTransportExecutionResult RecordingTransportExecutor::apply(
  const core::RecordingTransportTransition& transition,
  const core::RecordingTransportContext& context,
  MediaTrack* target_track)
{
  RecordingTransportExecutionResult result;
  if (!transition) {
    result.error = transition.error.empty()
      ? "The recording transport transition is invalid."
      : transition.error;
    return result;
  }

  const core::RecordingTransportActions& actions = transition.actions;
  if (actions.stop && (!api_.run_command || !api_.run_command(kStopCommand))) {
    result.error = "REAPER could not stop the recording transport.";
    return result;
  }
  if (actions.restore_loop_range && !restore_loop_range(result.error)) return result;

  bool configured_loop = false;
  if (actions.configure_loop_range) {
    if (!configure_loop_range(context, result.error)) return result;
    configured_loop = true;
  }
  if (actions.move_cursor &&
      (!api_.set_edit_cursor_position ||
       !api_.set_edit_cursor_position(actions.cursor_position, true, false))) {
    result.error = "REAPER could not position the recording cursor.";
    compensate_start_failure(false, configured_loop, result.error);
    return result;
  }

  bool isolated_arm = false;
  if (actions.isolate_recording_track) {
    const RecordArmApplyResult isolated = record_arm_.capture_and_isolate(target_track);
    if (!isolated) {
      result.error = isolated.error;
      compensate_start_failure(record_arm_.has_snapshot(), configured_loop, result.error);
      return result;
    }
    isolated_arm = true;
  }

  if (actions.play && (!api_.run_command || !api_.run_command(kPlayCommand))) {
    result.error = "REAPER could not start preroll playback.";
    compensate_start_failure(isolated_arm, configured_loop, result.error);
    return result;
  }
  if (actions.record && (!api_.run_command || !api_.run_command(kRecordCommand))) {
    result.error = "REAPER could not start recording.";
    compensate_start_failure(isolated_arm, configured_loop, result.error);
    return result;
  }
  if (actions.restore_record_arm) {
    const RecordArmApplyResult restored = record_arm_.restore();
    if (!restored) {
      result.error = restored.error;
      return result;
    }
  }

  result.pending.refresh_active_cue = actions.refresh_active_cue;
  result.pending.finalize_recorded_takes = actions.finalize_recorded_takes;
  result.pending.persist_preroll_preference = actions.persist_preroll_preference;
  result.state_accepted = true;
  return result;
}

} // namespace reaadr::reaper
