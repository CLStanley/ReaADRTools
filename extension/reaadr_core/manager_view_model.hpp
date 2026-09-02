#pragma once

#include "cue_manager_model.hpp"
#include "manager_preferences.hpp"
#include "manager_navigation.hpp"

namespace reaadr::core {

// Immutable payload for the graphical Manager: persisted UI state and the
// filtered canonical cue list are built together for one render pass.
struct ManagerViewModel {
  ManagerPreferences preferences;
  CueManagerModel cues;
  std::string session_name;
  std::string revision;
  std::string active_tab;
  std::size_t total_cues = 0;
  std::string error;
  explicit operator bool() const { return error.empty() && static_cast<bool>(cues); }
};

ManagerViewModel build_manager_view_model(const SessionModel& model,
                                          const ManagerPreferences& preferences,
                                          const CueManagerViewOptions& options,
                                          const std::string& revision = {},
                                          const std::string& requested_tab = {});

} // namespace reaadr::core
