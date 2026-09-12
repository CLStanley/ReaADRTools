#include "session_refresh_application_service.hpp"
#include "reaadr_core/region_timing_sync.hpp"

namespace reaadr::reaper {

SessionRefreshApplicationResult SessionRefreshApplicationService::refresh(
  std::function<ProjectInspectionResult()> inspect,
  std::function<bool(std::size_t)> confirm_overwrite)
{
  SessionRefreshApplicationResult result;
  const core::SessionLoadResult loaded = sessions_.load();
  if (!loaded) {
    result.error = core::session_load_error_message(loaded);
    return result;
  }

  if (inspect) {
    const auto current = inspect();
    if (!current) { result.error = current.error; return result; }
    // Reuse exact generated-name ownership and timing tolerance from Update
    // Cues From Regions, but discard its proposed cues: refresh stays model-first.
    const auto drift = core::sync_cue_timings_from_regions(loaded.model, current.state.regions);
    if (!drift) { result.error = drift.error; return result; }
    result.modified_regions = drift.changed_cues;
    if (result.modified_regions != 0 &&
        (!confirm_overwrite || !confirm_overwrite(result.modified_regions))) {
      result.cancelled = true;
      return result;
    }
  }

  SessionRenderOptions options = render_options_;
  options.commit.replacement.last_operation = "refresh_session";
  options.commit.replacement.build.cues_modified = true;
  options.commit.replacement.build.tracks_modified = true;
  options.commit.replacement.build.regions_modified = true;
  options.commit.snapshot_label = "Refresh Session";
  options.undo_description = "ReaADR: refresh session";
  options.commit_event_type = "SessionRefreshed";
  if (!utc_timestamp_.empty()) {
    options.commit.utc_timestamp = utc_timestamp_;
    options.event.utc_timestamp = utc_timestamp_;
  }

  result.synchronization = renderer_.commit_and_render(loaded.model.cues, options);
  if (!result.synchronization) result.error = result.synchronization.error;
  return result;
}

} // namespace reaadr::reaper
