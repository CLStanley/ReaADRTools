#include "cue_info_action.hpp"

#include "cue_info_command.hpp"

#include <reaper_plugin.h>

namespace reaadr::reaper {
namespace {
constexpr const char* kCueInfoCommandName = "ReaADRShowCueInfoNative";
constexpr const char* kCueInfoActionLabel = "ReaADR: Cue Info (Native)";
int g_cue_info_command_id = 0;
gaccel_register_t g_cue_info_accel = {};
bool g_cue_info_hook_registered = false;

bool hook_cue_info_command(int command, int)
{
  if (command != 0 && command == g_cue_info_command_id) {
    run_native_cue_info_command();
    return true;
  }
  return false;
}
} // namespace

bool register_cue_info_action(reaper_plugin_info_t* plugin)
{
  if (!plugin) return false;
  if (g_cue_info_command_id != 0) return true;

  g_cue_info_command_id = plugin->Register(
    "command_id", reinterpret_cast<void*>(const_cast<char*>(kCueInfoCommandName)));
  if (!g_cue_info_command_id) return false;

  g_cue_info_accel.accel.cmd = static_cast<WORD>(g_cue_info_command_id);
  g_cue_info_accel.desc = kCueInfoActionLabel;
  if (!plugin->Register("gaccel", reinterpret_cast<void*>(&g_cue_info_accel))) {
    g_cue_info_command_id = 0;
    g_cue_info_accel = {};
    return false;
  }

  if (!plugin->Register("hookcommand", reinterpret_cast<void*>(hook_cue_info_command))) {
    plugin->Register("-gaccel", reinterpret_cast<void*>(&g_cue_info_accel));
    g_cue_info_command_id = 0;
    g_cue_info_accel = {};
    return false;
  }

  g_cue_info_hook_registered = true;
  return true;
}

void unregister_cue_info_action(reaper_plugin_info_t* plugin)
{
  if (!plugin) return;
  if (g_cue_info_hook_registered) {
    plugin->Register("-hookcommand", reinterpret_cast<void*>(hook_cue_info_command));
    g_cue_info_hook_registered = false;
  }
  if (g_cue_info_command_id != 0) {
    plugin->Register("-gaccel", reinterpret_cast<void*>(&g_cue_info_accel));
    g_cue_info_command_id = 0;
    g_cue_info_accel = {};
  }
}

int cue_info_action_command_id()
{
  return g_cue_info_command_id;
}

} // namespace reaadr::reaper
