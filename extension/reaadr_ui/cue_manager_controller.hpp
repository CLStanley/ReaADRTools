#pragma once

#include "manager_view_model.hpp"
#include "app/cue_manager_application_service.hpp"
#include "app/manager_view_application_service.hpp"
#include "reaadr_reaper/cue_navigation_service.hpp"

#include <functional>

namespace reaadr::ui {

class CueManagerController {
public:
  CueManagerController(reaper::ManagerViewApplicationService& service,
                       reaper::CueManagerMutationService& mutations,
                       core::ProjectStateStore& project_state,
                       reaper::CueNavigationApi navigation_api,
                       std::function<void(const std::string&, bool, const std::string&, const std::string&)> trigger_import = {},
                       std::function<void(const std::string&)> trigger_action = {});
  bool reload();
  bool set_tab(const std::string& tab);
  void trigger_import(const std::string& mapping = {}, bool preview = false,
                      const std::string& mode = "all", const std::string& characters = {});
  std::string last_import_mapping() const;
  std::string session_characters_csv() const;
  void clear_import_mapping();
  void trigger_action(const std::string& action);
  bool set_filters(const std::string& query,
                   const std::string& character,
                   const std::string& status);
  void select_index(int index);
  void select_boundary(bool last);
  void select_relative(int delta);
  bool navigate_next();
  bool navigate_previous();
  bool navigate_to_id(const std::string& cue_id, std::string& error);
  bool edit_selected(const core::CueManagerEditOptions& edit, std::string& error);
  core::CueManagerAddOptions default_add_options() const;
  bool add_cue(const core::CueManagerAddOptions& cue, std::string& error);
  bool remove_selected(std::string& error);
  const core::CueManagerRow* selected_row() const;
  const core::ManagerViewModel& view() const { return view_; }

private:
  reaper::ManagerViewApplicationService& service_;
  reaper::CueManagerMutationService& mutations_;
  core::ProjectStateStore& project_state_;
  reaper::CueNavigationApi navigation_api_;
  core::CueManagerViewOptions options_;
  core::ManagerViewModel view_;
  std::string selected_key_;
  std::string requested_tab_ = "cues";
  std::function<void(const std::string&, bool, const std::string&, const std::string&)> trigger_import_;
  std::function<void(const std::string&)> trigger_action_;
};

} // namespace reaadr::ui
