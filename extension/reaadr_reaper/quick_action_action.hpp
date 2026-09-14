#pragma once

struct reaper_plugin_info_t;

namespace reaadr::reaper {

bool register_quick_action_actions(reaper_plugin_info_t* plugin);
void unregister_quick_action_actions(reaper_plugin_info_t* plugin);
int quick_action_command_id(int slot);

} // namespace reaadr::reaper
