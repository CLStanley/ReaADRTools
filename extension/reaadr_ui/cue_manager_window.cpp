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
constexpr int kCharacterFilter = 48006;
constexpr int kApplyFilter = 48007;
constexpr int kEditDialogue = 48008, kEditNotes = 48009, kEditType = 48010;
constexpr int kEditStart = 48011, kEditEnd = 48012, kEditStatus = 48013, kApplyEdit = 48014;
constexpr int kEditCueId = 48015, kEditCharacter = 48016;
constexpr int kSearchFilter = 48017, kStatusFilter = 48018, kResetFilter = 48019;
constexpr int kJumpCueId = 48020, kJump = 48021;
constexpr int kNewCue = 48022, kAddCue = 48023, kRemoveCue = 48024;
constexpr int kTabImport = 48025, kTabCues = 48026, kTabSession = 48027;
constexpr int kTabReports = 48028, kTabOverlay = 48029, kTabPreferences = 48030, kTabHelp = 48031;
constexpr int kImportBrowse = 48032, kImportRun = 48033, kImportPreview = 48035;
constexpr int kImportMapping = 48034;
constexpr int kImportMode = 48036, kImportCharacters = 48037;
CueManagerController* g_controller = nullptr;

void populate_add_editor(HWND hwnd)
{
  if (!g_controller) return;
  const core::CueManagerAddOptions cue = g_controller->default_add_options();
  SetDlgItemText(hwnd, kEditCueId, cue.cue_key.c_str());
  SetDlgItemText(hwnd, kEditCharacter, cue.character.c_str());
  SetDlgItemText(hwnd, kEditDialogue, cue.dialogue.c_str());
  SetDlgItemText(hwnd, kEditNotes, cue.notes.c_str());
  SetDlgItemText(hwnd, kEditType, cue.cue_type.c_str());
  SetDlgItemText(hwnd, kEditStart, cue.start_time.c_str());
  SetDlgItemText(hwnd, kEditEnd, cue.end_time.c_str());
  SetDlgItemText(hwnd, kEditStatus, cue.status.c_str());
  SetDlgItemText(hwnd, kDetails, "New cue: edit the fields below, then choose Add Cue");
}

void update_editor(HWND hwnd)
{
  const core::CueManagerRow* row = g_controller ? g_controller->selected_row() : nullptr;
  SetDlgItemText(hwnd, kEditCueId, row ? row->cue_key.c_str() : "");
  SetDlgItemText(hwnd, kEditCharacter, row ? row->character.c_str() : "");
  SetDlgItemText(hwnd, kEditDialogue, row ? row->dialogue.c_str() : "");
  SetDlgItemText(hwnd, kEditNotes, row ? row->notes.c_str() : "");
  SetDlgItemText(hwnd, kEditType, row ? row->cue_type.c_str() : "");
  SetDlgItemText(hwnd, kEditStart, row ? row->start_time.c_str() : "");
  SetDlgItemText(hwnd, kEditEnd, row ? row->end_time.c_str() : "");
  SetDlgItemText(hwnd, kEditStatus, row ? row->status.c_str() : "");
}

void update_details(HWND hwnd, int index)
{
  if (!g_controller || index < 0 || static_cast<std::size_t>(index) >= g_controller->view().cues.rows.size()) return;
  const auto& row = g_controller->view().cues.rows[static_cast<std::size_t>(index)];
  const std::string details = "Selected: " + row.cue_key + " | " + row.character +
    " | " + row.status + " | " + row.dialogue;
  SetDlgItemText(hwnd, kDetails, details.c_str());
  update_editor(hwnd);
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
  } else {
    SetDlgItemText(hwnd, kDetails, "Selected: (none)");
    update_editor(hwnd);
  }
}

#ifndef _WIN32
INT_PTR cue_manager_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  if (message == WM_INITDIALOG) {
    SetDlgItemText(hwnd, kImportMode, "all");
    if (g_controller) {
      const std::string mapping = g_controller->last_import_mapping();
      if (!mapping.empty()) SetDlgItemText(hwnd, kImportMapping, mapping.c_str());
      const auto& view = g_controller->view();
      refresh_rows(hwnd);
      for (std::size_t i = 0; i < view.cues.rows.size(); ++i)
        if (view.cues.rows[i].selected) {
          SendDlgItemMessage(hwnd, kRows, LB_SETCURSEL, i, 0);
          update_details(hwnd, static_cast<int>(i));
        }
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
    char query[256] = {}, character[256] = {}, status[128] = {};
    GetDlgItemText(hwnd, kSearchFilter, query, sizeof(query));
    GetDlgItemText(hwnd, kCharacterFilter, character, sizeof(character));
    GetDlgItemText(hwnd, kStatusFilter, status, sizeof(status));
    if (g_controller && g_controller->set_filters(query, character, status)) refresh_rows(hwnd);
    return 1;
  }
  if (message == WM_COMMAND) {
    const int command = LOWORD(wparam);
    const char* tab = command == kTabImport ? "import" : command == kTabCues ? "cues" :
      command == kTabSession ? "session" : command == kTabReports ? "reports" :
      command == kTabOverlay ? "overlay" : command == kTabPreferences ? "preferences" :
      command == kTabHelp ? "help" : nullptr;
    if (tab && g_controller && g_controller->set_tab(tab)) {
      const std::string title = "ReaADR Manager - " + g_controller->view().active_tab;
      SetWindowText(hwnd, title.c_str());
      SetDlgItemText(hwnd, kDetails, ("Active tab: " + g_controller->view().active_tab).c_str());
      return 1;
    }
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kImportRun) {
    if (g_controller) {
      char mapping[2048] = {};
      char mode[64] = {}, characters[512] = {};
      GetDlgItemText(hwnd, kImportMapping, mapping, sizeof(mapping));
      GetDlgItemText(hwnd, kImportMode, mode, sizeof(mode));
      GetDlgItemText(hwnd, kImportCharacters, characters, sizeof(characters));
      g_controller->trigger_import(mapping, false, mode, characters);
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kImportPreview) {
    if (g_controller) {
      char mapping[2048] = {};
      char mode[64] = {}, characters[512] = {};
      GetDlgItemText(hwnd, kImportMapping, mapping, sizeof(mapping));
      GetDlgItemText(hwnd, kImportMode, mode, sizeof(mode));
      GetDlgItemText(hwnd, kImportCharacters, characters, sizeof(characters));
      g_controller->trigger_import(mapping, true, mode, characters);
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kResetFilter) {
    SetDlgItemText(hwnd, kSearchFilter, "");
    SetDlgItemText(hwnd, kCharacterFilter, "");
    SetDlgItemText(hwnd, kStatusFilter, "");
    if (g_controller && g_controller->set_filters({}, {}, {})) refresh_rows(hwnd);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kJump) {
    char cue_id[128] = {};
    GetDlgItemText(hwnd, kJumpCueId, cue_id, sizeof(cue_id));
    std::string error;
    if (g_controller && g_controller->navigate_to_id(cue_id, error)) {
      SetDlgItemText(hwnd, kSearchFilter, "");
      SetDlgItemText(hwnd, kCharacterFilter, "");
      SetDlgItemText(hwnd, kStatusFilter, "");
      refresh_rows(hwnd);
    }
    else if (!error.empty()) MessageBox(nullptr, error.c_str(), "ReaADR Cue Manager", 0);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kNewCue) {
    populate_add_editor(hwnd);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kAddCue) {
    if (g_controller) {
      char cue_id[128] = {}, character[256] = {}, dialogue[512] = {}, notes[512] = {};
      char type[128] = {}, start[64] = {}, end[64] = {}, status[128] = {};
      GetDlgItemText(hwnd, kEditCueId, cue_id, sizeof(cue_id));
      GetDlgItemText(hwnd, kEditCharacter, character, sizeof(character));
      GetDlgItemText(hwnd, kEditDialogue, dialogue, sizeof(dialogue));
      GetDlgItemText(hwnd, kEditNotes, notes, sizeof(notes));
      GetDlgItemText(hwnd, kEditType, type, sizeof(type));
      GetDlgItemText(hwnd, kEditStart, start, sizeof(start));
      GetDlgItemText(hwnd, kEditEnd, end, sizeof(end));
      GetDlgItemText(hwnd, kEditStatus, status, sizeof(status));
      core::CueManagerAddOptions cue;
      cue.cue_key = cue_id; cue.character = character; cue.dialogue = dialogue;
      cue.notes = notes; cue.cue_type = type; cue.status = status;
      cue.start_time = start; cue.end_time = end;
      std::string error;
      if (g_controller->add_cue(cue, error)) refresh_rows(hwnd);
      else if (!error.empty()) MessageBox(nullptr, error.c_str(), "ReaADR Cue Manager", 0);
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kRemoveCue) {
    const core::CueManagerRow* row = g_controller ? g_controller->selected_row() : nullptr;
    if (!row) {
      MessageBox(nullptr, "Select a cue before removing it.", "ReaADR Cue Manager", 0);
      return 1;
    }
    const std::string prompt = "Remove cue " + row->cue_key + " (" + row->character +
      ")? Remaining cues will be renumbered and cue regions/audio will be rebuilt.";
    if (MessageBox(hwnd, prompt.c_str(), "ReaADR Cue Manager", MB_YESNO | MB_ICONWARNING) == IDYES) {
      std::string error;
      if (g_controller->remove_selected(error)) refresh_rows(hwnd);
      else if (!error.empty()) MessageBox(nullptr, error.c_str(), "ReaADR Cue Manager", 0);
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kApplyEdit) {
    if (g_controller) {
      char cue_id[128] = {}, character[256] = {}, dialogue[512] = {}, notes[512] = {};
      char type[128] = {}, start[64] = {}, end[64] = {}, status[128] = {};
      GetDlgItemText(hwnd, kEditCueId, cue_id, sizeof(cue_id));
      GetDlgItemText(hwnd, kEditCharacter, character, sizeof(character));
      GetDlgItemText(hwnd, kEditDialogue, dialogue, sizeof(dialogue));
      GetDlgItemText(hwnd, kEditNotes, notes, sizeof(notes));
      GetDlgItemText(hwnd, kEditType, type, sizeof(type));
      GetDlgItemText(hwnd, kEditStart, start, sizeof(start));
      GetDlgItemText(hwnd, kEditEnd, end, sizeof(end));
      GetDlgItemText(hwnd, kEditStatus, status, sizeof(status));
      core::CueManagerEditOptions edit;
      edit.new_cue_key = cue_id; edit.new_character = character;
      edit.dialogue = dialogue; edit.dialogue_set = true;
      edit.notes = notes; edit.notes_set = true;
      edit.cue_type = type; edit.status = status;
      edit.start_time = start; edit.end_time = end;
      std::string error;
      if (g_controller->edit_selected(edit, error)) refresh_rows(hwnd);
      else if (!error.empty()) MessageBox(nullptr, error.c_str(), "ReaADR Cue Manager", 0);
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
  PUSHBUTTON "Import", kTabImport, 320, 10, 72, 22
  PUSHBUTTON "Cues", kTabCues, 396, 10, 72, 22
  PUSHBUTTON "Session", kTabSession, 472, 10, 72, 22
  PUSHBUTTON "Reports", kTabReports, 548, 10, 72, 22
  PUSHBUTTON "Overlay", kTabOverlay, 624, 10, 72, 22
  PUSHBUTTON "Preferences", kTabPreferences, 700, 10, 92, 22
  PUSHBUTTON "Help", kTabHelp, 796, 10, 72, 22
  LTEXT "Cue sheet import uses the native transactional parser and renderer.", -1, 16, 70, 620, 16
  LTEXT "Mapping (optional)", -1, 16, 92, 110, 16
  EDITTEXT kImportMapping, 126, 90, 500, 20, ES_AUTOHSCROLL
  LTEXT "Mode (all/selected)", -1, 16, 118, 110, 16
  EDITTEXT kImportMode, 126, 116, 120, 20, ES_AUTOHSCROLL
  LTEXT "Characters (; separated)", -1, 260, 118, 150, 16
  EDITTEXT kImportCharacters, 414, 116, 300, 20, ES_AUTOHSCROLL
  PUSHBUTTON "Choose Cue Sheet and Import", kImportRun, 650, 66, 190, 24
  PUSHBUTTON "Preview Headers", kImportPreview, 846, 66, 120, 24
  LTEXT "Search", -1, 16, 42, 48, 14
  EDITTEXT kSearchFilter, 66, 40, 220, 20, ES_AUTOHSCROLL
  LTEXT "Character", -1, 294, 42, 68, 14
  EDITTEXT kCharacterFilter, 364, 40, 170, 20, ES_AUTOHSCROLL
  LTEXT "Status", -1, 542, 42, 48, 14
  EDITTEXT kStatusFilter, 592, 40, 145, 20, ES_AUTOHSCROLL
  PUSHBUTTON "Apply", kApplyFilter, 745, 40, 58, 20
  PUSHBUTTON "Reset", kResetFilter, 807, 40, 58, 20
  LTEXT "Jump", -1, 878, 42, 38, 14
  EDITTEXT kJumpCueId, 918, 40, 130, 20, ES_AUTOHSCROLL
  PUSHBUTTON "Go", kJump, 1054, 40, 50, 20
  PUSHBUTTON "New Cue", kNewCue, 16, 70, 82, 20
  PUSHBUTTON "Add Cue", kAddCue, 104, 70, 82, 20
  PUSHBUTTON "Remove Cue", kRemoveCue, 192, 70, 96, 20
  EDITTEXT kDetails, 16, 696, 1010, 20, ES_AUTOHSCROLL | ES_READONLY
  LTEXT "Cue ID", -1, 16, 730, 50, 14
  EDITTEXT kEditCueId, 70, 728, 120, 20, ES_AUTOHSCROLL
  LTEXT "Character", -1, 200, 730, 68, 14
  EDITTEXT kEditCharacter, 270, 728, 260, 20, ES_AUTOHSCROLL
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
  LISTBOX kRows, 16, 98, 1124, 594, LBS_NOTIFY | WS_VSCROLL | WS_BORDER
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
  (void)controller;
  return false;
#endif
}
} // namespace reaadr::ui
