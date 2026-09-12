#pragma once

#include "session_model.hpp"

#include <string>
#include <vector>

namespace reaadr::core {

struct ProjectMarkerCueSource {
  int marker_id = -1;
  bool is_region = false;
  double start_time = 0.0;
  double end_time = 0.0;
  std::string name;
};

struct MarkerCueGenerationOptions {
  bool include_markers = true;
  bool include_regions = true;
  double default_duration = 2.0;
  std::string character = "ADR";
};

// Converts a read-only snapshot of REAPER markers/regions into the same cue
// field shape consumed by the canonical session builder. Host enumeration and
// confirmation UI stay outside the domain core.
std::vector<Fields> build_cues_from_project_markers(
  const std::vector<ProjectMarkerCueSource>& sources,
  MarkerCueGenerationOptions options = {});

} // namespace reaadr::core
