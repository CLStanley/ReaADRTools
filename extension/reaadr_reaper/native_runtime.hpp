#pragma once

#include "cue_manager_session.hpp"
#include "native_action_registry.hpp"

#include <reaper_plugin.h>

#include <string>
#include <utility>

namespace reaadr::reaper {

using EnumProjectsFn = ReaProject* (*)(int index, char* project_filename, int project_filename_size);

// The runtime keeps only the host pointer required to resolve project identity
// and register/unregister actions. The Manager dependency graph remains owned
// exclusively by CueManagerSessionHost.
inline reaper_plugin_info_t* g_native_runtime_plugin = nullptr;

inline ReaProject* active_reaper_project()
{
  if (!g_native_runtime_plugin || !g_native_runtime_plugin->GetFunc) return nullptr;
  auto enumerate = reinterpret_cast<EnumProjectsFn>(
    g_native_runtime_plugin->GetFunc("EnumProjects"));
  return enumerate ? enumerate(-1, nullptr, 0) : nullptr;
}

// Small extension-host boundary for native workflow lifetime. Keeping action
// registration and persistent Manager ownership behind this API makes the final
// reaper_reaadr.cpp cut-over a pair of explicit load/unload calls rather than
// spreading native lifetime rules through the legacy host monolith.
inline bool initialize_native_runtime(
  reaper_plugin_info_t* plugin,
  std::string* error = nullptr)
{
  if (error) error->clear();
  if (!plugin || !plugin->GetFunc) {
    if (error) *error = "REAPER plug-in host is unavailable.";
    return false;
  }
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

  // Window/controller ownership must end before command hooks are removed and
  // before REAPER host APIs disappear during extension unload.
  if (!cue_manager_session_host().shutdown(error)) return false;

  reaper_plugin_info_t* registration_host = plugin ? plugin : g_native_runtime_plugin;
  if (registration_host) unregister_native_workflow_actions(registration_host);
  g_native_runtime_plugin = nullptr;
  return true;
}

inline bool open_native_cue_manager(
  CueManagerSessionConfig config,
  std::string& error)
{
  // nullptr has convenient "current project" semantics for many REAPER APIs,
  // but it is not a stable project identity. Resolve the concrete project here
  // so a persistent Manager can detect project-tab switches and rebind safely.
  if (!config.project) config.project = active_reaper_project();
  return cue_manager_session_host().open_or_activate(std::move(config), error);
}

} // namespace reaadr::reaper
