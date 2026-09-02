#pragma once
#include "overlay_settings.hpp"
#include <array>
#include <string>
namespace reaadr::core {
// Serializable state shared by the native Manager Preferences view and its
// compatibility bridge; overlay values remain owned by OverlaySettings.
struct ManagerPreferences {
  OverlaySettings overlay;
  std::array<std::string, 4> quick_actions = {"import", "cue_manager", "export_reports", "overlay_settings"};
  // Lua compatibility defaults this project-scoped option to disabled.
  bool remember_layout = false;
  bool hover_preview = true;
  bool tooltips = true;
  bool navigation_wrap = true;
  bool cue_manager_auto_dock = false;
};
struct ManagerPreferencesResult {
  ManagerPreferences preferences;
  bool changed = false;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};
// Apply one validated Manager preference update without mutating the input.
ManagerPreferencesResult update_manager_preferences(
  const ManagerPreferences& current, const std::string& key, const std::string& value);
}
