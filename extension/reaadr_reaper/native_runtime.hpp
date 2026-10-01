#pragma once

// Keep the C++ standard library ahead of REAPER/WDL headers. SWELL intentionally
// provides Win32-compatible min/max macros on non-Windows hosts; including the
// standard library first prevents those macros from rewriting std::min/std::max
// in libstdc++ when this header is the first include in a translation unit.
#include <string>
#include <utility>

#include "cue_manager_session.hpp"
#include "cue_info_command.hpp"
#include "recording_command.hpp"
#include "native_action_registry.hpp"
#include "workflow_action_ids.hpp"

#include <reaper_plugin.h>

namespace reaadr::reaper {

using EnumProjectsFn = ReaProject* (*)(int index, char* project_filename, int project_filename_size);

inline reaper_plugin_info_t* g_native_runtime_plugin = nullptr;

inline ReaProject* active_reaper_project()
{
  if (!g_native_runtime_plugin || !g_native_runtime_plugin->GetFunc) return nullptr;
  auto enumerate = reinterpret_cast<EnumProjectsFn>(
    g_native_runtime_plugin->GetFunc("EnumProjects"));
  return enumerate ? enumerate(-1, nullptr, 0) : nullptr;
}

inline bool initialize_native_runtime(
  reaper_plugin_info_t* plugin,
  std::string* error = nullptr)
{
  if (error) error->clear();
  if (!plugin || !plugin->GetFunc) {
    if (error) *error = "REAPER plug-in host is unavailable.";
    return false;
  }
  // The migrated workflow registry is implemented in workflow_action_ids.hpp,
  // but activation remains with the compatibility registrar until the host
  // relinquishes its gaccels in the same atomic cutover. Registering both here
  // would create duplicate Action List entries during the transition.
  if (!register_native_workflow_actions(plugin)) {
    if (error) *error = "Native ReaADR workflow actions could not be registered.";
    return false;
  }
  g_native_runtime_plugin = plugin;
  return true;
}

inline bool shutdown_native_runtime(
  reaper_plugin_info_t* plugin,
  std::string* error = nullptr)
{
  if (error) error->clear();

  bool ok = true;
  std::string shutdown_error;
  const auto append_error = [&](const std::string& message) {
    if (message.empty()) return;
    if (!shutdown_error.empty()) shutdown_error += " ";
    shutdown_error += message;
  };

  std::string manager_error;
  if (!cue_manager_session_host().shutdown(&manager_error)) {
    ok = false;
    append_error(manager_error.empty()
      ? "The native Cue Manager could not be closed safely."
      : manager_error);
  }
  if (!shutdown_native_record_cue_command()) {
    ok = false;
    append_error("The native Record Cue window could not be closed safely.");
  }
  if (!shutdown_native_cue_info_command()) {
    ok = false;
    append_error("The native Cue Info window could not be closed safely.");
  }

  reaper_plugin_info_t* registration_host = plugin ? plugin : g_native_runtime_plugin;
  if (registration_host) unregister_native_workflow_actions(registration_host);
  g_native_runtime_plugin = nullptr;

  if (error) *error = shutdown_error;
  return ok;
}

inline bool open_native_cue_manager(
  CueManagerSessionConfig config,
  std::string& error)
{
  if (!config.project) {
    config.project = active_reaper_project();
    if (!config.project) {
      error = "The active REAPER project could not be resolved for the native Cue Manager.";
      return false;
    }
  }
  return cue_manager_session_host().open_or_activate(std::move(config), error);
}

} // namespace reaadr::reaper
