#include "recording_session_service.hpp"

namespace reaadr::reaper {

RecordingSessionStartResult RecordingSessionService::begin(
  double timeline_position,
  const RecordingSessionOptions& options)
{
  RecordingSessionStartResult result;
  if (workflow_.active()) {
    result.error = "A recording session is already active.";
    return result;
  }

  result.target = target_.resolve(timeline_position);
  if (!result.target) {
    result.error = result.target.error;
    return result;
  }

  result.preferences = preferences_.load();
  if (!result.preferences) {
    result.error = result.preferences.error;
    return result;
  }

  RecordingWorkflowOptions workflow_options;
  workflow_options.setup.cue_key = result.target.target.cue.cue_key;
  workflow_options.setup.preroll_seconds = result.target.preroll_seconds;
  workflow_options.initial_state.include_preroll_each_loop =
    result.preferences.include_preroll_each_loop;
  workflow_options.application.cue_key = result.target.target.cue.cue_key;
  workflow_options.application.include_preroll_each_loop =
    result.preferences.include_preroll_each_loop;
  workflow_options.application.status_commit = options.status_commit;
  workflow_options.application.event = options.event;
  workflow_options.application.undo_description = options.undo_description;

  result.workflow = workflow_.start(workflow_options);
  if (!result.workflow) result.error = result.workflow.error;
  return result;
}

} // namespace reaadr::reaper
