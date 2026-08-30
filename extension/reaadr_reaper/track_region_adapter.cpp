#include "track_region_adapter.hpp"

#include <array>
#include <cmath>
#include <cstring>

namespace reaadr::reaper {
namespace {

constexpr int kCustomColorFlag = 0x1000000;

bool inspection_api_complete(const TrackRegionApi& api)
{
  return api.count_tracks && api.get_track && api.get_set_track_string && api.get_track_value &&
    api.count_project_markers && api.enum_project_markers && api.color_from_native;
}

bool apply_api_complete(const TrackRegionApi& api)
{
  return api.count_tracks && api.get_track && api.insert_track_at_index && api.get_set_track_string &&
    api.set_track_value && api.set_project_marker && api.add_project_marker && api.delete_project_marker &&
    api.color_to_native;
}

std::string track_string(const TrackRegionApi& api, MediaTrack* track, const char* parameter)
{
  std::array<char, 4096> buffer = {};
  if (!api.get_set_track_string(track, parameter, buffer.data(), false)) return {};
  return buffer.data();
}

bool set_track_string(const TrackRegionApi& api,
                      MediaTrack* track,
                      const char* parameter,
                      const std::string& value)
{
  // REAPER's historical API takes char* even when setNewValue is true. It does
  // not modify the supplied text, so a temporary mutable buffer is safest.
  std::string mutable_value = value;
  mutable_value.push_back('\0');
  return api.get_set_track_string(track, parameter, mutable_value.data(), true);
}

core::RgbColor unpack_color(const TrackRegionApi& api, int native_color)
{
  if (native_color == 0) return {};
  int red = 0;
  int green = 0;
  int blue = 0;
  api.color_from_native(native_color & ~kCustomColorFlag, &red, &green, &blue);
  return {red, green, blue, (native_color & kCustomColorFlag) != 0};
}

int pack_color(const TrackRegionApi& api, const core::RgbColor& color)
{
  if (!color.custom) return 0;
  return api.color_to_native(color.red, color.green, color.blue) | kCustomColorFlag;
}

bool configure_track(const TrackRegionApi& api, MediaTrack* track, const core::DesiredTrack& desired)
{
  return set_track_string(api, track, "P_NAME", desired.name) &&
    set_track_string(api, track, "P_EXT:ReaADR.role", desired.role) &&
    set_track_string(api, track, "P_EXT:ReaADR.key", desired.key) &&
    set_track_string(api, track, "P_EXT:ReaADR.version", "0.1.0") &&
    api.set_track_value(track, "I_CUSTOMCOLOR", static_cast<double>(pack_color(api, desired.color)));
}

} // namespace

ProjectInspectionResult TrackRegionAdapter::inspect() const
{
  ProjectInspectionResult result;
  if (!inspection_api_complete(api_)) {
    result.error = "The REAPER track/region inspection API is incomplete.";
    return result;
  }

  const int track_count = api_.count_tracks(project_);
  if (track_count < 0) {
    result.error = "REAPER returned an invalid track count.";
    return result;
  }
  result.state.tracks.reserve(static_cast<std::size_t>(track_count));
  for (int index = 0; index < track_count; ++index) {
    MediaTrack* track = api_.get_track(project_, index);
    if (!track) {
      result.error = "REAPER did not return track " + std::to_string(index) + ".";
      return result;
    }
    const int packed_color = static_cast<int>(std::llround(api_.get_track_value(track, "I_CUSTOMCOLOR")));
    result.state.tracks.push_back({
      static_cast<std::size_t>(index),
      track_string(api_, track, "P_EXT:ReaADR.role"),
      track_string(api_, track, "P_EXT:ReaADR.key"),
      track_string(api_, track, "P_NAME"),
      unpack_color(api_, packed_color),
    });
  }

  int marker_count = 0;
  int region_count = 0;
  const int total = api_.count_project_markers(project_, &marker_count, &region_count);
  if (total < 0 || marker_count < 0 || region_count < 0) {
    result.error = "REAPER returned an invalid marker/region count.";
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
    if (api_.enum_project_markers(project_, index, &is_region, &start_time, &end_time, &name, &id, &color) == 0) {
      result.error = "REAPER could not enumerate marker/region " + std::to_string(index) + ".";
      return result;
    }
    if (is_region) {
      result.state.regions.push_back({id, name ? name : "", start_time, end_time, unpack_color(api_, color)});
    }
  }

  return result;
}

RenderApplyResult TrackRegionAdapter::apply(const core::RenderPlan& plan) const
{
  RenderApplyResult result;
  if (plan.empty()) return result;
  if (plan.minimum_ruler_lane_count != 0 || !plan.ruler_lane_mutations.empty() ||
      !plan.region_lane_mutations.empty() || !plan.cue_audio_mutations.empty()) {
    result.error = "The track/region adapter received mutations owned by another render adapter.";
    return result;
  }
  if (!apply_api_complete(api_)) {
    result.error = "The REAPER track/region mutation API is incomplete.";
    return result;
  }
  for (const core::TrackMutation& mutation : plan.track_mutations) {
    if (mutation.kind == core::RenderMutationKind::remove) {
      // Dialogue tracks may contain irreplaceable recordings. Even a malformed
      // caller-supplied plan must not silently expand this adapter's boundary.
      result.error = "Automatic ADR track removal is intentionally unsupported.";
      return result;
    }
  }

  // Existing indexes remain stable because all updates are completed before
  // new tracks are appended to the project.
  for (const core::TrackMutation& mutation : plan.track_mutations) {
    if (mutation.kind != core::RenderMutationKind::update) continue;
    MediaTrack* track = api_.get_track(project_, static_cast<int>(mutation.project_index));
    if (!track || !configure_track(api_, track, mutation.desired)) {
      result.error = "REAPER could not update ADR track " + mutation.desired.name + ".";
      return result;
    }
    ++result.tracks_updated;
  }
  for (const core::TrackMutation& mutation : plan.track_mutations) {
    if (mutation.kind != core::RenderMutationKind::create) continue;
    const int index = api_.count_tracks(project_);
    if (index < 0) {
      result.error = "REAPER returned an invalid track count while creating an ADR track.";
      return result;
    }
    api_.insert_track_at_index(index, true);
    MediaTrack* track = api_.get_track(project_, index);
    if (!track || !configure_track(api_, track, mutation.desired)) {
      result.error = "REAPER could not create ADR track " + mutation.desired.name + ".";
      return result;
    }
    ++result.tracks_created;
  }

  for (const core::RegionMutation& mutation : plan.region_mutations) {
    const int color = pack_color(api_, mutation.desired.color);
    if (mutation.kind == core::RenderMutationKind::create) {
      if (api_.add_project_marker(project_, true, mutation.desired.start_time, mutation.desired.end_time,
                                  mutation.desired.name.c_str(), -1, color) < 0) {
        result.error = "REAPER could not create ADR region " + mutation.desired.name + ".";
        return result;
      }
      ++result.regions_created;
    } else if (mutation.kind == core::RenderMutationKind::update) {
      if (!api_.set_project_marker(project_, mutation.existing_id, true,
                                   mutation.desired.start_time, mutation.desired.end_time,
                                   mutation.desired.name.c_str(), color, 0)) {
        result.error = "REAPER could not update ADR region " + mutation.desired.name + ".";
        return result;
      }
      ++result.regions_updated;
    } else {
      if (!api_.delete_project_marker(project_, mutation.existing_id, true)) {
        result.error = "REAPER could not remove stale ADR region " + mutation.desired.name + ".";
        return result;
      }
      ++result.regions_removed;
    }
  }

  if (api_.adjust_track_windows) api_.adjust_track_windows(false);
  if (api_.update_arrange) api_.update_arrange();
  return result;
}

RenderApplyResult apply_render_plan_transactionally(ReaProject* project,
                                                    TrackRegionApi render_api,
                                                    TransactionApi transaction_api,
                                                    const core::RenderPlan& plan,
                                                    const std::string& description)
{
  if (plan.empty()) return {};
  if (plan.minimum_ruler_lane_count != 0 || !plan.ruler_lane_mutations.empty() ||
      !plan.region_lane_mutations.empty() || !plan.cue_audio_mutations.empty()) {
    RenderApplyResult result;
    result.error = "The track/region-only renderer cannot apply ruler-lane or cue-audio mutations.";
    return result;
  }
  ProjectTransaction transaction(project, transaction_api, description);
  UiRefreshScope refresh(transaction_api.prevent_ui_refresh);
  TrackRegionAdapter adapter(project, render_api);
  RenderApplyResult result = adapter.apply(plan);
  if (!result) transaction.mark_failed();
  return result;
}

} // namespace reaadr::reaper
