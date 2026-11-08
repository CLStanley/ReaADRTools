#pragma once

namespace reaadr::reaper {

// REAPER Action List entry points for the persistent native Cue Manager.
// Host dialog and command APIs are resolved after REAPERAPI_LoadAPI.
void run_persistent_native_cue_manager_action();
void run_persistent_native_import_action();

} // namespace reaadr::reaper
