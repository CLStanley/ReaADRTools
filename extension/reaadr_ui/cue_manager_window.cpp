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
constexpr int kFilter = 48006;
constexpr int kApplyFilter = 48007;
constexpr int kEditDialogue = 48008, kEditNotes = 48009, kEditType = 48010;
constexpr int kEditStart = 48011, kEditEnd = 48012, kEditStatus = 48013, kApplyEdit = 48014;
CueManagerController* g_controller = nullptr;

void update_details(HWND hwnd, int index)
{
  if (!g_controller || index < 0 || static_cast<std::size_t>(index) >= g_controller->view().cues.rows.size()) return;
  const auto& row = g_controller->view().cues.rows[static_cast<std::size_t>(index)];
  const std::string details = "Selected: " + row.cue_key + " | " + row.character +
    " | " + row.status + " | " + row.dialogue;
  SetDlgItemText(hwnd, kDetails, details.c_str());
}

void refresh_rows(HWND hwnd)
{
  if (!g_controller) return;
  SendDlgItemMessage(hwnd, kRows, LB_RESETCONTENT, 0, 0);
  const auto& view = g_controller->view();
  int selected = -1;
  for (std::size_t i = 0; i < view.cues.rows.size(); ++i) {
    const auto& row = view.cues.rows[i];
    const std::string line = row.cue_key + "    " + row.character + "    " + row.status + "    " + row.dialogue;
    SendDlgItemMessage(hwnd, kRows, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
    if (row.selected) selected = static_cast<int>(i);
  }
  if (selected >= 0) {
    SendDlgItemMessage(hwnd, kRows, LB_SETCURSEL, selected, 0);
    update_details(hwnd, selected);
  } else SetDlgItemText(hwnd, kDetails, "Selected: (none)");
}

#ifndef _WIN32
INT_PTR cue_manager_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  if (message == WM_INITDIALOG) {
    if (g_controller) {
      const auto& view = g_controller->view();
      refresh_rows(hwnd);
      for (std::size_t i = 0; i < view.cues.rows.size(); ++i)
        if (view.cues.rows[i].selected) {
          SendDlgItemMessage(hwnd, kRows, LB_SETCURSEL, i, 0);
          update_details(hwnd, static_cast<int>(i));
        }
      const std::string details = "Selected: " + view.cues.selected_cue_key +
        "    Session: " + view.session_name;
      SetDlgItemText(hwnd, kDetails, details.c_str());
    }
    return 1;
  }
  if (message == WM_COMMAND && (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL)) {
    EndDialog(hwnd, 0); return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kRows && HIWORD(wparam) == LBN_SELCHANGE) {
    const int index = static_cast<int>(SendDlgItemMessage(hwnd, kRows, LB_GETCURSEL, 0, 0));
    if (g_controller) g_controller->select_index(index);
    update_details(hwnd, index);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kApplyFilter) {
    char filter[256] = {};
    GetDlgItemText(hwnd, kFilter, filter, sizeof(filter));
    if (g_controller && g_controller->set_character_filter(filter)) refresh_rows(hwnd);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kApplyEdit) {
    if (g_controller) {
      char dialogue[512] = {}, notes[512] = {}, type[128] = {}, start[64] = {}, end[64] = {}, status[128] = {};
      GetDlgItemText(hwnd, kEditDialogue, dialogue, sizeof(dialogue));
      GetDlgItemText(hwnd, kEditNotes, notes, sizeof(notes));
      GetDlgItemText(hwnd, kEditType, type, sizeof(type));
      GetDlgItemText(hwnd, kEditStart, start, sizeof(start));
      GetDlgItemText(hwnd, kEditEnd, end, sizeof(end));
      GetDlgItemText(hwnd, kEditStatus, status, sizeof(status));
      core::CueManagerEditOptions edit;
      edit.dialogue = dialogue; edit.notes = notes; edit.notes_set = true;
      edit.cue_type = type; edit.start_time = start; edit.end_time = end;
      std::string error;
      if (g_controller->edit_selected(edit, error)) refresh_rows(hwnd);
      else if (!error.empty()) MessageBox(nullptr, error.c_str(), "ReaADR Cue Manager", 0);
      if (status[0] != '\0') {
        if (g_controller->set_selected_status(status, error)) refresh_rows(hwnd);
        else if (!error.empty()) MessageBox(nullptr, error.c_str(), "ReaADR Cue Manager", 0);
      }
    }
    return 1;
  }
  if (message == WM_COMMAND && (LOWORD(wparam) == kPrevious || LOWORD(wparam) == kNext)) {
    if (g_controller) {
      const bool moved = LOWORD(wparam) == kNext ? g_controller->navigate_next() : g_controller->navigate_previous();
      if (moved) refresh_rows(hwnd);
    }
    return 1;
  }
  return 0;
}

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2(kDialog, SWELL_DLG_WS_FLIPPED,
  "ReaADR Tools - Cue Manager", 1180, 820)
BEGIN
  LTEXT "ReaADR Cue Manager", -1, 16, 12, 300, 16
  LTEXT "Character / Filter", -1, 16, 42, 160, 14
  EDITTEXT kFilter, 180, 40, 760, 20, ES_AUTOHSCROLL
  PUSHBUTTON "Apply", kApplyFilter, 950, 40, 80, 20
  EDITTEXT kDetails, 16, 696, 1010, 20, ES_AUTOHSCROLL | ES_READONLY
  LTEXT "Dialogue", -1, 16, 756, 70, 14
  EDITTEXT kEditDialogue, 90, 754, 330, 20, ES_AUTOHSCROLL
  LTEXT "Notes", -1, 430, 756, 50, 14
  EDITTEXT kEditNotes, 480, 754, 330, 20, ES_AUTOHSCROLL
  LTEXT "Type", -1, 820, 756, 40, 14
  EDITTEXT kEditType, 860, 754, 100, 20, ES_AUTOHSCROLL
  LTEXT "Start", -1, 16, 782, 50, 14
  EDITTEXT kEditStart, 70, 780, 120, 20, ES_AUTOHSCROLL
  LTEXT "End", -1, 200, 782, 40, 14
  EDITTEXT kEditEnd, 245, 780, 120, 20, ES_AUTOHSCROLL
  LTEXT "Status", -1, 380, 782, 50, 14
  EDITTEXT kEditStatus, 435, 780, 180, 20, ES_AUTOHSCROLL
  PUSHBUTTON "Apply Edit", kApplyEdit, 630, 778, 100, 24
  LISTBOX kRows, 16, 72, 1124, 620, LBS_NOTIFY | WS_VSCROLL | WS_BORDER
  PUSHBUTTON "Previous", kPrevious, 740, 778, 90, 24
  PUSHBUTTON "Next", kNext, 836, 778, 90, 24
  DEFPUSHBUTTON "Close", IDCANCEL, 1050, 778, 90, 24
END
SWELL_DEFINE_DIALOG_RESOURCE_END2(kDialog)
#endif
}

bool show_cue_manager(CueManagerController& controller)
{
#ifndef _WIN32
  g_controller = &controller;
  const int result = DialogBoxParam(nullptr, MAKEINTRESOURCE(kDialog), nullptr, cue_manager_proc, 0);
  g_controller = nullptr;
  return result >= 0;
#else
  (void)view;
  return false;
#endif
}
} // namespace reaadr::ui
