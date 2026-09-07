#include "cue_manager_controller.hpp"

#include <iomanip>
#include <set>
#include <sstream>
#include <utility>

namespace reaadr::ui {

CueManagerController::CueManagerController(reaper::ManagerViewApplicationService& service,
                                           reaper::CueManagerMutationService& mutations,
                                           core::ProjectStateStore& project_state,
                                           reaper::CueNavigationApi navigation_api,
                                           std::function<void(const std::string&, bool, const std::string&, const std::string&)> trigger_import,
                                           std::function<void(const std::string&)> trigger_action)
  : service_(service), mutations_(mutations), project_state_(project_state),
    navigation_api_(navigation_api), trigger_import_(std::move(trigger_import)),
    trigger_action_(std::move(trigger_action)) {}

bool CueManagerController::reload()
{
  options_.selected_cue_key = selected_key_;
  const auto loaded = service_.load(options_, requested_tab_);
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

void CueManagerController::trigger_import(const std::string& mapping, bool preview,
                                          const std::string& mode, const std::string& characters)
{
  if (trigger_import_) trigger_import_(mapping, preview, mode, characters);
}

std::string CueManagerController::last_import_mapping() const
{
  const auto value = project_state_.read(core::SessionModelRepository::kNamespace,
                                         "import_mapping_last");
  return value ? value.value : std::string();
}

std::string CueManagerController::session_characters_csv() const
{
  std::set<std::string> names;
  for (const auto& row : view_.cues.rows) if (!row.character.empty()) names.insert(row.character);
  std::string result;
  for (const auto& name : names) {
    if (!result.empty()) result += ';';
    result += name;
  }
  return result;
}

void CueManagerController::clear_import_mapping()
{
  project_state_.write(core::SessionModelRepository::kNamespace, "import_mapping_last", {});
}

void CueManagerController::trigger_action(const std::string& action)
{
  if (trigger_action_) trigger_action_(action);
}

bool CueManagerController::set_tab(const std::string& tab)
{
  requested_tab_ = core::normalize_manager_tab(tab);
  return reload();
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

void CueManagerController::select_boundary(bool last)
{
  if (view_.cues.rows.empty()) return;
  select_index(last ? static_cast<int>(view_.cues.rows.size() - 1) : 0);
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

core::CueManagerAddOptions CueManagerController::default_add_options() const
{
  core::CueManagerAddOptions options;
  core::SessionModelRepository sessions(project_state_);
  const core::SessionLoadResult loaded = sessions.load();
  options.cue_key = loaded ? core::next_cue_manager_id(loaded.model)
                           : std::to_string(view_.total_cues + 1);

  double position = 0.0;
  const int play_state = navigation_api_.get_play_state ? navigation_api_.get_play_state() : 0;
  if ((play_state & 1) != 0 && navigation_api_.get_play_position)
    position = navigation_api_.get_play_position();
  else if (navigation_api_.get_cursor_position)
    position = navigation_api_.get_cursor_position();
  std::ostringstream start;
  std::ostringstream end;
  start << std::fixed << std::setprecision(3) << position;
  end << std::fixed << std::setprecision(3) << position + 2.0;
  options.start_time = start.str();
  options.end_time = end.str();
  return options;
}

bool CueManagerController::add_cue(const core::CueManagerAddOptions& cue, std::string& error)
{
  const auto result = mutations_.add(cue);
  if (!result) { error = result.error; return false; }
  selected_key_ = result.mutation.selected_cue_key;
  options_.query.clear();
  options_.character.clear();
  options_.status.clear();
  return reload();
}

bool CueManagerController::remove_selected(std::string& error)
{
  if (selected_key_.empty()) {
    error = "Select a cue before removing it.";
    return false;
  }
  const auto result = mutations_.remove(selected_key_);
  if (!result) { error = result.error; return false; }
  selected_key_ = result.mutation.selected_cue_key;
  return reload();
}

const core::CueManagerRow* CueManagerController::selected_row() const
{
  for (const auto& row : view_.cues.rows)
    if (row.selected) return &row;
  return nullptr;
}

} // namespace reaadr::ui
