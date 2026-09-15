#include "native_action_registry.hpp"

#include "cue_info_action.hpp"
#include "recording_action.hpp"

#include <array>
#include <cstddef>
#include <string>

#include <reaper_plugin.h>

namespace reaadr::reaper {
namespace {
constexpr const char* kNamespace = "ReaADRTools";
constexpr std::array<const char*, 4> kQuickCommandNames = {{
  "ReaADRQuickAction1Native",
  "ReaADRQuickAction2Native",
  "ReaADRQuickAction3Native",
  "ReaADRQuickAction4Native",
}};
constexpr std::array<const char*, 4> kQuickActionLabels = {{
  "ReaADR: Quick Action 1 (Native)",
  "ReaADR: Quick Action 2 (Native)",
  "ReaADR: Quick Action 3 (Native)",
  "ReaADR: Quick Action 4 (Native)",
}};
constexpr std::array<const char*, 4> kQuickDefaults = {{
  "import", "cue_manager", "export_reports", "overlay_settings",
}};

using NamedCommandLookupFn = int (*)(const char* command_name);
using MainOnCommandFn = void (*)(int command, int flag);
using GetExtStateFn = const char* (*)(const char* section, const char* key);
using SetProjExtStateFn = int (*)(ReaProject* project, const char* section,
                                  const char* key, const char* value);

std::array<int, 4> g_quick_command_ids = {};
std::array<gaccel_register_t, 4> g_quick_accels = {};
bool g_quick_hook_registered = false;
NamedCommandLookupFn g_named_command_lookup = nullptr;
MainOnCommandFn g_main_on_command = nullptr;
GetExtStateFn g_get_ext_state = nullptr;
SetProjExtStateFn g_set_proj_ext_state = nullptr;

void clear_quick_runtime_api()
{
  g_named_command_lookup = nullptr;
  g_main_on_command = nullptr;
  g_get_ext_state = nullptr;
  g_set_proj_ext_state = nullptr;
}

bool run_named_command(const char* name)
{
  if (!g_named_command_lookup || !g_main_on_command || !name) return false;
  const int command = g_named_command_lookup(name);
  if (command == 0) return false;
  g_main_on_command(command, 0);
  return true;
}

bool open_manager_tab(const char* tab)
{
  if (!g_set_proj_ext_state || !tab) return false;
  g_set_proj_ext_state(nullptr, kNamespace, "ui.manager.launch_tab", tab);
  if (run_named_command("_ReaADRShowCueManagerNative")) return true;
  g_set_proj_ext_state(nullptr, kNamespace, "ui.manager.launch_tab", "");
  return false;
}

std::string configured_quick_action(std::size_t index)
{
  if (index >= kQuickDefaults.size()) return {};
  if (!g_get_ext_state) return kQuickDefaults[index];
  const std::string key = "quick_action_" + std::to_string(index + 1);
  const char* configured = g_get_ext_state(kNamespace, key.c_str());
  return configured && *configured
    ? std::string(configured)
    : std::string(kQuickDefaults[index]);
}

bool dispatch_quick_action(const std::string& action)
{
  if (action == "import") return run_named_command("_ReaADRImportCueSheetNative");
  if (action == "cue_manager") return run_named_command("_ReaADRShowCueManagerNative");
  if (action == "record_cue") return run_named_command("_ReaADRRecordCueNative");
  if (action == "character_filter") return run_named_command("_ReaADRApplyCharacterFilterNative");
  if (action == "refresh_overlay") return run_named_command("_ReaADRRefreshVideoOverlayNative");
  if (action == "validate") return run_named_command("_ReaADRValidateSessionModelNative");
  if (action == "export_reports") return open_manager_tab("reports");
  if (action == "overlay_settings") return open_manager_tab("overlay");
  return false;
}

bool hook_quick_action_command(int command, int)
{
  if (command == 0) return false;
  for (std::size_t index = 0; index < g_quick_command_ids.size(); ++index) {
    if (g_quick_command_ids[index] != command) continue;
    dispatch_quick_action(configured_quick_action(index));
    return true;
  }
  return false;
}

void unregister_quick_action_actions(reaper_plugin_info_t* plugin)
{
  if (!plugin) return;
  if (g_quick_hook_registered) {
    plugin->Register("-hookcommand", reinterpret_cast<void*>(hook_quick_action_command));
    g_quick_hook_registered = false;
  }
  for (std::size_t index = g_quick_command_ids.size(); index > 0; --index) {
    const std::size_t slot = index - 1;
    if (g_quick_command_ids[slot] != 0)
      plugin->Register("-gaccel", reinterpret_cast<void*>(&g_quick_accels[slot]));
    g_quick_command_ids[slot] = 0;
    g_quick_accels[slot] = {};
  }
  clear_quick_runtime_api();
}

bool register_quick_action_actions(reaper_plugin_info_t* plugin)
{
  if (!plugin || !plugin->GetFunc) return false;
  if (g_quick_command_ids[0] != 0) return true;

  g_named_command_lookup = reinterpret_cast<NamedCommandLookupFn>(
    plugin->GetFunc("NamedCommandLookup"));
  g_main_on_command = reinterpret_cast<MainOnCommandFn>(plugin->GetFunc("Main_OnCommand"));
  g_get_ext_state = reinterpret_cast<GetExtStateFn>(plugin->GetFunc("GetExtState"));
  g_set_proj_ext_state = reinterpret_cast<SetProjExtStateFn>(plugin->GetFunc("SetProjExtState"));
  if (!g_named_command_lookup || !g_main_on_command || !g_get_ext_state || !g_set_proj_ext_state) {
    clear_quick_runtime_api();
    return false;
  }

  for (std::size_t index = 0; index < g_quick_command_ids.size(); ++index) {
    g_quick_command_ids[index] = plugin->Register(
      "command_id", reinterpret_cast<void*>(const_cast<char*>(kQuickCommandNames[index])));
    if (!g_quick_command_ids[index]) {
      unregister_quick_action_actions(plugin);
      return false;
    }
    g_quick_accels[index].accel.cmd = static_cast<WORD>(g_quick_command_ids[index]);
    g_quick_accels[index].desc = kQuickActionLabels[index];
    if (!plugin->Register("gaccel", reinterpret_cast<void*>(&g_quick_accels[index]))) {
      g_quick_command_ids[index] = 0;
      unregister_quick_action_actions(plugin);
      return false;
    }
  }

  if (!plugin->Register("hookcommand", reinterpret_cast<void*>(hook_quick_action_command))) {
    unregister_quick_action_actions(plugin);
    return false;
  }
  g_quick_hook_registered = true;
  return true;
}
} // namespace

bool register_native_workflow_actions(reaper_plugin_info_t* plugin)
{
  if (!plugin) return false;
  if (!register_recording_action(plugin)) return false;
  if (!register_cue_info_action(plugin)) {
    unregister_recording_action(plugin);
    return false;
  }
  if (!register_quick_action_actions(plugin)) {
    unregister_cue_info_action(plugin);
    unregister_recording_action(plugin);
    return false;
  }
  return true;
}

void unregister_native_workflow_actions(reaper_plugin_info_t* plugin)
{
  if (!plugin) return;
  // Unregister in reverse order so partial registration and future additions
  // retain a predictable teardown sequence.
  unregister_quick_action_actions(plugin);
  unregister_cue_info_action(plugin);
  unregister_recording_action(plugin);
}

} // namespace reaadr::reaper
