#pragma once

namespace reaadr::ui {

class RecordingController;

// Native modeless Record Cue presentation shared across the Win32 and
// host-provided SWELL paths. Recording workflow ownership is entirely native;
// the historical Lua action is only a compatibility launcher/reference while
// final in-REAPER visual, docking, and lifecycle parity is smoke-tested.
bool show_recording_window(RecordingController& controller);
bool close_recording_window();
// Host/runtime teardown bypasses interactive recovery and guarantees that the
// modeless window releases its controller before the owning session is reset.
bool force_close_recording_window();

} // namespace reaadr::ui
