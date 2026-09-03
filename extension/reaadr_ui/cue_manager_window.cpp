#include "cue_manager_window.hpp"
#include <reaper_plugin.h>
#ifndef _WIN32
#include <swell/swell-dlggen.h>
#endif
#include <string>

#ifndef LBS_NOTIFY
#define LBS_NOTIFY 0x0001L
#endif

namespace reaadr::ui {
namespace {
constexpr int kDialog = 48001;
constexpr int kRows = 48002;
constexpr int kDetails = 48003;
constexpr int kPrevious = 48004;
constexpr int kNext = 48005;
const core::ManagerViewModel* g_view = nullptr;

void update_details(HWND hwnd, int index)
{
  if (!g_view || index < 0 || static_cast<std::size_t>(index) >= g_view->cues.rows.size()) return;
  const auto& row = g_view->cues.rows[static_cast<std::size_t>(index)];
  const std::string details = "Selected: " + row.cue_key + " | " + row.character +
    " | " + row.status + " | " + row.dialogue;
  SetDlgItemText(hwnd, kDetails, details.c_str());
}

#ifndef _WIN32
INT_PTR cue_manager_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  if (message == WM_INITDIALOG) {
    if (g_view) {
      for (const auto& row : g_view->cues.rows) {
        const std::string line = row.cue_key + "    " + row.character + "    " + row.status + "    " + row.dialogue;
        SendDlgItemMessage(hwnd, kRows, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
      }
      for (std::size_t i = 0; i < g_view->cues.rows.size(); ++i)
        if (g_view->cues.rows[i].selected) {
          SendDlgItemMessage(hwnd, kRows, LB_SETCURSEL, i, 0);
          update_details(hwnd, static_cast<int>(i));
        }
      const std::string details = "Selected: " + g_view->cues.selected_cue_key +
        "    Session: " + g_view->session_name;
      SetDlgItemText(hwnd, kDetails, details.c_str());
    }
    return 1;
  }
  if (message == WM_COMMAND && (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL)) {
    EndDialog(hwnd, 0); return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kRows && HIWORD(wparam) == LBN_SELCHANGE) {
    update_details(hwnd, static_cast<int>(SendDlgItemMessage(hwnd, kRows, LB_GETCURSEL, 0, 0)));
    return 1;
  }
  if (message == WM_COMMAND && (LOWORD(wparam) == kPrevious || LOWORD(wparam) == kNext)) {
    const LRESULT current = SendDlgItemMessage(hwnd, kRows, LB_GETCURSEL, 0, 0);
    const int count = g_view ? static_cast<int>(g_view->cues.rows.size()) : 0;
    if (count > 0) {
      int next = static_cast<int>(current);
      if (next < 0) next = 0;
      else next = LOWORD(wparam) == kNext ? (next + 1) % count : (next + count - 1) % count;
      SendDlgItemMessage(hwnd, kRows, LB_SETCURSEL, next, 0);
      update_details(hwnd, next);
    }
    return 1;
  }
  return 0;
}

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2(kDialog, SWELL_DLG_WS_FLIPPED,
  "ReaADR Tools - Cue Manager", 1180, 760)
BEGIN
  LTEXT "ReaADR Cue Manager", -1, 16, 12, 300, 16
  LTEXT "Character / Filter", -1, 16, 42, 160, 14
  EDITTEXT kDetails, 180, 40, 960, 20, ES_AUTOHSCROLL
  LISTBOX kRows, 16, 72, 1124, 620, LBS_NOTIFY | WS_VSCROLL | WS_BORDER
  PUSHBUTTON "Previous", kPrevious, 16, 710, 90, 24
  PUSHBUTTON "Next", kNext, 112, 710, 90, 24
  DEFPUSHBUTTON "Close", IDCANCEL, 1050, 710, 90, 24
END
SWELL_DEFINE_DIALOG_RESOURCE_END2(kDialog)
#endif
}

bool show_cue_manager(const core::ManagerViewModel& view)
{
#ifndef _WIN32
  g_view = &view;
  const int result = DialogBoxParam(nullptr, MAKEINTRESOURCE(kDialog), nullptr, cue_manager_proc, 0);
  g_view = nullptr;
  return result >= 0;
#else
  (void)view;
  return false;
#endif
}
} // namespace reaadr::ui
