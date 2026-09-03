#include "cue_manager_controller.hpp"

namespace reaadr::ui {

CueManagerController::CueManagerController(reaper::ManagerViewApplicationService& service,
                                           reaper::CueManagerMutationService& mutations,
                                           core::ProjectStateStore& project_state,
                                           reaper::CueNavigationApi navigation_api)
  : service_(service), mutations_(mutations), project_state_(project_state),
    navigation_api_(navigation_api) {}

bool CueManagerController::reload()
{
  options_.selected_cue_key = selected_key_;
  const auto loaded = service_.load(options_, "cues");
  if (!loaded) { view_.error = loaded.error; return false; }
  view_ = loaded.view;
  if (view_.cues.rows.empty()) { selected_key_.clear(); view_.cues.selected_cue_key.clear(); return true; }
  std::size_t selected = 0;
  for (std::size_t i = 0; i < view_.cues.rows.size(); ++i)
    if (view_.cues.rows[i].selected || view_.cues.rows[i].cue_key == selected_key_) { selected = i; break; }
  selected_key_ = view_.cues.rows[selected].cue_key;
  view_.cues.selected_cue_key = selected_key_;
  for (std::size_t i = 0; i < view_.cues.rows.size(); ++i) view_.cues.rows[i].selected = i == selected;
  return true;
}

bool CueManagerController::set_filters(const std::string& query,
                                       const std::string& character,
                                       const std::string& status)
{
  options_.query = query;
  options_.character = character;
  options_.status = status;
  return reload();
}

void CueManagerController::select_index(int index)
{
  if (index < 0 || static_cast<std::size_t>(index) >= view_.cues.rows.size()) return;
  selected_key_ = view_.cues.rows[static_cast<std::size_t>(index)].cue_key;
  view_.cues.selected_cue_key = selected_key_;
  for (std::size_t i = 0; i < view_.cues.rows.size(); ++i) view_.cues.rows[i].selected = static_cast<int>(i) == index;
  project_state_.write(core::SessionModelRepository::kNamespace,
                       "manager_selected_cue_key", selected_key_);
}

void CueManagerController::select_relative(int delta)
{
  if (view_.cues.rows.empty()) return;
  int index = 0;
  for (std::size_t i = 0; i < view_.cues.rows.size(); ++i)
    if (view_.cues.rows[i].selected) { index = static_cast<int>(i); break; }
  index = (index + delta + static_cast<int>(view_.cues.rows.size())) % static_cast<int>(view_.cues.rows.size());
  select_index(index);
}

bool CueManagerController::navigate_next()
{
  if (!view_.preferences.navigation_wrap && !view_.cues.rows.empty() &&
      view_.cues.rows.back().selected) return false;
  core::SessionModelRepository sessions(project_state_);
  core::CueSelectionRepository selections(project_state_);
  reaper::CueNavigationService navigation(sessions, selections, navigation_api_);
  const auto result = navigation.navigate_next();
  if (!result) return false;
  selected_key_ = result.cue.cue_key;
  return reload();
}

bool CueManagerController::navigate_previous()
{
  if (!view_.preferences.navigation_wrap && !view_.cues.rows.empty() &&
      view_.cues.rows.front().selected) return false;
  core::SessionModelRepository sessions(project_state_);
  core::CueSelectionRepository selections(project_state_);
  reaper::CueNavigationService navigation(sessions, selections, navigation_api_);
  const auto result = navigation.navigate_previous();
  if (!result) return false;
  selected_key_ = result.cue.cue_key;
  return reload();
}

bool CueManagerController::navigate_to_id(const std::string& cue_id, std::string& error)
{
  core::SessionModelRepository sessions(project_state_);
  core::CueSelectionRepository selections(project_state_);
  reaper::CueNavigationService navigation(sessions, selections, navigation_api_);
  const auto result = navigation.navigate_to_id(cue_id);
  if (!result) { error = result.error; return false; }
  selected_key_ = result.cue.cue_key;
  // An explicit jump must reveal its target even if the previous filter would
  // otherwise hide it from the list.
  options_.query.clear();
  options_.character.clear();
  options_.status.clear();
  return reload();
}

bool CueManagerController::edit_selected(const core::CueManagerEditOptions& edit, std::string& error)
{
  core::CueManagerEditOptions options = edit;
  options.cue_key = selected_key_;
  const auto result = mutations_.edit(options);
  if (!result) { error = result.error; return false; }
  if (!options.new_cue_key.empty()) selected_key_ = options.new_cue_key;
  project_state_.write(core::SessionModelRepository::kNamespace,
                       "manager_selected_cue_key", selected_key_);
  return reload();
}

const core::CueManagerRow* CueManagerController::selected_row() const
{
  for (const auto& row : view_.cues.rows)
    if (row.selected) return &row;
  return nullptr;
}

} // namespace reaadr::ui
