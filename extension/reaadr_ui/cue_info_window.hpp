#pragma once

namespace reaadr::ui {

class CueInfoController;

// Opens the native Cue Info presentation when supported by the current host.
// The Lua panel remains installed as the parity reference until this window is
// smoke-tested and reaches platform/UI parity.
bool show_cue_info_window(CueInfoController& controller);
bool close_cue_info_window();
// Forced runtime teardown must never leave controller/session storage alive
// behind a destroyed REAPER extension. This bypasses the interactive dirty
// editor confirmation used by normal user closes.
bool force_close_cue_info_window();

} // namespace reaadr::ui
