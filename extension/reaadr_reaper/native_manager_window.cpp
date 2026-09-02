#include "native_manager_window.hpp"

#include "reaadr_core/manager_navigation.hpp"
#include "reaadr_core/manager_view_model.hpp"
#include <reaper_plugin.h>
#include <string>

#ifndef _WIN32
#include <swell/swell-dlggen.h>
#endif

namespace reaadr::reaper {
namespace {
constexpr int kManagerDialog = 47001;
constexpr int kTabImport = 47010;
constexpr int kTabCues = 47011;
constexpr int kTabSession = 47012;
constexpr int kTabReports = 47013;
constexpr int kTabOverlay = 47014;
constexpr int kTabPreferences = 47015;
constexpr int kTabHelp = 47016;
constexpr int kClose = 47020;
constexpr int kBody = 47021;
constexpr int kCueList = 47022;
const core::ManagerViewModel* g_view = nullptr;

#ifndef _WIN32
INT_PTR manager_dialog_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  if (message == WM_INITDIALOG) {
    SetDlgItemText(hwnd, kBody,
      "Select a Manager tab to configure ReaADR Tools.\r\n\r\n"
      "This native shell is driven by the C++ session and preference model.");
    if (g_view) {
      for (const auto& row : g_view->cues.rows) {
        const std::string line = (row.selected ? "> " : "  ") + row.cue_key + "  " +
          row.character + "  [" + row.status + "]  " + row.dialogue;
        SendDlgItemMessage(hwnd, kCueList, LB_ADDSTRING, 0,
          reinterpret_cast<LPARAM>(line.c_str()));
      }
    }
    return 1;
  }
  if (message == WM_COMMAND) {
    const int command = LOWORD(wparam);
    if (command == kClose || command == IDCANCEL) { EndDialog(hwnd, 0); return 1; }
    const char* title = nullptr;
    switch (command) {
      case kTabImport: title = "Import"; break;
      case kTabCues: title = "Cue Management"; break;
      case kTabSession: title = "Session Tools"; break;
      case kTabReports: title = "Reports"; break;
      case kTabOverlay: title = "Video Overlays"; break;
      case kTabPreferences: title = "Preferences"; break;
      case kTabHelp: title = "Help"; break;
      default: break;
    }
    if (title) {
      const std::string body = std::string(title) +
        "\r\n\r\nNative Manager view selected. Controls are backed by the C++ model.";
      SetDlgItemText(hwnd, kBody, body.c_str());
      return 1;
    }
  }
  return 0;
}

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2(kManagerDialog, SWELL_DLG_WS_FLIPPED,
  "ReaADR Tools Manager", 520, 300)
BEGIN
  LTEXT "ReaADR Tools Manager", -1, 12, 10, 300, 16
  PUSHBUTTON "Import", kTabImport, 12, 34, 68, 24
  PUSHBUTTON "Cues", kTabCues, 84, 34, 68, 24
  PUSHBUTTON "Session", kTabSession, 156, 34, 68, 24
  PUSHBUTTON "Reports", kTabReports, 228, 34, 68, 24
  PUSHBUTTON "Overlay", kTabOverlay, 300, 34, 68, 24
  PUSHBUTTON "Preferences", kTabPreferences, 372, 34, 68, 24
  PUSHBUTTON "Help", kTabHelp, 444, 34, 64, 24
  LISTBOX kCueList, 12, 70, 496, 170, LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_BORDER
  EDITTEXT kBody, 12, 245, 400, 42, ES_MULTILINE | ES_READONLY
  DEFPUSHBUTTON "Close", kClose, 430, 262, 78, 24
END
SWELL_DEFINE_DIALOG_RESOURCE_END2(kManagerDialog)
#endif
}

void show_native_manager_window(const core::ManagerViewModel* view)
{
  g_view = view;
#ifndef _WIN32
  DialogBoxParam(nullptr, MAKEINTRESOURCE(kManagerDialog), nullptr,
    manager_dialog_proc, 0);
#else
  // Windows resources are supplied by the MSVC resource build; until that
  // resource is linked, keep the command safe and visible to the user.
  ShowMessageBox("The native Manager UI resource is not available in this build.",
    "ReaADR Tools Manager", 0);
#endif
  g_view = nullptr;
}

} // namespace reaadr::reaper
