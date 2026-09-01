#include "cue_cleanup_adapter.hpp"

#include <array>

namespace reaadr::reaper {
namespace {

std::string read_string(CueCleanupApi api, MediaTrack* track, const char* parameter)
{
  std::array<char, 4096> value = {};
  if (!api.get_set_track_string(track, parameter, value.data(), false)) return {};
  return value.data();
}

std::string read_item_string(CueCleanupApi api, MediaItem* item, const char* parameter)
{
  std::array<char, 4096> value = {};
  if (!api.get_set_item_string(item, parameter, value.data(), false)) return {};
  return value.data();
}

bool complete(CueCleanupApi api)
{
  return api.count_tracks && api.get_track && api.get_set_track_string &&
    api.count_track_items && api.get_track_item && api.get_set_item_string &&
    api.delete_track_item && api.delete_track && api.count_project_markers &&
    api.enum_project_markers && api.delete_project_marker;
}

} // namespace

CueCleanupApplyResult apply_cue_cleanup_plan_transactionally(
  ReaProject* project,
  CueCleanupApi api,
  TransactionApi transaction_api,
  const core::CueCleanupPlan& plan,
  const std::string& description)
{
  CueCleanupApplyResult result;
  if (plan.empty()) return result;
  if (!complete(api)) {
    result.error = "The REAPER cue-cleanup API is incomplete.";
    return result;
  }
  ProjectTransaction transaction(project, transaction_api, description);
  UiRefreshScope refresh(transaction_api.prevent_ui_refresh);

  for (const auto& target : plan.regions) {
    int marker_count = 0;
    int region_count = 0;
    const int total = api.count_project_markers(project, &marker_count, &region_count);
    if (total < 0 || marker_count < 0 || region_count < 0) {
      result.error = "REAPER returned an invalid marker count during cue cleanup.";
      transaction.mark_failed();
      return result;
    }
    bool found = false;
    for (int index = 0; index < total; ++index) {
      bool is_region = false;
      const char* name = nullptr;
      int id = -1;
      if (!api.enum_project_markers(project, index, &is_region, nullptr, nullptr, &name, &id, nullptr)) {
        result.error = "REAPER could not enumerate regions during cue cleanup.";
        transaction.mark_failed();
        return result;
      }
      if (is_region && id == target.id && name && target.name == name) {
        found = api.delete_project_marker(project, id, true);
        break;
      }
    }
    if (!found) {
      result.error = "The cue-cleanup region plan became stale for " + target.name + ".";
      transaction.mark_failed();
      return result;
    }
    ++result.regions_removed;
  }

  for (const auto& target : plan.cue_audio_items) {
    MediaTrack* track = api.get_track(project, static_cast<int>(target.track_index));
    if (!track || static_cast<std::size_t>(target.item_index) >=
        static_cast<std::size_t>(api.count_track_items(track))) {
      result.error = "The cue-audio cleanup target became stale.";
      transaction.mark_failed();
      return result;
    }
    MediaItem* item = api.get_track_item(track, static_cast<int>(target.item_index));
    if (!item || read_item_string(api, item, "P_EXT:ReaADR.role") != "cue_audio" ||
        read_item_string(api, item, "P_EXT:ReaADR.cue_key") != target.cue_key ||
        !api.delete_track_item(track, item)) {
      result.error = "The cue-audio cleanup target changed before deletion.";
      transaction.mark_failed();
      return result;
    }
    ++result.cue_audio_removed;
  }

  for (const auto& target : plan.cue_character_tracks) {
    MediaTrack* track = api.get_track(project, static_cast<int>(target.project_index));
    if (!track || read_string(api, track, "P_EXT:ReaADR.role") != "cue_character" ||
        read_string(api, track, "P_EXT:ReaADR.key") != target.key ||
        !api.delete_track(project, track)) {
      result.error = "The cue-character track cleanup target changed before deletion.";
      transaction.mark_failed();
      return result;
    }
    ++result.tracks_removed;
  }

  result.cues_removed = static_cast<int>(plan.cue_keys.size());
  if (api.adjust_track_windows) api.adjust_track_windows(false);
  if (api.update_arrange) api.update_arrange();
  return result;
}

} // namespace reaadr::reaper
