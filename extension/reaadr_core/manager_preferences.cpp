#include "manager_preferences.hpp"
namespace reaadr::core {
ManagerPreferencesResult update_manager_preferences(const ManagerPreferences& current,
                                                    const std::string& key,
                                                    const std::string& value)
{
  ManagerPreferencesResult result{current, false, {}};
  if (key == "overlay_profile") {
    result.changed = apply_overlay_profile(result.preferences.overlay, value);
    if (!result.changed) result.error = "Unknown overlay profile: " + value;
    return result;
  }
  if (key.rfind("quick_action_", 0) == 0 && key.size() == 14 && key.back() >= '1' && key.back() <= '4') {
    result.preferences.quick_actions[static_cast<std::size_t>(key.back() - '1')] = value;
    result.changed = true;
    return result;
  }
  bool* flag = nullptr;
  if (key == "remember_layout") flag = &result.preferences.remember_layout;
  else if (key == "hover_preview") flag = &result.preferences.hover_preview;
  else if (key == "tooltips") flag = &result.preferences.tooltips;
  else if (key == "navigation_wrap") flag = &result.preferences.navigation_wrap;
  else if (key == "cue_manager_auto_dock") flag = &result.preferences.cue_manager_auto_dock;
  if (!flag) { result.error = "Unknown Manager preference: " + key; return result; }
  if (value != "0" && value != "1") { result.error = "Manager preference flags must be 0 or 1."; return result; }
  const bool next = value == "1";
  result.changed = *flag != next;
  *flag = next;
  return result;
}
}
