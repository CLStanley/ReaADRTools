#pragma once

#include "reaadr_core/marker_cue_generation.hpp"

#include <string>
#include <vector>

class ReaProject;

namespace reaadr::reaper {

struct MarkerSnapshotApi {
  int (*count_project_markers)(ReaProject*, int*, int*) = nullptr;
  int (*enum_project_markers)(ReaProject*, int, bool*, double*, double*, const char**, int*, int*) = nullptr;
};

struct MarkerSnapshotResult {
  std::vector<core::ProjectMarkerCueSource> sources;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Reads REAPER marker/region state once and converts it into a host-independent
// snapshot. Cue semantics remain in reaadr_core/marker_cue_generation.
MarkerSnapshotResult snapshot_project_markers(ReaProject* project,
                                              MarkerSnapshotApi api);

} // namespace reaadr::reaper
