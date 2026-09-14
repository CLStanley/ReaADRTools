#pragma once

struct reaper_plugin_info_t;

namespace reaadr::reaper {

// Registers the standalone native Record Cue action without coupling command
// plumbing to the extension entrypoint. The host only needs to call register
// during load and unregister during unload.
bool register_recording_action(reaper_plugin_info_t* plugin);
void unregister_recording_action(reaper_plugin_info_t* plugin);
int recording_action_command_id();

} // namespace reaadr::reaper
