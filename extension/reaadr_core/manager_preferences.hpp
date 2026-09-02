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
inline bool operator==(const ManagerPreferences& left, const ManagerPreferences& right)
{
  return left.overlay == right.overlay && left.quick_actions == right.quick_actions &&
    left.remember_layout == right.remember_layout && left.hover_preview == right.hover_preview &&
    left.tooltips == right.tooltips && left.navigation_wrap == right.navigation_wrap &&
    left.cue_manager_auto_dock == right.cue_manager_auto_dock;
}
struct ManagerPreferencesResult {
  ManagerPreferences preferences;
  bool changed = false;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};

struct ManagerPreferencesLoadResult {
  ManagerPreferences preferences;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};

struct ManagerPreferencesSaveResult {
  bool changed = false;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};

// Persists project-scoped Manager UI flags alongside the existing overlay
// repository. Quick-action slots remain global REAPER extstate by design.
class ManagerPreferencesRepository {
public:
  explicit ManagerPreferencesRepository(ProjectStateStore& store) : store_(store) {}
  ManagerPreferencesLoadResult load() const;
  ManagerPreferencesSaveResult save(const ManagerPreferences& preferences);

private:
  ProjectStateStore& store_;
};

// Apply one validated Manager preference update without mutating the input.
ManagerPreferencesResult update_manager_preferences(
  const ManagerPreferences& current, const std::string& key, const std::string& value);
}
