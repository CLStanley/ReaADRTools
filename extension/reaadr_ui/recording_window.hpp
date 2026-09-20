#pragma once

namespace reaadr::ui {

class RecordingController;

// Transitional SWELL presentation for the native Record Cue controller. The
// Lua window remains installed as the parity reference until all platforms and
// interactions have been smoke-tested against it.
bool show_recording_window(RecordingController& controller);
bool close_recording_window();
// Host/runtime teardown bypasses interactive recovery and guarantees that the
// modeless window releases its controller before the owning session is reset.
bool force_close_recording_window();

} // namespace reaadr::ui
