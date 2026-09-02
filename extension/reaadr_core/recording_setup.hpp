#pragma once

#include "render_plan.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace reaadr::core {

struct RecordingSetupOptions {
  std::string cue_key;
  double preroll_seconds = 3.0;
};

struct RecordingSetupPlan {
  Fields cue;
  std::size_t cue_model_index = 0;
  std::size_t track_project_index = 0;
  std::string cue_key;
  std::string character;
  std::string expected_track_role;
  std::string expected_track_key;
  int lane = 1;
  double cue_start = 0.0;
  double cue_end = 0.0;
  double record_start = 0.0;
  bool used_lane_fallback = false;
};

struct RecordingSetupPlanResult {
  RecordingSetupPlan plan;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Resolves one canonical cue to its deterministic overlap lane and recording
// track. Exact role/key ownership wins; the lane fallback is retained solely
// for compatibility with older rendered projects and never adopts user tracks.
RecordingSetupPlanResult build_recording_setup_plan(
  const SessionModel& model,
  const std::vector<ExistingTrack>& project_tracks,
  const RecordingSetupOptions& options);

} // namespace reaadr::core
