// Native REAPER extension entrypoint. Legacy behavior is isolated behind
// the compatibility-host interface and compiled separately.
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
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <reaper_plugin.h>
#include "reaadr_reaper/reaper_api.hpp"
#include "reaadr_reaper/legacy_host.hpp"
#include "reaadr_reaper/project_state.hpp"
#include "reaadr_ui/reaadr_ui.hpp"

#include "reaadr_core/model_repository.hpp"
#include "reaadr_core/manager_preferences.hpp"
#include "reaadr_reaper/project_transaction.hpp"
#include "reaadr_core/overlay_settings.hpp"
#include "reaadr_core/character_filter.hpp"
#include "app/overlay_application_service.hpp"
#include "app/session_refresh_application_service.hpp"
#include "app/region_timing_application_service.hpp"
#include "app/cue_cleanup_application_service.hpp"
#include "app/character_filter_application_service.hpp"
#include "reaadr_reaper/session_render_service.hpp"
#include "reaadr_reaper/cue_navigation_service.hpp"
#include "app/cue_import_request.hpp"
#include "app/cue_import_workflow.hpp"
#include "app/session_export_service.hpp"
#include "reaadr_reaper/native_runtime.hpp"
#include "reaadr_reaper/xlsx_import.hpp"
#include "reaadr_reaper/workflow_action_ids.hpp"

namespace {

bool g_runtime_host_hook_registered = false;

std::string persistent_export_path(const char* title)
{
  if (!GetUserInputs) return {};
  std::array<char, 4096> path = {};
  if (!GetUserInputs(title, 1, "Output CSV path", path.data(), path.size())) return {};
  std::string output(path.data());
  const auto first = output.find_first_not_of(" \t\r\n");
  const auto last = output.find_last_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  output = output.substr(first, last - first + 1);
  const std::size_t slash = output.find_last_of("/\\");
  const std::size_t dot = output.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) output += ".csv";
  return output;
}

void run_persistent_native_export_action(const std::string& action)
{
  const char* title = action == "export_cue_sheet" ? "ReaADR: Export Cue Sheet" :
                      action == "export_timing_report" ? "ReaADR: Export Timing Report" :
                      "ReaADR: Export Session Metadata";
  const std::string output_path = persistent_export_path(title);
  if (output_path.empty()) return;

  const auto loaded = reaadr::reaper::cue_manager_session_host().load_session();
  if (!loaded) {
    ShowMessageBox(reaadr::core::session_load_error_message(loaded), "ReaADR Export", 0);
    return;
  }

  const auto kind = action == "export_cue_sheet"
    ? reaadr::reaper::SessionExportKind::cue_sheet
    : action == "export_timing_report"
      ? reaadr::reaper::SessionExportKind::timing_report
      : reaadr::reaper::SessionExportKind::session_metadata;
  const auto formatted = reaadr::reaper::format_session_export(loaded.model, kind);
  if (!formatted) {
    ShowMessageBox(formatted.error.c_str(), "ReaADR Export", 0);
    return;
  }

  std::string error;
  if (!reaadr::reaper::write_session_export(output_path, formatted.content, &error)) {
    ShowMessageBox(error.c_str(), "ReaADR Export", 0);
    return;
  }
  ShowMessageBox(("Exported to:\n" + output_path).c_str(), "ReaADR Export", 0);
}

void run_persistent_native_clear_character_cues_action()
{
  if (!GetUserInputs) return;
  std::array<char, 4096> input = {};
  if (!GetUserInputs("ReaADR: Clear Character Cues", 1, "Characters (semicolon-separated):", input.data(), input.size())) return;
  std::vector<std::string> characters; std::stringstream values(input.data()); std::string value;
  while (std::getline(values, value, ';')) { const auto first=value.find_first_not_of(" \t\r\n"), last=value.find_last_not_of(" \t\r\n"); if(first!=std::string::npos) characters.push_back(value.substr(first,last-first+1)); }
  std::string error; const auto result = reaadr::reaper::cue_manager_session_host().clear_characters(characters, error);
  if (!result) { if (!error.empty()) ShowMessageBox(error.c_str(), "ReaADR Clear Character Cues", 0); return; }
  const std::string summary = "Removed " + std::to_string(result.cues_removed) + " cue(s).";
  ShowMessageBox(summary.c_str(), "ReaADR Clear Character Cues", 0);
}

void run_persistent_native_manager_import(
  const std::string& serialized_mapping, bool preview_only,
  const std::string& mode, const std::string& serialized_characters)
{
  reaadr::reaper::CueImportWorkflowRequest request;
  request.serialized_mapping = serialized_mapping;
  request.preview_only = preview_only;
  request.mode = mode;
  request.serialized_characters = serialized_characters;

  reaadr::reaper::ProjectStateStore project_state(nullptr, {GetProjExtState, SetProjExtState});
  if (serialized_mapping.empty()) {
    std::array<char, 4096> saved = {};
    if (GetProjExtState &&
        GetProjExtState(nullptr, "ReaADRTools", "import_mapping_last",
                        saved.data(), saved.size()) > 0) {
      request.persisted_mapping = saved.data();
    }
  }

  reaadr::reaper::CueImportHostUi ui;
  ui.choose_source = [preview_only](std::string& source_path) {
    if (!GetUserInputs) return false;
    std::array<char, 4096> path = {};
    if (!GetUserInputs(preview_only ? "ReaADR: Preview Cue Sheet Import"
                                    : "ReaADR: Import Cue Sheet",
                       1, "Cue sheet path", path.data(), path.size())) {
      return false;
    }
    source_path = path.data();
    return true;
  };
  ui.prompt = [](const std::string& title, const std::string& caption,
                 std::string& value) {
    if (!GetUserInputs) return false;
    std::array<char, 2048> input = {};
    if (!value.empty()) std::strncpy(input.data(), value.c_str(), input.size() - 1);
    if (!GetUserInputs(title.c_str(), 1, caption.c_str(), input.data(), input.size()))
      return false;
    value = input.data();
    return true;
  };
  ui.message = [](const std::string& title, const std::string& message) {
    if (ShowMessageBox) ShowMessageBox(message.c_str(), title.c_str(), 0);
  };

  reaadr::reaper::CueImportWorkflowCallbacks callbacks;
  callbacks.preview = [](const std::string& content, const std::string& source_path,
                         const std::optional<reaadr::core::ColumnMapping>& mapping) {
    return reaadr::reaper::cue_manager_session_host().preview_import_content(
      content, source_path, mapping);
  };
  callbacks.import = [](const std::string& content, const std::string& source_path,
                        const std::optional<reaadr::core::ColumnMapping>& mapping,
                        const std::string& import_mode,
                        const std::vector<std::string>& characters) {
    return reaadr::reaper::cue_manager_session_host().import_content(
      content, source_path, mapping, import_mode, characters);
  };

  reaadr::reaper::run_cue_import_workflow(request, ui, callbacks);
}

void run_persistent_native_import_action()
{
  reaadr::reaper::CueImportWorkflowRequest request;

  reaadr::reaper::ProjectStateStore project_state(nullptr, {GetProjExtState, SetProjExtState});
  std::array<char, 4096> saved = {};
  if (GetProjExtState &&
      GetProjExtState(nullptr, "ReaADRTools", "import_mapping_last",
                      saved.data(), saved.size()) > 0) {
    request.persisted_mapping = saved.data();
  }

  reaadr::reaper::CueImportHostUi ui;
  ui.choose_source = [](std::string& source_path) {
    if (!GetUserFileNameForRead) return false;
    std::array<char, 4096> path = {};
    if (!GetUserFileNameForRead(path.data(), "ReaADR: Import Cue Sheet",
                                "csv;tsv;tab;txt;xlsx")) {
      return false;
    }
    source_path = path.data();
    return true;
  };
  ui.prompt = [](const std::string& title, const std::string& caption,
                 std::string& value) {
    if (!GetUserInputs) return false;
    std::array<char, 2048> input = {};
    if (!value.empty()) std::strncpy(input.data(), value.c_str(), input.size() - 1);
    if (!GetUserInputs(title.c_str(), 1, caption.c_str(), input.data(), input.size()))
      return false;
    value = input.data();
    return true;
  };
  ui.message = [](const std::string& title, const std::string& message) {
    if (ShowMessageBox) ShowMessageBox(message.c_str(), title.c_str(), 0);
  };

  reaadr::reaper::CueImportWorkflowCallbacks callbacks;
  callbacks.preview = [](const std::string& content, const std::string& source_path,
                         const std::optional<reaadr::core::ColumnMapping>& mapping) {
    return reaadr::reaper::cue_manager_session_host().preview_import_content(
      content, source_path, mapping);
  };
  callbacks.import = [](const std::string& content, const std::string& source_path,
                        const std::optional<reaadr::core::ColumnMapping>& mapping,
                        const std::string& import_mode,
                        const std::vector<std::string>& characters) {
    return reaadr::reaper::cue_manager_session_host().import_content(
      content, source_path, mapping, import_mode, characters);
  };

  // The public Action List command shares the persistent native session used by
  // the Manager. Open it on demand so import no longer falls back to the legacy
  // command implementation when the Manager has not been shown yet.
  if (!reaadr::reaper::cue_manager_session_host().has_session()) {
    reaadr::reaper::CueManagerSessionConfig config;
    config.project_state_api = {GetProjExtState, SetProjExtState};
    config.global_state_api = {GetExtState, SetExtState};
    reaadr::reaper::legacy_host::configure_session(config);
    config.callbacks.trigger_import =
      [](const std::string& mapping, bool preview, const std::string& mode,
         const std::string& characters) {
        run_persistent_native_manager_import(mapping, preview, mode, characters);
      };
    std::string error;
    if (!reaadr::reaper::open_native_cue_manager(std::move(config), error)) {
      if (!error.empty()) ShowMessageBox(error.c_str(), "ReaADR Import", 0);
      return;
    }
  }

  reaadr::reaper::run_cue_import_workflow(request, ui, callbacks);
}

void run_persistent_native_cue_manager_action()
{
  reaadr::reaper::CueManagerSessionConfig config;
  config.project_state_api={GetProjExtState,SetProjExtState};config.global_state_api={GetExtState,SetExtState};reaadr::reaper::legacy_host::configure_session(config);
  config.callbacks.trigger_import=[](const std::string& mapping,bool preview,const std::string& mode,const std::string& characters){run_persistent_native_manager_import(mapping,preview,mode,characters);};
  config.callbacks.trigger_action=[](const std::string& action){if(action=="sync_regions"){std::string error;if(!reaadr::reaper::cue_manager_session_host().sync_regions(error)&&!error.empty())ShowMessageBox(error.c_str(),"ReaADR Cue Manager",0);}else if(action=="clear_character_cues")run_persistent_native_clear_character_cues_action();else if(action=="export_cue_sheet"||action=="export_timing_report"||action=="export_session_metadata")run_persistent_native_export_action(action);};
  std::string error;if(!reaadr::reaper::open_native_cue_manager(std::move(config),error)&&!error.empty())ShowMessageBox(error.c_str(),"ReaADR Cue Manager",0);
}

bool refresh_native_navigation_overlay(std::string* error)
{
  reaadr::reaper::ProjectStateStore state(nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository sessions(state);
  reaadr::core::OverlaySettingsRepository settings(state);
  reaadr::core::CueSelectionRepository selections(state);
  reaadr::core::CharacterFilterRepository filters(state);
  reaadr::reaper::OverlayApplicationService service(
    sessions, settings, selections, filters, reaadr::reaper::legacy_host::overlay_api());
  const auto result = service.refresh();
  if (!result && error) *error = result.error;
  return static_cast<bool>(result);
}

void run_native_cue_navigation_action(bool next)
{
  reaadr::reaper::ProjectStateStore state(nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository sessions(state);
  reaadr::core::CueSelectionRepository selections(state);
  reaadr::reaper::CueNavigationService service(
    sessions, selections, reaadr::reaper::legacy_host::cue_navigation_api(),
    refresh_native_navigation_overlay);
  const auto result = next ? service.navigate_next() : service.navigate_previous();
  if (!result) {
    ShowMessageBox(result.error.c_str(), "ReaADR Cue Navigation", 0);
    return;
  }
  reaadr::reaper::legacy_host::log(
    std::string("Native cue navigation moved to ") + result.selection.state.active_overlay_cue_key);
}

void run_native_jump_to_cue_action()
{
  if (!GetUserInputs) {
    ShowMessageBox("The REAPER cue-ID input API is unavailable.", "ReaADR Cue Navigation", 0);
    return;
  }
  std::array<char, 1024> input = {};
  if (!GetUserInputs("ReaADR: Jump To Cue", 1, "Cue ID:", input.data(), input.size())) return;
  reaadr::reaper::ProjectStateStore state(nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository sessions(state);
  reaadr::core::CueSelectionRepository selections(state);
  reaadr::reaper::CueNavigationService service(
    sessions, selections, reaadr::reaper::legacy_host::cue_navigation_api(),
    refresh_native_navigation_overlay);
  const auto result = service.navigate_to_id(input.data());
  if (!result) {
    ShowMessageBox(result.error.c_str(), "ReaADR Cue Navigation", 0);
    return;
  }
  reaadr::reaper::legacy_host::log(
    "Native cue navigation jumped to " + result.selection.state.active_overlay_cue_key);
}

void run_native_preferences_action()
{
  reaadr::reaper::ProjectStateStore project_state(nullptr, {GetProjExtState, SetProjExtState});
  reaadr::reaper::GlobalStateStore global_state({GetExtState, SetExtState});
  reaadr::core::ManagerPreferencesRepository preference_repository(project_state, &global_state);
  const auto loaded = preference_repository.load();
  if (!loaded) {
    ShowMessageBox(loaded.error.c_str(), "ReaADR Preferences", 0);
    return;
  }
  const auto& view_preferences = loaded.preferences;
  std::ostringstream summary;
  summary << "Overlay preset: " << reaadr::core::detect_overlay_profile(view_preferences.overlay) << "\n"
          << "Remember layout: " << (view_preferences.remember_layout ? "yes" : "no") << "\n"
          << "Hover preview: " << (view_preferences.hover_preview ? "yes" : "no") << "\n"
          << "Tooltips: " << (view_preferences.tooltips ? "yes" : "no") << "\n"
          << "Navigation wrap: " << (view_preferences.navigation_wrap ? "yes" : "no");
  ShowMessageBox(summary.str().c_str(), "ReaADR Preferences (Native)", 0);
  if (!GetUserInputs) return;
  std::array<char, 1024> input = {};
  if (!GetUserInputs("ReaADR Preferences: Update", 9,
                    "Overlay profile (actor/engineer/studio/minimal),Preroll seconds,Remember layout (0/1),Hover preview (0/1),Tooltips (0/1),Navigation wrap (0/1),Show status (0/1),Show metadata (0/1),Preroll each loop (0/1)",
                    input.data(), input.size())) return;
  std::array<std::string, 9> values;
  std::stringstream fields(input.data());
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (!std::getline(fields, values[index], ',')) {
      ShowMessageBox("Enter nine comma-separated preference values.", "ReaADR Preferences", 0);
      return;
    }
    const auto first = values[index].find_first_not_of(" \t\r\n");
    const auto last = values[index].find_last_not_of(" \t\r\n");
    values[index] = first == std::string::npos ? std::string() : values[index].substr(first, last - first + 1);
    if (index >= 2 && values[index] != "0" && values[index] != "1") {
      ShowMessageBox("Preference values must be 0 or 1.", "ReaADR Preferences", 0);
      return;
    }
  }
  reaadr::core::ManagerPreferences updated = loaded.preferences;
  const std::array<const char*, 9> keys = {
    "overlay_profile", "preroll_seconds", "remember_layout", "hover_preview", "tooltips", "navigation_wrap",
    "show_status", "show_metadata", "include_preroll_each_loop",
  };
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const auto result = reaadr::core::update_manager_preferences(updated, keys[index], values[index]);
    if (!result) {
      ShowMessageBox(result.error.c_str(), "ReaADR Preferences", 0);
      return;
    }
    updated = result.preferences;
  }
  if (updated == loaded.preferences) {
    ShowMessageBox("Native Manager preferences are unchanged.", "ReaADR Preferences", 0);
    return;
  }
  const auto host = reaadr::reaper::legacy_host::session_refresh_host();
  reaadr::reaper::ProjectTransaction transaction(
    nullptr, host.transaction, "ReaADR: update preferences", -1, true);
  const auto saved = preference_repository.save(updated);
  if (!saved) {
    transaction.mark_failed();
    ShowMessageBox(saved.error.c_str(), "ReaADR Preferences", 0);
    return;
  }
  ShowMessageBox("Native Manager preferences updated.", "ReaADR Preferences", 0);
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

void run_native_character_filter_action()
{
  if (!GetUserInputs) {
    ShowMessageBox("The character input API is unavailable.", "ReaADR Character Filter", 0);
    return;
  }
  std::array<char, 1024> input = {};
  if (!GetUserInputs("ReaADR: Character Filter", 2,
                     "Characters (; separated, blank=all),Hide inactive regions (0/1):",
                     input.data(), input.size())) return;
  char* separator = std::strchr(input.data(), ',');
  if (!separator) {
    ShowMessageBox("Enter characters and 0/1 separated by a comma.", "ReaADR Character Filter", 0);
    return;
  }
  *separator = '\0';
  std::vector<std::string> characters;
  std::stringstream values(input.data());
  std::string value;
  while (std::getline(values, value, ';')) {
    const auto first = value.find_first_not_of(" \\t\\r\\n");
    const auto last = value.find_last_not_of(" \\t\\r\\n");
    if (first != std::string::npos)
      characters.push_back(value.substr(first, last - first + 1));
  }
  const std::string hide_value = separator + 1;
  const bool hide_regions = hide_value == "1" || hide_value == "true";

  reaadr::reaper::ProjectStateStore state(nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository repository(state);
  reaadr::core::CharacterFilterRepository filters(state);
  const auto host = reaadr::reaper::legacy_host::session_refresh_host();
  reaadr::reaper::CharacterFilterApplicationService service(
    repository, filters, host.transaction,
    reaadr::reaper::legacy_host::character_filter_api());
  const auto result = service.apply(characters, hide_regions);
  if (!result) {
    ShowMessageBox(result.error.c_str(), "ReaADR Character Filter", 0);
    return;
  }
  const std::string summary = "Muted " + std::to_string(result.applied.tracks_muted) +
    " track(s), unmuted " + std::to_string(result.applied.tracks_unmuted) +
    ".\\n\\nHidden regions: " + std::to_string(result.applied.regions_hidden) +
    "\\nShown regions: " + std::to_string(result.applied.regions_shown);
  ShowMessageBox(summary.c_str(), "ReaADR Character Filter", 0);
}

void run_native_clear_character_cues_action()
{
  if (!GetUserInputs) {
    ShowMessageBox("The character input API is unavailable.", "ReaADR Cue Cleanup", 0);
    return;
  }
  std::array<char, 1024> input = {};
  if (!GetUserInputs("ReaADR: Clear Character Cues", 1,
                     "Characters (comma-separated):", input.data(), input.size())) return;
  std::vector<std::string> characters;
  std::stringstream values(input.data());
  std::string value;
  while (std::getline(values, value, ',')) {
    const auto first = value.find_first_not_of(" \\t\\r\\n");
    const auto last = value.find_last_not_of(" \\t\\r\\n");
    if (first != std::string::npos)
      characters.push_back(value.substr(first, last - first + 1));
  }
  if (characters.empty()) {
    ShowMessageBox("Select at least one character.", "ReaADR Cue Cleanup", 0);
    return;
  }
  const std::string prompt = "Remove generated cues, regions, cue audio, and cue tracks for " +
    std::to_string(characters.size()) + " character(s)? Recording tracks and takes are preserved.";
  if (ShowMessageBox(prompt.c_str(), "ReaADR Cue Cleanup", 4) != 6) return;

  reaadr::reaper::ProjectStateStore state(nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository repository(state);
  const auto host = reaadr::reaper::legacy_host::session_refresh_host();
  reaadr::reaper::CueCleanupApplicationService service(
    repository, host.transaction, reaadr::reaper::legacy_host::cue_cleanup_api());
  const auto result = service.clear_characters(characters);
  if (!result) {
    ShowMessageBox(result.error.c_str(), "ReaADR Cue Cleanup", 0);
    return;
  }
  const std::string summary = "Removed " + std::to_string(result.cues_removed) +
    " cue(s), " + std::to_string(result.regions_removed) + " region(s), and " +
    std::to_string(result.cue_audio_removed) + " cue-audio item(s).\\n\\nCue tracks removed: " +
    std::to_string(result.tracks_removed);
  ShowMessageBox(summary.c_str(), "ReaADR Cue Cleanup", 0);
}

void run_native_update_cues_from_regions_action()
{
  reaadr::reaper::ProjectStateStore state(nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository repository(state);
  reaadr::core::EventLogRepository events(state);
  reaadr::core::CharacterFilterRepository filters(state);
  reaadr::core::OverlaySettingsRepository settings(state);
  reaadr::core::CueSelectionRepository selections(state);
  reaadr::reaper::OverlayApplicationService overlay(
    repository, settings, selections, filters,
    reaadr::reaper::legacy_host::overlay_api());
  const auto host = reaadr::reaper::legacy_host::session_refresh_host();
  reaadr::reaper::SessionRenderService renderer(
    repository, events, filters, nullptr, host.tracks, host.ruler,
    host.audio, host.transaction);
  reaadr::reaper::RegionTimingRenderOptions options;
  options.session.cue_audio_path = host.cue_audio_path;
  options.session.event.source = "native_region_timing";
  options.session.refresh_overlay = [&overlay](std::string* error) {
    const auto refreshed = overlay.refresh();
    if (!refreshed && error) *error = refreshed.error;
    return static_cast<bool>(refreshed);
  };
  reaadr::reaper::RegionTimingApplicationService service(renderer, std::move(options));
  const auto result = service.update();
  if (!result) {
    ShowMessageBox(result.error.c_str(), "ReaADR Region Timing", 0);
    return;
  }
  const std::string summary = "Updated " + std::to_string(result.timing.changed_cues) +
    " cue(s) from current region timing.\n\nMissing ReaADR regions: " +
    std::to_string(result.timing.missing_regions);
  ShowMessageBox(summary.c_str(), "ReaADR Region Timing", 0);
}

void run_native_refresh_session_action()
{
  reaadr::reaper::ProjectStateStore state(nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository repository(state);
  reaadr::core::EventLogRepository events(state);
  reaadr::core::CharacterFilterRepository filters(state);
  reaadr::core::OverlaySettingsRepository settings(state);
  reaadr::core::CueSelectionRepository selections(state);
  reaadr::reaper::OverlayApplicationService overlay(
    repository, settings, selections, filters,
    reaadr::reaper::legacy_host::overlay_api());
  const auto host = reaadr::reaper::legacy_host::session_refresh_host();
  reaadr::reaper::SessionRenderService renderer(
    repository, events, filters, nullptr, host.tracks, host.ruler,
    host.audio, host.transaction);
  reaadr::reaper::SessionRenderOptions options;
  options.cue_audio_path = host.cue_audio_path;
  options.event.source = "native_refresh_session";
  options.refresh_overlay = [&overlay](std::string* error) {
    const auto result = overlay.refresh();
    if (!result && error) *error = result.error;
    return static_cast<bool>(result);
  };
  reaadr::reaper::SessionRefreshApplicationService refresh(
    repository, renderer, options, host.timestamp);
  const auto result = refresh.refresh(
    [tracks = host.tracks] {
      return reaadr::reaper::TrackRegionAdapter(nullptr, tracks).inspect();
    },
    [](std::size_t modified) {
      const std::string message = "Detected " + std::to_string(modified) +
        " cue region(s) whose timing differs from the saved session.\\n\\n"
        "Refresh Session will overwrite those moved regions. Use Update Cues From Regions "
        "first if the moved regions are correct.\\n\\nContinue with Refresh Session?";
      return ShowMessageBox(message.c_str(), "ReaADR Refresh Session", 4) == 6;
    });
  if (result.cancelled) return;
  if (!result) {
    ShowMessageBox(result.error.c_str(), "ReaADR Session Refresh", 0);
    return;
  }
  ShowMessageBox("The canonical session and ReaADR-owned project artifacts were refreshed.",
                 "ReaADR Session Refresh", 0);
}

void run_native_refresh_overlay_action()
{
  reaadr::reaper::ProjectStateStore state(nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository sessions(state);
  reaadr::core::OverlaySettingsRepository settings(state);
  reaadr::core::CueSelectionRepository selections(state);
  reaadr::core::CharacterFilterRepository filters(state);
  reaadr::reaper::OverlayApplicationService service(
    sessions, settings, selections, filters,
    reaadr::reaper::legacy_host::overlay_api());
  const auto result = service.refresh();
  if (!result) {
    ShowMessageBox(result.error.c_str(), "ReaADR Video Overlay", 0);
    return;
  }
  reaadr::reaper::legacy_host::log(
    "Native overlay refresh completed; displayed cues=" +
    std::to_string(result.displayed_cue_count));
}

void run_native_validate_session_action()
{
  reaadr::reaper::ProjectStateStore state(nullptr, {GetProjExtState, SetProjExtState});
  reaadr::core::SessionModelRepository repository(state);
  const auto result = repository.load();
  if (!result) {
    ShowMessageBox(reaadr::core::session_load_error_message(result), "ReaADR Session Model", 0);
    return;
  }
  const auto& model = result.model;
  const auto revision = repository.revision();
  const auto name = model.session.find("session_name");
  std::ostringstream summary;
  summary << "The ADR Session Model is valid.\\n\\n";
  summary << "Session ID: " << model.session_id() << '\\n';
  if (name != model.session.end() && !name->second.empty()) summary << "Session: " << name->second << '\\n';
  if (revision) summary << "Revision: " << revision.revision << '\\n';
  summary << "Scripts: " << model.scripts.size() << '\\n';
  summary << "Characters: " << model.characters.size() << '\\n';
  summary << "Cues: " << model.cues.size() << '\\n';
  summary << "Tracks: " << model.tracks.size() << '\\n';
  summary << "Regions: " << model.regions.size();
  const std::string message = summary.str();
  ShowMessageBox(message.c_str(), "ReaADR Session Model", 0);
}

bool runtime_host_hook(int command, int)
{
  const auto& ids = reaadr::reaper::workflow_action_ids();
  if (command == ids.validate_session && command != 0) { run_native_validate_session_action(); return true; }
  if (command == ids.refresh_overlay && command != 0) { run_native_refresh_overlay_action(); return true; }
  if (command == ids.refresh_session && command != 0) { run_native_refresh_session_action(); return true; }
  if (command == ids.update_cues_from_regions && command != 0) { run_native_update_cues_from_regions_action(); return true; }
  if (command == ids.clear_character_cues && command != 0) { run_native_clear_character_cues_action(); return true; }
  if (command == ids.character_filter && command != 0) { run_native_character_filter_action(); return true; }
  if (command == ids.next_cue && command != 0) { run_native_cue_navigation_action(true); return true; }
  if (command == ids.previous_cue && command != 0) { run_native_cue_navigation_action(false); return true; }
  if (command == ids.jump_to_cue && command != 0) { run_native_jump_to_cue_action(); return true; }
  if (command == ids.cue_manager && command != 0) { run_persistent_native_cue_manager_action(); return true; }
  if (command == ids.import_cue_sheet && command != 0) { run_persistent_native_import_action(); return true; }
  if (command == ids.preferences && command != 0) { run_native_preferences_action(); return true; }
  if (command == ids.ui_test && command != 0) { reaadr::ui::show_test_window(); return true; }
  return false;
}

bool activate_native_runtime(reaper_plugin_info_t* plugin)
{
  // The compatibility entrypoint still performs host/API bootstrap, but it no
  // longer owns these public workflow registrations after this point. Remove
  // its hook and gaccels first, then atomically recreate the same stable named
  // commands under the native registry so shortcuts and toolbar bindings keep
  // resolving by their persisted command names.
  reaadr::reaper::legacy_host::release_action_registrations();
  if(!reaadr::reaper::register_migrated_workflow_actions(plugin)){
    reaadr::reaper::legacy_host::log("Could not cut workflow Action List registration over to the native registry.");
    return false;
  }
  reaadr::reaper::legacy_host::bind_migrated_action_ids();
  if(!plugin->Register("hookcommand",reinterpret_cast<void*>(runtime_host_hook))){
    reaadr::reaper::legacy_host::log("Could not install persistent native runtime command hook.");
    reaadr::reaper::unregister_migrated_workflow_actions(plugin);
    return false;
  }
  g_runtime_host_hook_registered=true;
  std::string error;
  if(!reaadr::reaper::initialize_native_runtime(plugin,&error)){
    if(!error.empty())reaadr::reaper::legacy_host::log(error);
    plugin->Register("-hookcommand",reinterpret_cast<void*>(runtime_host_hook));
    g_runtime_host_hook_registered=false;
    reaadr::reaper::unregister_migrated_workflow_actions(plugin);
    return false;
  }
  if(!reaadr::reaper::legacy_host::promote_native_quick_actions(plugin)){
    reaadr::reaper::legacy_host::log("Could not promote Quick Actions to native registrations.");
    reaadr::reaper::shutdown_native_runtime(plugin,nullptr);
    plugin->Register("-hookcommand",reinterpret_cast<void*>(runtime_host_hook));
    g_runtime_host_hook_registered=false;
    reaadr::reaper::unregister_migrated_workflow_actions(plugin);
    return false;
  }
  reaadr::reaper::legacy_host::log("Persistent native workflow runtime activated with native Action List ownership.");
  return true;
}

void deactivate_native_runtime(reaper_plugin_info_t* plugin)
{
  std::string error;
  if(!reaadr::reaper::shutdown_native_runtime(plugin,&error)&&!error.empty())reaadr::reaper::legacy_host::log(error);
  if(plugin&&g_runtime_host_hook_registered){plugin->Register("-hookcommand",reinterpret_cast<void*>(runtime_host_hook));g_runtime_host_hook_registered=false;}
  reaadr::reaper::unregister_migrated_workflow_actions(plugin);
}

} // namespace

extern "C" REAPER_PLUGIN_DLL_EXPORT int ReaperPluginEntry(
  REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t* plugin)
{
  if (!plugin) {
    deactivate_native_runtime(reaadr::reaper::g_native_runtime_plugin);
    reaadr::reaper::legacy_host::unload();
    return 0;
  }

  if (!reaadr::reaper::legacy_host::load(instance, plugin)) return 0;
  if (activate_native_runtime(plugin)) return 1;

  reaadr::reaper::legacy_host::unload();
  return 0;
}