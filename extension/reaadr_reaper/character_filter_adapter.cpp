#include "character_filter_adapter.hpp"

#include <array>

namespace reaadr::reaper {
namespace {

std::string track_string(const TrackRegionApi& api, MediaTrack* track, const char* parameter)
{
  std::array<char, 4096> buffer = {};
  if (!api.get_set_track_string(track, parameter, buffer.data(), false)) return {};
  return buffer.data();
}

bool inspection_api_complete(const TrackRegionApi& track_api, const RulerLaneApi& region_api)
{
  return track_api.count_tracks && track_api.get_track && track_api.get_set_track_string &&
    track_api.get_track_value && region_api.count_project_markers &&
    region_api.enum_project_markers && region_api.get_region_or_marker &&
    region_api.get_region_or_marker_value;
}

bool apply_api_complete(const TrackRegionApi& track_api, const RulerLaneApi& region_api)
{
  return track_api.count_tracks && track_api.get_track && track_api.get_set_track_string &&
    track_api.set_track_value && region_api.count_project_markers &&
    region_api.enum_project_markers && region_api.get_region_or_marker &&
    region_api.get_region_or_marker_value && region_api.set_region_or_marker_value;
}

} // namespace

CharacterFilterInspectionResult inspect_character_filter_project(
  ReaProject* project,
  TrackRegionApi track_api,
  RulerLaneApi region_api)
{
  CharacterFilterInspectionResult result;
  if (!inspection_api_complete(track_api, region_api)) {
    result.error = "The REAPER character-filter inspection API is incomplete.";
    return result;
  }

  const int track_count = track_api.count_tracks(project);
  if (track_count < 0) {
    result.error = "REAPER returned an invalid track count while inspecting the character filter.";
    return result;
  }
  result.state.tracks.reserve(static_cast<std::size_t>(track_count));
  for (int index = 0; index < track_count; ++index) {
    MediaTrack* track = track_api.get_track(project, index);
    if (!track) {
      result.error = "REAPER could not resolve a track while inspecting the character filter.";
      return result;
    }
    result.state.tracks.push_back({
      static_cast<std::size_t>(index),
      track_string(track_api, track, "P_EXT:ReaADR.role"),
      track_string(track_api, track, "P_EXT:ReaADR.key"),
      track_api.get_track_value(track, "B_MUTE") != 0.0,
    });
  }

  int marker_count = 0;
  int region_count = 0;
  const int total = region_api.count_project_markers(project, &marker_count, &region_count);
  if (total < 0 || marker_count < 0 || region_count < 0) {
    result.error = "REAPER returned an invalid region count while inspecting the character filter.";
    return result;
  }
  result.state.regions.reserve(static_cast<std::size_t>(region_count));
  for (int index = 0; index < total; ++index) {
    bool is_region = false;
    double start_time = 0.0;
    double end_time = 0.0;
    const char* name = nullptr;
    int id = -1;
    int color = 0;
    if (region_api.enum_project_markers(project, index, &is_region, &start_time, &end_time,
                                        &name, &id, &color) == 0) {
      result.error = "REAPER could not enumerate regions while inspecting the character filter.";
      return result;
    }
    if (!is_region) continue;
    ProjectMarker* marker = region_api.get_region_or_marker(project, index, "");
    if (!marker) {
      result.error = "REAPER could not resolve a region while inspecting the character filter.";
      return result;
    }
    result.state.regions.push_back({
      id,
      name ? name : "",
      region_api.get_region_or_marker_value(project, marker, "B_HIDDEN") != 0.0,
    });
  }
  return result;
}

CharacterFilterApplyResult apply_character_filter_plan_transactionally(
  ReaProject* project,
  TrackRegionApi track_api,
  RulerLaneApi region_api,
  TransactionApi transaction_api,
  const core::CharacterFilterPlan& plan,
  const std::string& description)
{
  CharacterFilterApplyResult result;
  if (plan.empty()) return result;
  if (!apply_api_complete(track_api, region_api)) {
    result.error = "The REAPER character-filter mutation API is incomplete.";
    return result;
  }
  ProjectTransaction transaction(project, transaction_api, description);
  UiRefreshScope refresh(transaction_api.prevent_ui_refresh);

  for (const core::TrackMuteMutation& mutation : plan.track_mutations) {
    MediaTrack* track = track_api.get_track(project, static_cast<int>(mutation.project_index));
    if (!track ||
        track_string(track_api, track, "P_EXT:ReaADR.role") != mutation.expected_role ||
        track_string(track_api, track, "P_EXT:ReaADR.key") != mutation.expected_key) {
      result.error = "The character-filter track plan is stale for " + mutation.expected_key + ".";
      transaction.mark_failed();
      return result;
    }
    if (!track_api.set_track_value(track, "B_MUTE", mutation.muted ? 1.0 : 0.0) ||
        (track_api.get_track_value &&
         (track_api.get_track_value(track, "B_MUTE") != 0.0) != mutation.muted)) {
      result.error = "REAPER could not update mute state for ADR track " + mutation.expected_key + ".";
      transaction.mark_failed();
      return result;
    }
    if (mutation.muted) ++result.tracks_muted;
    else ++result.tracks_unmuted;
  }

  for (const core::RegionVisibilityMutation& mutation : plan.region_mutations) {
    int marker_count = 0;
    int region_count = 0;
    const int total = region_api.count_project_markers(project, &marker_count, &region_count);
    if (total < 0 || marker_count < 0 || region_count < 0) {
      result.error = "REAPER returned an invalid region count while applying the character filter.";
      transaction.mark_failed();
      return result;
    }
    ProjectMarker* target = nullptr;
    for (int index = 0; index < total; ++index) {
      bool is_region = false;
      double start_time = 0.0;
      double end_time = 0.0;
      const char* name = nullptr;
      int id = -1;
      int color = 0;
      if (region_api.enum_project_markers(project, index, &is_region, &start_time, &end_time,
                                          &name, &id, &color) == 0) {
        result.error = "REAPER could not enumerate regions while applying the character filter.";
        transaction.mark_failed();
        return result;
      }
      if (is_region && id == mutation.existing_id && name && mutation.expected_name == name) {
        target = region_api.get_region_or_marker(project, index, "");
        break;
      }
    }
    if (!target) {
      result.error = "The character-filter region plan is stale for " + mutation.expected_name + ".";
      transaction.mark_failed();
      return result;
    }
    region_api.set_region_or_marker_value(
      project, target, "B_HIDDEN", mutation.hidden ? 1.0 : 0.0);
    const bool verified =
      region_api.get_region_or_marker_value(project, target, "B_HIDDEN") != 0.0;
    if (verified != mutation.hidden) {
      result.error = "REAPER could not update visibility for ADR region " + mutation.expected_name + ".";
      transaction.mark_failed();
      return result;
    }
    if (mutation.hidden) ++result.regions_hidden;
    else ++result.regions_shown;
  }

  if (track_api.update_arrange) track_api.update_arrange();
  return result;
}

} // namespace reaadr::reaper
