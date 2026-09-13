#include "cue_info_controller.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

namespace reaadr::ui {
namespace {
constexpr const char* kStateNamespace = "ReaADRTools";
constexpr const char* kRememberLayoutKey = "ui.remember_window_layout";
constexpr const char* kWindowWidthKey = "ui.window.cue_info.width";
constexpr const char* kWindowHeightKey = "ui.window.cue_info.height";
constexpr const char* kWindowDockKey = "ui.window.cue_info.dock";
constexpr const char* kWindowXKey = "ui.window.cue_info.x";
constexpr const char* kWindowYKey = "ui.window.cue_info.y";

bool parse_int(const core::StateReadResult& value, int& output)
{
  if (!value || value.value.empty()) return false;
  char* end = nullptr;
  const long parsed = std::strtol(value.value.c_str(), &end, 10);
  if (!end || *end != '\0') return false;
  output = static_cast<int>(parsed);
  return true;
}

} // namespace

double CueInfoController::timeline_position() const
{
  if (!api_.get_play_state || !api_.get_play_position || !api_.get_cursor_position)
    return -1.0;
  const int play_state = api_.get_play_state();
  const double position = (play_state & 1) != 0
    ? api_.get_play_position() : api_.get_cursor_position();
  return std::isfinite(position) ? position : -1.0;
}

double CueInfoController::frame_rate() const
{
  if (!api_.frame_rate) return 24.0;
  const double value = api_.frame_rate();
  return std::isfinite(value) && value > 0.0 ? value : 24.0;
}

bool CueInfoController::remember_window_layout() const
{
  const auto value = project_state_.read(kStateNamespace, kRememberLayoutKey);
  if (!value) return false;
  return value.value == "1" || value.value == "true" || value.value == "yes";
}

CueInfoWindowLayout CueInfoController::load_window_layout() const
{
  CueInfoWindowLayout layout;
  if (!remember_window_layout()) return layout;

  int value = 0;
  if (parse_int(project_state_.read(kStateNamespace, kWindowWidthKey), value) && value > 0)
    layout.width = (std::max)(820, value);
  if (parse_int(project_state_.read(kStateNamespace, kWindowHeightKey), value) && value > 0)
    layout.height = (std::max)(560, value);
  if (parse_int(project_state_.read(kStateNamespace, kWindowDockKey), value))
    layout.dock = value;
  const bool has_x = parse_int(project_state_.read(kStateNamespace, kWindowXKey), layout.x);
  const bool has_y = parse_int(project_state_.read(kStateNamespace, kWindowYKey), layout.y);
  layout.has_position = has_x && has_y;
  return layout;
}

bool CueInfoController::save_window_layout(const CueInfoWindowLayout& layout)
{
  if (!remember_window_layout()) return false;
  return project_state_.write(kStateNamespace, kWindowDockKey, "0") &&
    project_state_.write(kStateNamespace, kWindowXKey, std::to_string(layout.x)) &&
    project_state_.write(kStateNamespace, kWindowYKey, std::to_string(layout.y)) &&
    project_state_.write(kStateNamespace, kWindowWidthKey, std::to_string((std::max)(820, layout.width))) &&
    project_state_.write(kStateNamespace, kWindowHeightKey, std::to_string((std::max)(560, layout.height)));
}

bool CueInfoController::refresh()
{
  error_.clear();
  const double position = timeline_position();
  if (position < 0.0) {
    error_ = "The REAPER Cue Info timeline API is incomplete.";
    return false;
  }
  current_ = info_.load(position, frame_rate());
  if (!current_) {
    error_ = current_.error;
    return false;
  }
  return true;
}

CueInfoEditValues CueInfoController::edit_values() const
{
  CueInfoEditValues values;
  const auto& view = current_.view;
  values.cue_key = view.cue_key;
  values.character = view.character;
  values.status = view.status;
  values.cue_type = view.cue_type;
  values.direction = view.direction;
  values.start_time = view.start_timecode;
  values.end_time = view.end_timecode;
  values.dialogue = view.dialogue;
  values.notes = view.notes;
  return values;
}

std::vector<std::string> CueInfoController::character_choices() const
{
  const core::SessionLoadResult loaded = sessions_.load();
  if (!loaded) return {};

  std::set<std::string> unique;
  for (const auto& cue : loaded.model.cues) {
    const auto found = cue.find("character");
    if (found != cue.end() && !found->second.empty()) unique.insert(found->second);
  }
  return {unique.begin(), unique.end()};
}

bool CueInfoController::save(const CueInfoEditValues& values)
{
  error_.clear();
  if (!current_ || current_.view.cue_key.empty()) {
    error_ = "No active ADR cue is available to edit.";
    return false;
  }

  core::CueManagerEditOptions edit;
  edit.cue_key = current_.view.cue_key;
  edit.new_cue_key = values.cue_key;
  edit.new_character = values.character;
  edit.status = values.status;
  edit.cue_type = values.cue_type;
  edit.direction = values.direction;
  edit.direction_set = true;
  edit.start_time = values.start_time;
  edit.end_time = values.end_time;
  edit.dialogue = values.dialogue;
  edit.dialogue_set = true;
  edit.notes = values.notes;
  edit.notes_set = true;
  edit.input_frame_rate = frame_rate();

  const auto saved = mutations_.edit(edit);
  if (!saved) {
    error_ = saved.error;
    return false;
  }
  return refresh();
}

bool CueInfoController::navigate_relative(int delta)
{
  error_.clear();
  if (!current_ || current_.target.visible_cues.empty()) {
    error_ = "No active ADR cues are available for navigation.";
    return false;
  }

  const auto& cues = current_.target.visible_cues;
  std::size_t index = 0;
  bool found = false;
  for (std::size_t i = 0; i < cues.size(); ++i) {
    if (cues[i].cue_key == current_.view.cue_key) {
      index = i;
      found = true;
      break;
    }
  }
  if (!found) {
    error_ = "The active cue is no longer present in the filtered cue list.";
    return false;
  }

  const auto preference_result = preferences_.load();
  if (!preference_result) {
    error_ = preference_result.error;
    return false;
  }

  long next_index = static_cast<long>(index) + static_cast<long>(delta);
  const long count = static_cast<long>(cues.size());
  if (preference_result.preferences.navigation_wrap && count > 0) {
    while (next_index < 0) next_index += count;
    while (next_index >= count) next_index -= count;
  } else {
    next_index = (std::max)(0L, (std::min)(next_index, count - 1));
  }

  return jump_to_id(cues[static_cast<std::size_t>(next_index)].cue_key);
}

bool CueInfoController::previous()
{
  return navigate_relative(-1);
}

bool CueInfoController::next()
{
  return navigate_relative(1);
}

bool CueInfoController::jump_to_id(const std::string& cue_id)
{
  error_.clear();
  reaper::CueNavigationService navigation(
    sessions_, selections_, navigation_api_, refresh_overlay_);
  const auto moved = navigation.navigate_to_id(cue_id);
  if (!moved) {
    error_ = moved.error;
    return false;
  }
  return refresh();
}

} // namespace reaadr::ui
