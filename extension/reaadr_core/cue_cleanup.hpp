#pragma once

#include "render_plan.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace reaadr::core {

struct CueCleanupPlan {
  struct RegionTarget { int id = -1; std::string name; };
  struct CueAudioTarget {
    std::size_t track_index = 0;
    std::size_t item_index = 0;
    std::string cue_key;
  };
  struct CueCharacterTrackTarget {
    std::size_t project_index = 0;
    std::string key;
  };
  std::vector<std::string> cue_keys;
  std::vector<RegionTarget> regions;
  std::vector<CueAudioTarget> cue_audio_items;
  std::vector<CueCharacterTrackTarget> cue_character_tracks;

  bool empty() const {
    return regions.empty() && cue_audio_items.empty() && cue_character_tracks.empty();
  }
};

struct CueCleanupPlanResult {
  CueCleanupPlan plan;
  std::vector<Fields> remaining_cues;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Plans character-specific cleanup from canonical cues and an inspected
// project. Only exact generated identities and explicit ReaADR ownership
// metadata may enter the destructive plan.
CueCleanupPlanResult build_cue_cleanup_plan(
  const SessionModel& model,
  const ProjectRenderState& existing,
  const std::vector<std::string>& characters);

} // namespace reaadr::core
