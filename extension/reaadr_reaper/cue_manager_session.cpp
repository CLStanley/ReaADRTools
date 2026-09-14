#include "cue_manager_session.hpp"

#include "native_host_services.hpp"
#include "reaadr_ui/cue_manager_window.hpp"

#include <utility>

namespace reaadr::reaper {

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
  : project_state_(config.project, config.project_state_api),
    global_state_(config.global_state_api),
    repository_(project_state_),
    view_service_(project_state_, &global_state_),
    event_log_(project_state_),
    character_filter_(project_state_),
    overlay_settings_(project_state_),
    cue_selection_(project_state_),
    overlay_application_(repository_, overlay_settings_, cue_selection_,
                         character_filter_, config.overlay_api),
    renderer_(repository_, event_log_, character_filter_, config.project,
              native_track_region_api(), native_ruler_lane_api(),
              native_cue_audio_api(), native_transaction_api()),
    render_options_(make_render_options(overlay_application_, config.cue_audio_path)),
    mutations_(repository_, overlay_settings_, cue_selection_, renderer_,
               render_options_, config.mutation_api),
    controller_(view_service_, mutations_, project_state_, config.navigation_api,
                std::move(config.callbacks.trigger_import),
                std::move(config.callbacks.trigger_action),
                render_options_.refresh_overlay),
    frame_rate_(config.overlay_api.frame_rate)
{
}

bool CueManagerSession::show()
{
  const double frame_rate = frame_rate_ ? frame_rate_() : 24.0;
  return ui::show_cue_manager(controller_, frame_rate);
}

} // namespace reaadr::reaper
