#pragma once

namespace reaadr::reaper {

// Opens the native Record Cue presentation. Lua ReaADR_Record_Cue.lua remains
// installed and unchanged as the parity reference/fallback during migration.
bool run_native_record_cue_command();

} // namespace reaadr::reaper
