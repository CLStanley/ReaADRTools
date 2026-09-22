#include "cue_manager_controller.hpp"
#include "cue_manager_ui_contract.hpp"
#include "character_filter_window.hpp"
#include "reaadr_reaper/character_filter_command.hpp"
#include "reaadr_reaper/cue_info_command.hpp"
#include "reaadr_reaper/dialogue_detection_command.hpp"
#include "reaadr_reaper/marker_cue_generation_command.hpp"
#include "reaadr_reaper/recording_command.hpp"

#include <iomanip>
#include <sstream>
#include <utility>

namespace reaadr::ui {
namespace {
constexpr const char* kManagerLaunchTabKey = "ui.manager.launch_tab";
}

CueManagerController::CueManagerController(reaper::ManagerViewApplicationService& service,
                                           reaper::CueManagerMutationService& mutations,
                                           core::ProjectStateStore& project_state,
                                           reaper::CueNavigationApi navigation_api,
                                           std::function<void(const std::string&, bool, const std::string&, const std::string&)> trigger_import,
                                           std::function<void(const std::string&)> trigger_action,
                                           std::function<bool(std::string*)> refresh_overlay)
  : refresh_overlay_(std::move(refresh_overlay)), service_(service), mutations_(mutations), project_state_(project_state),
    navigation_api_(navigation_api), trigger_import_(std::move(trigger_import)),
    trigger_action_(std::move(trigger_action)) {}

bool CueManagerController::reload()
{
  const auto launch_tab = project_state_.read(
    core::SessionModelRepository::kNamespace, kManagerLaunchTabKey);
  if (launch_tab && !launch_tab.value.empty() && core::is_manager_tab(launch_tab.value)) {
    if (!project_state_.write(core::SessionModelRepository::kNamespace,
                              kManagerLaunchTabKey, "")) {
      view_.error = "Could not consume the pending Manager launch tab.";
      return false;
    }
    requested_tab_ = launch_tab.value;
  }

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

bool CueManagerController::reload_if_revision_changed(bool& changed)
{
  changed = false;
  core::SessionModelRepository sessions(project_state_);
  const auto revision = sessions.revision();
  if (!revision) {
    view_.error = revision.error;
    return false;
  }
  if (view_.revision == std::to_string(revision.revision)) return true;
  changed = true;
  return reload();
}

void CueManagerController::trigger_import(const std::string& mapping, bool preview,
                                          const std::string& mode, const std::string& characters)
{
  view_.error.clear();
  if (!trigger_import_) {
    view_.error = "The native cue-sheet import workflow is unavailable.";
    return;
  }
  trigger_import_(mapping, preview, mode, characters);
  // Preview is intentionally non-mutating. A completed import may replace the
  // canonical session, so consume that new state immediately instead of waiting
  // for the modeless Manager revision poll.
  if (!preview) reload();
}

std::string CueManagerController::last_import_mapping() const
{
  const auto value = project_state_.read(core::SessionModelRepository::kNamespace,
                                         "import_mapping_last");
  return value ? value.value : std::string();
}

void CueManagerController::trigger_action(const std::string& action)
{
  // Action errors belong to the current invocation. Without clearing a prior
  // failure, modeless Manager controls can keep reporting a stale error after
  // a later native action succeeds.
  view_.error.clear();
  if (action == "adopt_legacy_project") {
    const auto adopted = reaper::run_legacy_project_adoption_command();
    if (!adopted) view_.error = adopted.error;
    else if (!adopted.cancelled) reload();
    return;
  }
  if (action == "generate_cues") {
    const auto generated = reaper::run_marker_cue_generation_command();
    if (!generated) view_.error = generated.error;
    else if (!generated.cancelled) reload();
    return;
  }
  if (action == "detect_dialogue") {
    const auto detected = reaper::run_dialogue_detection_command();
    if (!detected) view_.error = detected.error;
    else if (!detected.cancelled) reload();
    return;
  }
  if (action == "cue_info") {
    if (!reaper::run_native_cue_info_command())
      view_.error = "The native Cue Info window could not be opened.";
    else
      reload();
    return;
  }
  if (action == "record_cue") {
    if (!reaper::run_native_record_cue_command())
      view_.error = "The native Record Cue workflow could not be opened.";
    else
      reload();
    return;
  }
  if (action == "character_filter") {
    if (show_character_filter_window(*this)) reload();
    return;
  }
  if (action == "refresh_session") {
    reload();
    return;
  }
  if (action == "refresh_overlay") {
    if (!refresh_overlay_) {
      view_.error = "The native overlay refresh workflow is unavailable.";
      return;
    }
    std::string error;
    if (!refresh_overlay_(&error)) {
      view_.error = error.empty() ? "The native overlay could not be refreshed." : error;
      return;
    }
    reload();
    return;
  }
  if (action == "preferences") {
    requested_tab_ = "preferences";
    reload();
    return;
  }
  if (trigger_action_) {
    trigger_action_(action);
    return;
  }
  view_.error = "The requested native Cue Manager action is unavailable: " + action;
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

core::WindowLayout CueManagerController::load_window_layout() const
{
  core::WindowLayoutRepository layouts(
    const_cast<core::ProjectStateStore&>(project_state_), "cue_manager", 1180, 820);
  const auto loaded = layouts.load();
  core::WindowLayout layout = loaded ? loaded.layout
                                     : core::WindowLayout{1180, 820, -1, 0, 0, false};
  if (loaded && !loaded.remembered && view_.preferences.cue_manager_auto_dock)
    layout.dock = 0;
  return layout;
}

bool CueManagerController::save_window_layout(const core::WindowLayout& layout)
{
  core::WindowLayoutRepository layouts(project_state_, "cue_manager", 1180, 820);
  return layouts.save(layout);
}

core::CharacterFilterCatalogResult CueManagerController::character_filter_catalog() const
{
  return reaper::load_native_character_filter_catalog();
}

core::CharacterFilterLoadResult CueManagerController::character_filter_state() const
{
  core::CharacterFilterRepository filters(project_state_);
  return filters.load();
}

bool CueManagerController::apply_character_filter(const std::vector<std::string>& tokens,
                                                  bool hide_inactive_regions,
                                                  std::string& error)
{
  error.clear();
  const auto result = reaper::apply_native_character_filter_tokens(tokens, hide_inactive_regions);
  if (!result) {
    error = result.error;
    view_.error = result.error;
    return false;
  }
  if (refresh_overlay_) {
    std::string overlay_error;
    if (!refresh_overlay_(&overlay_error)) {
      error = overlay_error.empty() ? "The character filter was applied, but the overlay could not be refreshed."
                                    : overlay_error;
      view_.error = error;
      return false;
    }
  }
  return reload();
}

bool CueManagerController::show_all_character_filter(bool hide_inactive_regions,
                                                     std::string& error)
{
  return apply_character_filter({}, hide_inactive_regions, error);
}

bool CueManagerController::toggle_character_filter_group(const std::string& character,
                                                         bool hide_inactive_regions,
                                                         std::string& error)
{
  const auto catalog = character_filter_catalog();
  if (!catalog) {
    error = catalog.error;
    view_.error = error;
    return false;
  }
  return apply_character_filter(
    core::toggle_character_filter_group(catalog, character), hide_inactive_regions, error);
}

bool CueManagerController::toggle_character_filter_target(const std::string& target_key,
                                                          bool hide_inactive_regions,
                                                          std::string& error)
{
  const auto catalog = character_filter_catalog();
  if (!catalog) {
    error = catalog.error;
    view_.error = error;
    return false;
  }
  return apply_character_filter(
    core::toggle_character_filter_target(catalog, target_key), hide_inactive_regions, error);
}

bool CueManagerController::sort_by(const std::string& key)
{
  if (!core::is_cue_manager_sort_key(key)) return false;
  options_.sort_ascending = options_.sort_key == key ? !options_.sort_ascending : true;
  options_.sort_key = key;
  return reload();
}

bool CueManagerController::select_index(int index)
{
  if (index < 0 || static_cast<std::size_t>(index) >= view_.cues.rows.size()) return false;
  const std::string key = view_.cues.rows[static_cast<std::size_t>(index)].cue_key;
  if (!reaper::select_manager_cue(project_state_, key, refresh_overlay_, view_.error)) return false;
  selected_key_ = key;
  view_.cues.selected_cue_key = key;
  for (std::size_t i = 0; i < view_.cues.rows.size(); ++i)
    view_.cues.rows[i].selected = static_cast<int>(i) == index;
  return true;
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
  return navigate_displayed_row(true);
}

bool CueManagerController::navigate_previous()
{
  return navigate_displayed_row(false);
}

bool CueManagerController::navigate_displayed_row(bool next)
{
  const auto* target = core::adjacent_cue_manager_row(view_.cues, next);
  if (!target) return false;
  const std::string target_key = target->cue_key;
  core::SessionModelRepository sessions(project_state_);
  core::CueSelectionRepository selections(project_state_);
  reaper::CueNavigationService navigation(sessions, selections, navigation_api_, refresh_overlay_);
  const auto result = navigation.navigate_to_id(target_key);
  if (!result) { view_.error = result.error; return false; }
  selected_key_ = result.cue.cue_key;
  return reload();
}

bool CueManagerController::navigate_to_id(const std::string& cue_id, std::string& error)
{
  core::SessionModelRepository sessions(project_state_);
  core::CueSelectionRepository selections(project_state_);
  reaper::CueNavigationService navigation(sessions, selections, navigation_api_, refresh_overlay_);
  error.clear();
  const auto result = navigation.navigate_to_id(cue_id);
  if (!result) { error = result.error; return false; }
  selected_key_ = result.cue.cue_key;
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
