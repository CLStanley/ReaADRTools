#pragma once

#include "character_filter.hpp"
#include "cue_navigation.hpp"
#include "session_model.hpp"

#include <string>

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

  explicit operator bool() const { return error.empty(); }
};

// Mirrors Lua active_cue(): first apply the active character/lane filter, then
// honor the Manager-selected cue when it remains visible, otherwise choose the
// cue under the current timeline position or the next visible cue.
RecordingTargetResult resolve_recording_target(
  const SessionModel& model,
  const CharacterFilterState& filter,
  const RecordingTargetOptions& options);

} // namespace reaadr::core
