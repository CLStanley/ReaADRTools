#pragma once

#include "manager_view_model.hpp"
#include "app/manager_view_application_service.hpp"
#include "reaadr_reaper/cue_navigation_service.hpp"
#include "reaadr_core/cue_status.hpp"

namespace reaadr::ui {

class CueManagerController {
public:
  CueManagerController(reaper::ManagerViewApplicationService& service,
                       core::ProjectStateStore& project_state,
                       reaper::CueNavigationApi navigation_api);
  bool reload();
  bool set_filters(const std::string& query,
                   const std::string& character,
                   const std::string& status);
  void select_index(int index);
  void select_relative(int delta);
  bool navigate_next();
  bool navigate_previous();
  bool navigate_to_id(const std::string& cue_id, std::string& error);
  bool edit_selected(const core::CueManagerEditOptions& edit, std::string& error);
  bool set_selected_status(const std::string& status, std::string& error);
  const core::CueManagerRow* selected_row() const;
  const core::ManagerViewModel& view() const { return view_; }

private:
  reaper::ManagerViewApplicationService& service_;
  core::ProjectStateStore& project_state_;
  reaper::CueNavigationApi navigation_api_;
  core::CueManagerViewOptions options_;
  core::ManagerViewModel view_;
  std::string selected_key_;
};

} // namespace reaadr::ui
