#pragma once

#include <cstddef>
#include <string>

class ReaProject;

namespace reaadr::reaper {

struct MarkerCueGenerationCommandResult {
  bool cancelled = false;
  std::size_t cue_count = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Native interactive entry point used by the Manager while the historical Lua
// action remains installed only as a compatibility route. All mutation is
// delegated to MarkerCueGenerationApplicationService/SessionRenderService.
MarkerCueGenerationCommandResult run_marker_cue_generation_command(
  ReaProject* project = nullptr);

} // namespace reaadr::reaper
