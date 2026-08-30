#pragma once

#include "reaadr_core/character_filter.hpp"
#include "project_transaction.hpp"
#include "render_artifact_adapter.hpp"
#include "track_region_adapter.hpp"

#include <string>

struct ReaProject;

namespace reaadr::reaper {

struct CharacterFilterInspectionResult {
  core::CharacterFilterProjectState state;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct CharacterFilterApplyResult {
  int tracks_muted = 0;
  int tracks_unmuted = 0;
  int regions_hidden = 0;
  int regions_shown = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Inspection reuses the established typed API bundles, but collects only the
// mute/hidden fields needed by the host-independent filter planner.
CharacterFilterInspectionResult inspect_character_filter_project(
  ReaProject* project,
  TrackRegionApi track_api,
  RulerLaneApi region_api);

// Applies only preplanned mute and region-visibility changes. Every target is
// revalidated against its expected ownership metadata immediately before use.
CharacterFilterApplyResult apply_character_filter_plan_transactionally(
  ReaProject* project,
  TrackRegionApi track_api,
  RulerLaneApi region_api,
  TransactionApi transaction_api,
  const core::CharacterFilterPlan& plan,
  const std::string& description);

} // namespace reaadr::reaper
