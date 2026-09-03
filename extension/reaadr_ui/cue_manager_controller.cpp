#include "cue_manager_controller.hpp"

namespace reaadr::ui {

CueManagerController::CueManagerController(reaper::ManagerViewApplicationService& service)
  : service_(service) {}

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

bool CueManagerController::set_character_filter(const std::string& character)
{
  options_.character = character;
  return reload();
}

void CueManagerController::select_index(int index)
{
  if (index < 0 || static_cast<std::size_t>(index) >= view_.cues.rows.size()) return;
  selected_key_ = view_.cues.rows[static_cast<std::size_t>(index)].cue_key;
  view_.cues.selected_cue_key = selected_key_;
  for (std::size_t i = 0; i < view_.cues.rows.size(); ++i) view_.cues.rows[i].selected = static_cast<int>(i) == index;
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

} // namespace reaadr::ui
