// Migration host shell: keep the legacy monolithic host implementation intact
// while replacing its entrypoint and command hook with the persistent native
// runtime. This lets migration complete without duplicating or partially
// rewriting the 100KB legacy host; cleanup can split that host after parity.
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
#define hook_native_command hook_native_command_legacy
#include "reaper_reaadr_legacy.cpp"
#undef hook_native_command

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
    config.cleanup_api = {native_cleanup_inspect, native_cleanup_apply, native_utc_timestamp()};
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
  config.project_state_api={GetProjExtState,SetProjExtState};config.global_state_api={GetExtState,SetExtState};config.cleanup_api={native_cleanup_inspect,native_cleanup_apply,native_utc_timestamp()};
  config.callbacks.trigger_import=[](const std::string& mapping,bool preview,const std::string& mode,const std::string& characters){run_persistent_native_manager_import(mapping,preview,mode,characters);};
  config.callbacks.trigger_action=[](const std::string& action){if(action=="sync_regions"){std::string error;if(!reaadr::reaper::cue_manager_session_host().sync_regions(error)&&!error.empty())ShowMessageBox(error.c_str(),"ReaADR Cue Manager",0);}else if(action=="clear_character_cues")run_persistent_native_clear_character_cues_action();else if(action=="export_cue_sheet"||action=="export_timing_report"||action=="export_session_metadata")run_persistent_native_export_action(action);};
  std::string error;if(!reaadr::reaper::open_native_cue_manager(std::move(config),error)&&!error.empty())ShowMessageBox(error.c_str(),"ReaADR Cue Manager",0);
}

bool promote_native_quick_actions(reaper_plugin_info_t* plugin)
{
  if(!plugin||!plugin->GetFunc)return false;using NamedCommandLookupFn=int(*)(const char*);auto named_command_lookup=reinterpret_cast<NamedCommandLookupFn>(plugin->GetFunc("NamedCommandLookup"));if(!named_command_lookup)return false;
  constexpr std::array<const char*,4> command_names={{"_ReaADRQuickAction1Native","_ReaADRQuickAction2Native","_ReaADRQuickAction3Native","_ReaADRQuickAction4Native"}};std::array<int,4> command_ids={};
  for(std::size_t index=0;index<command_names.size();++index){command_ids[index]=named_command_lookup(command_names[index]);if(!command_ids[index]){log_line(std::string("Native Quick Action command unavailable: ")+command_names[index]);return false;}}
  for(std::size_t index=0;index<command_ids.size();++index)g_actions[index+1].command_id=command_ids[index];log_line("Promoted all four Quick Actions to native command registrations.");return true;
}

bool runtime_host_hook(int command, int)
{
  const auto& ids = reaadr::reaper::workflow_action_ids();
  if (command == ids.validate_session && command != 0) { run_validate_session_action(); return true; }
  if (command == ids.refresh_overlay && command != 0) { run_refresh_overlay_action(); return true; }
  if (command == ids.refresh_session && command != 0) { run_refresh_session_action(); return true; }
  if (command == ids.update_cues_from_regions && command != 0) { run_update_cues_from_regions_action(); return true; }
  if (command == ids.clear_character_cues && command != 0) { run_clear_character_cues_action(); return true; }
  if (command == ids.character_filter && command != 0) { run_character_filter_action(); return true; }
  if (command == ids.next_cue && command != 0) { run_cue_navigation_action(true); return true; }
  if (command == ids.previous_cue && command != 0) { run_cue_navigation_action(false); return true; }
  if (command == ids.jump_to_cue && command != 0) { run_jump_to_cue_action(); return true; }
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
    log_line("Could not cut workflow Action List registration over to the native registry.");
    return false;
  }
  reaadr::reaper::legacy_host::bind_migrated_action_ids();
  if(!plugin->Register("hookcommand",reinterpret_cast<void*>(runtime_host_hook))){
    log_line("Could not install persistent native runtime command hook.");
    reaadr::reaper::unregister_migrated_workflow_actions(plugin);
    return false;
  }
  g_runtime_host_hook_registered=true;
  std::string error;
  if(!reaadr::reaper::initialize_native_runtime(plugin,&error)){
    if(!error.empty())log_line(error);
    plugin->Register("-hookcommand",reinterpret_cast<void*>(runtime_host_hook));
    g_runtime_host_hook_registered=false;
    reaadr::reaper::unregister_migrated_workflow_actions(plugin);
    return false;
  }
  if(!promote_native_quick_actions(plugin)){
    log_line("Could not promote Quick Actions to native registrations.");
    reaadr::reaper::shutdown_native_runtime(plugin,nullptr);
    plugin->Register("-hookcommand",reinterpret_cast<void*>(runtime_host_hook));
    g_runtime_host_hook_registered=false;
    reaadr::reaper::unregister_migrated_workflow_actions(plugin);
    return false;
  }
  log_line("Persistent native workflow runtime activated with native Action List ownership.");
  return true;
}

void deactivate_native_runtime(reaper_plugin_info_t* plugin)
{
  std::string error;
  if(!reaadr::reaper::shutdown_native_runtime(plugin,&error)&&!error.empty())log_line(error);
  if(plugin&&g_runtime_host_hook_registered){plugin->Register("-hookcommand",reinterpret_cast<void*>(runtime_host_hook));g_runtime_host_hook_registered=false;}
  reaadr::reaper::unregister_migrated_workflow_actions(plugin);
}

} // namespace

extern "C" REAPER_PLUGIN_DLL_EXPORT int ReaperPluginEntry(
  REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t* plugin)
{
  if (!plugin) {
    deactivate_native_runtime(g_plugin);
    reaadr::reaper::legacy_host::unload();
    return 0;
  }

  if (!reaadr::reaper::legacy_host::load(instance, plugin)) return 0;
  if (activate_native_runtime(plugin)) return 1;

  reaadr::reaper::legacy_host::unload();
  return 0;
}