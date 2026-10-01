#pragma once

struct reaper_plugin_info_t;

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

// Registers the migrated workflow commands without installing a command hook.
// The persistent runtime host owns dispatch for these IDs.
bool register_migrated_workflow_actions(reaper_plugin_info_t* plugin);
void unregister_migrated_workflow_actions(reaper_plugin_info_t* plugin);
const WorkflowActionIds& workflow_action_ids();

} // namespace reaadr::reaper
