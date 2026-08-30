#pragma once

#include "reaadr_core/render_plan.hpp"
#include "project_transaction.hpp"
#include "track_region_adapter.hpp"

#include <string>

struct MediaItem;
struct MediaItem_Take;
struct MediaTrack;
struct PCM_source;
struct ProjectMarker;
struct ReaProject;

namespace reaadr::reaper {

struct RulerLaneApi {
  double (*get_set_project_info)(ReaProject*, const char*, double, bool) = nullptr;
  bool (*get_set_project_info_string)(ReaProject*, const char*, char*, bool) = nullptr;
  int (*count_project_markers)(ReaProject*, int*, int*) = nullptr;
  int (*enum_project_markers)(ReaProject*, int, bool*, double*, double*, const char**, int*, int*) = nullptr;
  ProjectMarker* (*get_region_or_marker)(ReaProject*, int, const char*) = nullptr;
  double (*get_region_or_marker_value)(ReaProject*, ProjectMarker*, const char*) = nullptr;
  double (*set_region_or_marker_value)(ReaProject*, ProjectMarker*, const char*, double) = nullptr;
  int (*color_to_native)(int, int, int) = nullptr;
  void (*color_from_native)(int, int*, int*, int*) = nullptr;
};

struct RulerLaneInspectionResult {
  std::vector<core::ExistingRulerLane> ruler_lanes;
  std::vector<core::ExistingRegionLane> region_lanes;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct RulerLaneApplyResult {
  int lanes_updated = 0;
  int regions_assigned = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

class RulerLaneAdapter {
public:
  RulerLaneAdapter(ReaProject* project, RulerLaneApi api) : project_(project), api_(api) {}

  RulerLaneInspectionResult inspect() const;
  RulerLaneApplyResult apply(const core::RenderPlan& plan) const;

private:
  ReaProject* project_ = nullptr;
  RulerLaneApi api_;
};

struct CueAudioApi {
  int (*count_tracks)(ReaProject*) = nullptr;
  MediaTrack* (*get_track)(ReaProject*, int) = nullptr;
  bool (*get_set_track_string)(MediaTrack*, const char*, char*, bool) = nullptr;
  int (*count_track_items)(MediaTrack*) = nullptr;
  MediaItem* (*get_track_item)(MediaTrack*, int) = nullptr;
  bool (*get_set_item_string)(MediaItem*, const char*, char*, bool) = nullptr;
  double (*get_item_value)(MediaItem*, const char*) = nullptr;
  bool (*set_item_value)(MediaItem*, const char*, double) = nullptr;
  MediaItem* (*add_item_to_track)(MediaTrack*) = nullptr;
  bool (*delete_track_item)(MediaTrack*, MediaItem*) = nullptr;
  bool (*move_item_to_track)(MediaItem*, MediaTrack*) = nullptr;
  MediaItem_Take* (*get_active_take)(MediaItem*) = nullptr;
  MediaItem_Take* (*add_take_to_item)(MediaItem*) = nullptr;
  bool (*get_set_take_string)(MediaItem_Take*, const char*, char*, bool) = nullptr;
  void* (*get_set_take_info)(MediaItem_Take*, const char*, void*) = nullptr;
  PCM_source* (*create_source_from_file)(const char*) = nullptr;
  double (*get_source_length)(PCM_source*, bool*) = nullptr;
  void (*destroy_source)(PCM_source*) = nullptr;
};

struct CueAudioSourceResult {
  double duration = 0.0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct CueAudioInspectionResult {
  std::vector<core::ExistingCueAudioItem> items;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct CueAudioApplyResult {
  int items_created = 0;
  int items_updated = 0;
  int items_removed = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

class CueAudioAdapter {
public:
  CueAudioAdapter(ReaProject* project, CueAudioApi api) : project_(project), api_(api) {}

  CueAudioSourceResult inspect_source(const std::string& path) const;
  CueAudioInspectionResult inspect() const;
  CueAudioApplyResult apply(const core::RenderPlan& plan) const;

private:
  ReaProject* project_ = nullptr;
  CueAudioApi api_;
};

struct CompleteRenderApplyResult {
  RenderApplyResult tracks_and_regions;
  RulerLaneApplyResult ruler_lanes;
  CueAudioApplyResult cue_audio;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct CompleteRenderInspectionResult {
  core::ProjectRenderState state;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Combines the three narrow adapter snapshots into the single state consumed
// by the deterministic planner. Inspection is read-only and deliberately does
// not open an undo block.
CompleteRenderInspectionResult inspect_complete_render_state(
  ReaProject* project,
  TrackRegionApi track_region_api,
  RulerLaneApi ruler_lane_api,
  CueAudioApi cue_audio_api);

// Applies all currently native render artifacts under one outer undo block.
// The ordering is deliberate: cue tracks and regions must exist before ruler
// assignments and cue-audio items resolve their targets.
CompleteRenderApplyResult apply_complete_render_plan_transactionally(
  ReaProject* project,
  TrackRegionApi track_region_api,
  RulerLaneApi ruler_lane_api,
  CueAudioApi cue_audio_api,
  TransactionApi transaction_api,
  const core::RenderPlan& plan,
  const std::string& description);

} // namespace reaadr::reaper
