#pragma once

struct reaper_plugin_info_t;

namespace reaadr::reaper {

// Registers standalone native workflow actions that are implemented outside
// the legacy monolithic extension entrypoint. Keep this boundary small so the
// host only needs one load/unload handoff as Lua ownership is retired.
bool register_native_workflow_actions(reaper_plugin_info_t* plugin);
void unregister_native_workflow_actions(reaper_plugin_info_t* plugin);

} // namespace reaadr::reaper
