#pragma once

namespace reaadr::reaper {

// Opens the native Cue Info presentation. The Lua Cue Info Panel remains
// installed and unchanged until behavior/UI/platform parity is smoke-tested.
bool run_native_cue_info_command();

} // namespace reaadr::reaper
