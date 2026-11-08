#pragma once

#include <reaper_plugin.h>
#include <string>
#include "render_artifact_adapter.hpp"
#include "app/cue_cleanup_application_service.hpp"
#include "app/character_filter_application_service.hpp"
#include "cue_navigation_service.hpp"

namespace reaadr::reaper { struct OverlayApplicationApi; }

// Temporary compatibility-host lifecycle. The native migration shell calls this
// explicit boundary instead of macro-renaming and invoking a second plug-in
// entrypoint. Remove this interface when legacy registration/menu ownership is
// fully retired.
namespace reaadr::reaper::legacy_host {

bool load(REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t* plugin);
void unload();
void release_action_registrations();
bool bind_migrated_action_ids(reaper_plugin_info_t* plugin);

void log(const std::string& message);
// Compatibility callbacks; orchestration belongs to the native runtime.
OverlayApplicationApi overlay_api();
// Invoke the existing host-bound session refresh while native adapters are extracted.
struct SessionRefreshHost {
  TrackRegionApi tracks;
  RulerLaneApi ruler;
  CueAudioApi audio;
  TransactionApi transaction;
  std::string cue_audio_path;
  std::string timestamp;
};
SessionRefreshHost session_refresh_host();
CueCleanupApplicationApi cue_cleanup_api();
CharacterFilterApplicationApi character_filter_api();
CueNavigationApi cue_navigation_api();

} // namespace reaadr::reaper::legacy_host
