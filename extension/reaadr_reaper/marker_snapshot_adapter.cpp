#include "marker_snapshot_adapter.hpp"

namespace reaadr::reaper {

MarkerSnapshotResult snapshot_project_markers(ReaProject* project,
                                              MarkerSnapshotApi api)
{
  MarkerSnapshotResult result;
  if (!project) {
    result.error = "No active REAPER project is available.";
    return result;
  }
  if (!api.count_project_markers || !api.enum_project_markers) {
    result.error = "REAPER marker APIs are unavailable.";
    return result;
  }

  int marker_count = 0;
  int region_count = 0;
  const int total = api.count_project_markers(project, &marker_count, &region_count);
  if (total < 0) {
    result.error = "REAPER failed to count project markers and regions.";
    return result;
  }

  result.sources.reserve(static_cast<std::size_t>(marker_count + region_count));
  for (int i = 0; i < marker_count + region_count; ++i) {
    bool is_region = false;
    double position = 0.0;
    double region_end = 0.0;
    const char* name = nullptr;
    int marker_id = -1;
    int color = 0;
    if (api.enum_project_markers(project, i, &is_region, &position, &region_end,
                                 &name, &marker_id, &color) <= 0) {
      result.error = "REAPER failed while enumerating project markers and regions.";
      result.sources.clear();
      return result;
    }

    core::ProjectMarkerCueSource source;
    source.marker_id = marker_id;
    source.is_region = is_region;
    source.start_time = position;
    source.end_time = region_end;
    if (name) source.name = name;
    result.sources.push_back(std::move(source));
  }
  return result;
}

} // namespace reaadr::reaper
