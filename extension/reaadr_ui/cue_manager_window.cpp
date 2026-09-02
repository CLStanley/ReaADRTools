#include "cue_manager_window.hpp"
#include <reaper_plugin.h>
#ifndef _WIN32
#include <swell/swell-dlggen.h>
#endif
#include <string>

namespace reaadr::ui {
namespace {
constexpr int kDialog = 48001;
constexpr int kRows = 48002;
constexpr int kDetails = 48003;
const core::ManagerViewModel* g_view = nullptr;

#ifndef _WIN32
INT_PTR cue_manager_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  if (message == WM_INITDIALOG) {
    if (g_view) {
      std::string rows = "Cue    Character                 Status        Dialogue\r\n";
      for (const auto& row : g_view->cues.rows)
        rows += row.cue_key + "    " + row.character + "    " + row.status + "    " + row.dialogue + "\r\n";
      SetDlgItemText(hwnd, kRows, rows.c_str());
      const std::string details = "Selected: " + g_view->cues.selected_cue_key +
        "    Session: " + g_view->session_name;
      SetDlgItemText(hwnd, kDetails, details.c_str());
    }
    return 1;
  }
  if (message == WM_COMMAND && (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL)) {
    EndDialog(hwnd, 0); return 1;
  }
  return 0;
}

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2(kDialog, SWELL_DLG_WS_FLIPPED,
  "ReaADR Tools - Cue Manager", 1180, 760)
BEGIN
  LTEXT "ReaADR Cue Manager", -1, 16, 12, 300, 16
  LTEXT "Character / Filter", -1, 16, 42, 160, 14
  EDITTEXT kDetails, 180, 40, 960, 20, ES_AUTOHSCROLL
  EDITTEXT kRows, 16, 72, 1124, 620, ES_MULTILINE | ES_READONLY | WS_VSCROLL
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
