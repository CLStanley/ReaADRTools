#pragma once

#include <reaper_plugin.h>

// Temporary compatibility-host lifecycle. The native migration shell calls this
// explicit boundary instead of macro-renaming and invoking a second plug-in
// entrypoint. Remove this interface when legacy registration/menu ownership is
// fully retired.
namespace reaadr::reaper::legacy_host {

bool load(REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t* plugin);
void unload();
void release_action_registrations();
void bind_migrated_action_ids();

} // namespace reaadr::reaper::legacy_host
