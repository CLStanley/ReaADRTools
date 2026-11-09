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
#include "reaadr_reaper/cue_manager_host_actions.hpp"

namespace {

bool g_runtime_host_hook_registered = false;

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
  if (command == ids.cue_manager && command != 0) { reaadr::reaper::run_persistent_native_cue_manager_action(); return true; }
  if (command == ids.import_cue_sheet && command != 0) { reaadr::reaper::run_persistent_native_import_action(); return true; }
  if (command == ids.preferences && command != 0) { run_native_preferences_action(); return true; }
  if (command == ids.ui_test && command != 0) { reaadr::ui::show_test_window(); return true; }
  return false;
}

bool activate_native_runtime(reaper_plugin_info_t* plugin)
{
  // The native registry is the sole owner of public workflow commands.
  if(!reaadr::reaper::register_migrated_workflow_actions(plugin)){
    reaadr::reaper::legacy_host::log("Could not cut workflow Action List registration over to the native registry.");
    return false;
  }
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
  if(!reaadr::reaper::legacy_host::bind_migrated_action_ids(plugin)){
    reaadr::reaper::legacy_host::log("Could not bind native workflow and Quick Action IDs to the ReaADR menu.");
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