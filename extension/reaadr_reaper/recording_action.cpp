#include "recording_action.hpp"

#include "recording_command.hpp"

#include <reaper_plugin.h>

namespace reaadr::reaper {
namespace {
constexpr const char* kRecordingCommandName = "ReaADRRecordCueNative";
constexpr const char* kRecordingActionLabel = "ReaADR: Record Cue (Native)";
int g_recording_command_id = 0;
gaccel_register_t g_recording_accel = {};
bool g_recording_hook_registered = false;

bool hook_recording_command(int command, int)
{
  if (command != 0 && command == g_recording_command_id) {
    run_native_record_cue_command();
    return true;
  }
  return false;
}
} // namespace

bool register_recording_action(reaper_plugin_info_t* plugin)
{
  if (!plugin) return false;
  if (g_recording_command_id != 0) return true;

  g_recording_command_id = plugin->Register(
    "command_id", reinterpret_cast<void*>(const_cast<char*>(kRecordingCommandName)));
  if (!g_recording_command_id) return false;

  g_recording_accel.accel.cmd = static_cast<WORD>(g_recording_command_id);
  g_recording_accel.desc = kRecordingActionLabel;
  if (!plugin->Register("gaccel", reinterpret_cast<void*>(&g_recording_accel))) {
    g_recording_command_id = 0;
    g_recording_accel = {};
    return false;
  }

  if (!plugin->Register("hookcommand", reinterpret_cast<void*>(hook_recording_command))) {
    plugin->Register("-gaccel", reinterpret_cast<void*>(&g_recording_accel));
    g_recording_command_id = 0;
    g_recording_accel = {};
    return false;
  }

  g_recording_hook_registered = true;
  return true;
}

void unregister_recording_action(reaper_plugin_info_t* plugin)
{
  if (!plugin) return;
  if (g_recording_hook_registered) {
    plugin->Register("-hookcommand", reinterpret_cast<void*>(hook_recording_command));
    g_recording_hook_registered = false;
  }
  if (g_recording_command_id != 0) {
    plugin->Register("-gaccel", reinterpret_cast<void*>(&g_recording_accel));
    g_recording_command_id = 0;
    g_recording_accel = {};
  }
}

int recording_action_command_id()
{
  return g_recording_command_id;
}

} // namespace reaadr::reaper
