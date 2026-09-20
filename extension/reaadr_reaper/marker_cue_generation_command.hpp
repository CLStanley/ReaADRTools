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

// Native legacy-project adoption. This is intentionally distinct from normal
// cue generation: it only runs when no canonical ADR session exists and
// treats existing REAPER regions as the source of truth for the initial
// session model.
MarkerCueGenerationCommandResult run_legacy_project_adoption_command(
  ReaProject* project = nullptr);

} // namespace reaadr::reaper
