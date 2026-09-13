#include "cue_status_window.hpp"

#include <reaper_plugin.h>
#ifndef _WIN32
#include <swell/swell-dlggen.h>
#endif

namespace reaadr::ui {
namespace {
constexpr int kDialog = 48400;
constexpr int kStatus = 48401;
constexpr int kApply = 48402;

const std::vector<std::string>* g_statuses = nullptr;
std::string* g_selected_status = nullptr;

#ifndef _WIN32
INT_PTR cue_status_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  switch (message) {
    case WM_INITDIALOG: {
      if (!g_statuses || !g_selected_status || g_statuses->empty()) return 0;
      const HWND combo = GetDlgItem(hwnd, kStatus);
      for (const auto& status : *g_statuses)
        SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(status.c_str()));
      SendMessage(combo, CB_SETCURSEL, 0, 0);
      return 1;
    }

    case WM_COMMAND: {
      const int command = LOWORD(wparam);
      if (command == kApply || command == IDOK) {
        const HWND combo = GetDlgItem(hwnd, kStatus);
        const int index = static_cast<int>(SendMessage(combo, CB_GETCURSEL, 0, 0));
        if (g_statuses && g_selected_status && index >= 0 &&
            static_cast<std::size_t>(index) < g_statuses->size()) {
          *g_selected_status = (*g_statuses)[static_cast<std::size_t>(index)];
          EndDialog(hwnd, 1);
        }
        return 1;
      }
      if (command == IDCANCEL) {
        EndDialog(hwnd, 0);
        return 1;
      }
      return 0;
    }

    case WM_CLOSE:
      EndDialog(hwnd, 0);
      return 1;
  }
  return 0;
}

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2(kDialog, SWELL_DLG_WS_FLIPPED,
                                    "Set ADR Cue Status", 330, 120)
BEGIN
  LTEXT "Status", -1, 18, 18, 58, 18
  COMBOBOX kStatus, 78, 14, 226, 120, CBS_DROPDOWNLIST | WS_VSCROLL
  DEFPUSHBUTTON "Apply", kApply, 146, 66, 74, 26
  PUSHBUTTON "Cancel", IDCANCEL, 230, 66, 74, 26
END
SWELL_DEFINE_DIALOG_RESOURCE_END2(kDialog)
#endif

} // namespace

bool choose_cue_status(const std::vector<std::string>& statuses,
                       std::string& selected_status)
{
#ifndef _WIN32
  if (statuses.empty() || g_statuses || g_selected_status) return false;
  g_statuses = &statuses;
  g_selected_status = &selected_status;
  selected_status.clear();
  const int result = DialogBoxParam(
    nullptr, MAKEINTRESOURCE(kDialog), nullptr, cue_status_proc, 0);
  g_statuses = nullptr;
  g_selected_status = nullptr;
  return result == 1 && !selected_status.empty();
#else
  selected_status.clear();
  if (statuses.empty()) return false;

  HMENU menu = CreatePopupMenu();
  if (!menu) return false;

  constexpr UINT kFirstStatusCommand = 1;
  for (std::size_t index = 0; index < statuses.size(); ++index) {
    AppendMenuA(menu, MF_STRING,
                kFirstStatusCommand + static_cast<UINT>(index),
                statuses[index].c_str());
  }

  POINT point{};
  if (!GetCursorPos(&point)) {
    DestroyMenu(menu);
    return false;
  }

  HWND owner = GetForegroundWindow();
  if (owner) SetForegroundWindow(owner);
  const UINT command = TrackPopupMenu(
    menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
    point.x, point.y, 0, owner, nullptr);
  DestroyMenu(menu);

  if (command < kFirstStatusCommand) return false;
  const std::size_t index = static_cast<std::size_t>(command - kFirstStatusCommand);
  if (index >= statuses.size()) return false;
  selected_status = statuses[index];
  return true;
#endif
}

} // namespace reaadr::ui
