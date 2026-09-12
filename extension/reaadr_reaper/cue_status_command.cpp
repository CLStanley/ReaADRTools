#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_GetProjExtState
#define REAPERAPI_WANT_SetProjExtState

#include "cue_status_command.hpp"

#include "native_host_services.hpp"
#include "overlay_refresh_adapter.hpp"
#include "../app/cue_status_application_service.hpp"
#include "../app/overlay_application_service.hpp"
#include "../reaadr_core/cue_navigation.hpp"
#include "../reaadr_core/model_repository.hpp"
#include "../reaadr_core/character_filter.hpp"
#include "../reaadr_core/overlay_settings.hpp"

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>

namespace reaadr::reaper {
namespace {

double command_frame_rate()
{
  return native_project_frame_rate(nullptr);
}

OverlaySelectionInput empty_overlay_selection()
{
  return {};
}

bool command_refresh_overlay(const core::OverlayRefreshOptions& options,
                             std::string* error)
{
  const auto refreshed = refresh_generated_overlay_transactionally(
    nullptr, native_overlay_refresh_api(), native_transaction_api(), options,
    "ReaADR: refresh cue status overlay");
  if (!refreshed && error) *error = refreshed.error;
  return static_cast<bool>(refreshed);
}

double current_timeline_position()
{
  return (native_play_state() & 1) != 0
    ? native_play_position()
    : native_cursor_position();
}

} // namespace

const std::vector<std::string>& native_cue_status_choices()
{
  static const std::vector<std::string> statuses = {
    "Not Recorded", "In Progress", "Recorded", "Needs Review", "Approved", "Needs Retake",
  };
  return statuses;
}

CueStatusCommandResult set_cue_status_at_current_position(
  const std::string& status,
  ReaProject* project)
{
  CueStatusCommandResult result;
  core::ProjectStateStore project_state(project, {GetProjExtState, SetProjExtState});
  core::SessionModelRepository sessions(project_state);
  core::EventLogRepository event_log(project_state);
  core::CharacterFilterRepository filters(project_state);
  core::OverlaySettingsRepository overlay_settings(project_state);
  core::CueSelectionRepository selections(project_state);

  const core::SessionLoadResult session = sessions.load();
  if (!session) {
    result.error = core::session_load_error_message(session);
    return result;
  }
  const core::CueNavigationCatalogResult catalog =
    core::build_cue_navigation_catalog(session.model);
  if (!catalog) {
    result.error = catalog.error;
    return result;
  }
  const core::CueNavigationEntry* cue =
    core::find_cue_at_position(catalog.cues, current_timeline_position());
  if (!cue) {
    result.error = "No ADR cue exists at the current timeline position.";
    return result;
  }
  result.cue_key = cue->cue_key;

  OverlayApplicationService overlay(
    sessions, overlay_settings, selections, filters,
    {command_frame_rate, empty_overlay_selection, command_refresh_overlay});
  CueStatusApplicationService application(
    sessions, event_log, project, native_transaction_api());

  CueStatusApplicationOptions options;
  options.commit.snapshot_label = "Set Cue Status";
  options.commit.utc_timestamp = native_utc_timestamp();
  options.commit.update.last_operation = "set_cue_status";
  options.event.source = "native_set_cue_status";
  options.event.utc_timestamp = options.commit.utc_timestamp;
  options.undo_description = "ReaADR: set cue status";
  options.refresh_overlay = [&overlay](std::string* error) {
    const auto refreshed = overlay.refresh();
    if (!refreshed && error) *error = refreshed.error;
    return static_cast<bool>(refreshed);
  };

  const CueStatusApplicationResult applied =
    application.apply(result.cue_key, status, options);
  if (!applied) {
    result.error = applied.error;
    return result;
  }
  result.normalized_status = applied.status.update.normalized_status;
  result.changed = applied.status.update.changed;
  result.event_warning = applied.event_warning;
  return result;
}

} // namespace reaadr::reaper
