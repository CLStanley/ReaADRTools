#include "native_runtime.hpp"

#include "native_action_registry.hpp"

#include <reaper_plugin.h>

#include <utility>

namespace reaadr::reaper {

bool initialize_native_runtime(reaper_plugin_info_t* plugin, std::string* error)
{
  if (error) error->clear();
  if (!plugin) {
    if (error) *error = "REAPER plug-in host is unavailable.";
    return false;
  }
  if (!register_native_workflow_actions(plugin)) {
    if (error) *error = "Native ReaADR workflow actions could not be registered.";
    return false;
  }
  return true;
}

bool shutdown_native_runtime(reaper_plugin_info_t* plugin, std::string* error)
{
  if (error) error->clear();

  // Window/controller ownership must end before command hooks are removed and
  // before REAPER host APIs disappear during extension unload.
  if (!cue_manager_session_host().shutdown(error)) return false;

  if (plugin) unregister_native_workflow_actions(plugin);
  return true;
}

bool open_native_cue_manager(CueManagerSessionConfig config, std::string& error)
{
  return cue_manager_session_host().open_or_activate(std::move(config), error);
}

} // namespace reaadr::reaper
