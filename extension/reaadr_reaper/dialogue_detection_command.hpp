#pragma once

#include <cstddef>
#include <string>

class ReaProject;

namespace reaadr::reaper {

struct DialogueDetectionCommandResult {
  bool cancelled = false;
  std::size_t cue_count = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

DialogueDetectionCommandResult run_dialogue_detection_command(
  ReaProject* project = nullptr);

} // namespace reaadr::reaper
