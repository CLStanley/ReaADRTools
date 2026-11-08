#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

// SWELL defines min/max as function-like macros on non-Windows hosts.
// Load standard library headers before the REAPER/SWELL API declarations.
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "reaadr_reaper/reaper_api.hpp"
#include "reaadr_reaper/legacy_host.hpp"
#include "reaadr_reaper/workflow_action_ids.hpp"
#include "reaadr_reaper/cue_manager_session.hpp"
#include "reaadr_reaper/native_host_services.hpp"
#include "reaadr_reaper/host_api_registration.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <reaper_plugin.h>

#include "reaadr_core/session_model.hpp"
#include "reaadr_core/model_repository.hpp"
#include "reaadr_core/cue_manager_model.hpp"
#include "app/overlay_application_service.hpp"
#include "app/manager_view_application_service.hpp"
#include "app/cue_manager_application_service.hpp"
#include "app/session_refresh_application_service.hpp"
#include "app/region_timing_application_service.hpp"
#include "app/cue_cleanup_application_service.hpp"
#include "app/character_filter_application_service.hpp"
#include "app/cue_import_application_service.hpp"
#include "app/cue_import_request.hpp"
#include "reaadr_reaper/overlay_refresh_adapter.hpp"
#include "reaadr_reaper/cue_navigation_service.hpp"
#include "reaadr_reaper/session_render_service.hpp"
#include "reaadr_reaper/xlsx_import.hpp"
#include "reaadr_reaper/project_state.hpp"
#include "reaadr_reaper/project_transaction.hpp"
#include "reaadr_ui/reaadr_ui.hpp"
#include "reaadr_ui/cue_manager_controller.hpp"
#include "reaadr_ui/cue_manager_window.hpp"

#ifndef _WIN32
#include <dlfcn.h>
#include <swell/swell.h>
#endif

namespace {

constexpr int kMainSection = 0;
constexpr const char* kReaADRMenuId = "ReaADR Tools";
constexpr const char* kValidateSessionCommandName = "ReaADRValidateSessionModelNative";
constexpr const char* kValidateSessionActionLabel = "ReaADR: Validate Session Model (Native Preview)";
constexpr const char* kRefreshOverlayCommandName = "ReaADRRefreshVideoOverlayNative";
constexpr const char* kRefreshOverlayActionLabel = "ReaADR: Refresh Video Overlay (Native)";
constexpr const char* kRefreshSessionCommandName = "ReaADRRefreshSessionNative";
constexpr const char* kRefreshSessionActionLabel = "ReaADR: Refresh Session (Native)";
constexpr const char* kUpdateCuesFromRegionsCommandName = "ReaADRUpdateCuesFromRegionsNative";
constexpr const char* kUpdateCuesFromRegionsActionLabel = "ReaADR: Update Cues From Regions (Native)";
constexpr const char* kClearCharacterCuesCommandName = "ReaADRClearCharacterCuesNative";
constexpr const char* kClearCharacterCuesActionLabel = "ReaADR: Clear Character Cues (Native)";
constexpr const char* kCharacterFilterCommandName = "ReaADRApplyCharacterFilterNative";
constexpr const char* kCharacterFilterActionLabel = "ReaADR: Character Filter (Native)";
constexpr const char* kNextCueCommandName = "ReaADRNextCueNative";
constexpr const char* kPreviousCueCommandName = "ReaADRPreviousCueNative";
constexpr const char* kJumpToCueCommandName = "ReaADRJumpToCueNative";
constexpr const char* kCueManagerCommandName = "ReaADRShowCueManagerNative";
constexpr const char* kImportCueSheetCommandName = "ReaADRImportCueSheetNative";
constexpr const char* kPreferencesCommandName = "ReaADRShowPreferencesNative";
constexpr const char* kUiTestCommandName = "ReaADRNativeUiTestWindowV2";

reaper_plugin_info_t* g_plugin = nullptr;
REAPER_PLUGIN_HINSTANCE g_instance = nullptr;

using CreatePopupMenuFn = HMENU (*)();
using GetMenuItemCountFn = int (*)(HMENU);
using InsertMenuItemFn = void (*)(HMENU, int, BOOL, MENUITEMINFO*);
using SetMenuItemInfoFn = BOOL (*)(HMENU, UINT, BOOL, MENUITEMINFO*);

CreatePopupMenuFn g_create_popup_menu = nullptr;
GetMenuItemCountFn g_get_menu_item_count = nullptr;
InsertMenuItemFn g_insert_menu_item = nullptr;
SetMenuItemInfoFn g_set_menu_item_info = nullptr;
std::string g_log_path;
int g_validate_session_command_id = 0;
gaccel_register_t g_validate_session_accel = {};
int g_refresh_overlay_command_id = 0;
gaccel_register_t g_refresh_overlay_accel = {};
int g_refresh_session_command_id = 0;
gaccel_register_t g_refresh_session_accel = {};
int g_update_cues_from_regions_command_id = 0;
gaccel_register_t g_update_cues_from_regions_accel = {};
int g_clear_character_cues_command_id = 0;
gaccel_register_t g_clear_character_cues_accel = {};
int g_character_filter_command_id = 0;
gaccel_register_t g_character_filter_accel = {};
int g_next_cue_command_id = 0;
gaccel_register_t g_next_cue_accel = {};
int g_previous_cue_command_id = 0;
gaccel_register_t g_previous_cue_accel = {};
int g_jump_to_cue_command_id = 0;
gaccel_register_t g_jump_to_cue_accel = {};
int g_cue_manager_command_id = 0;
gaccel_register_t g_cue_manager_accel = {};
int g_import_cue_sheet_command_id = 0;
gaccel_register_t g_import_cue_sheet_accel = {};
int g_preferences_command_id = 0;
gaccel_register_t g_preferences_accel = {};
int g_ui_test_command_id = 0;
gaccel_register_t g_ui_test_accel = {};
bool g_native_command_hook_registered = false;
const char kDetectDialogueSegmentsDef[] =
  "bool\0"
  "double,double,double,double,int,char*,int,char*,int\0"
  "threshold_db,min_speech_seconds,min_silence_seconds,pad_seconds,sample_rate,segmentsOut,segmentsOut_sz,errorOut,errorOut_sz\0"
  "Detect dialogue segments from the first selected media item and return timeline start/end pairs as tab-delimited lines.";
const char kReadXlsxAsTsvDef[] =
  "bool\0"
  "const char*,char*,int,char*,int\0"
  "path,tsvOut,tsvOut_sz,errorOut,errorOut_sz\0"
  "Read the first worksheet from an XLSX file and return tab-delimited text.";
struct NativeMenuAction {
  const char* label;
  int command_id = 0;
};

struct LegacyScriptRegistration {
  const char* relative_path;
};

std::vector<NativeMenuAction> g_actions = {
  {"Open Manager", 0},
  {"Quick Action 1", 0},
  {"Quick Action 2", 0},
  {"Quick Action 3", 0},
  {"Quick Action 4", 0},
};

// This action is native; the shared menu helpers only require its label and
// command ID, so there is intentionally no script path.
NativeMenuAction g_validate_session_action = {"Validate Session Model (Native Preview)", 0};

NativeMenuAction g_refresh_overlay_action = {"Refresh Video Overlay (Native)", 0};
NativeMenuAction g_refresh_session_action = {"Refresh Session (Native)", 0};
NativeMenuAction g_update_cues_from_regions_action = {"Update Cues From Regions (Native)", 0};
NativeMenuAction g_clear_character_cues_action = {"Clear Character Cues (Native)", 0};
NativeMenuAction g_character_filter_action = {"Character Filter (Native)", 0};
NativeMenuAction g_next_cue_action = {"Next Cue (Native)", 0};
NativeMenuAction g_previous_cue_action = {"Previous Cue (Native)", 0};
NativeMenuAction g_jump_to_cue_action = {"Jump To Cue (Native)", 0};
// The stable native command now owns the public Manager launch. The Lua
// launcher remains in the repository as a parity reference and upgrade target,
// but is no longer registered as the active Open Manager action.
NativeMenuAction g_cue_manager_action = {"Open Manager", 0};
NativeMenuAction g_import_cue_sheet_action = {"Import Cue Sheet (Native)", 0};
NativeMenuAction g_preferences_action = {"Preferences (Native Preview)", 0};
NativeMenuAction g_ui_test_action = {"Native UI Test Window (C++)", 0};

// Upgrade-only paths used to remove ReaScript registrations left by older
// ReaADR releases. These files are not registered or executed by the native
// extension; keep this list only while in-place upgrades need cleanup.
std::vector<LegacyScriptRegistration> g_legacy_registration_cleanup = {
  {"Scripts/ReaADRTools/scripts/ReaADR_Open_Manager.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Quick_Action_1.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Quick_Action_2.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Quick_Action_3.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Quick_Action_4.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Import_Script.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Export_Reports.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Preferences.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Import_Cue_Sheet.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Export_Cue_Sheet.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Next_Cue.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Previous_Cue.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Jump_To_Cue.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Set_Cue_Status.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Character_Filter.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Generate_Cues.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Clean_Generated_Cues.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Overlay_Settings.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Menu.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Start_Recording_Workflow.lua"},
  {"Scripts/ReaADRTools/scripts/ReaADR_Jump_To_Selected_Cue.lua"},
};

std::string parent_path(const std::string& path)
{
  const std::string::size_type slash = path.find_last_of("/\\");
  if (slash == std::string::npos) return ".";
  if (slash == 0) return path.substr(0, 1);
  return path.substr(0, slash);
}

#ifdef _WIN32
std::string module_directory_from_instance(REAPER_PLUGIN_HINSTANCE instance)
{
  char path[MAX_PATH] = {};
  if (instance && GetModuleFileNameA(instance, path, static_cast<DWORD>(sizeof(path))) > 0) {
    return parent_path(path);
  }
  return ".";
}
#endif

std::string plugin_directory()
{
#ifdef _WIN32
  return module_directory_from_instance(g_instance);
#else
  Dl_info info = {};
  if (dladdr(reinterpret_cast<void*>(&plugin_directory), &info) && info.dli_fname) {
    return parent_path(info.dli_fname);
  }
  return ".";
#endif
}

std::string resource_directory()
{
  if (GetResourcePath) {
    const char* path = GetResourcePath();
    if (path && *path) return path;
  }
  return parent_path(plugin_directory());
}

std::string join_path(const std::string& base, const char* relative)
{
  if (base.empty() || base == ".") return relative;
  return base + "/" + relative;
}

void log_line(const std::string& message)
{
  if (g_log_path.empty()) return;

  FILE* file = std::fopen(g_log_path.c_str(), "a");
  if (!file) return;

  std::fputs(message.c_str(), file);
  std::fputc('\n', file);
  std::fclose(file);
}

#ifdef _WIN32
void log_windows_dll_load(REAPER_PLUGIN_HINSTANCE instance)
{
  const std::string path = join_path(module_directory_from_instance(instance), "reaper_reaadr_dll_load.log");
  FILE* file = std::fopen(path.c_str(), "a");
  if (!file) return;

  std::fputs("DllMain process attach reached.\n", file);
  std::fclose(file);
}
#endif

void initialize_log_path()
{
  const std::string root = plugin_directory();
  const std::string bundled_log_path = join_path(root, "ReaADRTools/reaper_reaadr.log");
  FILE* file = std::fopen(bundled_log_path.c_str(), "a");
  if (file) {
    g_log_path = bundled_log_path;
    std::fclose(file);
    return;
  }

  g_log_path = join_path(root, "reaper_reaadr.log");
}

void retire_legacy_script_registrations()
{
  if (!AddRemoveReaScript) {
    log_line("REAPER script-registration cleanup API unavailable; skipping historical action retirement.");
    return;
  }

  const std::string root = resource_directory();
  const std::string old_root = plugin_directory();
  log_line("Retiring historical ReaADR ReaScript registrations.");

  for (const LegacyScriptRegistration& legacy_action : g_legacy_registration_cleanup) {
    const std::string old_script_path = join_path(old_root, legacy_action.relative_path + 8);
    AddRemoveReaScript(false, kMainSection, old_script_path.c_str(), false);
    const std::string script_path = join_path(root, legacy_action.relative_path);
    AddRemoveReaScript(false, kMainSection, script_path.c_str(), false);
  }
}

void add_menu_item(HMENU menu, int position, const NativeMenuAction& action)
{
  if (!action.command_id || !g_insert_menu_item) return;

  MENUITEMINFO item = {};
  item.cbSize = sizeof(item);
  item.fMask = MIIM_TYPE | MIIM_ID;
  item.fType = MFT_STRING;
  item.wID = static_cast<UINT>(action.command_id);
  item.dwTypeData = const_cast<char*>(action.label);
  g_insert_menu_item(menu, position, TRUE, &item);
}

std::string quick_action_label(int slot)
{
  if (!GetExtState) return "Quick Action " + std::to_string(slot);

  const std::string key = "quick_action_" + std::to_string(slot);
  reaadr::reaper::GlobalStateStore global_state({GetExtState, SetExtState});
  const std::string action_key = global_state.read("ReaADRTools", key.c_str());

  if (action_key == "import" || (action_key.empty() && slot == 1)) return "Quick Action " + std::to_string(slot) + ": Import Cue Sheet";
  if (action_key == "cue_manager" || (action_key.empty() && slot == 2)) return "Quick Action " + std::to_string(slot) + ": Open Cue Manager";
  if (action_key == "export_reports" || (action_key.empty() && slot == 3)) return "Quick Action " + std::to_string(slot) + ": Export Reports";
  if (action_key == "overlay_settings" || (action_key.empty() && slot == 4)) return "Quick Action " + std::to_string(slot) + ": Video Overlays Tab";
  if (action_key == "character_filter") return "Quick Action " + std::to_string(slot) + ": Character Filter";
  if (action_key == "refresh_overlay") return "Quick Action " + std::to_string(slot) + ": Refresh Video Overlay";
  if (action_key == "validate") return "Quick Action " + std::to_string(slot) + ": Validate Session";
  return "Quick Action " + std::to_string(slot);
}

void copy_to_buffer(const std::string& value, char* buffer, int buffer_size)
{
  if (!buffer || buffer_size <= 0) return;
  std::snprintf(buffer, static_cast<std::size_t>(buffer_size), "%s", value.c_str());
}

std::string native_utc_timestamp()
{
  const std::time_t now = std::time(nullptr);
  std::tm utc = {};
#ifdef _WIN32
  if (gmtime_s(&utc, &now) != 0) return {};
#else
  if (!gmtime_r(&now, &utc)) return {};
#endif
  std::array<char, 32> buffer = {};
  return std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ", &utc)
    ? std::string(buffer.data()) : std::string();
}

std::string native_cue_audio_path()
{
  std::array<char, 4096> path = {};
  if (GetProjectPathEx) GetProjectPathEx(nullptr, path.data(), static_cast<int>(path.size()));
  std::string directory = path.data();
  if (directory.empty() && GetResourcePath) {
    const char* resource_path = GetResourcePath();
    if (resource_path) directory = resource_path;
  }
  if (directory.empty()) directory = ".";
  const char last = directory.back();
  if (last != '/' && last != '\\') directory.push_back('/');
  return directory + "reaadr_cue.wav";
}

reaadr::reaper::TrackRegionApi native_track_region_api()
{
  return {
    CountTracks, GetTrack, InsertTrackAtIndex, GetSetMediaTrackInfo_String,
    GetMediaTrackInfo_Value, SetMediaTrackInfo_Value, CountProjectMarkers,
    EnumProjectMarkers3, SetProjectMarker4, AddProjectMarker2,
    DeleteProjectMarker, ColorToNative, ColorFromNative,
    TrackList_AdjustWindows, UpdateArrange,
  };
}

reaadr::reaper::RulerLaneApi native_ruler_lane_api()
{
  return {
    GetSetProjectInfo, GetSetProjectInfo_String, CountProjectMarkers,
    EnumProjectMarkers3, GetRegionOrMarker, GetRegionOrMarkerInfo_Value,
    SetRegionOrMarkerInfo_Value, ColorToNative, ColorFromNative,
  };
}

reaadr::reaper::CueAudioApi native_cue_audio_api()
{
  return {
    CountTracks, GetTrack, GetSetMediaTrackInfo_String, CountTrackMediaItems,
    GetTrackMediaItem, GetSetMediaItemInfo_String, GetMediaItemInfo_Value,
    SetMediaItemInfo_Value, AddMediaItemToTrack, DeleteTrackMediaItem,
    MoveMediaItemToTrack, GetActiveTake, AddTakeToMediaItem,
    GetSetMediaItemTakeInfo_String, GetSetMediaItemTakeInfo,
    PCM_Source_CreateFromFile, GetMediaSourceLength, PCM_Source_Destroy,
  };
}

reaadr::reaper::TransactionApi native_session_transaction_api()
{
  return {
    Undo_BeginBlock2, Undo_EndBlock2, Undo_CanUndo2, Undo_DoUndo2,
    PreventUIRefresh,
  };
}

reaadr::core::ProjectRenderState native_cleanup_inspect(std::string* error)
{
  const auto inspected = reaadr::reaper::inspect_complete_render_state(
    nullptr, native_track_region_api(), native_ruler_lane_api(), native_cue_audio_api());
  if (!inspected && error) *error = inspected.error;
  return inspected.state;
}

bool native_delete_cleanup_track(ReaProject*, MediaTrack* track)
{
  if (!DeleteTrack || !track) return false;
  DeleteTrack(track);
  return true;
}

reaadr::reaper::CueCleanupApplyResult native_cleanup_apply(
  const reaadr::core::CueCleanupPlan& plan, std::string* error)
{
  const auto applied = reaadr::reaper::apply_cue_cleanup_plan_transactionally(
    nullptr, {
      CountTracks, GetTrack, GetSetMediaTrackInfo_String, CountTrackMediaItems,
      GetTrackMediaItem, GetSetMediaItemInfo_String, DeleteTrackMediaItem, native_delete_cleanup_track,
      CountProjectMarkers, EnumProjectMarkers3, DeleteProjectMarker,
      TrackList_AdjustWindows, UpdateArrange,
    }, native_session_transaction_api(), plan, "ReaADR: clear character cues");
  if (!applied && error) *error = applied.error;
  return applied;
}

reaadr::reaper::CharacterFilterInspectionResult native_character_filter_inspect(
  std::string* error)
{
  const auto inspected = reaadr::reaper::inspect_character_filter_project(
    nullptr, native_track_region_api(), native_ruler_lane_api());
  if (!inspected && error) *error = inspected.error;
  return inspected;
}

reaadr::reaper::CharacterFilterApplyResult native_character_filter_apply(
  const reaadr::core::CharacterFilterPlan& plan, std::string* error)
{
  const auto applied = reaadr::reaper::apply_character_filter_plan_transactionally(
    nullptr, native_track_region_api(), native_ruler_lane_api(),
    native_session_transaction_api(), plan, "ReaADR: apply character filter");
  if (!applied && error) *error = applied.error;
  return applied;
}


bool hook_native_command(int command, int)
{
  if (command == g_ui_test_command_id && command != 0) {
    reaadr::ui::show_test_window();
    return true;
  }
  return false;
}

bool register_native_actions()
{
  if (!g_plugin) return false;

  // The identifier is persisted in REAPER keyboard mappings, so it must never
  // be renamed after release. The user-facing label may evolve independently.
  g_validate_session_command_id = g_plugin->Register(
    "command_id",
    reinterpret_cast<void*>(const_cast<char*>(kValidateSessionCommandName)));
  if (!g_validate_session_command_id) {
    log_line("Could not allocate the native session validation command ID.");
    return false;
  }

  g_validate_session_accel.accel.cmd = static_cast<WORD>(g_validate_session_command_id);
  g_validate_session_accel.desc = kValidateSessionActionLabel;
  if (!g_plugin->Register("gaccel", reinterpret_cast<void*>(&g_validate_session_accel))) {
    log_line("Could not add the native session validation command to the Action List.");
    g_validate_session_command_id = 0;
    return false;
  }
  if (!g_plugin->Register("hookcommand", reinterpret_cast<void*>(hook_native_command))) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_validate_session_accel));
    g_validate_session_command_id = 0;
    log_line("Could not register the native command hook.");
    return false;
  }

  g_native_command_hook_registered = true;
  g_validate_session_action.command_id = g_validate_session_command_id;
  g_refresh_overlay_command_id = g_plugin->Register(
    "command_id", reinterpret_cast<void*>(const_cast<char*>(kRefreshOverlayCommandName)));
  if (g_refresh_overlay_command_id) {
    g_refresh_overlay_accel.accel.cmd = static_cast<WORD>(g_refresh_overlay_command_id);
    g_refresh_overlay_accel.desc = kRefreshOverlayActionLabel;
    if (g_plugin->Register("gaccel", reinterpret_cast<void*>(&g_refresh_overlay_accel))) {
      g_refresh_overlay_action.command_id = g_refresh_overlay_command_id;
    } else {
      g_refresh_overlay_command_id = 0;
      log_line("Could not add the native overlay refresh command to the Action List.");
    }
  } else {
    log_line("Could not allocate the native overlay refresh command ID.");
  }
  const auto register_secondary_action = [=](const char* command_name,
                                              const char* label,
                                              int& command_id,
                                              gaccel_register_t& accel,
                                              NativeMenuAction& action) {
    command_id = g_plugin->Register(
      "command_id", reinterpret_cast<void*>(const_cast<char*>(command_name)));
    if (!command_id) {
      log_line(std::string("Could not allocate native command: ") + label);
      return;
    }
    accel.accel.cmd = static_cast<WORD>(command_id);
    accel.desc = label;
    if (g_plugin->Register("gaccel", reinterpret_cast<void*>(&accel))) {
      action.command_id = command_id;
    } else {
      command_id = 0;
      log_line(std::string("Could not add native command to the Action List: ") + label);
    }
  };
  register_secondary_action(kNextCueCommandName, "ReaADR: Next Cue (Native)",
    g_next_cue_command_id, g_next_cue_accel, g_next_cue_action);
  register_secondary_action(kPreviousCueCommandName, "ReaADR: Previous Cue (Native)",
    g_previous_cue_command_id, g_previous_cue_accel, g_previous_cue_action);
  register_secondary_action(kJumpToCueCommandName, "ReaADR: Jump To Cue (Native)",
    g_jump_to_cue_command_id, g_jump_to_cue_accel, g_jump_to_cue_action);
  register_secondary_action(kCueManagerCommandName, "ReaADR: Cue Manager (Native)",
    g_cue_manager_command_id, g_cue_manager_accel, g_cue_manager_action);
  register_secondary_action(kImportCueSheetCommandName, "ReaADR: Import Cue Sheet (Native)",
    g_import_cue_sheet_command_id, g_import_cue_sheet_accel, g_import_cue_sheet_action);
  register_secondary_action(kPreferencesCommandName, "ReaADR: Preferences (Native Preview)",
    g_preferences_command_id, g_preferences_accel, g_preferences_action);
  register_secondary_action(kRefreshSessionCommandName, kRefreshSessionActionLabel,
    g_refresh_session_command_id, g_refresh_session_accel, g_refresh_session_action);
  register_secondary_action(kUpdateCuesFromRegionsCommandName, kUpdateCuesFromRegionsActionLabel,
    g_update_cues_from_regions_command_id, g_update_cues_from_regions_accel,
    g_update_cues_from_regions_action);
  register_secondary_action(kClearCharacterCuesCommandName, kClearCharacterCuesActionLabel,
    g_clear_character_cues_command_id, g_clear_character_cues_accel,
    g_clear_character_cues_action);
  register_secondary_action(kCharacterFilterCommandName, kCharacterFilterActionLabel,
    g_character_filter_command_id, g_character_filter_accel, g_character_filter_action);
  register_secondary_action(kUiTestCommandName, "ReaADR: Native UI Test Window",
    g_ui_test_command_id, g_ui_test_accel, g_ui_test_action);
  log_line("Registered native UI test command_id=" + std::to_string(g_ui_test_command_id));
  log_line("Registered native action: " + std::string(kValidateSessionActionLabel));
  return true;
}

void unregister_native_actions()
{
  if (!g_plugin) return;
  if (g_native_command_hook_registered) {
    g_plugin->Register("-hookcommand", reinterpret_cast<void*>(hook_native_command));
    g_native_command_hook_registered = false;
  }
  if (g_validate_session_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_validate_session_accel));
    g_validate_session_command_id = 0;
    g_validate_session_action.command_id = 0;
    g_validate_session_accel = {};
  }
  if (g_refresh_overlay_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_refresh_overlay_accel));
    g_refresh_overlay_command_id = 0;
    g_refresh_overlay_action.command_id = 0;
    g_refresh_overlay_accel = {};
  }
  if (g_refresh_session_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_refresh_session_accel));
    g_refresh_session_command_id = 0;
    g_refresh_session_action.command_id = 0;
    g_refresh_session_accel = {};
  }
  if (g_update_cues_from_regions_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_update_cues_from_regions_accel));
    g_update_cues_from_regions_command_id = 0;
    g_update_cues_from_regions_action.command_id = 0;
    g_update_cues_from_regions_accel = {};
  }
  if (g_clear_character_cues_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_clear_character_cues_accel));
    g_clear_character_cues_command_id = 0;
    g_clear_character_cues_action.command_id = 0;
    g_clear_character_cues_accel = {};
  }
  if (g_character_filter_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_character_filter_accel));
    g_character_filter_command_id = 0;
    g_character_filter_action.command_id = 0;
    g_character_filter_accel = {};
  }
  if (g_next_cue_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_next_cue_accel));
    g_next_cue_command_id = 0;
    g_next_cue_action.command_id = 0;
    g_next_cue_accel = {};
  }
  if (g_previous_cue_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_previous_cue_accel));
    g_previous_cue_command_id = 0;
    g_previous_cue_action.command_id = 0;
    g_previous_cue_accel = {};
  }
  if (g_jump_to_cue_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_jump_to_cue_accel));
    g_jump_to_cue_command_id = 0;
    g_jump_to_cue_action.command_id = 0;
    g_jump_to_cue_accel = {};
  }
  if (g_cue_manager_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_cue_manager_accel));
    g_cue_manager_command_id = 0;
    g_cue_manager_action.command_id = 0;
    g_cue_manager_accel = {};
  }
  if (g_import_cue_sheet_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_import_cue_sheet_accel));
    g_import_cue_sheet_command_id = 0;
    g_import_cue_sheet_action.command_id = 0;
    g_import_cue_sheet_accel = {};
  }
  if (g_preferences_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_preferences_accel));
    g_preferences_command_id = 0;
    g_preferences_action.command_id = 0;
    g_preferences_accel = {};
  }
  if (g_ui_test_command_id) {
    g_plugin->Register("-gaccel", reinterpret_cast<void*>(&g_ui_test_accel));
    g_ui_test_command_id = 0;
    g_ui_test_action.command_id = 0;
    g_ui_test_accel = {};
  }
}

bool read_xlsx_as_tsv(const char* path, char* tsv_out, int tsv_out_sz, char* error_out, int error_out_sz)
{
  copy_to_buffer("", tsv_out, tsv_out_sz);
  copy_to_buffer("", error_out, error_out_sz);

  const auto result = reaadr::reaper::read_xlsx_first_sheet_as_tsv(path ? path : "");
  if (!result) {
    copy_to_buffer(result.error, error_out, error_out_sz);
    return false;
  }
  if (static_cast<int>(result.tsv.size()) >= tsv_out_sz) {
    copy_to_buffer("XLSX import output exceeded the provided buffer size.", error_out, error_out_sz);
    return false;
  }

  copy_to_buffer(result.tsv, tsv_out, tsv_out_sz);
  return true;
}

bool detect_dialogue_segments(double threshold_db,
                              double min_speech_seconds,
                              double min_silence_seconds,
                              double pad_seconds,
                              int sample_rate,
                              char* segments_out,
                              int segments_out_sz,
                              char* error_out,
                              int error_out_sz)
{
  copy_to_buffer("", segments_out, segments_out_sz);
  copy_to_buffer("", error_out, error_out_sz);

  if (!CountSelectedMediaItems || !GetSelectedMediaItem || !GetActiveTake || !CreateTakeAudioAccessor ||
      !DestroyAudioAccessor || !GetAudioAccessorStartTime || !GetAudioAccessorEndTime ||
      !GetAudioAccessorSamples || !GetMediaItemInfo_Value) {
    copy_to_buffer("Required REAPER audio accessor APIs are unavailable.", error_out, error_out_sz);
    return false;
  }

  if (CountSelectedMediaItems(nullptr) <= 0) {
    copy_to_buffer("Select one audio or video media item to analyze.", error_out, error_out_sz);
    return false;
  }

  MediaItem* item = GetSelectedMediaItem(nullptr, 0);
  MediaItem_Take* take = item ? GetActiveTake(item) : nullptr;
  if (!item || !take) {
    copy_to_buffer("The selected media item does not have an active take.", error_out, error_out_sz);
    return false;
  }

  AudioAccessor* accessor = CreateTakeAudioAccessor(take);
  if (!accessor) {
    copy_to_buffer("Could not create an audio accessor for the selected media.", error_out, error_out_sz);
    return false;
  }

  struct Segment {
    double start_time = 0.0;
    double end_time = 0.0;
  };

  const int safe_sample_rate = sample_rate > 0 ? sample_rate : 12000;
  const int channels = 1;
  const int block_samples = (std::max)(64, static_cast<int>(safe_sample_rate * 0.025 + 0.5));
  const double block_duration = static_cast<double>(block_samples) / static_cast<double>(safe_sample_rate);
  const double threshold = std::pow(10.0, threshold_db / 20.0);
  const double min_speech = (std::max)(0.0, min_speech_seconds);
  const double min_silence = (std::max)(0.0, min_silence_seconds);
  const double pad = (std::max)(0.0, pad_seconds);
  const double item_position = GetMediaItemInfo_Value(item, "D_POSITION");
  const double item_length = (std::max)(0.0, GetMediaItemInfo_Value(item, "D_LENGTH"));
  const double item_end = item_position + item_length;
  const double start_time = GetAudioAccessorStartTime(accessor);
  const double end_time = (std::min)(GetAudioAccessorEndTime(accessor), start_time + item_length);

  if (end_time <= start_time) {
    DestroyAudioAccessor(accessor);
    copy_to_buffer("The selected media item has no readable audio in the placed item range.", error_out, error_out_sz);
    return false;
  }

  std::vector<double> buffer(static_cast<std::size_t>(block_samples) * static_cast<std::size_t>(channels), 0.0);
  std::vector<Segment> segments;
  double active_start = -1.0;
  double last_loud_end = -1.0;
  double t = start_time;

  auto timeline_time = [item_position, start_time](double accessor_time) {
    return item_position + (std::max)(0.0, accessor_time - start_time);
  };

  auto flush_segment = [&]() {
    if (active_start >= 0.0 && last_loud_end >= 0.0 && (last_loud_end - active_start) >= min_speech) {
      Segment segment;
      segment.start_time = (std::max)(item_position, timeline_time(active_start - pad));
      segment.end_time = (std::min)(item_end, timeline_time(last_loud_end + pad));
      if (segment.end_time > segment.start_time) {
        segments.push_back(segment);
      }
    }
    active_start = -1.0;
    last_loud_end = -1.0;
  };

  while (t < end_time) {
    std::fill(buffer.begin(), buffer.end(), 0.0);
    const double block_end = (std::min)(end_time, t + block_duration);
    const int got = GetAudioAccessorSamples(accessor, safe_sample_rate, channels, t, block_samples, buffer.data());
    bool loud = false;

    if (got < 0) {
      DestroyAudioAccessor(accessor);
      copy_to_buffer("REAPER returned an error while reading the selected media.", error_out, error_out_sz);
      return false;
    }

    if (got == 1) {
      double sum = 0.0;
      for (double sample : buffer) {
        sum += sample * sample;
      }
      const double rms = std::sqrt(sum / static_cast<double>(block_samples));
      loud = rms >= threshold;
    }

    if (loud) {
      if (active_start < 0.0) active_start = t;
      last_loud_end = block_end;
    } else if (active_start >= 0.0 && last_loud_end >= 0.0 && (t - last_loud_end) >= min_silence) {
      flush_segment();
    }

    t = block_end;
  }

  flush_segment();
  DestroyAudioAccessor(accessor);

  std::ostringstream output;
  output.setf(std::ios::fixed);
  output.precision(9);
  for (const Segment& segment : segments) {
    output << segment.start_time << '\t' << segment.end_time << '\n';
  }

  const std::string text = output.str();
  if (static_cast<int>(text.size()) >= segments_out_sz) {
    copy_to_buffer("Detected segment output exceeded the provided buffer size.", error_out, error_out_sz);
    return false;
  }

  copy_to_buffer(text, segments_out, segments_out_sz);
  return true;
}

void add_menu_item_with_label(HMENU menu, int position, const NativeMenuAction& action, const std::string& label)
{
  if (!action.command_id || !g_insert_menu_item) return;

  MENUITEMINFO item = {};
  item.cbSize = sizeof(item);
  item.fMask = MIIM_TYPE | MIIM_ID;
  item.fType = MFT_STRING;
  item.wID = static_cast<UINT>(action.command_id);
  item.dwTypeData = const_cast<char*>(label.c_str());
  g_insert_menu_item(menu, position, TRUE, &item);
}

void update_menu_item_label(HMENU menu, int position, const NativeMenuAction& action, const std::string& label)
{
  if (!action.command_id || !g_set_menu_item_info) return;

  MENUITEMINFO item = {};
  item.cbSize = sizeof(item);
  item.fMask = MIIM_TYPE | MIIM_ID;
  item.fType = MFT_STRING;
  item.wID = static_cast<UINT>(action.command_id);
  item.dwTypeData = const_cast<char*>(label.c_str());
  g_set_menu_item_info(menu, static_cast<UINT>(position), TRUE, &item);
}

void hook_custom_menu(const char* menu_id, void* menu, int flag)

{
  log_line(std::string("hookcustommenu id=") + (menu_id ? menu_id : "(null)") + " flag=" + std::to_string(flag));
  if ((flag != 0 && flag != 1) || !menu_id || !menu) return;
  if (std::strcmp(menu_id, kReaADRMenuId) != 0) return;
  if (!g_create_popup_menu || !g_get_menu_item_count || !g_insert_menu_item) {
    log_line("Menu helpers unavailable; skipping ReaADR Tools menu creation.");
    return;
  }

  HMENU hmenu = static_cast<HMENU>(menu);
  const int existing_items = g_get_menu_item_count ? g_get_menu_item_count(hmenu) : 0;
  int position = existing_items > 0 ? existing_items : 0;

  if (existing_items <= 0) {
    add_menu_item(hmenu, position++, g_cue_manager_action);
    for (std::size_t i = 1; i < g_actions.size(); ++i) {
      add_menu_item_with_label(hmenu, position++, g_actions[i], quick_action_label(static_cast<int>(i)));
    }
    add_menu_item(hmenu, position, g_validate_session_action);
    add_menu_item(hmenu, position + 1, g_refresh_overlay_action);
    add_menu_item(hmenu, position + 2, g_refresh_session_action);
    add_menu_item(hmenu, position + 3, g_update_cues_from_regions_action);
    add_menu_item(hmenu, position + 4, g_clear_character_cues_action);
    add_menu_item(hmenu, position + 5, g_character_filter_action);
    add_menu_item(hmenu, position + 6, g_next_cue_action);
    add_menu_item(hmenu, position + 7, g_previous_cue_action);
    add_menu_item(hmenu, position + 8, g_jump_to_cue_action);
    add_menu_item(hmenu, position + 9, g_import_cue_sheet_action);
    add_menu_item(hmenu, position + 10, g_preferences_action);
    log_line("Added top-level ReaADR Tools menu.");
    return;
  }

  update_menu_item_label(hmenu, 0, g_cue_manager_action, g_cue_manager_action.label);

  for (std::size_t i = 1; i < g_actions.size(); ++i) {
    if (static_cast<int>(i) < existing_items) {
      update_menu_item_label(hmenu, static_cast<int>(i), g_actions[i], quick_action_label(static_cast<int>(i)));
    } else {
      add_menu_item_with_label(hmenu, position++, g_actions[i], quick_action_label(static_cast<int>(i)));
    }
  }
  const int validation_position = static_cast<int>(g_actions.size());
  if (validation_position < existing_items) {
    update_menu_item_label(
      hmenu,
      validation_position,
      g_validate_session_action,
      g_validate_session_action.label);
  } else {
    add_menu_item(hmenu, position, g_validate_session_action);
  }
  const int refresh_position = validation_position + 1;
  if (refresh_position < existing_items) {
    update_menu_item_label(hmenu, refresh_position, g_refresh_overlay_action,
      g_refresh_overlay_action.label);
  } else {
    add_menu_item(hmenu, position + 1, g_refresh_overlay_action);
  }
  const int refresh_session_position = validation_position + 2;
  if (refresh_session_position < existing_items) {
    update_menu_item_label(hmenu, refresh_session_position, g_refresh_session_action,
      g_refresh_session_action.label);
  } else {
    add_menu_item(hmenu, position + 2, g_refresh_session_action);
  }
  const int update_regions_position = validation_position + 3;
  if (update_regions_position < existing_items) {
    update_menu_item_label(hmenu, update_regions_position, g_update_cues_from_regions_action,
      g_update_cues_from_regions_action.label);
  } else {
    add_menu_item(hmenu, position + 3, g_update_cues_from_regions_action);
  }
  const int clear_character_position = validation_position + 4;
  if (clear_character_position < existing_items) {
    update_menu_item_label(hmenu, clear_character_position, g_clear_character_cues_action,
      g_clear_character_cues_action.label);
  } else {
    add_menu_item(hmenu, position + 4, g_clear_character_cues_action);
  }
  const int character_filter_position = validation_position + 5;
  if (character_filter_position < existing_items) {
    update_menu_item_label(hmenu, character_filter_position, g_character_filter_action,
      g_character_filter_action.label);
  } else {
    add_menu_item(hmenu, position + 5, g_character_filter_action);
  }
  const int next_position = validation_position + 6;
  if (next_position < existing_items) {
    update_menu_item_label(hmenu, next_position, g_next_cue_action, g_next_cue_action.label);
  } else {
    add_menu_item(hmenu, position + 6, g_next_cue_action);
  }
  const int previous_position = validation_position + 7;
  if (previous_position < existing_items) {
    update_menu_item_label(hmenu, previous_position, g_previous_cue_action, g_previous_cue_action.label);
  } else {
    add_menu_item(hmenu, position + 7, g_previous_cue_action);
  }
  const int jump_position = validation_position + 8;
  if (jump_position < existing_items) {
    update_menu_item_label(hmenu, jump_position, g_jump_to_cue_action, g_jump_to_cue_action.label);
  } else {
    add_menu_item(hmenu, position + 8, g_jump_to_cue_action);
  }
  const int import_position = validation_position + 9;
  if (import_position < existing_items) {
    update_menu_item_label(hmenu, import_position, g_import_cue_sheet_action,
      g_import_cue_sheet_action.label);
  } else {
    add_menu_item(hmenu, position + 9, g_import_cue_sheet_action);
  }
  const int preferences_position = validation_position + 10;
  if (preferences_position < existing_items) {
    update_menu_item_label(hmenu, preferences_position, g_preferences_action,
      g_preferences_action.label);
  } else {
    add_menu_item(hmenu, position + 10, g_preferences_action);
  }
  log_line("Updated top-level ReaADR quick-action labels.");
}

void load_menu_functions()
{
#ifdef _WIN32
  g_create_popup_menu = []() -> HMENU { return CreatePopupMenu(); };
  g_get_menu_item_count = [](HMENU menu) -> int { return GetMenuItemCount(menu); };
  g_insert_menu_item = [](HMENU menu, int position, BOOL by_position, MENUITEMINFO* item) {
    InsertMenuItem(menu, position, by_position, item);
  };
  g_set_menu_item_info = [](HMENU menu, UINT item, BOOL by_position, MENUITEMINFO* info) -> BOOL {
    return SetMenuItemInfoA(menu, item, by_position, info);
  };
#endif
#ifndef _WIN32
  g_create_popup_menu = []() -> HMENU { return CreatePopupMenu(); };
  g_get_menu_item_count = [](HMENU menu) -> int { return GetMenuItemCount(menu); };
  g_insert_menu_item = [](HMENU menu, int position, BOOL by_position, MENUITEMINFO* item) {
    InsertMenuItem(menu, position, by_position, item);
  };
#endif
  log_line(std::string("CreatePopupMenu available: ") + (g_create_popup_menu ? "yes" : "no"));
  log_line(std::string("GetMenuItemCount available: ") + (g_get_menu_item_count ? "yes" : "no"));
  log_line(std::string("InsertMenuItem available: ") + (g_insert_menu_item ? "yes" : "no"));
  log_line(std::string("SetMenuItemInfo available: ") + (g_set_menu_item_info ? "yes" : "no"));
}

std::array<reaadr::reaper::HostApiExport, 4> host_api_exports()
{
  return {{
    {"API_ReaADR_DetectDialogueSegments", reinterpret_cast<void*>(detect_dialogue_segments)},
    {"APIdef_ReaADR_DetectDialogueSegments",
     reinterpret_cast<void*>(const_cast<char*>(kDetectDialogueSegmentsDef))},
    {"API_ReaADR_ReadXlsxAsTsv", reinterpret_cast<void*>(read_xlsx_as_tsv)},
    {"APIdef_ReaADR_ReadXlsxAsTsv",
     reinterpret_cast<void*>(const_cast<char*>(kReadXlsxAsTsvDef))},
  }};
}

bool load(reaper_plugin_info_t* plugin)
{
  g_plugin = plugin;
  initialize_log_path();
  log_line("Loading ReaADR extension.");
  if (REAPERAPI_LoadAPI(plugin->GetFunc) != 0) {
    log_line("REAPERAPI_LoadAPI failed.");
    return false;
  }
  reaadr::ui::initialize(ShowMessageBox);
  const auto exports = host_api_exports();
  if (!reaadr::reaper::register_host_api_exports(
        exports, [plugin](const char* key, void* value) {
          return plugin->Register(key, value) != 0;
        })) {
    log_line("Failed to register REAPER API exports.");
    return false;
  }
  register_native_actions();
  retire_legacy_script_registrations();
  load_menu_functions();
  if (AddCustomizableMenu) {
    // Register after script actions so REAPER does not rebuild the main menu
    // and discard this custom top-level entry during action registration.
    const bool added = AddCustomizableMenu(kReaADRMenuId, kReaADRMenuId, nullptr, true);
    log_line(std::string("AddCustomizableMenu result: ") + (added ? "success" : "failure"));
    plugin->Register("hookcustommenu", reinterpret_cast<void*>(hook_custom_menu));
  } else {
    log_line("AddCustomizableMenu API unavailable; actions will register without the top-level menu.");
  }
  return true;
}

void unload()
{
  log_line("Unloading ReaADR extension.");
  if (g_plugin) {
    unregister_native_actions();
    g_plugin->Register("-hookcustommenu", reinterpret_cast<void*>(hook_custom_menu));
    reaadr::reaper::unregister_host_api_exports(
      host_api_exports(), [](const char* key, void* value) {
        return g_plugin->Register(key, value) != 0;
      });
  }
  g_plugin = nullptr;
}

} // namespace

namespace reaadr::reaper::legacy_host {

bool load(REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t* plugin)
{
  if (!plugin) return false;
  g_instance = instance;
  return ::load(plugin);
}

void unload()
{
  ::unload();
}

void log(const std::string& message)
{
  log_line(message);
}

reaadr::reaper::OverlayApplicationApi overlay_api()
{
  return reaadr::reaper::native_overlay_application_api();
}

CueNavigationApi cue_navigation_api()
{
  return {GetPlayState, GetPlayPosition, GetCursorPosition, SetEditCurPos};
}

CharacterFilterApplicationApi character_filter_api()
{
  return {native_character_filter_inspect, native_character_filter_apply};
}

CueCleanupApplicationApi cue_cleanup_api()
{
  return {native_cleanup_inspect, native_cleanup_apply, native_utc_timestamp()};
}

SessionRefreshHost session_refresh_host()
{
  return {native_track_region_api(), native_ruler_lane_api(),
          native_cue_audio_api(), native_session_transaction_api(),
          native_cue_audio_path(), native_utc_timestamp()};
}

void release_action_registrations()
{
  unregister_native_actions();
}

bool bind_migrated_action_ids(reaper_plugin_info_t* plugin)
{
  if (!plugin || !plugin->GetFunc) return false;
  const auto& ids = reaadr::reaper::workflow_action_ids();
  g_validate_session_action.command_id = ids.validate_session;
  g_refresh_overlay_action.command_id = ids.refresh_overlay;
  g_refresh_session_action.command_id = ids.refresh_session;
  g_update_cues_from_regions_action.command_id = ids.update_cues_from_regions;
  g_clear_character_cues_action.command_id = ids.clear_character_cues;
  g_character_filter_action.command_id = ids.character_filter;
  g_next_cue_action.command_id = ids.next_cue;
  g_previous_cue_action.command_id = ids.previous_cue;
  g_jump_to_cue_action.command_id = ids.jump_to_cue;
  g_cue_manager_action.command_id = ids.cue_manager;
  g_import_cue_sheet_action.command_id = ids.import_cue_sheet;
  g_preferences_action.command_id = ids.preferences;
  g_ui_test_action.command_id = ids.ui_test;

  using NamedCommandLookupFn = int (*)(const char*);
  auto lookup = reinterpret_cast<NamedCommandLookupFn>(plugin->GetFunc("NamedCommandLookup"));
  if (!lookup) return false;
  constexpr std::array<const char*, 4> names = {{
    "_ReaADRQuickAction1Native", "_ReaADRQuickAction2Native",
    "_ReaADRQuickAction3Native", "_ReaADRQuickAction4Native"
  }};
  std::array<int, 4> quick_ids = {};
  for (std::size_t index = 0; index < names.size(); ++index) {
    quick_ids[index] = lookup(names[index]);
    if (!quick_ids[index]) {
      log_line(std::string("Native Quick Action command unavailable: ") + names[index]);
      return false;
    }
  }
  for (std::size_t index = 0; index < quick_ids.size(); ++index)
    g_actions[index + 1].command_id = quick_ids[index];
  log_line("Bound native workflow and Quick Action command IDs to the ReaADR menu.");
  return true;
}

} // namespace reaadr::reaper::legacy_host
