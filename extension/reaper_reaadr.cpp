#define REAPERAPI_IMPLEMENT
#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_AddCustomizableMenu
#define REAPERAPI_WANT_AddRemoveReaScript
#define REAPERAPI_WANT_CountSelectedMediaItems
#define REAPERAPI_WANT_CountTracks
#define REAPERAPI_WANT_CreateTakeAudioAccessor
#define REAPERAPI_WANT_DestroyAudioAccessor
#define REAPERAPI_WANT_GetActiveTake
#define REAPERAPI_WANT_GetAudioAccessorEndTime
#define REAPERAPI_WANT_GetAudioAccessorSamples
#define REAPERAPI_WANT_GetAudioAccessorStartTime
#define REAPERAPI_WANT_GetExtState
#define REAPERAPI_WANT_GetMediaItemInfo_Value
#define REAPERAPI_WANT_GetProjExtState
#define REAPERAPI_WANT_GetRegionOrMarker
#define REAPERAPI_WANT_GetRegionOrMarkerInfo_Value
#define REAPERAPI_WANT_GetResourcePath
#define REAPERAPI_WANT_GetSelectedMediaItem
#define REAPERAPI_WANT_GetTrack
#define REAPERAPI_WANT_GetUserInputs
#define REAPERAPI_WANT_GetSetMediaItemInfo_String
#define REAPERAPI_WANT_GetSetMediaTrackInfo_String
#define REAPERAPI_WANT_EnumProjectMarkers3
#define REAPERAPI_WANT_PreventUIRefresh
#define REAPERAPI_WANT_SetProjExtState
#define REAPERAPI_WANT_SetExtState
#define REAPERAPI_WANT_ShowMessageBox
#define REAPERAPI_WANT_TimeMap_curFrameRate
#define REAPERAPI_WANT_TrackFX_AddByName
#define REAPERAPI_WANT_TrackFX_Delete
#define REAPERAPI_WANT_TrackFX_GetCount
#define REAPERAPI_WANT_TrackFX_GetEnabled
#define REAPERAPI_WANT_TrackFX_GetNamedConfigParm
#define REAPERAPI_WANT_TrackFX_SetEnabled
#define REAPERAPI_WANT_TrackFX_SetNamedConfigParm
#define REAPERAPI_WANT_TrackList_AdjustWindows
#define REAPERAPI_WANT_UpdateArrange
#define REAPERAPI_WANT_Undo_BeginBlock2
#define REAPERAPI_WANT_Undo_CanUndo2
#define REAPERAPI_WANT_Undo_DoUndo2
#define REAPERAPI_WANT_Undo_EndBlock2
#define REAPERAPI_WANT_ValidatePtr2
#define REAPERAPI_WANT_GetNumRegionsOrMarkers
#define REAPERAPI_WANT_GetCursorPosition
#define REAPERAPI_WANT_GetPlayPosition
#define REAPERAPI_WANT_GetPlayState
#define REAPERAPI_WANT_SetEditCurPos

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>

#include "reaadr_core/session_model.hpp"
#include "reaadr_core/model_repository.hpp"
#include "reaadr_core/cue_status.hpp"
#include "reaadr_core/cue_manager_model.hpp"
#include "reaadr_reaper/overlay_application_service.hpp"
#include "reaadr_reaper/overlay_refresh_adapter.hpp"
#include "reaadr_reaper/cue_navigation_service.hpp"
#include "reaadr_reaper/project_state.hpp"
#include "reaadr_reaper/project_transaction.hpp"

#ifndef _WIN32
#include <dlfcn.h>
#endif

namespace {

constexpr int kMainSection = 0;
constexpr const char* kReaADRMenuId = "ReaADR Tools";
constexpr const char* kValidateSessionCommandName = "ReaADRValidateSessionModelNative";
constexpr const char* kValidateSessionActionLabel = "ReaADR: Validate Session Model (Native Preview)";
constexpr const char* kRefreshOverlayCommandName = "ReaADRRefreshVideoOverlayNative";
constexpr const char* kRefreshOverlayActionLabel = "ReaADR: Refresh Video Overlay (Native)";
constexpr const char* kNextCueCommandName = "ReaADRNextCueNative";
constexpr const char* kPreviousCueCommandName = "ReaADRPreviousCueNative";
constexpr const char* kJumpToCueCommandName = "ReaADRJumpToCueNative";
constexpr const char* kCueManagerCommandName = "ReaADRShowCueManagerNative";
constexpr const char* kPreferencesCommandName = "ReaADRShowPreferencesNative";

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
int g_next_cue_command_id = 0;
gaccel_register_t g_next_cue_accel = {};
int g_previous_cue_command_id = 0;
gaccel_register_t g_previous_cue_accel = {};
int g_jump_to_cue_command_id = 0;
gaccel_register_t g_jump_to_cue_accel = {};
int g_cue_manager_command_id = 0;
gaccel_register_t g_cue_manager_accel = {};
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
const char kValidateSessionModelDef[] =
  "bool\0"
  "const char*,char*,int\0"
  "model,errorOut,errorOut_sz\0"
  "Validate an adr_session_model_v1 blob using the native C++ session-model codec.";

struct ScriptAction {
  const char* label;
  const char* relative_path;
  int command_id = 0;
};

std::vector<ScriptAction> g_actions = {
  {"Open Manager", "Scripts/ReaADRTools/scripts/ReaADR_Open_Manager.lua", 0},
  {"Quick Action 1", "Scripts/ReaADRTools/scripts/ReaADR_Quick_Action_1.lua", 0},
  {"Quick Action 2", "Scripts/ReaADRTools/scripts/ReaADR_Quick_Action_2.lua", 0},
  {"Quick Action 3", "Scripts/ReaADRTools/scripts/ReaADR_Quick_Action_3.lua", 0},
  {"Quick Action 4", "Scripts/ReaADRTools/scripts/ReaADR_Quick_Action_4.lua", 0},
};

// This action is native; the shared menu helpers only require its label and
// command ID, so there is intentionally no script path.
ScriptAction g_validate_session_action = {
  "Validate Session Model (Native Preview)",
  nullptr,
  0,
};

ScriptAction g_refresh_overlay_action = {
  "Refresh Video Overlay (Native)",
  nullptr,
  0,
};
ScriptAction g_next_cue_action = {"Next Cue (Native)", nullptr, 0};
ScriptAction g_previous_cue_action = {"Previous Cue (Native)", nullptr, 0};
ScriptAction g_jump_to_cue_action = {"Jump To Cue (Native)", nullptr, 0};
ScriptAction g_cue_manager_action = {"Cue Summary (Native Preview)", nullptr, 0};

std::vector<ScriptAction> g_legacy_actions = {
  {"Import Script", "Scripts/ReaADRTools/scripts/ReaADR_Import_Script.lua", 0},
  {"Export Reports", "Scripts/ReaADRTools/scripts/ReaADR_Export_Reports.lua", 0},
  {"Preferences", "Scripts/ReaADRTools/scripts/ReaADR_Preferences.lua", 0},
  {"Import Cue Sheet", "Scripts/ReaADRTools/scripts/ReaADR_Import_Cue_Sheet.lua", 0},
  {"Export Cue Sheet", "Scripts/ReaADRTools/scripts/ReaADR_Export_Cue_Sheet.lua", 0},
  {"Next Cue", "Scripts/ReaADRTools/scripts/ReaADR_Next_Cue.lua", 0},
  {"Previous Cue", "Scripts/ReaADRTools/scripts/ReaADR_Previous_Cue.lua", 0},
  {"Jump To Cue", "Scripts/ReaADRTools/scripts/ReaADR_Jump_To_Cue.lua", 0},
  {"Set Cue Status", "Scripts/ReaADRTools/scripts/ReaADR_Set_Cue_Status.lua", 0},
  {"Character Filter", "Scripts/ReaADRTools/scripts/ReaADR_Character_Filter.lua", 0},
  {"Generate Cues from Markers/Regions", "Scripts/ReaADRTools/scripts/ReaADR_Generate_Cues.lua", 0},
  {"Clear Character Cues", "Scripts/ReaADRTools/scripts/ReaADR_Clean_Generated_Cues.lua", 0},
  {"Overlay Settings", "Scripts/ReaADRTools/scripts/ReaADR_Overlay_Settings.lua", 0},
  {"Open ReaADR Menu", "Scripts/ReaADRTools/scripts/ReaADR_Menu.lua", 0},
  {"Start Recording Workflow", "Scripts/ReaADRTools/scripts/ReaADR_Start_Recording_Workflow.lua", 0},
  {"Monitor New Markers/Regions", "Scripts/ReaADRTools/scripts/ReaADR_Monitor_Markers.lua", 0},
  {"Jump To Selected Cue", "Scripts/ReaADRTools/scripts/ReaADR_Jump_To_Selected_Cue.lua", 0},
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

void register_scripts()
{
  const std::string root = resource_directory();
  const std::string old_root = plugin_directory();
  log_line("Registering resource scripts from: " + root);

  for (const ScriptAction& legacy_action : g_legacy_actions) {
    const std::string old_script_path = join_path(old_root, legacy_action.relative_path + 8);
    AddRemoveReaScript(false, kMainSection, old_script_path.c_str(), false);
    const std::string script_path = join_path(root, legacy_action.relative_path);
    AddRemoveReaScript(false, kMainSection, script_path.c_str(), false);
  }

  for (std::size_t i = 0; i < g_actions.size(); ++i) {
    const bool commit = i + 1 == g_actions.size();
    const std::string old_script_path = join_path(old_root, g_actions[i].relative_path + 8);
    AddRemoveReaScript(false, kMainSection, old_script_path.c_str(), false);
    const std::string script_path = join_path(root, g_actions[i].relative_path);
    g_actions[i].command_id = AddRemoveReaScript(true, kMainSection, script_path.c_str(), commit);
    log_line(std::string("Registered ") + g_actions[i].label + " command_id=" + std::to_string(g_actions[i].command_id));
  }
}

void unregister_scripts()
{
  for (std::size_t i = 0; i < g_actions.size(); ++i) {
    const bool commit = i + 1 == g_actions.size();
    const std::string script_path = join_path(resource_directory(), g_actions[i].relative_path);
    AddRemoveReaScript(false, kMainSection, script_path.c_str(), commit);
    g_actions[i].command_id = 0;
  }
}

void add_menu_item(HMENU menu, int position, const ScriptAction& action)
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

bool validate_session_model(const char* model, char* error_out, int error_out_sz)
{
  // Transitional bridge: Lua can ask the new native core to validate a blob
  // while Lua remains the writer. Remove this API after the native repository
  // and all model consumers have moved into the extension.
  copy_to_buffer("", error_out, error_out_sz);
  const reaadr::core::ParseResult result = reaadr::core::parse_session_model(model ? model : "");
  if (result) return true;
  copy_to_buffer(reaadr::core::parse_error_message(result.error), error_out, error_out_sz);
  return false;
}

void run_validate_session_action()
{
  reaadr::reaper::ProjectStateStore project_state(
    nullptr,
    {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository repository(project_state);
  const reaadr::core::SessionLoadResult result = repository.load();

  if (!result) {
    ShowMessageBox(reaadr::core::session_load_error_message(result), "ReaADR Session Model", 0);
    return;
  }

  const reaadr::core::SessionModel& model = result.model;
  const reaadr::core::RevisionResult revision = repository.revision();
  const auto name = model.session.find("session_name");
  std::ostringstream summary;
  summary << "The ADR Session Model is valid.\n\n";
  summary << "Session ID: " << model.session_id() << '\n';
  if (name != model.session.end() && !name->second.empty()) summary << "Session: " << name->second << '\n';
  if (revision) summary << "Revision: " << revision.revision << '\n';
  summary << "Scripts: " << model.scripts.size() << '\n';
  summary << "Characters: " << model.characters.size() << '\n';
  summary << "Cues: " << model.cues.size() << '\n';
  summary << "Tracks: " << model.tracks.size() << '\n';
  summary << "Regions: " << model.regions.size();
  const std::string message = summary.str();
  ShowMessageBox(message.c_str(), "ReaADR Session Model", 0);
}

void run_native_cue_manager_action()
{
  reaadr::reaper::ProjectStateStore project_state(nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository repository(project_state);
  const auto loaded = repository.load();
  if (!loaded) {
    ShowMessageBox(reaadr::core::session_load_error_message(loaded), "ReaADR Cue Manager", 0);
    return;
  }
  reaadr::core::CueManagerViewOptions view_options;
  if (GetUserInputs) {
    std::array<char, 1024> filter_input = {};
    if (GetUserInputs("ReaADR Cue Manager: Filter", 3, "Search,Character,Status",
                      filter_input.data(), filter_input.size())) {
      char* first = std::strchr(filter_input.data(), ',');
      char* second = first ? std::strchr(first + 1, ',') : nullptr;
      if (first && second) {
        *first = '\0'; *second = '\0';
        view_options.query = filter_input.data();
        view_options.character = first + 1;
        view_options.status = second + 1;
      }
    }
  }
  const auto view = reaadr::core::build_cue_manager_view(loaded.model, view_options);
  if (!view) { ShowMessageBox(view.error.c_str(), "ReaADR Cue Manager", 0); return; }
  std::ostringstream summary;
  summary << "Session: " << view.session_id << "\n"
          << "Showing " << view.rows.size() << " of " << loaded.model.cues.size() << " cues\n\n";
  for (const auto& row : view.rows) {
    summary << (row.selected ? "> " : "  ") << row.cue_key << "  " << row.character
            << "  [" << row.status << "]  " << row.dialogue << "\n";
  }
  ShowMessageBox(summary.str().c_str(), "ReaADR Cue Manager (Native)", 0);
  if (!GetUserInputs) return;
  std::array<char, 1024> input = {};
  if (!GetUserInputs("ReaADR Cue Manager: Set Status", 2, "Cue ID,Status", input.data(), input.size())) return;
  char* comma = std::strchr(input.data(), ',');
  if (!comma || comma == input.data() || *(comma + 1) == '\0') {
    ShowMessageBox("Enter a cue ID and status separated by a comma.", "ReaADR Cue Manager", 0);
    return;
  }
  *comma = '\0';
  reaadr::core::CueStatusCommitOptions options;
  options.update.cue_key = input.data();
  options.update.status = comma + 1;
  options.utc_timestamp = "";
  const auto updated = reaadr::core::commit_cue_status(repository, options);
  if (!updated) ShowMessageBox(updated.error.c_str(), "ReaADR Cue Manager", 0);
}

MediaTrack* native_overlay_get_track(ReaProject* project, int index)
{
  return GetTrack ? GetTrack(project, index) : nullptr;
}

bool native_overlay_validate_track(ReaProject* project, MediaTrack* track)
{
  return ValidatePtr2 && ValidatePtr2(project, track, "MediaTrack*");
}

bool native_overlay_get_set_track_string(MediaTrack* track, const char* parameter,
                                         char* value, bool set_value)
{
  return GetSetMediaTrackInfo_String &&
    GetSetMediaTrackInfo_String(track, parameter, value, set_value);
}

std::string selected_overlay_cue_key_from_regions()
{
  if (!GetNumRegionsOrMarkers || !EnumProjectMarkers3 || !GetRegionOrMarker ||
      !GetRegionOrMarkerInfo_Value) return {};
  const int count = GetNumRegionsOrMarkers(nullptr);
  for (int index = 0; index < count; ++index) {
    bool is_region = false;
    const char* name = nullptr;
    if (!EnumProjectMarkers3(nullptr, index, &is_region, nullptr, nullptr, &name, nullptr, nullptr) ||
        !is_region || !name) continue;
    ProjectMarker* marker = GetRegionOrMarker(nullptr, index, "");
    if (!marker || GetRegionOrMarkerInfo_Value(nullptr, marker, "B_UISEL") == 0.0) continue;
    const std::string text(name);
    const std::string marker_prefix = "[ReaADR]:id=";
    const std::size_t begin = text.find(marker_prefix);
    if (begin == std::string::npos) continue;
    const std::size_t value_begin = begin + marker_prefix.size();
    const std::size_t value_end = text.find_first_of(" \t\r\n", value_begin);
    return text.substr(value_begin, value_end == std::string::npos ? std::string::npos : value_end - value_begin);
  }
  return {};
}

reaadr::reaper::OverlaySelectionInput native_overlay_selection()
{
  reaadr::reaper::OverlaySelectionInput selection;
  selection.selected_region_cue_key = selected_overlay_cue_key_from_regions();
  if (CountSelectedMediaItems && GetSelectedMediaItem && GetSetMediaItemInfo_String) {
    const int count = CountSelectedMediaItems(nullptr);
    std::array<char, 4096> value = {};
    for (int index = 0; index < count; ++index) {
      MediaItem* item = GetSelectedMediaItem(nullptr, index);
      if (item && GetSetMediaItemInfo_String(item, "P_EXT:ReaADR.cue_key", value.data(), false) &&
          value[0] != '\0') {
        selection.selected_item_cue_key = value.data();
        break;
      }
    }
  }
  return selection;
}

double native_overlay_frame_rate()
{
  bool drop_frame = false;
  const double frame_rate = TimeMap_curFrameRate
    ? TimeMap_curFrameRate(nullptr, &drop_frame) : 24.0;
  return std::isfinite(frame_rate) && frame_rate > 0.0 ? frame_rate : 24.0;
}

reaadr::reaper::OverlayRefreshApi native_overlay_refresh_api()
{
  return {
    CountTracks,
    native_overlay_get_track,
    native_overlay_validate_track,
    native_overlay_get_set_track_string,
    TrackFX_GetCount,
    TrackFX_GetNamedConfigParm,
    TrackFX_GetEnabled,
    TrackFX_AddByName,
    TrackFX_Delete,
    TrackFX_SetNamedConfigParm,
    TrackFX_SetEnabled,
    TrackList_AdjustWindows,
    UpdateArrange,
  };
}

reaadr::reaper::TransactionApi native_overlay_transaction_api()
{
  return {
    Undo_BeginBlock2,
    Undo_EndBlock2,
    Undo_CanUndo2,
    Undo_DoUndo2,
    PreventUIRefresh,
  };
}

bool native_overlay_refresh_callback(
  const reaadr::core::OverlayRefreshOptions& options, std::string* error)
{
  const auto applied = reaadr::reaper::refresh_generated_overlay_transactionally(
    nullptr, native_overlay_refresh_api(), native_overlay_transaction_api(), options,
    "ReaADR: refresh video overlay");
  if (!applied && error) *error = applied.error;
  return static_cast<bool>(applied);
}

void run_refresh_overlay_action()
{
  reaadr::reaper::ProjectStateStore project_state(
    nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository sessions(project_state);
  reaadr::core::OverlaySettingsRepository settings(project_state);
  reaadr::core::CueSelectionRepository selections(project_state);
  reaadr::core::CharacterFilterRepository filters(project_state);
  const reaadr::reaper::OverlayApplicationApi api = {
    native_overlay_frame_rate, native_overlay_selection, native_overlay_refresh_callback,
  };
  reaadr::reaper::OverlayApplicationService service(sessions, settings, selections, filters, api);
  const auto result = service.refresh();
  if (!result) {
    ShowMessageBox(result.error.c_str(), "ReaADR Video Overlay", 0);
    return;
  }
  log_line("Native overlay refresh completed; displayed cues=" +
    std::to_string(result.displayed_cue_count));
}

void run_cue_navigation_action(bool next)
{
  reaadr::reaper::ProjectStateStore project_state(
    nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository sessions(project_state);
  reaadr::core::CueSelectionRepository selections(project_state);
  const reaadr::reaper::CueNavigationApi api = {
    GetPlayState, GetPlayPosition, GetCursorPosition, SetEditCurPos,
  };
  reaadr::reaper::CueNavigationService service(sessions, selections, api);
  const auto result = next ? service.navigate_next() : service.navigate_previous();
  if (!result) {
    ShowMessageBox(result.error.c_str(), "ReaADR Cue Navigation", 0);
    return;
  }
  log_line(std::string("Native cue navigation moved to ") + result.selection.state.active_overlay_cue_key);
}

void run_jump_to_cue_action()
{
  if (!GetUserInputs) {
    ShowMessageBox("The REAPER cue-ID input API is unavailable.", "ReaADR Cue Navigation", 0);
    return;
  }
  std::array<char, 1024> input = {};
  if (!GetUserInputs("ReaADR: Jump To Cue", 1, "Cue ID:", input.data(), input.size())) return;

  reaadr::reaper::ProjectStateStore project_state(
    nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository sessions(project_state);
  reaadr::core::CueSelectionRepository selections(project_state);
  const reaadr::reaper::CueNavigationApi api = {
    GetPlayState, GetPlayPosition, GetCursorPosition, SetEditCurPos,
  };
  reaadr::reaper::CueNavigationService service(sessions, selections, api);
  const auto result = service.navigate_to_id(input.data());
  if (!result) {
    ShowMessageBox(result.error.c_str(), "ReaADR Cue Navigation", 0);
    return;
  }
  log_line("Native cue navigation jumped to " + result.selection.state.active_overlay_cue_key);
}

bool hook_native_command(int command, int)
{
  if (command == g_validate_session_command_id && command != 0) {
    run_validate_session_action();
    return true;
  }
  if (command == g_refresh_overlay_command_id && command != 0) {
    run_refresh_overlay_action();
    return true;
  }
  if (command == g_next_cue_command_id && command != 0) {
    run_cue_navigation_action(true);
    return true;
  }
  if (command == g_previous_cue_command_id && command != 0) {
    run_cue_navigation_action(false);
    return true;
  }
  if (command == g_jump_to_cue_command_id && command != 0) {
    run_jump_to_cue_action();
    return true;
  }
  if (command == g_cue_manager_command_id && command != 0) {
    run_native_cue_manager_action();
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
                                              ScriptAction& action) {
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
}

std::string read_text_file(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) return {};
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

std::string native_separator()
{
#ifdef _WIN32
  return "\\";
#else
  return "/";
#endif
}

std::string join_native_path(const std::string& base, const std::string& relative)
{
  if (base.empty()) return relative;
  const char last = base[base.size() - 1];
  if (last == '/' || last == '\\') return base + relative;
  return base + native_separator() + relative;
}

std::string shell_quote_posix(const std::string& value)
{
  std::string out = "'";
  for (char ch : value) {
    if (ch == '\'') {
      out += "'\"'\"'";
    } else {
      out += ch;
    }
  }
  out += "'";
  return out;
}

#ifdef _WIN32
std::string powershell_quote(const std::string& value)
{
  std::string out = "'";
  for (char ch : value) {
    if (ch == '\'') out += '\'';
    out += ch;
  }
  out += "'";
  return out;
}
#endif

bool run_command(const std::string& command)
{
  const int result = std::system(command.c_str());
  return result == 0;
}

std::string make_temp_directory()
{
  const char* base_env = std::getenv("TMPDIR");
  if (!base_env || !*base_env) base_env = std::getenv("TEMP");
  if (!base_env || !*base_env) base_env = std::getenv("TMP");
#ifdef _WIN32
  const std::string base = (base_env && *base_env) ? base_env : ".";
#else
  const std::string base = (base_env && *base_env) ? base_env : "/tmp";
#endif
  const std::string dir = join_native_path(base, "reaadr_xlsx_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(std::rand()));
#ifdef _WIN32
  const std::string command = "powershell -NoProfile -ExecutionPolicy Bypass -Command \"New-Item -ItemType Directory -Force -LiteralPath " + powershell_quote(dir) + " | Out-Null\"";
#else
  const std::string command = "mkdir -p " + shell_quote_posix(dir);
#endif
  return run_command(command) ? dir : "";
}

void remove_temp_directory(const std::string& dir)
{
  if (dir.empty()) return;
#ifdef _WIN32
  const std::string command = "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Remove-Item -LiteralPath " + powershell_quote(dir) + " -Recurse -Force -ErrorAction SilentlyContinue\"";
#else
  const std::string command = "rm -rf " + shell_quote_posix(dir);
#endif
  run_command(command);
}

std::string xml_unescape(std::string value)
{
  auto replace_all = [&value](const std::string& from, const std::string& to) {
    std::string::size_type pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
      value.replace(pos, from.size(), to);
      pos += to.size();
    }
  };
  replace_all("&lt;", "<");
  replace_all("&gt;", ">");
  replace_all("&quot;", "\"");
  replace_all("&apos;", "'");
  replace_all("&amp;", "&");
  return value;
}

int column_letters_to_index(const std::string& letters)
{
  int index = 0;
  for (char ch : letters) {
    if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    if (ch < 'A' || ch > 'Z') continue;
    index = (index * 26) + (ch - 'A' + 1);
  }
  return index;
}

std::string regex_first_group(const std::string& text, const std::regex& pattern)
{
  std::smatch match;
  return std::regex_search(text, match, pattern) && match.size() > 1 ? match[1].str() : "";
}

std::vector<std::string> parse_xlsx_shared_strings(const std::string& dir)
{
  const std::string xml = read_text_file(join_native_path(join_native_path(dir, "xl"), "sharedStrings.xml"));
  std::vector<std::string> strings;
  if (xml.empty()) return strings;

  const std::regex si_re(R"(<si[\s\S]*?</si>)");
  const std::regex text_re(R"(<t[^>]*>([\s\S]*?)</t>)");
  for (std::sregex_iterator it(xml.begin(), xml.end(), si_re), end; it != end; ++it) {
    const std::string si = it->str();
    std::string value;
    for (std::sregex_iterator text_it(si.begin(), si.end(), text_re), text_end; text_it != text_end; ++text_it) {
      value += xml_unescape((*text_it)[1].str());
    }
    strings.push_back(value);
  }
  return strings;
}

std::string first_xlsx_sheet_path(const std::string& dir)
{
  const std::string workbook = read_text_file(join_native_path(join_native_path(dir, "xl"), "workbook.xml"));
  const std::string rels = read_text_file(join_native_path(join_native_path(join_native_path(dir, "xl"), "_rels"), "workbook.xml.rels"));
  const std::string rel_id = regex_first_group(workbook, std::regex(R"re(<sheet[^>]*r:id="([^"]+)")re"));
  if (!rel_id.empty() && !rels.empty()) {
    const std::regex rel_re(R"(<Relationship[^>]+>)");
    for (std::sregex_iterator it(rels.begin(), rels.end(), rel_re), end; it != end; ++it) {
      const std::string rel = it->str();
      const std::string id = regex_first_group(rel, std::regex(R"re(Id="([^"]+)")re"));
      std::string target = regex_first_group(rel, std::regex(R"re(Target="([^"]+)")re"));
      if (id == rel_id && !target.empty()) {
        if (!target.empty() && target[0] == '/') target.erase(0, 1);
        if (target.rfind("xl/", 0) != 0) target = "xl/" + target;
        std::replace(target.begin(), target.end(), '/', native_separator()[0]);
        return join_native_path(dir, target);
      }
    }
  }
  return join_native_path(join_native_path(join_native_path(dir, "xl"), "worksheets"), "sheet1.xml");
}

std::string parse_xlsx_cell_value(const std::string& cell_xml, const std::vector<std::string>& shared_strings)
{
  const std::string cell_type = regex_first_group(cell_xml, std::regex(R"re(<c[^>]*t="([^"]+)")re"));
  if (cell_type == "inlineStr") {
    const std::regex text_re(R"(<t[^>]*>([\s\S]*?)</t>)");
    std::string value;
    for (std::sregex_iterator it(cell_xml.begin(), cell_xml.end(), text_re), end; it != end; ++it) {
      value += xml_unescape((*it)[1].str());
    }
    return value;
  }

  const std::string raw = xml_unescape(regex_first_group(cell_xml, std::regex(R"(<v[^>]*>([\s\S]*?)</v>)")));
  if (cell_type == "s") {
    const int index = std::atoi(raw.c_str());
    return index >= 0 && static_cast<std::size_t>(index) < shared_strings.size() ? shared_strings[static_cast<std::size_t>(index)] : "";
  }
  if (cell_type == "b") return raw == "1" ? "TRUE" : "FALSE";
  return raw;
}

std::string tsv_escape(std::string value)
{
  for (char& ch : value) {
    if (ch == '\r' || ch == '\n') ch = ' ';
  }

  const bool needs_quote = value.find('\t') != std::string::npos || value.find('"') != std::string::npos;
  if (!needs_quote) return value;

  std::string out = "\"";
  for (char ch : value) {
    if (ch == '"') out += '"';
    out += ch;
  }
  out += '"';
  return out;
}

bool extract_xlsx_to_directory(const std::string& path, const std::string& dir)
{
#ifdef _WIN32
  const std::string zip_path = join_native_path(dir, "workbook.zip");
  const std::string command =
    "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Copy-Item -LiteralPath " +
    powershell_quote(path) + " -Destination " + powershell_quote(zip_path) +
    " -Force; Expand-Archive -LiteralPath " + powershell_quote(zip_path) +
    " -DestinationPath " + powershell_quote(dir) + " -Force\"";
#else
  const std::string command =
    "(unzip -qq " + shell_quote_posix(path) + " -d " + shell_quote_posix(dir) +
    ") || (ditto -x -k " + shell_quote_posix(path) + " " + shell_quote_posix(dir) + ")";
#endif
  return run_command(command);
}

bool read_xlsx_as_tsv(const char* path, char* tsv_out, int tsv_out_sz, char* error_out, int error_out_sz)
{
  copy_to_buffer("", tsv_out, tsv_out_sz);
  copy_to_buffer("", error_out, error_out_sz);
  if (!path || !*path) {
    copy_to_buffer("No XLSX path was provided.", error_out, error_out_sz);
    return false;
  }

  const std::string dir = make_temp_directory();
  if (dir.empty()) {
    copy_to_buffer("Could not create a temporary directory for XLSX import.", error_out, error_out_sz);
    return false;
  }

  std::string error;
  std::string output;
  if (!extract_xlsx_to_directory(path, dir)) {
    error = "Could not extract XLSX file. Make sure the system archive tools are available.";
  } else {
    const std::vector<std::string> shared_strings = parse_xlsx_shared_strings(dir);
    const std::string sheet_xml = read_text_file(first_xlsx_sheet_path(dir));
    if (sheet_xml.empty()) {
      error = "Could not read the first worksheet in the XLSX file.";
    } else {
      const std::regex row_re(R"(<row[^>]*>[\s\S]*?</row>)");
      const std::regex cell_re(R"(<c[^>]*>[\s\S]*?</c>)");
      const std::regex ref_re(R"(<c[^>]*r="([A-Z]+)\d+")");
      std::vector<std::map<int, std::string>> rows;
      int max_col = 0;

      for (std::sregex_iterator row_it(sheet_xml.begin(), sheet_xml.end(), row_re), row_end; row_it != row_end; ++row_it) {
        const std::string row_xml = row_it->str();
        std::map<int, std::string> row;
        bool has_value = false;
        for (std::sregex_iterator cell_it(row_xml.begin(), row_xml.end(), cell_re), cell_end; cell_it != cell_end; ++cell_it) {
          const std::string cell_xml = cell_it->str();
          const int col = column_letters_to_index(regex_first_group(cell_xml, ref_re));
          if (col <= 0) continue;
          const std::string value = parse_xlsx_cell_value(cell_xml, shared_strings);
          if (!value.empty()) has_value = true;
          row[col] = value;
          max_col = (std::max)(max_col, col);
        }
        if (has_value) rows.push_back(row);
      }

      if (rows.empty() || max_col <= 0) {
        error = "XLSX worksheet is empty.";
      } else {
        std::ostringstream tsv;
        for (const auto& row : rows) {
          for (int col = 1; col <= max_col; ++col) {
            if (col > 1) tsv << '\t';
            const auto found = row.find(col);
            if (found != row.end()) tsv << tsv_escape(found->second);
          }
          tsv << '\n';
        }
        output = tsv.str();
      }
    }
  }

  remove_temp_directory(dir);
  if (!error.empty()) {
    copy_to_buffer(error, error_out, error_out_sz);
    return false;
  }
  if (static_cast<int>(output.size()) >= tsv_out_sz) {
    copy_to_buffer("XLSX import output exceeded the provided buffer size.", error_out, error_out_sz);
    return false;
  }

  copy_to_buffer(output, tsv_out, tsv_out_sz);
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

void add_menu_item_with_label(HMENU menu, int position, const ScriptAction& action, const std::string& label)
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

void update_menu_item_label(HMENU menu, int position, const ScriptAction& action, const std::string& label)
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
    for (std::size_t i = 0; i < g_actions.size(); ++i) {
      if (i == 0) {
        add_menu_item(hmenu, position++, g_actions[i]);
      } else {
        add_menu_item_with_label(hmenu, position++, g_actions[i], quick_action_label(static_cast<int>(i)));
      }
    }
    add_menu_item(hmenu, position, g_validate_session_action);
    add_menu_item(hmenu, position + 1, g_refresh_overlay_action);
    add_menu_item(hmenu, position + 2, g_next_cue_action);
    add_menu_item(hmenu, position + 3, g_previous_cue_action);
    add_menu_item(hmenu, position + 4, g_jump_to_cue_action);
    add_menu_item(hmenu, position + 5, g_cue_manager_action);
    log_line("Added top-level ReaADR Tools menu.");
    return;
  }

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
  const int next_position = validation_position + 2;
  if (next_position < existing_items) {
    update_menu_item_label(hmenu, next_position, g_next_cue_action, g_next_cue_action.label);
  } else {
    add_menu_item(hmenu, position + 2, g_next_cue_action);
  }
  const int previous_position = validation_position + 3;
  if (previous_position < existing_items) {
    update_menu_item_label(hmenu, previous_position, g_previous_cue_action, g_previous_cue_action.label);
  } else {
    add_menu_item(hmenu, position + 3, g_previous_cue_action);
  }
  const int jump_position = validation_position + 4;
  if (jump_position < existing_items) {
    update_menu_item_label(hmenu, jump_position, g_jump_to_cue_action, g_jump_to_cue_action.label);
  } else {
    add_menu_item(hmenu, position + 4, g_jump_to_cue_action);
  }
  const int manager_position = validation_position + 5;
  if (manager_position < existing_items) {
    update_menu_item_label(hmenu, manager_position, g_cue_manager_action, g_cue_manager_action.label);
  } else {
    add_menu_item(hmenu, position + 5, g_cue_manager_action);
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
  log_line(std::string("CreatePopupMenu available: ") + (g_create_popup_menu ? "yes" : "no"));
  log_line(std::string("GetMenuItemCount available: ") + (g_get_menu_item_count ? "yes" : "no"));
  log_line(std::string("InsertMenuItem available: ") + (g_insert_menu_item ? "yes" : "no"));
  log_line(std::string("SetMenuItemInfo available: ") + (g_set_menu_item_info ? "yes" : "no"));
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
  if (!AddRemoveReaScript) {
    log_line("AddRemoveReaScript API unavailable; cannot register ReaADR scripts.");
    return false;
  }

  if (AddCustomizableMenu) {
    AddCustomizableMenu(kReaADRMenuId, kReaADRMenuId, nullptr, true);
  } else {
    log_line("AddCustomizableMenu API unavailable; actions will register without the top-level menu.");
  }
  plugin->Register("API_ReaADR_DetectDialogueSegments", reinterpret_cast<void*>(detect_dialogue_segments));
  plugin->Register("APIdef_ReaADR_DetectDialogueSegments", reinterpret_cast<void*>(const_cast<char*>(kDetectDialogueSegmentsDef)));
  plugin->Register("API_ReaADR_ReadXlsxAsTsv", reinterpret_cast<void*>(read_xlsx_as_tsv));
  plugin->Register("APIdef_ReaADR_ReadXlsxAsTsv", reinterpret_cast<void*>(const_cast<char*>(kReadXlsxAsTsvDef)));
  plugin->Register("API_ReaADR_ValidateSessionModel", reinterpret_cast<void*>(validate_session_model));
  plugin->Register("APIdef_ReaADR_ValidateSessionModel", reinterpret_cast<void*>(const_cast<char*>(kValidateSessionModelDef)));
  register_native_actions();
  register_scripts();
  load_menu_functions();
  if (AddCustomizableMenu) {
    plugin->Register("hookcustommenu", reinterpret_cast<void*>(hook_custom_menu));
  }
  return true;
}

void unload()
{
  log_line("Unloading ReaADR extension.");
  if (g_plugin) {
    unregister_native_actions();
    g_plugin->Register("-hookcustommenu", reinterpret_cast<void*>(hook_custom_menu));
    g_plugin->Register("-API_ReaADR_DetectDialogueSegments", reinterpret_cast<void*>(detect_dialogue_segments));
    g_plugin->Register("-APIdef_ReaADR_DetectDialogueSegments", reinterpret_cast<void*>(const_cast<char*>(kDetectDialogueSegmentsDef)));
    g_plugin->Register("-API_ReaADR_ReadXlsxAsTsv", reinterpret_cast<void*>(read_xlsx_as_tsv));
    g_plugin->Register("-APIdef_ReaADR_ReadXlsxAsTsv", reinterpret_cast<void*>(const_cast<char*>(kReadXlsxAsTsvDef)));
    g_plugin->Register("-API_ReaADR_ValidateSessionModel", reinterpret_cast<void*>(validate_session_model));
    g_plugin->Register("-APIdef_ReaADR_ValidateSessionModel", reinterpret_cast<void*>(const_cast<char*>(kValidateSessionModelDef)));
  }
  unregister_scripts();
  g_plugin = nullptr;
}

} // namespace

extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t* plugin)
{
  if (!plugin) {
    unload();
    return 0;
  }

  g_instance = instance;
  return load(plugin) ? 1 : 0;
}

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
  if (reason == DLL_PROCESS_ATTACH) {
    g_instance = instance;
    log_windows_dll_load(instance);
  }
  return TRUE;
}
#endif

#ifndef _WIN32
extern "C" REAPER_PLUGIN_DLL_EXPORT int SWELL_dllMain(HINSTANCE, DWORD call_mode, LPVOID get_func)
{
  constexpr DWORD kProcessAttach = 1;
  if (call_mode != kProcessAttach || !get_func) return 1;

  auto api_get_func = reinterpret_cast<void* (*)(const char*)>(get_func);
  g_create_popup_menu = reinterpret_cast<CreatePopupMenuFn>(api_get_func("CreatePopupMenu"));
  g_get_menu_item_count = reinterpret_cast<GetMenuItemCountFn>(api_get_func("GetMenuItemCount"));
  g_insert_menu_item = reinterpret_cast<InsertMenuItemFn>(api_get_func("InsertMenuItem"));
  return 1;
}
#endif
