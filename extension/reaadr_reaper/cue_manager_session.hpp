#pragma once

#include "app/cue_manager_application_service.hpp"
#include "app/manager_view_application_service.hpp"
#include "app/overlay_application_service.hpp"
#include "reaadr_core/character_filter.hpp"
#include "reaadr_core/event_log.hpp"
#include "reaadr_core/model_repository.hpp"
#include "reaadr_core/overlay_settings.hpp"
#include "reaadr_reaper/cue_navigation_service.hpp"
#include "reaadr_reaper/project_state.hpp"
#include "reaadr_reaper/session_render_service.hpp"
#include "reaadr_ui/cue_manager_controller.hpp"

#include <functional>
#include <string>

class ReaProject;

namespace reaadr::reaper {

struct CueManagerSessionCallbacks {
  std::function<void(const std::string&, bool, const std::string&, const std::string&)> trigger_import;
  std::function<void(const std::string&)> trigger_action;
};

// Host dependencies that are still owned by reaper_reaadr.cpp. Keeping these
// callbacks at the boundary lets the persistent Manager graph move out of the
// monolithic plug-in entrypoint without duplicating command registration or
// legacy action routing during the migration.
struct CueManagerSessionConfig {
  ReaProject* project = nullptr;
  ProjectStateApi project_state_api;
  GlobalStateApi global_state_api;
  OverlayApplicationApi overlay_api;
  CueNavigationApi navigation_api;
  CueManagerApplicationApi mutation_api;
  std::string cue_audio_path;
  CueManagerSessionCallbacks callbacks;
};

// Owns the complete native Cue Manager dependency graph. All services and
// repositories outlive the controller, allowing the host to keep one Manager
// session alive independently of the action invocation that opened its window.
class CueManagerSession final {
public:
  explicit CueManagerSession(CueManagerSessionConfig config);

  CueManagerSession(const CueManagerSession&) = delete;
  CueManagerSession& operator=(const CueManagerSession&) = delete;

  bool reload() { return controller_.reload(); }
  bool show();
  ui::CueManagerController& controller() { return controller_; }
  const ui::CueManagerController& controller() const { return controller_; }

private:
  static SessionRenderOptions make_render_options(
    OverlayApplicationService& overlay,
    const std::string& cue_audio_path);

  ProjectStateStore project_state_;
  GlobalStateStore global_state_;
  core::SessionModelRepository repository_;
  ManagerViewApplicationService view_service_;
  core::EventLogRepository event_log_;
  core::CharacterFilterRepository character_filter_;
  core::OverlaySettingsRepository overlay_settings_;
  core::CueSelectionRepository cue_selection_;
  OverlayApplicationService overlay_application_;
  SessionRenderService renderer_;
  SessionRenderOptions render_options_;
  CueManagerApplicationService mutations_;
  ui::CueManagerController controller_;
  double (*frame_rate_)() = nullptr;
};

} // namespace reaadr::reaper
