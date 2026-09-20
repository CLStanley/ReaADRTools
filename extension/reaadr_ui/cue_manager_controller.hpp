#pragma once

#include "manager_view_model.hpp"
#include "app/cue_manager_application_service.hpp"
#include "app/manager_view_application_service.hpp"
#include "reaadr_core/character_filter.hpp"
#include "reaadr_core/window_layout.hpp"
#include "reaadr_reaper/cue_navigation_service.hpp"

#include <functional>
#include <utility>
#include <vector>

namespace reaadr::ui {

class CueManagerController {
public:
  CueManagerController(reaper::ManagerViewApplicationService& service,
                       reaper::CueManagerMutationService& mutations,
                       core::ProjectStateStore& project_state,
                       reaper::CueNavigationApi navigation_api,
                       std::function<void(const std::string&, bool, const std::string&, const std::string&)> trigger_import = {},
                       std::function<void(const std::string&)> trigger_action = {},
                       std::function<bool(std::string*)> refresh_overlay = {});
  bool reload();
  // Cheap polling boundary for modeless windows. Reads only the canonical
  // session revision and rebuilds the Manager view when another workflow has
  // changed the project since the last successful load.
  bool reload_if_revision_changed(bool& changed);
  bool set_tab(const std::string& tab);
  void trigger_import(const std::string& mapping = {}, bool preview = false,
                      const std::string& mode = "all", const std::string& characters = {});
  std::string last_import_mapping() const;
  void trigger_action(const std::string& action);
  void set_external_error(std::string error) { view_.error = std::move(error); }
  bool set_filters(const std::string& query,
                   const std::string& character,
                   const std::string& status);

  core::WindowLayout load_window_layout() const;
  bool save_window_layout(const core::WindowLayout& layout);

  // Native project filter used by the Cue Manager's grouped character/lane UI.
  core::CharacterFilterCatalogResult character_filter_catalog() const;
  core::CharacterFilterLoadResult character_filter_state() const;
  bool apply_character_filter(const std::vector<std::string>& tokens,
                              bool hide_inactive_regions,
                              std::string& error);
  bool show_all_character_filter(bool hide_inactive_regions, std::string& error);
  bool toggle_character_filter_group(const std::string& character,
                                     bool hide_inactive_regions,
                                     std::string& error);
  bool toggle_character_filter_target(const std::string& target_key,
                                      bool hide_inactive_regions,
                                      std::string& error);

  // Header clicks toggle direction while selection follows the canonical cue key.
  bool sort_by(const std::string& key);
  bool select_index(int index);
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
  bool navigate_displayed_row(bool next);
  std::function<bool(std::string*)> refresh_overlay_;
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
