#pragma once

#include "render_plan.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace reaadr::core {

struct RegionTimingSyncOptions {
  double timing_epsilon = 0.0005;
};

struct RegionTimingSyncResult {
  std::vector<Fields> cues;
  std::size_t changed_cues = 0;
  std::size_t missing_regions = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Imports timing only from regions whose complete generated names match the
// canonical cues. Ambiguous duplicate names fail closed so a user region can
// never become an accidental writer to adr_session_model_v1.
RegionTimingSyncResult sync_cue_timings_from_regions(
  const SessionModel& model,
  const std::vector<ExistingRegion>& project_regions,
  const RegionTimingSyncOptions& options = {});

} // namespace reaadr::core
