#include "manager_view_application_service.hpp"

namespace reaadr::reaper {

ManagerViewLoadResult ManagerViewApplicationService::load(
  const core::CueManagerViewOptions& options, const std::string& requested_tab) const
{
  ManagerViewLoadResult result;
  core::SessionModelRepository sessions(project_state_);
  const auto session = sessions.load();
  if (!session) { result.error = core::session_load_error_message(session); return result; }
  core::ManagerPreferencesRepository preferences(project_state_, global_state_);
  const auto loaded_preferences = preferences.load();
  if (!loaded_preferences) { result.error = loaded_preferences.error; return result; }
  const auto revision = sessions.revision();
  if (!revision) { result.error = revision.error; return result; }
  result.view = core::build_manager_view_model(
    session.model, loaded_preferences.preferences, options,
    std::to_string(revision.revision), requested_tab);
  result.error = result.view.error;
  return result;
}

} // namespace reaadr::reaper
