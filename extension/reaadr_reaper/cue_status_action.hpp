#pragma once

struct reaper_plugin_info_t;

namespace reaadr::reaper {

bool register_cue_status_action(reaper_plugin_info_t* plugin);
void unregister_cue_status_action(reaper_plugin_info_t* plugin);
int cue_status_action_command_id();

} // namespace reaadr::reaper
