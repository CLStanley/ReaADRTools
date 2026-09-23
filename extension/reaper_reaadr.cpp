// Migration host shell: keep the legacy monolithic host implementation intact
// while replacing its entrypoint and command hook with the persistent native
// runtime. This lets migration complete without duplicating or partially
// rewriting the 100KB legacy host; cleanup can split that host after parity.
//
// SWELL defines Win32-compatible min/max macros from reaper_plugin.h. Preload
// the standard-library headers used by the legacy host before the SDK so those
// macros cannot rewrite libstdc++ internals. Then include reaper_plugin.h once,
// rename its entrypoint macro, and let the legacy source's guarded SDK include
// preserve that rename.
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
#undef REAPER_PLUGIN_ENTRYPOINT
#define REAPER_PLUGIN_ENTRYPOINT REAPER_PLUGIN_ENTRYPOINT_LEGACY
#define hook_native_command hook_native_command_legacy
#include "reaper_reaadr_legacy.cpp"
#undef hook_native_command
#undef REAPER_PLUGIN_ENTRYPOINT

#include "reaadr_reaper/native_runtime.hpp"

namespace {

bool g_runtime_host_hook_registered = false;

void run_persistent_native_cue_manager_action()
{
  reaadr::reaper::CueManagerSessionConfig config;
  config.project_state_api = {GetProjExtState, SetProjExtState};
  config.global_state_api = {GetExtState, SetExtState};
  config.callbacks.trigger_import = [](
    const std::string& mapping,
    bool preview,
    const std::string& mode,
    const std::string& characters) {
      run_native_import_cue_sheet_action(mapping, preview, mode, characters);
    };
  config.callbacks.trigger_action = [](const std::string& action) {
    int command = 0;
    if (action == "sync_regions") command = g_update_cues_from_regions_command_id;
    else if (action == "clear_character_cues") command = g_clear_character_cues_command_id;

    if (command && Main_OnCommand) Main_OnCommand(command, 0);
    else if (action == "export_cue_sheet") run_native_export_cue_sheet_action();
    else if (action == "export_timing_report") run_native_export_timing_report_action();
    else if (action == "export_session_metadata") run_native_export_session_metadata_action();
    else if (action.rfind("overlay_profile:", 0) == 0)
      run_native_overlay_profile_action(action.substr(16));
    else if (action.rfind("overlay_toggle:", 0) == 0)
      run_native_overlay_toggle_action(action.substr(15));
    else if (action.rfind("overlay_text_color:", 0) == 0)
      run_native_overlay_text_color_action(action.substr(19));
    else if (action.rfind("overlay_settings:", 0) == 0)
      run_native_overlay_settings_action(action.substr(17));
    else if (action.rfind("quick_actions:", 0) == 0)
      run_native_quick_actions_action(action.substr(14));
    else if (action.rfind("preference_toggles:", 0) == 0)
      run_native_preference_toggles_action(action.substr(19));
  };

  std::string error;
  if (!reaadr::reaper::open_native_cue_manager(std::move(config), error) && !error.empty())
    ShowMessageBox(error.c_str(), "ReaADR Cue Manager", 0);
}

bool promote_native_quick_actions(reaper_plugin_info_t* plugin)
{
  if (!plugin || !plugin->GetFunc) return false;
  using NamedCommandLookupFn = int (*)(const char*);
  auto named_command_lookup =
    reinterpret_cast<NamedCommandLookupFn>(plugin->GetFunc("NamedCommandLookup"));
  if (!named_command_lookup) return false;

  constexpr std::array<const char*, 4> command_names = {{
    "_ReaADRQuickAction1Native",
    "_ReaADRQuickAction2Native",
    "_ReaADRQuickAction3Native",
    "_ReaADRQuickAction4Native",
  }};
  std::array<int, 4> command_ids = {};
  for (std::size_t index = 0; index < command_names.size(); ++index) {
    command_ids[index] = named_command_lookup(command_names[index]);
    if (!command_ids[index]) {
      log_line(std::string("Native Quick Action command unavailable: ") + command_names[index]);
      return false;
    }
  }

  // Historical Lua registrations are retired opportunistically by the legacy
  // host. Native Quick Action ownership itself must not depend on the
  // AddRemoveReaScript compatibility API being present.
  for (std::size_t index = 0; index < command_ids.size(); ++index) {
    const std::size_t action_index = index + 1;
    g_actions[action_index].command_id = command_ids[index];
  }
  log_line("Promoted all four Quick Actions to native command registrations.");
  return true;
}

bool runtime_host_hook(int command, int flag)
{
  if (command == g_cue_manager_command_id && command != 0) {
    run_persistent_native_cue_manager_action();
    return true;
  }
  return hook_native_command_legacy(command, flag);
}

bool activate_native_runtime(reaper_plugin_info_t* plugin)
{
  // The legacy host registered its own hook during load. Replace it with a
  // delegating hook so every existing native command keeps its behavior while
  // Cue Manager is routed through CueManagerSessionHost.
  if (g_native_command_hook_registered) {
    plugin->Register("-hookcommand", reinterpret_cast<void*>(hook_native_command_legacy));
    g_native_command_hook_registered = false;
  }
  if (!plugin->Register("hookcommand", reinterpret_cast<void*>(runtime_host_hook))) {
    log_line("Could not install persistent native runtime command hook.");
    return false;
  }
  g_runtime_host_hook_registered = true;

  std::string error;
  if (!reaadr::reaper::initialize_native_runtime(plugin, &error)) {
    if (!error.empty()) log_line(error);
    plugin->Register("-hookcommand", reinterpret_cast<void*>(runtime_host_hook));
    g_runtime_host_hook_registered = false;
    return false;
  }
  if (!promote_native_quick_actions(plugin)) {
    log_line("Could not promote Quick Actions to native registrations.");
    reaadr::reaper::shutdown_native_runtime(plugin, nullptr);
    plugin->Register("-hookcommand", reinterpret_cast<void*>(runtime_host_hook));
    g_runtime_host_hook_registered = false;
    return false;
  }
  log_line("Persistent native workflow runtime activated.");
  return true;
}

void deactivate_native_runtime(reaper_plugin_info_t* plugin)
{
  std::string error;
  if (!reaadr::reaper::shutdown_native_runtime(plugin, &error) && !error.empty())
    log_line(error);
  if (plugin && g_runtime_host_hook_registered) {
    plugin->Register("-hookcommand", reinterpret_cast<void*>(runtime_host_hook));
    g_runtime_host_hook_registered = false;
  }
}

} // namespace

extern "C" REAPER_PLUGIN_DLL_EXPORT int ReaperPluginEntry(
  REAPER_PLUGIN_HINSTANCE instance,
  reaper_plugin_info_t* plugin)
{
  if (!plugin) {
    deactivate_native_runtime(g_plugin);
    return REAPER_PLUGIN_ENTRYPOINT_LEGACY(instance, nullptr);
  }

  const int loaded = REAPER_PLUGIN_ENTRYPOINT_LEGACY(instance, plugin);
  if (!loaded) return 0;
  if (activate_native_runtime(plugin)) return 1;

  REAPER_PLUGIN_ENTRYPOINT_LEGACY(instance, nullptr);
  return 0;
}
