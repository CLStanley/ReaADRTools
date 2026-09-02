#include "manager_preferences.hpp"
#include <array>
namespace reaadr::core {

namespace {
struct UiFlag { const char* key; bool ManagerPreferences::*member; };
constexpr std::array<UiFlag, 5> kUiFlags = {{
  {"ui.remember_window_layout", &ManagerPreferences::remember_layout},
  {"ui.cue_hover_preview", &ManagerPreferences::hover_preview},
  {"ui.tooltips_enabled", &ManagerPreferences::tooltips},
  {"ui.navigation_wrap_enabled", &ManagerPreferences::navigation_wrap},
  {"ui.cue_manager_auto_dock", &ManagerPreferences::cue_manager_auto_dock},
}};
bool truthy(const std::string& value)
{
  return value == "1" || value == "true" || value == "yes";
}
}

ManagerPreferencesLoadResult ManagerPreferencesRepository::load() const
{
  ManagerPreferencesLoadResult result;
  OverlaySettingsRepository overlays(store_);
  const auto overlay = overlays.load();
  if (!overlay) { result.error = overlay.error; return result; }
  result.preferences.overlay = overlay.settings;
  for (const UiFlag& flag : kUiFlags) {
    const auto stored = store_.read(SessionModelRepository::kNamespace, flag.key);
    if (stored.error == StateReadError::not_found) continue;
    if (!stored) { result.error = "REAPER project extstate is unavailable while loading Manager preferences."; return result; }
    result.preferences.*(flag.member) = truthy(stored.value);
  }
  return result;
}

ManagerPreferencesSaveResult ManagerPreferencesRepository::save(const ManagerPreferences& preferences)
{
  ManagerPreferencesSaveResult result;
  const auto loaded = load();
  if (!loaded) { result.error = loaded.error; return result; }
  if (loaded.preferences == preferences) return result;
  OverlaySettingsRepository overlays(store_);
  std::array<std::pair<std::string, std::string>, kUiFlags.size()> written{};
  std::size_t written_count = 0;
  for (const UiFlag& flag : kUiFlags) {
    const std::string value = preferences.*(flag.member) ? "1" : "0";
    const auto previous = store_.read(SessionModelRepository::kNamespace, flag.key);
    if (!previous && previous.error != StateReadError::not_found) {
      result.error = "REAPER project extstate is unavailable while saving Manager preferences.";
      break;
    }
    if (previous && previous.value == value) continue;
    if (!store_.write(SessionModelRepository::kNamespace, flag.key, value)) {
      result.error = "Could not persist all Manager preferences.";
      break;
    }
    written[written_count++] = {flag.key, previous ? previous.value : std::string()};
  }
  if (!result.error.empty()) {
    for (std::size_t i = written_count; i > 0; --i) {
      const auto& entry = written[i - 1];
      store_.write(SessionModelRepository::kNamespace, entry.first.c_str(), entry.second);
    }
    return result;
  }
  // Persist overlay keys last. The overlay repository has its own rollback;
  // keeping it last means a failure here leaves the UI flags already restored.
  const auto overlay_saved = overlays.save(preferences.overlay);
  if (!overlay_saved) {
    for (std::size_t i = written_count; i > 0; --i) {
      const auto& entry = written[i - 1];
      store_.write(SessionModelRepository::kNamespace, entry.first.c_str(), entry.second);
    }
    result.error = overlay_saved.error;
    return result;
  }
  result.changed = overlay_saved.changed || written_count != 0;
  return result;
}

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
