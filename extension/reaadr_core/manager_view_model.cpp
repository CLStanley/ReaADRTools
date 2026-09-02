#include "manager_view_model.hpp"

namespace reaadr::core {

ManagerViewModel build_manager_view_model(const SessionModel& model,
                                          const ManagerPreferences& preferences,
                                          const CueManagerViewOptions& options,
                                          const std::string& revision)
{
  ManagerViewModel result;
  result.preferences = preferences;
  result.cues = build_cue_manager_view(model, options);
  result.revision = revision;
  const auto session_name = model.session.find("session_name");
  if (session_name != model.session.end()) result.session_name = session_name->second;
  if (!result.cues) result.error = result.cues.error;
  return result;
}

} // namespace reaadr::core
