#pragma once

#include "reaadr_core/render_plan.hpp"
#include "project_transaction.hpp"

#include <string>

struct MediaTrack;
struct ReaProject;

namespace reaadr::reaper {

// Every host call is injected so inspection and partial-failure behavior can
// be tested without loading the extension inside REAPER.
struct TrackRegionApi {
  int (*count_tracks)(ReaProject*) = nullptr;
  MediaTrack* (*get_track)(ReaProject*, int) = nullptr;
  void (*insert_track_at_index)(int, bool) = nullptr;
  bool (*get_set_track_string)(MediaTrack*, const char*, char*, bool) = nullptr;
  double (*get_track_value)(MediaTrack*, const char*) = nullptr;
  bool (*set_track_value)(MediaTrack*, const char*, double) = nullptr;
  int (*count_project_markers)(ReaProject*, int*, int*) = nullptr;
  int (*enum_project_markers)(ReaProject*, int, bool*, double*, double*, const char**, int*, int*) = nullptr;
  bool (*set_project_marker)(ReaProject*, int, bool, double, double, const char*, int, int) = nullptr;
  int (*add_project_marker)(ReaProject*, bool, double, double, const char*, int, int) = nullptr;
  bool (*delete_project_marker)(ReaProject*, int, bool) = nullptr;
  int (*color_to_native)(int, int, int) = nullptr;
  void (*color_from_native)(int, int*, int*, int*) = nullptr;
  void (*adjust_track_windows)(bool) = nullptr;
  void (*update_arrange)() = nullptr;
};

struct ProjectInspectionResult {
  core::ProjectRenderState state;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct RenderApplyResult {
  int tracks_created = 0;
  int tracks_updated = 0;
  int regions_created = 0;
  int regions_updated = 0;
  int regions_removed = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

class TrackRegionAdapter {
public:
  TrackRegionAdapter(ReaProject* project, TrackRegionApi api) : project_(project), api_(api) {}

  ProjectInspectionResult inspect() const;
  RenderApplyResult apply(const core::RenderPlan& plan) const;

private:
  ReaProject* project_ = nullptr;
  TrackRegionApi api_;
};

// Applies one plan as one undo point. Any host-call failure marks the native
// transaction failed, allowing ProjectTransaction to roll back only its own
// matching undo block while UiRefreshScope keeps refresh counters balanced.
RenderApplyResult apply_render_plan_transactionally(ReaProject* project,
                                                    TrackRegionApi render_api,
                                                    TransactionApi transaction_api,
                                                    const core::RenderPlan& plan,
                                                    const std::string& description);

} // namespace reaadr::reaper
