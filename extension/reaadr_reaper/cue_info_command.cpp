#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_GetCursorPosition
#define REAPERAPI_WANT_GetPlayPosition
#define REAPERAPI_WANT_GetPlayState
#define REAPERAPI_WANT_GetProjExtState
#define REAPERAPI_WANT_SetEditCurPos
#define REAPERAPI_WANT_SetProjExtState

#include "cue_info_command.hpp"

#include "native_host_services.hpp"
#include "overlay_refresh_adapter.hpp"
#include "recording_command.hpp"
#include "project_state.hpp"
#include "../app/cue_info_application_service.hpp"
#include "../app/cue_manager_application_service.hpp"
#include "../app/overlay_application_service.hpp"
#include "../app/recording_target_application_service.hpp"
#include "../reaadr_core/event_log.hpp"
#include "../reaadr_core/manager_preferences.hpp"
#include "../reaadr_core/model_repository.hpp"
#include "../reaadr_core/overlay_settings.hpp"
#include "../reaadr_ui/cue_info_controller.hpp"
#include "../reaadr_ui/cue_info_window.hpp"

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>

#include <memory>

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
    "ReaADR: refresh Cue Info overlay");
  if (!refreshed && error) *error = refreshed.error;
  return static_cast<bool>(refreshed);
}

bool refresh_overlay_application(OverlayApplicationService& overlay,
                                 std::string* error)
{
  const auto refreshed = overlay.refresh();
  if (!refreshed && error) *error = refreshed.error;
  return static_cast<bool>(refreshed);
}

SessionRenderOptions make_cue_info_render_options(OverlayApplicationService& overlay)
{
  SessionRenderOptions options;
  options.cue_audio_path = native_project_cue_audio_path(nullptr);
  options.event.source = "native_cue_info";
  options.refresh_overlay = [&overlay](std::string* error) {
    return refresh_overlay_application(overlay, error);
  };
  return options;
}

struct CueInfoWindowSession {
  ProjectStateStore project_state;
  core::SessionModelRepository sessions;
  core::EventLogRepository event_log;
  core::CharacterFilterRepository filters;
  core::OverlaySettingsRepository overlay_settings;
  core::CueSelectionRepository selections;
  core::ManagerPreferencesRepository preferences;
  RecordingTargetApplicationService targets;
  CueInfoApplicationService info;
  OverlayApplicationService overlay;
  SessionRenderService renderer;
  SessionRenderOptions render_options;
  CueManagerApplicationService mutations;
  CueNavigationApi navigation_api;
  ui::CueInfoController controller;

  CueInfoWindowSession()
    : project_state(nullptr, {GetProjExtState, SetProjExtState}),
      sessions(project_state),
      event_log(project_state),
      filters(project_state),
      overlay_settings(project_state),
      selections(project_state),
      preferences(project_state),
      targets(sessions, selections, filters, overlay_settings),
      info(targets, nullptr, native_cue_take_count_api()),
      overlay(sessions, overlay_settings, selections, filters,
              {command_frame_rate, empty_overlay_selection, command_refresh_overlay}),
      renderer(sessions, event_log, filters, nullptr,
               native_track_region_api(), native_ruler_lane_api(), native_cue_audio_api(),
               native_transaction_api()),
      render_options(make_cue_info_render_options(overlay)),
      mutations(sessions, overlay_settings, selections, renderer, render_options,
                {native_utc_timestamp, command_frame_rate}),
      navigation_api{GetPlayState, GetPlayPosition, GetCursorPosition, SetEditCurPos},
      controller(info, mutations, sessions, selections, preferences, project_state, navigation_api,
                 render_options.refresh_overlay,
                 []() { return run_native_record_cue_command(); },
                 {native_play_state, native_play_position, native_cursor_position, command_frame_rate})
  {
  }
};

std::unique_ptr<CueInfoWindowSession> g_cue_info_session;

} // namespace

bool run_native_cue_info_command()
{
  if (!g_cue_info_session)
    g_cue_info_session = std::make_unique<CueInfoWindowSession>();

  if (ui::show_cue_info_window(g_cue_info_session->controller)) return true;

  g_cue_info_session.reset();
  return false;
}

bool shutdown_native_cue_info_command()
{
  if (!g_cue_info_session) return true;
  if (!ui::force_close_cue_info_window()) return false;
  g_cue_info_session.reset();
  return true;
}

} // namespace reaadr::reaper
