#pragma once

#include "session_model.hpp"

#include <string>
#include <vector>

namespace reaadr::core {

struct DialogueSegment {
  double start_time = 0.0;
  double end_time = 0.0;
};

struct DialogueCueGenerationOptions {
  std::string character = "ADR";
  std::string notes = "Detected from selected media";
};

// Converts already-detected timeline segments into the canonical cue shape used
// by the session renderer. Audio scanning and REAPER item access remain at the
// host boundary.
std::vector<Fields> build_dialogue_cues(
  const std::vector<DialogueSegment>& segments,
  DialogueCueGenerationOptions options = {});

} // namespace reaadr::core
