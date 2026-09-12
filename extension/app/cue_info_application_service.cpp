#include "cue_info_application_service.hpp"

namespace reaadr::reaper {

CueInfoApplicationResult CueInfoApplicationService::load(
  double timeline_position,
  double frame_rate) const
{
  CueInfoApplicationResult result;
  const RecordingTargetApplicationResult target = targets_.resolve(timeline_position);
  if (!target) {
    result.error = target.error;
    return result;
  }
  result.target = target.target;

  const CueTakeCountResult takes = count_recorded_takes_for_cue(
    project_, take_count_api_, result.target.cue.cue);
  if (!takes) {
    result.error = takes.error;
    return result;
  }

  result.view = core::build_cue_info_view(
    result.target.cue.cue,
    {timeline_position, frame_rate, takes.take_count});
  if (!result.view) result.error = result.view.error;
  return result;
}

} // namespace reaadr::reaper
