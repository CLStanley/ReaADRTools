#pragma once

#include "reaadr_core/manager_view_model.hpp"
#include "reaadr_core/model_repository.hpp"
#include "reaadr_core/manager_preferences.hpp"

namespace reaadr::reaper {

struct ManagerViewLoadResult {
  core::ManagerViewModel view;
  std::string error;
  explicit operator bool() const { return error.empty() && static_cast<bool>(view); }
};

class ManagerViewApplicationService {
public:
  ManagerViewApplicationService(core::ProjectStateStore& project_state,
                                core::GlobalStateStore* global_state = nullptr)
    : project_state_(project_state), global_state_(global_state) {}

  ManagerViewLoadResult load(const core::CueManagerViewOptions& options,
                             const std::string& requested_tab = {}) const;

private:
  core::ProjectStateStore& project_state_;
  core::GlobalStateStore* global_state_ = nullptr;
};

} // namespace reaadr::reaper
