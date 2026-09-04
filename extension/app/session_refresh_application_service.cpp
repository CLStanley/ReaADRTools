#include "session_refresh_application_service.hpp"

namespace reaadr::reaper {

SessionRefreshApplicationResult SessionRefreshApplicationService::refresh()
{
  SessionRefreshApplicationResult result;
  const core::SessionLoadResult loaded = sessions_.load();
  if (!loaded) {
    result.error = core::session_load_error_message(loaded);
    return result;
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
