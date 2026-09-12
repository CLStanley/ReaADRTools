#include "recording_target_application_service.hpp"

namespace reaadr::reaper {

RecordingTargetApplicationResult RecordingTargetApplicationService::resolve(
  double timeline_position) const
{
  RecordingTargetApplicationResult result;

  const core::SessionLoadResult session = sessions_.load();
  if (!session) {
    result.error = core::session_load_error_message(session);
    return result;
  }
  const core::CueSelectionLoadResult selection = selections_.load();
  if (!selection) {
    result.error = selection.error;
    return result;
  }
  const core::CharacterFilterLoadResult filter = filters_.load();
  if (!filter) {
    result.error = filter.error;
    return result;
  }
  const core::OverlaySettingsLoadResult overlay = overlay_settings_.load();
  if (!overlay) {
    result.error = overlay.error;
    return result;
  }

  result.preroll_seconds = overlay.settings.preroll_seconds;
  core::RecordingTargetOptions options;
  options.selected_cue_key = selection.state.manager_selected_cue_key;
  options.timeline_position = timeline_position;
  options.preroll_seconds = result.preroll_seconds;
  result.target = core::resolve_recording_target(session.model, filter.state, options);
  if (!result.target) result.error = result.target.error;
  return result;
}

} // namespace reaadr::reaper
