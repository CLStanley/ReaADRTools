#pragma once

#include <reaper_plugin.h>
#include <string>

namespace reaadr::reaper { struct CueManagerSessionConfig; }

// Temporary compatibility-host lifecycle. The native migration shell calls this
// explicit boundary instead of macro-renaming and invoking a second plug-in
// entrypoint. Remove this interface when legacy registration/menu ownership is
// fully retired.
namespace reaadr::reaper::legacy_host {

bool load(REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t* plugin);
void unload();
void release_action_registrations();
void bind_migrated_action_ids();
bool promote_native_quick_actions(reaper_plugin_info_t* plugin);

// Compatibility command dispatch until these workflows are individually
// migrated into standalone native command modules.
enum class Command {
  refresh_overlay,
  refresh_session,
  update_cues_from_regions,
  clear_character_cues,
  character_filter,
  next_cue,
  previous_cue,
  jump_to_cue,
  preferences
};
void run_command(Command command);
void log(const std::string& message);
void configure_session(CueManagerSessionConfig& config);

} // namespace reaadr::reaper::legacy_host
