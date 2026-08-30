#pragma once

#include "session_model.hpp"

#include <string>
#include <vector>

namespace reaadr::core {

struct LaneAssignmentResult {
  // Lane numbers correspond to the input cue indexes and are one-based.
  std::vector<int> lanes;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Assigns overlap lanes using the same cue-plus-preroll windows used by the
// legacy renderer. Keeping this rule shared prevents the canonical track model
// and the later REAPER render plan from independently choosing different lanes.
LaneAssignmentResult assign_character_lanes(const std::vector<Fields>& cues,
                                            double preroll_seconds = 3.0);

} // namespace reaadr::core
