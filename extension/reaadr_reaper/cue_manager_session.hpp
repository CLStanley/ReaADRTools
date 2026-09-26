#pragma once

#include "app/cue_cleanup_application_service.hpp"
#include "app/cue_import_application_service.hpp"
#include "app/cue_manager_application_service.hpp"
#include "app/manager_view_application_service.hpp"
#include "app/overlay_application_service.hpp"
#include "app/region_timing_application_service.hpp"
#include "reaadr_core/character_filter.hpp"
#include "reaadr_core/event_log.hpp"
#include "reaadr_core/model_repository.hpp"
#include "reaadr_core/overlay_settings.hpp"
#include "reaadr_reaper/cue_navigation_service.hpp"
#include "reaadr_reaper/project_state.hpp"
#include "reaadr_reaper/session_render_service.hpp"
#include "reaadr_ui/cue_manager_controller.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ReaProject;

namespace reaadr::reaper {

struct CueManagerSessionCallbacks {
  std::function<void(const std::string&, bool, const std::string&, const std::string&)> trigger_import;
  std::function<void(const std::string&)> trigger_action;
};

struct CueManagerSessionConfig {
  ReaProject* project = nullptr;
  ProjectStateApi project_state_api;
  GlobalStateApi global_state_api;
  OverlayApplicationApi overlay_api;
  CueNavigationApi navigation_api;
  CueManagerApplicationApi mutation_api;
  CueCleanupApplicationApi cleanup_api;
  std::string cue_audio_path;
  CueManagerSessionCallbacks callbacks;
};

class CueManagerSession final {
public:
  explicit CueManagerSession(CueManagerSessionConfig config);

  CueManagerSession(const CueManagerSession&) = delete;
  CueManagerSession& operator=(const CueManagerSession&) = delete;

  bool reload() { return controller_.reload(); }
  bool show();
  CueImportPreviewResult preview_import_content(
    const std::string& content,
    const std::string& source_path,
    const std::optional<core::ColumnMapping>& mapping);
  CueImportApplicationResult import_content(
    const std::string& content,
    const std::string& source_path,
    const std::optional<core::ColumnMapping>& mapping,
    const std::string& mode,
    const std::vector<std::string>& characters);
  bool sync_regions(std::string& error);
  CueCleanupApplicationResult clear_characters(
    const std::vector<std::string>& characters,
    std::string& error);
  core::SessionLoadResult load_session() const;
  ReaProject* project() const { return project_; }
  ui::CueManagerController& controller() { return controller_; }
  const ui::CueManagerController& controller() const { return controller_; }

private:
  static OverlayApplicationApi resolve_overlay_api(OverlayApplicationApi api);
  static CueNavigationApi resolve_navigation_api(CueNavigationApi api);
  static CueManagerApplicationApi resolve_mutation_api(CueManagerApplicationApi api);
  static SessionRenderOptions make_render_options(
    OverlayApplicationService& overlay,
    const std::string& cue_audio_path);

  ReaProject* project_ = nullptr;
  ProjectStateStore project_state_;
  GlobalStateStore global_state_;
  core::SessionModelRepository repository_;
  ManagerViewApplicationService view_service_;
  core::EventLogRepository event_log_;
  core::CharacterFilterRepository character_filter_;
  core::OverlaySettingsRepository overlay_settings_;
  core::CueSelectionRepository cue_selection_;
  OverlayApplicationApi overlay_api_;
  OverlayApplicationService overlay_application_;
  SessionRenderService renderer_;
  SessionRenderOptions render_options_;
  CueManagerApplicationApi mutation_api_;
  CueManagerApplicationService mutations_;
  CueCleanupApplicationApi cleanup_api_;
  CueCleanupApplicationService cleanup_;
  CueNavigationApi navigation_api_;
  ui::CueManagerController controller_;
  double (*frame_rate_)() = nullptr;
};

class CueManagerSessionHost final {
public:
  bool open_or_activate(CueManagerSessionConfig config, std::string& error);
  CueImportPreviewResult preview_import_content(
    const std::string& content,
    const std::string& source_path,
    const std::optional<core::ColumnMapping>& mapping);
  CueImportApplicationResult import_content(
    const std::string& content,
    const std::string& source_path,
    const std::optional<core::ColumnMapping>& mapping,
    const std::string& mode,
    const std::vector<std::string>& characters);
  bool sync_regions(std::string& error);
  CueCleanupApplicationResult clear_characters(
    const std::vector<std::string>& characters,
    std::string& error);
  core::SessionLoadResult load_session() const;
  bool shutdown(std::string* error = nullptr);
  bool has_session() const { return static_cast<bool>(session_); }
  CueManagerSession* session() { return session_.get(); }
  const CueManagerSession* session() const { return session_.get(); }

private:
  std::unique_ptr<CueManagerSession> session_;
};

CueManagerSessionHost& cue_manager_session_host();

} // namespace reaadr::reaper
