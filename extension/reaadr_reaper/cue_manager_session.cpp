#include "cue_manager_session.hpp"

#include "native_host_services.hpp"
#include "reaadr_ui/cue_manager_lifecycle.hpp"
#include "reaadr_ui/cue_manager_window.hpp"

#include <utility>

namespace reaadr::reaper {

OverlayApplicationApi CueManagerSession::resolve_overlay_api(OverlayApplicationApi api)
{
  const auto native = native_overlay_application_api();
  if (!api.frame_rate) api.frame_rate = native.frame_rate;
  if (!api.selection) api.selection = native.selection;
  if (!api.refresh_overlay) api.refresh_overlay = native.refresh_overlay;
  return api;
}

CueNavigationApi CueManagerSession::resolve_navigation_api(CueNavigationApi api)
{
  const auto native = native_cue_navigation_api();
  if (!api.get_play_state) api.get_play_state = native.get_play_state;
  if (!api.get_play_position) api.get_play_position = native.get_play_position;
  if (!api.get_cursor_position) api.get_cursor_position = native.get_cursor_position;
  if (!api.set_edit_cursor_position)
    api.set_edit_cursor_position = native.set_edit_cursor_position;
  return api;
}

CueManagerApplicationApi CueManagerSession::resolve_mutation_api(
  CueManagerApplicationApi api)
{
  if (!api.utc_timestamp) api.utc_timestamp = native_utc_timestamp;
  if (!api.frame_rate) api.frame_rate = native_current_project_frame_rate;
  return api;
}

SessionRenderOptions CueManagerSession::make_render_options(
  OverlayApplicationService& overlay,
  const std::string& cue_audio_path)
{
  SessionRenderOptions options;
  options.cue_audio_path = cue_audio_path;
  options.event.source = "native_cue_manager";
  options.refresh_overlay = [&overlay](std::string* error) {
    const auto refreshed = overlay.refresh();
    if (!refreshed && error) *error = refreshed.error;
    return static_cast<bool>(refreshed);
  };
  return options;
}

CueManagerSession::CueManagerSession(CueManagerSessionConfig config)
  : project_(config.project),
    project_state_(config.project, config.project_state_api),
    global_state_(config.global_state_api),
    repository_(project_state_),
    view_service_(project_state_, &global_state_),
    event_log_(project_state_),
    character_filter_(project_state_),
    overlay_settings_(project_state_),
    cue_selection_(project_state_),
    overlay_api_(resolve_overlay_api(config.overlay_api)),
    overlay_application_(repository_, overlay_settings_, cue_selection_,
                         character_filter_, overlay_api_),
    renderer_(repository_, event_log_, character_filter_, config.project,
              native_track_region_api(), native_ruler_lane_api(),
              native_cue_audio_api(), native_transaction_api()),
    render_options_(make_render_options(
      overlay_application_,
      config.cue_audio_path.empty()
        ? native_project_cue_audio_path(config.project)
        : config.cue_audio_path)),
    mutation_api_(resolve_mutation_api(config.mutation_api)),
    mutations_(repository_, overlay_settings_, cue_selection_, renderer_,
               render_options_, mutation_api_),
    navigation_api_(resolve_navigation_api(config.navigation_api)),
    controller_(view_service_, mutations_, project_state_, navigation_api_,
                std::move(config.callbacks.trigger_import),
                std::move(config.callbacks.trigger_action),
                render_options_.refresh_overlay, &global_state_),
    frame_rate_(overlay_api_.frame_rate)
{
}

bool CueManagerSession::show()
{
  const double frame_rate = frame_rate_ ? frame_rate_() : 24.0;
  return ui::show_cue_manager(controller_, frame_rate);
}

bool CueManagerSession::sync_regions(std::string& error)
{
  error.clear();
  RegionTimingRenderOptions options;
  options.session = render_options_;
  options.session.commit.utc_timestamp = native_utc_timestamp();
  options.session.event.utc_timestamp = options.session.commit.utc_timestamp;
  RegionTimingApplicationService service(renderer_, std::move(options));
  const auto result = service.update();
  if (!result) {
    error = result.error;
    return false;
  }
  if (!controller_.reload()) {
    error = controller_.view().error;
    return false;
  }
  return true;
}

bool CueManagerSessionHost::open_or_activate(
  CueManagerSessionConfig config,
  std::string& error)
{
  error.clear();
  if (session_ && session_->project() != config.project) {
    if (!shutdown(&error)) return false;
  }
  if (!session_) session_ = std::make_unique<CueManagerSession>(std::move(config));
  if (!session_->reload()) {
    error = session_->controller().view().error;
    if (!ui::cue_manager_lifecycle().is_open()) session_.reset();
    return false;
  }
  if (!session_->show()) {
    error = "The native Cue Manager window could not be opened.";
    if (!ui::cue_manager_lifecycle().is_open()) session_.reset();
    return false;
  }
  return true;
}

bool CueManagerSessionHost::sync_regions(std::string& error)
{
  error.clear();
  if (!session_) {
    error = "The native Cue Manager session is not active.";
    return false;
  }
  return session_->sync_regions(error);
}

bool CueManagerSessionHost::shutdown(std::string* error)
{
  if (error) error->clear();
  if (!session_) return true;
  if (ui::cue_manager_lifecycle().is_open() && !ui::request_close_cue_manager()) {
    if (error) *error = "The native Cue Manager window could not be closed safely.";
    return false;
  }
  if (ui::cue_manager_lifecycle().is_open()) {
    if (error) *error = "The native Cue Manager window is still active.";
    return false;
  }
  session_.reset();
  return true;
}

CueManagerSessionHost& cue_manager_session_host()
{
  static CueManagerSessionHost host;
  return host;
}

} // namespace reaadr::reaper
