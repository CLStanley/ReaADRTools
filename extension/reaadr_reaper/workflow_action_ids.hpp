#pragma once

#include <array>
#include <cstddef>

#include <reaper_plugin.h>

namespace reaadr::reaper {

// Stable REAPER named-command identifiers. These strings are part of the
// compatibility contract: existing keyboard shortcuts/toolbars and native
// Quick Actions resolve them by name, so migration must not rename them.
inline constexpr const char* kValidateSessionCommandName = "ReaADRValidateSessionModelNative";
inline constexpr const char* kValidateSessionActionLabel = "ReaADR: Validate Session Model (Native Preview)";
inline constexpr const char* kRefreshOverlayCommandName = "ReaADRRefreshVideoOverlayNative";
inline constexpr const char* kRefreshOverlayActionLabel = "ReaADR: Refresh Video Overlay (Native)";
inline constexpr const char* kRefreshSessionCommandName = "ReaADRRefreshSessionNative";
inline constexpr const char* kRefreshSessionActionLabel = "ReaADR: Refresh Session (Native)";
inline constexpr const char* kUpdateCuesFromRegionsCommandName = "ReaADRUpdateCuesFromRegionsNative";
inline constexpr const char* kUpdateCuesFromRegionsActionLabel = "ReaADR: Update Cues From Regions (Native)";
inline constexpr const char* kClearCharacterCuesCommandName = "ReaADRClearCharacterCuesNative";
inline constexpr const char* kClearCharacterCuesActionLabel = "ReaADR: Clear Character Cues (Native)";
inline constexpr const char* kCharacterFilterCommandName = "ReaADRApplyCharacterFilterNative";
inline constexpr const char* kCharacterFilterActionLabel = "ReaADR: Character Filter (Native)";
inline constexpr const char* kNextCueCommandName = "ReaADRNextCueNative";
inline constexpr const char* kPreviousCueCommandName = "ReaADRPreviousCueNative";
inline constexpr const char* kJumpToCueCommandName = "ReaADRJumpToCueNative";
inline constexpr const char* kCueManagerCommandName = "ReaADRShowCueManagerNative";
inline constexpr const char* kImportCueSheetCommandName = "ReaADRImportCueSheetNative";
inline constexpr const char* kPreferencesCommandName = "ReaADRShowPreferencesNative";
inline constexpr const char* kUiTestCommandName = "ReaADRNativeUiTestWindowV2";

struct WorkflowActionIds {
  int validate_session = 0;
  int refresh_overlay = 0;
  int refresh_session = 0;
  int update_cues_from_regions = 0;
  int clear_character_cues = 0;
  int character_filter = 0;
  int next_cue = 0;
  int previous_cue = 0;
  int jump_to_cue = 0;
  int cue_manager = 0;
  int import_cue_sheet = 0;
  int preferences = 0;
  int ui_test = 0;
};

struct WorkflowActionDefinition {
  const char* command_name;
  const char* label;
  int WorkflowActionIds::*id;
};

inline constexpr std::array<WorkflowActionDefinition, 13> kMigratedWorkflowActions = {{
  {kValidateSessionCommandName, kValidateSessionActionLabel, &WorkflowActionIds::validate_session},
  {kRefreshOverlayCommandName, kRefreshOverlayActionLabel, &WorkflowActionIds::refresh_overlay},
  {kRefreshSessionCommandName, kRefreshSessionActionLabel, &WorkflowActionIds::refresh_session},
  {kUpdateCuesFromRegionsCommandName, kUpdateCuesFromRegionsActionLabel, &WorkflowActionIds::update_cues_from_regions},
  {kClearCharacterCuesCommandName, kClearCharacterCuesActionLabel, &WorkflowActionIds::clear_character_cues},
  {kCharacterFilterCommandName, kCharacterFilterActionLabel, &WorkflowActionIds::character_filter},
  {kNextCueCommandName, "ReaADR: Next Cue (Native)", &WorkflowActionIds::next_cue},
  {kPreviousCueCommandName, "ReaADR: Previous Cue (Native)", &WorkflowActionIds::previous_cue},
  {kJumpToCueCommandName, "ReaADR: Jump To Cue (Native)", &WorkflowActionIds::jump_to_cue},
  {kCueManagerCommandName, "ReaADR: Cue Manager (Native)", &WorkflowActionIds::cue_manager},
  {kImportCueSheetCommandName, "ReaADR: Import Cue Sheet (Native)", &WorkflowActionIds::import_cue_sheet},
  {kPreferencesCommandName, "ReaADR: Preferences (Native Preview)", &WorkflowActionIds::preferences},
  {kUiTestCommandName, "ReaADR: Native UI Test Window", &WorkflowActionIds::ui_test},
}};

inline WorkflowActionIds g_workflow_action_ids;
inline std::array<gaccel_register_t, kMigratedWorkflowActions.size()> g_workflow_action_accels = {};

inline const WorkflowActionIds& workflow_action_ids()
{
  return g_workflow_action_ids;
}

inline void unregister_migrated_workflow_actions(reaper_plugin_info_t* plugin)
{
  if (plugin) {
    for (std::size_t index = kMigratedWorkflowActions.size(); index > 0; --index) {
      const std::size_t slot = index - 1;
      if (g_workflow_action_ids.*(kMigratedWorkflowActions[slot].id) != 0)
        plugin->Register("-gaccel", reinterpret_cast<void*>(&g_workflow_action_accels[slot]));
    }
  }
  g_workflow_action_ids = {};
  g_workflow_action_accels = {};
}

inline bool register_migrated_workflow_actions(reaper_plugin_info_t* plugin)
{
  if (!plugin) return false;
  if (g_workflow_action_ids.validate_session != 0) return true;

  for (std::size_t index = 0; index < kMigratedWorkflowActions.size(); ++index) {
    const auto& action = kMigratedWorkflowActions[index];
    int& command_id = g_workflow_action_ids.*(action.id);
    command_id = plugin->Register(
      "command_id", reinterpret_cast<void*>(const_cast<char*>(action.command_name)));
    if (!command_id) {
      unregister_migrated_workflow_actions(plugin);
      return false;
    }
    auto& accel = g_workflow_action_accels[index];
    accel.accel.cmd = static_cast<WORD>(command_id);
    accel.desc = action.label;
    if (!plugin->Register("gaccel", reinterpret_cast<void*>(&accel))) {
      command_id = 0;
      unregister_migrated_workflow_actions(plugin);
      return false;
    }
  }
  return true;
}

} // namespace reaadr::reaper
