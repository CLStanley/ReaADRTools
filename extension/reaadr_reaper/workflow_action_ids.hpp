#pragma once

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

// During the bootstrap cutover this snapshot decouples runtime dispatch from
// globals in reaper_reaadr_legacy.cpp. Registration ownership can then move to
// the native registry without changing the dispatcher again.
inline WorkflowActionIds migrated_workflow_action_ids(int validate_session,
                                                       int refresh_overlay,
                                                       int refresh_session,
                                                       int update_cues_from_regions,
                                                       int clear_character_cues,
                                                       int character_filter,
                                                       int next_cue,
                                                       int previous_cue,
                                                       int jump_to_cue,
                                                       int cue_manager,
                                                       int import_cue_sheet,
                                                       int preferences,
                                                       int ui_test)
{
  return {validate_session, refresh_overlay, refresh_session,
          update_cues_from_regions, clear_character_cues, character_filter,
          next_cue, previous_cue, jump_to_cue, cue_manager, import_cue_sheet,
          preferences, ui_test};
}

} // namespace reaadr::reaper
