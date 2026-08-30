#include "render_artifact_adapter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>

namespace reaadr::reaper {
namespace {

constexpr int kCustomColorFlag = 0x1000000;
constexpr double kValueEpsilon = 0.0005;

template <typename Object, typename Function>
std::string read_string(Function function, Object* object, const char* parameter)
{
  std::array<char, 4096> buffer = {};
  if (!function(object, parameter, buffer.data(), false)) return {};
  return buffer.data();
}

template <typename Object, typename Function>
bool write_string(Function function, Object* object, const char* parameter, const std::string& value)
{
  std::string mutable_value = value;
  mutable_value.push_back('\0');
  return function(object, parameter, mutable_value.data(), true);
}

core::RgbColor unpack_color(const RulerLaneApi& api, int native_color)
{
  if (native_color == 0) return {};
  int red = 0;
  int green = 0;
  int blue = 0;
  api.color_from_native(native_color & ~kCustomColorFlag, &red, &green, &blue);
  return {red, green, blue, (native_color & kCustomColorFlag) != 0};
}

int pack_color(const RulerLaneApi& api, const core::RgbColor& color)
{
  if (!color.custom) return 0;
  return api.color_to_native(color.red, color.green, color.blue) | kCustomColorFlag;
}

bool ruler_inspection_api_complete(const RulerLaneApi& api)
{
  return api.get_set_project_info && api.get_set_project_info_string && api.count_project_markers &&
    api.enum_project_markers && api.get_region_or_marker && api.get_region_or_marker_value &&
    api.color_from_native;
}

bool ruler_apply_api_complete(const RulerLaneApi& api)
{
  return api.get_set_project_info && api.get_set_project_info_string && api.count_project_markers &&
    api.enum_project_markers && api.get_region_or_marker && api.get_region_or_marker_value &&
    api.set_region_or_marker_value && api.color_to_native;
}

std::string project_string(const RulerLaneApi& api, ReaProject* project, const std::string& parameter)
{
  std::array<char, 4096> buffer = {};
  if (!api.get_set_project_info_string(project, parameter.c_str(), buffer.data(), false)) return {};
  return buffer.data();
}

bool set_project_string(const RulerLaneApi& api,
                        ReaProject* project,
                        const std::string& parameter,
                        const std::string& value)
{
  std::string mutable_value = value;
  mutable_value.push_back('\0');
  return api.get_set_project_info_string(project, parameter.c_str(), mutable_value.data(), true);
}

bool cue_audio_inspection_api_complete(const CueAudioApi& api)
{
  return api.count_tracks && api.get_track && api.get_set_track_string && api.count_track_items &&
    api.get_track_item && api.get_set_item_string && api.get_item_value && api.get_active_take &&
    api.get_set_take_string;
}

bool cue_audio_apply_api_complete(const CueAudioApi& api)
{
  return cue_audio_inspection_api_complete(api) && api.set_item_value && api.add_item_to_track &&
    api.delete_track_item && api.move_item_to_track && api.add_take_to_item && api.get_set_take_info &&
    api.create_source_from_file && api.get_source_length && api.destroy_source;
}

bool cue_source_api_complete(const CueAudioApi& api)
{
  return api.create_source_from_file && api.get_source_length && api.destroy_source;
}

} // namespace

RulerLaneInspectionResult RulerLaneAdapter::inspect() const
{
  RulerLaneInspectionResult result;
  if (!ruler_inspection_api_complete(api_)) {
    result.error = "The REAPER ruler-lane inspection API is incomplete.";
    return result;
  }

  const int lane_count = static_cast<int>(std::llround(
    api_.get_set_project_info(project_, "RULER_LANE_COUNT", 0.0, false)));
  if (lane_count < 0 || lane_count > 100000) {
    result.error = "REAPER returned an invalid ruler-lane count.";
    return result;
  }
  result.ruler_lanes.reserve(static_cast<std::size_t>(lane_count));
  for (int index = 0; index < lane_count; ++index) {
    const std::string suffix = std::to_string(index);
    const int color = static_cast<int>(std::llround(
      api_.get_set_project_info(project_, ("RULER_LANE_COLOR:" + suffix).c_str(), 0.0, false)));
    const bool hidden = api_.get_set_project_info(
      project_, ("RULER_LANE_HIDDEN:" + suffix).c_str(), 0.0, false) != 0.0;
    result.ruler_lanes.push_back({
      index,
      project_string(api_, project_, "RULER_LANE_NAME:" + suffix),
      unpack_color(api_, color),
      hidden,
    });
  }

  int marker_count = 0;
  int region_count = 0;
  const int total = api_.count_project_markers(project_, &marker_count, &region_count);
  if (total < 0 || marker_count < 0 || region_count < 0) {
    result.error = "REAPER returned an invalid marker/region count while inspecting ruler lanes.";
    return result;
  }
  result.region_lanes.reserve(static_cast<std::size_t>(region_count));
  for (int index = 0; index < total; ++index) {
    bool is_region = false;
    double start_time = 0.0;
    double end_time = 0.0;
    const char* name = nullptr;
    int id = -1;
    int color = 0;
    if (api_.enum_project_markers(project_, index, &is_region, &start_time, &end_time,
                                  &name, &id, &color) == 0) {
      result.error = "REAPER could not enumerate regions while inspecting ruler lanes.";
      return result;
    }
    if (!is_region) continue;
    ProjectMarker* marker = api_.get_region_or_marker(project_, index, "");
    if (!marker) {
      result.error = "REAPER could not resolve a region while inspecting ruler lanes.";
      return result;
    }
    const int lane = static_cast<int>(std::llround(
      api_.get_region_or_marker_value(project_, marker, "I_LANENUMBER")));
    result.region_lanes.push_back({name ? name : "", lane});
  }
  return result;
}

RulerLaneApplyResult RulerLaneAdapter::apply(const core::RenderPlan& plan) const
{
  RulerLaneApplyResult result;
  if (plan.minimum_ruler_lane_count == 0 && plan.ruler_lane_mutations.empty() &&
      plan.region_lane_mutations.empty()) {
    return result;
  }
  if (!ruler_apply_api_complete(api_)) {
    result.error = "The REAPER ruler-lane mutation API is incomplete.";
    return result;
  }

  const int current_count = static_cast<int>(std::llround(
    api_.get_set_project_info(project_, "RULER_LANE_COUNT", 0.0, false)));
  if (plan.minimum_ruler_lane_count > current_count) {
    api_.get_set_project_info(project_, "RULER_LANE_COUNT",
                              static_cast<double>(plan.minimum_ruler_lane_count), true);
    const int verified = static_cast<int>(std::llround(
      api_.get_set_project_info(project_, "RULER_LANE_COUNT", 0.0, false)));
    if (verified < plan.minimum_ruler_lane_count) {
      result.error = "REAPER could not increase the ruler-lane count.";
      return result;
    }
  }

  for (const core::DesiredRulerLane& lane : plan.ruler_lane_mutations) {
    const std::string suffix = std::to_string(lane.index);
    const std::string name_parameter = "RULER_LANE_NAME:" + suffix;
    const std::string color_parameter = "RULER_LANE_COLOR:" + suffix;
    const std::string hidden_parameter = "RULER_LANE_HIDDEN:" + suffix;
    const int color = pack_color(api_, lane.color);
    if (!set_project_string(api_, project_, name_parameter, lane.name)) {
      result.error = "REAPER could not name ruler lane " + suffix + ".";
      return result;
    }
    api_.get_set_project_info(project_, color_parameter.c_str(), static_cast<double>(color), true);
    api_.get_set_project_info(project_, hidden_parameter.c_str(), lane.hidden ? 1.0 : 0.0, true);
    const int verified_color = static_cast<int>(std::llround(
      api_.get_set_project_info(project_, color_parameter.c_str(), 0.0, false)));
    const bool verified_hidden = api_.get_set_project_info(
      project_, hidden_parameter.c_str(), 0.0, false) != 0.0;
    if (project_string(api_, project_, name_parameter) != lane.name ||
        verified_color != color || verified_hidden != lane.hidden) {
      result.error = "REAPER could not verify ruler lane " + suffix + ".";
      return result;
    }
    ++result.lanes_updated;
  }

  for (const core::RegionLaneMutation& assignment : plan.region_lane_mutations) {
    int marker_count = 0;
    int region_count = 0;
    const int total = api_.count_project_markers(project_, &marker_count, &region_count);
    if (total < 0 || marker_count < 0 || region_count < 0) {
      result.error = "REAPER returned an invalid marker/region count while assigning ruler lanes.";
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
      if (api_.enum_project_markers(project_, index, &is_region, &start_time, &end_time,
                                    &name, &id, &color) == 0) {
        result.error = "REAPER could not enumerate regions while assigning ruler lanes.";
        return result;
      }
      if (is_region && name && assignment.region_name == name) {
        target = api_.get_region_or_marker(project_, index, "");
        break;
      }
    }
    if (!target) {
      result.error = "REAPER could not find ADR region " + assignment.region_name + " for ruler assignment.";
      return result;
    }
    api_.set_region_or_marker_value(project_, target, "I_LANENUMBER",
                                    static_cast<double>(assignment.lane_index));
    const int verified = static_cast<int>(std::llround(
      api_.get_region_or_marker_value(project_, target, "I_LANENUMBER")));
    if (verified != assignment.lane_index) {
      result.error = "REAPER could not assign ADR region " + assignment.region_name + " to its ruler lane.";
      return result;
    }
    ++result.regions_assigned;
  }
  return result;
}

CueAudioSourceResult CueAudioAdapter::inspect_source(const std::string& path) const
{
  CueAudioSourceResult result;
  if (path.empty()) {
    result.error = "A cue-audio source path is required.";
    return result;
  }
  if (!cue_source_api_complete(api_)) {
    result.error = "The REAPER cue-audio source API is incomplete.";
    return result;
  }
  PCM_source* source = api_.create_source_from_file(path.c_str());
  if (!source) {
    result.error = "REAPER could not open the cue-audio source: " + path;
    return result;
  }
  bool length_is_quarters = false;
  result.duration = api_.get_source_length(source, &length_is_quarters);
  api_.destroy_source(source);
  if (!std::isfinite(result.duration) || result.duration <= 0.0 || length_is_quarters) {
    result.duration = 0.0;
    result.error = "The cue-audio source does not have a valid duration in seconds: " + path;
  }
  return result;
}

CueAudioInspectionResult CueAudioAdapter::inspect() const
{
  CueAudioInspectionResult result;
  if (!cue_audio_inspection_api_complete(api_)) {
    result.error = "The REAPER cue-audio inspection API is incomplete.";
    return result;
  }
  const int track_count = api_.count_tracks(project_);
  if (track_count < 0) {
    result.error = "REAPER returned an invalid track count while inspecting cue audio.";
    return result;
  }
  for (int track_index = 0; track_index < track_count; ++track_index) {
    MediaTrack* track = api_.get_track(project_, track_index);
    if (!track) {
      result.error = "REAPER could not resolve a track while inspecting cue audio.";
      return result;
    }
    const std::string track_role = read_string(api_.get_set_track_string, track, "P_EXT:ReaADR.role");
    const std::string track_key = read_string(api_.get_set_track_string, track, "P_EXT:ReaADR.key");
    const int item_count = api_.count_track_items(track);
    if (item_count < 0) {
      result.error = "REAPER returned an invalid item count while inspecting cue audio.";
      return result;
    }
    for (int item_index = 0; item_index < item_count; ++item_index) {
      MediaItem* item = api_.get_track_item(track, item_index);
      if (!item) {
        result.error = "REAPER could not resolve an item while inspecting cue audio.";
        return result;
      }
      const std::string role = read_string(api_.get_set_item_string, item, "P_EXT:ReaADR.role");
      if (role != "cue_audio") continue;
      MediaItem_Take* take = api_.get_active_take(item);
      result.items.push_back({
        static_cast<std::size_t>(track_index),
        static_cast<std::size_t>(item_index),
        track_role,
        track_key,
        role,
        read_string(api_.get_set_item_string, item, "P_EXT:ReaADR.cue_key"),
        read_string(api_.get_set_item_string, item, "P_EXT:ReaADR.source_path"),
        take ? read_string(api_.get_set_take_string, take, "P_NAME") : std::string(),
        api_.get_item_value(item, "D_POSITION"),
        api_.get_item_value(item, "D_LENGTH"),
        api_.get_item_value(item, "B_LOOPSRC") != 0.0,
        take != nullptr,
      });
    }
  }
  return result;
}

CueAudioApplyResult CueAudioAdapter::apply(const core::RenderPlan& plan) const
{
  CueAudioApplyResult result;
  if (plan.cue_audio_mutations.empty()) return result;
  if (!cue_audio_apply_api_complete(api_)) {
    result.error = "The REAPER cue-audio mutation API is incomplete.";
    return result;
  }

  auto find_track = [&](const std::string& role, const std::string& key) -> MediaTrack* {
    const int count = api_.count_tracks(project_);
    for (int index = 0; index < count; ++index) {
      MediaTrack* track = api_.get_track(project_, index);
      if (track && read_string(api_.get_set_track_string, track, "P_EXT:ReaADR.role") == role &&
          read_string(api_.get_set_track_string, track, "P_EXT:ReaADR.key") == key) {
        return track;
      }
    }
    return nullptr;
  };

  struct ResolvedItem {
    const core::CueAudioMutation* mutation = nullptr;
    MediaTrack* original_track = nullptr;
    MediaItem* item = nullptr;
  };
  std::vector<ResolvedItem> resolved;
  resolved.reserve(plan.cue_audio_mutations.size());
  for (const core::CueAudioMutation& mutation : plan.cue_audio_mutations) {
    if (mutation.kind == core::RenderMutationKind::create) {
      resolved.push_back({&mutation, nullptr, nullptr});
      continue;
    }
    MediaTrack* track = api_.get_track(project_, static_cast<int>(mutation.existing_track_index));
    MediaItem* item = track ? api_.get_track_item(track, static_cast<int>(mutation.existing_item_index)) : nullptr;
    if (!track || !item ||
        read_string(api_.get_set_item_string, item, "P_EXT:ReaADR.role") != "cue_audio" ||
        read_string(api_.get_set_item_string, item, "P_EXT:ReaADR.cue_key") != mutation.desired.cue_key) {
      result.error = "The cue-audio render plan is stale for cue " + mutation.desired.cue_key + ".";
      return result;
    }
    resolved.push_back({&mutation, track, item});
  }

  for (ResolvedItem& entry : resolved) {
    const core::CueAudioMutation& mutation = *entry.mutation;
    if (mutation.kind == core::RenderMutationKind::remove) continue;
    MediaTrack* target_track = find_track("cue_character", mutation.desired.target_track_key);
    if (!target_track) {
      result.error = "REAPER could not find cue track " + mutation.desired.target_track_key + ".";
      return result;
    }
    MediaItem* item = entry.item;
    if (!item) {
      item = api_.add_item_to_track(target_track);
      if (!item) {
        result.error = "REAPER could not create cue-audio item " + mutation.desired.cue_key + ".";
        return result;
      }
    } else if (entry.original_track != target_track && !api_.move_item_to_track(item, target_track)) {
      result.error = "REAPER could not move cue-audio item " + mutation.desired.cue_key + " to its cue track.";
      return result;
    }

    MediaItem_Take* take = api_.get_active_take(item);
    if (!take) take = api_.add_take_to_item(item);
    if (!take ||
        !api_.set_item_value(item, "D_POSITION", mutation.desired.position) ||
        !api_.set_item_value(item, "D_LENGTH", mutation.desired.length) ||
        !api_.set_item_value(item, "B_LOOPSRC", 0.0) ||
        !write_string(api_.get_set_take_string, take, "P_NAME", mutation.desired.take_name) ||
        !write_string(api_.get_set_item_string, item, "P_EXT:ReaADR.role", "cue_audio") ||
        !write_string(api_.get_set_item_string, item, "P_EXT:ReaADR.cue_key", mutation.desired.cue_key) ||
        !write_string(api_.get_set_item_string, item, "P_EXT:ReaADR.version", "0.1.0") ||
        !write_string(api_.get_set_item_string, item, "P_EXT:ReaADR.source_path", mutation.desired.source_path)) {
      result.error = "REAPER could not configure cue-audio item " + mutation.desired.cue_key + ".";
      return result;
    }

    if (mutation.desired.replace_source) {
      PCM_source* source = api_.create_source_from_file(mutation.desired.source_path.c_str());
      if (!source) {
        result.error = "REAPER could not open cue-audio source " + mutation.desired.source_path + ".";
        return result;
      }
      bool length_is_quarters = false;
      const double duration = api_.get_source_length(source, &length_is_quarters);
      if (length_is_quarters || !std::isfinite(duration) ||
          std::abs(duration - mutation.desired.length) > kValueEpsilon) {
        api_.destroy_source(source);
        result.error = "The cue-audio source changed after the render plan was created.";
        return result;
      }
      auto* old_source = static_cast<PCM_source*>(api_.get_set_take_info(take, "P_SOURCE", nullptr));
      api_.get_set_take_info(take, "P_SOURCE", source);
      auto* installed_source = static_cast<PCM_source*>(api_.get_set_take_info(take, "P_SOURCE", nullptr));
      if (installed_source != source) {
        api_.destroy_source(source);
        result.error = "REAPER could not install the cue-audio source for " + mutation.desired.cue_key + ".";
        return result;
      }
      if (old_source && old_source != source) api_.destroy_source(old_source);
    }

    if (mutation.kind == core::RenderMutationKind::create) ++result.items_created;
    else ++result.items_updated;
  }

  // Item handles were resolved before any moves or deletions, so removing one
  // item cannot invalidate a later plan entry's track-relative index.
  for (const ResolvedItem& entry : resolved) {
    if (entry.mutation->kind != core::RenderMutationKind::remove) continue;
    if (!api_.delete_track_item(entry.original_track, entry.item)) {
      result.error = "REAPER could not remove stale cue-audio item " + entry.mutation->desired.cue_key + ".";
      return result;
    }
    ++result.items_removed;
  }
  return result;
}

CompleteRenderInspectionResult inspect_complete_render_state(
  ReaProject* project,
  TrackRegionApi track_region_api,
  RulerLaneApi ruler_lane_api,
  CueAudioApi cue_audio_api)
{
  CompleteRenderInspectionResult result;
  const ProjectInspectionResult tracks_and_regions = TrackRegionAdapter(project, track_region_api).inspect();
  if (!tracks_and_regions) {
    result.error = tracks_and_regions.error;
    return result;
  }
  result.state = tracks_and_regions.state;

  const RulerLaneInspectionResult ruler_lanes = RulerLaneAdapter(project, ruler_lane_api).inspect();
  if (!ruler_lanes) {
    result.error = ruler_lanes.error;
    return result;
  }
  result.state.ruler_lanes = ruler_lanes.ruler_lanes;
  result.state.region_lanes = ruler_lanes.region_lanes;

  const CueAudioInspectionResult cue_audio = CueAudioAdapter(project, cue_audio_api).inspect();
  if (!cue_audio) {
    result.error = cue_audio.error;
    return result;
  }
  result.state.cue_audio_items = cue_audio.items;
  return result;
}

CompleteRenderApplyResult apply_complete_render_plan_transactionally(
  ReaProject* project,
  TrackRegionApi track_region_api,
  RulerLaneApi ruler_lane_api,
  CueAudioApi cue_audio_api,
  TransactionApi transaction_api,
  const core::RenderPlan& plan,
  const std::string& description)
{
  CompleteRenderApplyResult result;
  if (plan.empty()) return result;
  ProjectTransaction transaction(project, transaction_api, description);
  UiRefreshScope refresh(transaction_api.prevent_ui_refresh);

  // The complete coordinator owns the one final view refresh. Suppressing the
  // narrower adapter callbacks avoids redundant arrange rebuilds mid-plan.
  TrackRegionApi quiet_track_region_api = track_region_api;
  quiet_track_region_api.adjust_track_windows = nullptr;
  quiet_track_region_api.update_arrange = nullptr;
  core::RenderPlan track_region_plan;
  track_region_plan.track_mutations = plan.track_mutations;
  track_region_plan.region_mutations = plan.region_mutations;
  result.tracks_and_regions = TrackRegionAdapter(project, quiet_track_region_api).apply(track_region_plan);
  if (!result.tracks_and_regions) {
    result.error = result.tracks_and_regions.error;
    transaction.mark_failed();
    return result;
  }

  result.ruler_lanes = RulerLaneAdapter(project, ruler_lane_api).apply(plan);
  if (!result.ruler_lanes) {
    result.error = result.ruler_lanes.error;
    transaction.mark_failed();
    return result;
  }

  result.cue_audio = CueAudioAdapter(project, cue_audio_api).apply(plan);
  if (!result.cue_audio) {
    result.error = result.cue_audio.error;
    transaction.mark_failed();
    return result;
  }
  if (track_region_api.adjust_track_windows) track_region_api.adjust_track_windows(false);
  if (track_region_api.update_arrange) track_region_api.update_arrange();
  return result;
}

} // namespace reaadr::reaper
