#pragma once
#include "overlay_settings.hpp"
#include <array>
#include <string>
#include <vector>
namespace reaadr::core {

enum class ManagerPreferenceFieldType { checkbox, text, number, choice };
struct ManagerPreferenceField {
  std::string key;
  std::string label;
  std::string tab;
  ManagerPreferenceFieldType type;
};

const std::vector<ManagerPreferenceField>& manager_preference_fields();
const std::vector<std::string>& manager_quick_action_choices();

struct ManagerQuickAction {
  std::string key;
  std::string label;
  std::string action;
};

struct ManagerQuickActionResult {
  ManagerQuickAction quick_action;
  bool used_default = false;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct ManagerMenuEntry {
  std::string label;
  std::string action;
  int quick_action_slot = 0;
};

struct ManagerMenuResult {
  std::vector<ManagerMenuEntry> entries;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

const std::vector<ManagerQuickAction>& manager_quick_actions();

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

class GlobalStateStore {
public:
  virtual ~GlobalStateStore() = default;
  virtual std::string read(const char* name_space, const char* key) const = 0;
  virtual bool write(const char* name_space, const char* key, const std::string& value) = 0;
};

// Persists project-scoped Manager UI flags alongside the existing overlay
// repository. Quick-action slots remain global REAPER extstate by design.
class ManagerPreferencesRepository {
public:
  explicit ManagerPreferencesRepository(ProjectStateStore& store, GlobalStateStore* global = nullptr)
    : store_(store), global_(global) {}
  ManagerPreferencesLoadResult load() const;
  ManagerPreferencesSaveResult save(const ManagerPreferences& preferences);

private:
  ProjectStateStore& store_;
  GlobalStateStore* global_ = nullptr;
};

// Resolve one 1-based quick-action slot using the same compatibility behavior
// as ReaADR_App.lua: invalid persisted keys fall back to that slot's default.
ManagerQuickActionResult resolve_manager_quick_action(
  const ManagerPreferences& preferences, std::size_t slot);

// Build the top-level ReaADR Tools menu model used by the native host layer:
// Open Manager first, followed by four resolved configurable quick actions.
ManagerMenuResult build_manager_menu(const ManagerPreferences& preferences);

// Apply one validated Manager preference update without mutating the input.
ManagerPreferencesResult update_manager_preferences(
  const ManagerPreferences& current, const std::string& key, const std::string& value);
}
