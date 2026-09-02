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

const std::vector<ManagerPreferenceField>& manager_preference_fields()
{
  static const std::vector<ManagerPreferenceField> fields = {
    {"enabled", "Enable video overlays", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_cue_id", "Cue ID", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_character", "Character name", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_dialogue", "Dialogue line", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_cue_timecode", "Cue timecode", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_project_timer", "Live project timer", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_visual_cue", "Visual cue indicator", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_direction", "Notes", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_cue_type", "Cue type", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_streamer", "Streamer bar", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_flash", "Flash at cue start", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_status", "Standby / take / clear status", "overlay", ManagerPreferenceFieldType::checkbox},
    {"show_metadata", "Studio metadata", "overlay", ManagerPreferenceFieldType::checkbox},
    {"text_color", "Overlay text color", "overlay", ManagerPreferenceFieldType::choice},
    {"metadata_fields", "Studio metadata fields", "overlay", ManagerPreferenceFieldType::text},
    {"preroll_seconds", "Overlay preroll seconds", "overlay", ManagerPreferenceFieldType::number},
    {"remember_layout", "Remember window layout", "preferences", ManagerPreferenceFieldType::checkbox},
    {"hover_preview", "Cue hover preview", "preferences", ManagerPreferenceFieldType::checkbox},
    {"tooltips", "Show tooltips", "preferences", ManagerPreferenceFieldType::checkbox},
    {"navigation_wrap", "Wrap cue navigation", "preferences", ManagerPreferenceFieldType::checkbox},
    {"cue_manager_auto_dock", "Auto-dock Cue Manager", "preferences", ManagerPreferenceFieldType::checkbox},
  };
  return fields;
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
  if (global_) {
    for (std::size_t i = 0; i < result.preferences.quick_actions.size(); ++i) {
      const std::string key = "quick_action_" + std::to_string(i + 1);
      const std::string value = global_->read(SessionModelRepository::kNamespace, key.c_str());
      if (!value.empty()) result.preferences.quick_actions[i] = value;
    }
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
  std::vector<std::pair<std::string, std::string>> global_written;
  if (global_) {
    for (std::size_t i = 0; i < preferences.quick_actions.size(); ++i) {
      const std::string key = "quick_action_" + std::to_string(i + 1);
      const std::string previous = global_->read(SessionModelRepository::kNamespace, key.c_str());
      if (previous == preferences.quick_actions[i]) continue;
      if (!global_->write(SessionModelRepository::kNamespace, key.c_str(), preferences.quick_actions[i])) {
        result.error = "Could not persist all Manager quick actions.";
        for (auto it = global_written.rbegin(); it != global_written.rend(); ++it)
          global_->write(SessionModelRepository::kNamespace, it->first.c_str(), it->second);
        for (std::size_t i = written_count; i > 0; --i) {
          const auto& entry = written[i - 1];
          store_.write(SessionModelRepository::kNamespace, entry.first.c_str(), entry.second);
        }
        return result;
      }
      global_written.emplace_back(key, previous);
    }
  }
  // Persist overlay keys last; its repository has its own rollback.
  const auto overlay_saved = overlays.save(preferences.overlay);
  if (!overlay_saved) {
    for (auto it = global_written.rbegin(); it != global_written.rend(); ++it)
      global_->write(SessionModelRepository::kNamespace, it->first.c_str(), it->second);
    for (std::size_t i = written_count; i > 0; --i) {
      const auto& entry = written[i - 1];
      store_.write(SessionModelRepository::kNamespace, entry.first.c_str(), entry.second);
    }
    result.error = overlay_saved.error;
    return result;
  }
  result.changed = overlay_saved.changed || written_count != 0 || !global_written.empty();
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
