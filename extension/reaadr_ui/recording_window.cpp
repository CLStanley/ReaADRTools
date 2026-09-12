#include "recording_window.hpp"

#include "recording_controller.hpp"

#include <reaper_plugin.h>
#ifndef _WIN32
#include <swell/swell-dlggen.h>
#endif

#include <iomanip>
#include <sstream>
#include <string>

namespace reaadr::ui {
namespace {
constexpr int kDialog = 48200;
constexpr int kCue = 48201;
constexpr int kDialogue = 48202;
constexpr int kTiming = 48203;
constexpr int kTrack = 48204;
constexpr int kStatus = 48205;
constexpr int kRecord = 48206;
constexpr int kLoop = 48207;
constexpr int kPreroll = 48208;
constexpr int kStop = 48209;
constexpr int kTimer = 1;

RecordingController* g_controller = nullptr;

std::string number(double value)
{
  std::ostringstream output;
  output << std::fixed << std::setprecision(3) << value;
  return output.str();
}

void update_window(HWND hwnd)
{
  if (!g_controller) return;
  const auto& view = g_controller->view();
  const std::string cue = "Cue " + view.cue_key + " - " + view.character;
  SetDlgItemText(hwnd, kCue, cue.c_str());
  SetDlgItemText(hwnd, kDialogue,
                 view.dialogue.empty() ? "(no dialogue)" : view.dialogue.c_str());

  const double duration = view.cue_end - view.cue_start;
  const std::string timing = "Start " + number(view.cue_start) + "s   Duration " +
    number(duration) + "s   Preroll " + number(view.preroll_seconds) + "s";
  SetDlgItemText(hwnd, kTiming, timing.c_str());

  const std::string track = "Track: " + view.track_key + "   Lane " +
    std::to_string(view.lane);
  SetDlgItemText(hwnd, kTrack, track.c_str());
  SetDlgItemText(hwnd, kStatus, view.status_text.c_str());
  SetDlgItemText(hwnd, kLoop, view.loop_enabled ? "Loop: ON" : "Loop: OFF");
  SetDlgItemText(hwnd, kPreroll,
                 view.include_preroll_each_loop ? "Pre-roll Each Loop" : "Default Repeat");

  EnableWindow(GetDlgItem(hwnd, kRecord),
               view.mode == core::RecordingTransportMode::idle);
}

bool show_error(HWND hwnd)
{
  if (!g_controller || g_controller->view().error.empty()) return false;
  MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Record Cue", MB_OK);
  return true;
}

bool close_recording(HWND hwnd)
{
  if (g_controller && !g_controller->shutdown()) {
    show_error(hwnd);
    update_window(hwnd);
    return false;
  }
  KillTimer(hwnd, kTimer);
  EndDialog(hwnd, 0);
  return true;
}

#ifndef _WIN32
INT_PTR recording_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  switch (message) {
    case WM_INITDIALOG:
      if (!g_controller || !g_controller->begin()) {
        show_error(hwnd);
        EndDialog(hwnd, -1);
        return 1;
      }
      update_window(hwnd);
      SetTimer(hwnd, kTimer, 30, nullptr);
      return 1;

    case WM_TIMER:
      if (wparam == kTimer && g_controller) {
        if (!g_controller->tick()) show_error(hwnd);
        update_window(hwnd);
      }
      return 1;

    case WM_COMMAND: {
      const int command = LOWORD(wparam);
      if (!g_controller) return 0;
      bool changed = false;
      if (command == kRecord) {
        changed = g_controller->record();
      } else if (command == kStop) {
        changed = g_controller->stop();
      } else if (command == kPreroll) {
        changed = g_controller->toggle_preroll_each_loop();
      } else if (command == kLoop) {
        if (!g_controller->view().loop_enabled) {
          const int answer = MessageBox(
            hwnd,
            "Loop recording should create new takes or lanes for each pass.\n\n"
            "If REAPER is configured to replace or trim overlapping recordings, prior passes can be overwritten.\n\n"
            "Check Options > New recording that overlaps existing media items before continuing.\n\n"
            "Enable loop recording?",
            "ReaADR Loop Recording", MB_YESNO);
          if (answer != IDYES) return 1;
        }
        changed = g_controller->toggle_loop();
      } else if (command == IDCANCEL) {
        close_recording(hwnd);
        return 1;
      } else {
        return 0;
      }
      if (!changed) show_error(hwnd);
      update_window(hwnd);
      return 1;
    }

    case WM_CLOSE:
      close_recording(hwnd);
      return 1;
  }
  return 0;
}

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(kDialog, SWELL_DLG_WS_FLIPPED,
                                   "ReaADR Record Cue", 570, 300, 1.0)
BEGIN
  LTEXT "", kCue, 20, 18, 520, 22
  LTEXT "", kDialogue, 20, 50, 520, 38
  LTEXT "", kTiming, 20, 96, 520, 20
  LTEXT "", kTrack, 20, 124, 520, 20
  LTEXT "Ready", kStatus, 20, 158, 520, 24
  PUSHBUTTON "Record", kRecord, 20, 220, 105, 32
  PUSHBUTTON "Loop: OFF", kLoop, 137, 220, 105, 32
  PUSHBUTTON "Pre-roll Each Loop", kPreroll, 254, 220, 145, 32
  PUSHBUTTON "Stop", kStop, 435, 220, 105, 32
  DEFPUSHBUTTON "Close", IDCANCEL, 435, 262, 105, 24
END
SWELL_DEFINE_DIALOG_RESOURCE_END2(kDialog)
#endif

} // namespace

bool show_recording_window(RecordingController& controller)
{
#ifndef _WIN32
  if (g_controller) return false;
  g_controller = &controller;
  const int result = DialogBoxParam(
    nullptr, MAKEINTRESOURCE(kDialog), nullptr, recording_proc, 0);
  g_controller = nullptr;
  return result >= 0;
#else
  (void)controller;
  return false;
#endif
}

} // namespace reaadr::ui
