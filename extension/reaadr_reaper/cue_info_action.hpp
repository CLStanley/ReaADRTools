#pragma once

struct reaper_plugin_info_t;

namespace reaadr::reaper {

bool register_cue_info_action(reaper_plugin_info_t* plugin);
void unregister_cue_info_action(reaper_plugin_info_t* plugin);
int cue_info_action_command_id();

} // namespace reaadr::reaper
