#include "recording_setup_adapter.hpp"

#include <array>

namespace reaadr::reaper {
namespace {

bool api_complete(const RecordingSetupApi& api)
{
  return api.count_tracks && api.get_track && api.validate_track &&
    api.get_set_track_string;
}

std::string track_string(const RecordingSetupApi& api,
                         MediaTrack* track,
                         const char* parameter)
{
  std::array<char, 4096> buffer = {};
  if (!api.get_set_track_string(track, parameter, buffer.data(), false)) return {};
  return buffer.data();
}

} // namespace

PreparedRecordingSetup RecordingSetupService::prepare(
  const core::RecordingSetupOptions& options) const
{
  PreparedRecordingSetup result;
  if (!api_complete(api_)) {
    result.error = "The REAPER recording-setup API is incomplete.";
    return result;
  }
  const core::SessionLoadResult loaded = repository_.load();
  if (!loaded) {
    result.error = core::session_load_error_message(loaded);
    return result;
  }

  const int track_count = api_.count_tracks(project_);
  if (track_count < 0) {
    result.error = "REAPER returned an invalid track count while preparing recording.";
    return result;
  }
  std::vector<core::ExistingTrack> tracks;
  tracks.reserve(static_cast<std::size_t>(track_count));
  for (int index = 0; index < track_count; ++index) {
    MediaTrack* track = api_.get_track(project_, index);
    if (!track || !api_.validate_track(project_, track)) {
      result.error = "REAPER could not resolve a track while preparing recording.";
      return result;
    }
    tracks.push_back({
      static_cast<std::size_t>(index),
      track_string(api_, track, "P_EXT:ReaADR.role"),
      track_string(api_, track, "P_EXT:ReaADR.key"),
      track_string(api_, track, "P_NAME"),
      {},
    });
  }

  const core::RecordingSetupPlanResult planned =
    core::build_recording_setup_plan(loaded.model, tracks, options);
  if (!planned) {
    result.error = planned.error;
    return result;
  }
  result.plan = planned.plan;
  result.target_track = api_.get_track(
    project_, static_cast<int>(result.plan.track_project_index));
  if (!result.target_track || !api_.validate_track(project_, result.target_track) ||
      track_string(api_, result.target_track, "P_EXT:ReaADR.role") !=
        result.plan.expected_track_role ||
      track_string(api_, result.target_track, "P_EXT:ReaADR.key") !=
        result.plan.expected_track_key) {
    result.target_track = nullptr;
    result.error = "The selected ADR recording track changed while recording was prepared.";
  }
  return result;
}

} // namespace reaadr::reaper
