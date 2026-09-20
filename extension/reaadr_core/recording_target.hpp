#pragma once

#include "character_filter.hpp"
#include "cue_navigation.hpp"
#include "session_model.hpp"

#include <string>
#include <vector>

namespace reaadr::core {

struct RecordingTargetOptions {
  std::string selected_cue_key;
  double timeline_position = 0.0;
  double preroll_seconds = 3.0;
};

struct RecordingTargetResult {
  CueNavigationEntry cue;
  int lane = 1;
  bool used_selected_cue = false;
  bool used_next_cue = false;
  std::string error;
  // Filtered/lane-aware canonical cue list shared with Cue Info navigation so
  // Previous/Next cannot escape the active character filter or silently
  // disagree with Record Cue targeting.
  std::vector<CueNavigationEntry> visible_cues;

  explicit operator bool() const { return error.empty(); }
};

// Applies the native active-cue contract: first the character/lane filter,
// then the Manager-selected cue when it remains visible, otherwise the cue
// under the timeline position or the next visible cue.
RecordingTargetResult resolve_recording_target(
  const SessionModel& model,
  const CharacterFilterState& filter,
  const RecordingTargetOptions& options);

} // namespace reaadr::core
