#include "cue_manager_session.hpp"

#include "native_host_services.hpp"
#include "reaadr_ui/cue_manager_lifecycle.hpp"
#include "reaadr_ui/cue_manager_window.hpp"

#include <sstream>
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
  if (!api.set_edit_cursor_position) api.set_edit_cursor_position = native.set_edit_cursor_position;
  return api;
}

CueManagerApplicationApi CueManagerSession::resolve_mutation_api(CueManagerApplicationApi api)
{
  if (!api.utc_timestamp) api.utc_timestamp = native_utc_timestamp;
  if (!api.frame_rate) api.frame_rate = native_current_project_frame_rate;
  return api;
}

SessionRenderOptions CueManagerSession::make_render_options(
  OverlayApplicationService& overlay, const std::string& cue_audio_path)
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
  : project_(config.project), project_state_(config.project, config.project_state_api),
    global_state_(config.global_state_api), repository_(project_state_),
    view_service_(project_state_, &global_state_), event_log_(project_state_),
    character_filter_(project_state_), overlay_settings_(project_state_), cue_selection_(project_state_),
    overlay_api_(resolve_overlay_api(config.overlay_api)),
    overlay_application_(repository_, overlay_settings_, cue_selection_, character_filter_, overlay_api_),
    renderer_(repository_, event_log_, character_filter_, config.project, native_track_region_api(),
              native_ruler_lane_api(), native_cue_audio_api(), native_transaction_api()),
    render_options_(make_render_options(overlay_application_, config.cue_audio_path.empty()
      ? native_project_cue_audio_path(config.project) : config.cue_audio_path)),
    mutation_api_(resolve_mutation_api(config.mutation_api)),
    mutations_(repository_, overlay_settings_, cue_selection_, renderer_, render_options_, mutation_api_),
    cleanup_api_(std::move(config.cleanup_api)), cleanup_(repository_, native_transaction_api(), cleanup_api_),
    navigation_api_(resolve_navigation_api(config.navigation_api)),
    controller_(view_service_, mutations_, project_state_, navigation_api_,
                std::move(config.callbacks.trigger_import), std::move(config.callbacks.trigger_action),
                render_options_.refresh_overlay, &global_state_), frame_rate_(overlay_api_.frame_rate)
{
}

bool CueManagerSession::show()
{
  const double frame_rate = frame_rate_ ? frame_rate_() : 24.0;
  return ui::show_cue_manager(controller_, frame_rate);
}

CueImportPreviewResult CueManagerSession::preview_import_content(
  const std::string& content, const std::string& source_path,
  const std::optional<core::ColumnMapping>& mapping)
{
  const double frame_rate = frame_rate_ ? frame_rate_() : 24.0;
  CueImportApplicationService importer(renderer_, frame_rate, &repository_);
  return importer.preview_content(content, source_path, mapping);
}

CueImportApplicationResult CueManagerSession::import_content(
  const std::string& content, const std::string& source_path,
  const std::optional<core::ColumnMapping>& mapping, const std::string& mode,
  const std::vector<std::string>& characters)
{
  SessionRenderOptions options = render_options_;
  options.event.source = "native_import";
  const double frame_rate = frame_rate_ ? frame_rate_() : 24.0;
  options.commit.replacement.build.frame_rate = std::to_string(frame_rate);
  const auto existing_session = repository_.load();
  if (!existing_session && existing_session.error != core::SessionLoadError::missing) {
    CueImportApplicationResult result;
    result.error = core::session_load_error_message(existing_session);
    return result;
  }
  if (existing_session.error == core::SessionLoadError::missing) {
    options.commit.replacement.build.session_id = "native-import-" + native_utc_timestamp();
    options.commit.replacement.build.session_name = source_path;
  }
  CueImportApplicationService importer(renderer_, frame_rate, &repository_);
  auto result = importer.import_content(content, source_path, mapping, options, mode, characters);
  if (!result) return result;

  if (mapping) {
    std::ostringstream serialized;
    bool first = true;
    for (const auto& entry : *mapping) {
      if (!first) serialized << ';';
      first = false;
      serialized << entry.first << '=' << entry.second;
    }
    if (!project_state_.write("ReaADRTools", "import_mapping_last", serialized.str())) {
      result.error = "The cue sheet imported, but its column mapping could not be saved.";
      return result;
    }
  }
  if (!controller_.reload()) result.error = controller_.view().error;
  return result;
}

core::SessionLoadResult CueManagerSession::load_session() const
{
  return repository_.load();
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
  if (!result) { error = result.error; return false; }
  if (!controller_.reload()) { error = controller_.view().error; return false; }
  return true;
}

CueCleanupApplicationResult CueManagerSession::clear_characters(
  const std::vector<std::string>& characters, std::string& error)
{
  error.clear();
  if (cleanup_api_.utc_timestamp.empty()) cleanup_api_.utc_timestamp = native_utc_timestamp();
  CueCleanupApplicationService service(repository_, native_transaction_api(), cleanup_api_);
  const auto result = service.clear_characters(characters);
  if (!result) { error = result.error; return result; }
  if (!controller_.reload()) error = controller_.view().error;
  return result;
}

bool CueManagerSessionHost::open_or_activate(CueManagerSessionConfig config, std::string& error)
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

CueImportPreviewResult CueManagerSessionHost::preview_import_content(
  const std::string& content, const std::string& source_path,
  const std::optional<core::ColumnMapping>& mapping)
{
  if (session_) return session_->preview_import_content(content, source_path, mapping);
  CueImportPreviewResult result;
  result.error = "The native Cue Manager session is not active.";
  return result;
}

CueImportApplicationResult CueManagerSessionHost::import_content(
  const std::string& content, const std::string& source_path,
  const std::optional<core::ColumnMapping>& mapping, const std::string& mode,
  const std::vector<std::string>& characters)
{
  if (session_) return session_->import_content(content, source_path, mapping, mode, characters);
  CueImportApplicationResult result;
  result.error = "The native Cue Manager session is not active.";
  return result;
}

bool CueManagerSessionHost::sync_regions(std::string& error)
{
  error.clear();
  if (!session_) { error = "The native Cue Manager session is not active."; return false; }
  return session_->sync_regions(error);
}

CueCleanupApplicationResult CueManagerSessionHost::clear_characters(
  const std::vector<std::string>& characters, std::string& error)
{
  error.clear();
  if (!session_) {
    CueCleanupApplicationResult result;
    result.error = "The native Cue Manager session is not active.";
    error = result.error;
    return result;
  }
  return session_->clear_characters(characters, error);
}

core::SessionLoadResult CueManagerSessionHost::load_session() const
{
  if (session_) return session_->load_session();
  core::SessionLoadResult result;
  result.error = core::SessionLoadError::missing;
  return result;
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
