#include "cue_manager_application_service.hpp"

namespace reaadr::reaper {

CueManagerApplicationResult CueManagerApplicationService::edit(
  const core::CueManagerEditOptions& options)
{
  CueManagerApplicationResult result;
  const core::SessionLoadResult loaded = sessions_.load();
  if (!loaded) {
    result.error = core::session_load_error_message(loaded);
    return result;
  }

  // Validate before generating media or opening an Undo block. The renderer
  // will rebuild cue-derived model collections from this edited cue set.
  result.edit = core::edit_cue_manager_row(loaded.model, options);
  if (!result.edit) {
    result.error = result.edit.error;
    return result;
  }
  if (!result.edit.changed) {
    const core::RevisionResult revision = sessions_.revision();
    if (!revision) result.error = revision.error;
    else result.revision = revision.revision;
    return result;
  }

  const core::OverlaySettingsLoadResult overlay = overlay_settings_.load();
  if (!overlay) {
    result.error = overlay.error;
    return result;
  }

  SessionRenderOptions render = render_options_;
  render.commit.replacement.last_operation = "edit_cue";
  render.commit.replacement.build.cues_modified = true;
  render.commit.replacement.build.tracks_modified = true;
  render.commit.replacement.build.regions_modified = true;
  render.commit.replacement.build.preroll_seconds = overlay.settings.preroll_seconds;
  render.commit.snapshot_label = "Edit Cue";
  render.undo_description = "ReaADR: edit cue";
  render.commit_event_type = "CueUpdated";
  if (api_.utc_timestamp) {
    render.commit.utc_timestamp = api_.utc_timestamp();
    render.event.utc_timestamp = render.commit.utc_timestamp;
  }
  if (render.event.source.empty()) render.event.source = "native_cue_manager";

  result.synchronization = renderer_.commit_and_render(result.edit.model.cues, render);
  if (!result.synchronization) {
    result.error = result.synchronization.error;
    return result;
  }
  result.revision = result.synchronization.commit.revision;
  return result;
}

} // namespace reaadr::reaper
