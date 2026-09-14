#include "native_action_registry.hpp"

#include "cue_info_action.hpp"
#include "quick_action_action.hpp"
#include "recording_action.hpp"

namespace reaadr::reaper {

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
