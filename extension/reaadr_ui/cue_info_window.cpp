#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_Main_OnCommand

#include "cue_info_window.hpp"

#include "cue_info_controller.hpp"
#include "cue_manager_ui_contract.hpp"
#include "reaadr_ui.hpp"

#include <iomanip>
#include <sstream>
#include <string>

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>
#ifndef _WIN32
#include <swell/swell-dlggen.h>
#endif

namespace reaadr::ui {
namespace {
constexpr int kDialog = 48300;
constexpr int kHeader = 48301;
constexpr int kLiveMetrics = 48302;
constexpr int kCueId = 48303;
constexpr int kCharacter = 48304;
constexpr int kStatus = 48305;
constexpr int kCueType = 48306;
constexpr int kStart = 48307;
constexpr int kEnd = 48308;
constexpr int kDirection = 48309;
constexpr int kDialogue = 48310;
constexpr int kNotes = 48311;
constexpr int kPrevious = 48312;
constexpr int kNext = 48313;
constexpr int kJumpId = 48314;
constexpr int kJump = 48315;
constexpr int kSave = 48316;
constexpr int kError = 48317;
constexpr int kTimer = 1;
constexpr int kTransportPlayStop = 40044;

CueInfoController* g_controller = nullptr;
bool g_populating = false;
bool g_dirty = false;
std::string g_loaded_key;

std::string control_text(HWND hwnd, int id)
{
  const HWND control = GetDlgItem(hwnd, id);
  return read_control_text(GetWindowTextLength(control), [control](char* text, int capacity) {
    GetWindowText(control, text, capacity);
  });
}

std::string number(double value, int precision = 2)
{
  std::ostringstream text;
  text << std::fixed << std::setprecision(precision) << value;
  return text.str();
}

void fill_combo(HWND hwnd, int id, const std::vector<std::string>& values)
{
  const HWND combo = GetDlgItem(hwnd, id);
  SendMessage(combo, CB_RESETCONTENT, 0, 0);
  for (const auto& value : values)
    SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str()));
}

void refresh_character_choices(HWND hwnd)
{
  if (!g_controller) return;
  const std::string current = control_text(hwnd, kCharacter);
  fill_combo(hwnd, kCharacter, g_controller->character_choices());
  SetDlgItemText(hwnd, kCharacter, current.c_str());
}

void update_live(HWND hwnd)
{
  if (!g_controller) return;
  const auto& view = g_controller->view();
  const std::string header = "Cue " + view.cue_key + " | " + view.character + " | " + view.status;
  SetDlgItemText(hwnd, kHeader, header.c_str());
  const std::string metrics =
    "Length " + number(view.duration) + "s   Position " + view.position_timecode +
    "   Countdown " + number(view.countdown) + "s   Takes " + std::to_string(view.take_count);
  SetDlgItemText(hwnd, kLiveMetrics, metrics.c_str());
  SetDlgItemText(hwnd, kError, g_controller->error().c_str());
}

void populate_editors(HWND hwnd)
{
  if (!g_controller) return;
  const CueInfoEditValues values = g_controller->edit_values();
  g_populating = true;
  SetDlgItemText(hwnd, kCueId, values.cue_key.c_str());
  SetDlgItemText(hwnd, kCharacter, values.character.c_str());
  SetDlgItemText(hwnd, kStatus, values.status.c_str());
  SetDlgItemText(hwnd, kCueType, values.cue_type.c_str());
  SetDlgItemText(hwnd, kStart, values.start_time.c_str());
  SetDlgItemText(hwnd, kEnd, values.end_time.c_str());
  SetDlgItemText(hwnd, kDirection, values.direction.c_str());
  SetDlgItemText(hwnd, kDialogue, values.dialogue.c_str());
  SetDlgItemText(hwnd, kNotes, values.notes.c_str());
  g_populating = false;
  g_dirty = false;
  g_loaded_key = values.cue_key;
  update_live(hwnd);
}

CueInfoEditValues read_editors(HWND hwnd)
{
  CueInfoEditValues values;
  values.cue_key = control_text(hwnd, kCueId);
  values.character = control_text(hwnd, kCharacter);
  values.status = control_text(hwnd, kStatus);
  values.cue_type = control_text(hwnd, kCueType);
  values.start_time = control_text(hwnd, kStart);
  values.end_time = control_text(hwnd, kEnd);
  values.direction = control_text(hwnd, kDirection);
  values.dialogue = control_text(hwnd, kDialogue);
  values.notes = control_text(hwnd, kNotes);
  return values;
}

bool editor_control(int id)
{
  return id == kCueId || id == kCharacter || id == kStatus || id == kCueType ||
    id == kStart || id == kEnd || id == kDirection || id == kDialogue || id == kNotes;
}

bool editor_has_focus(HWND hwnd)
{
  const HWND focus = GetFocus();
  if (!focus) return false;
  const int editors[] = {
    kCueId, kCharacter, kStatus, kCueType, kStart, kEnd, kDirection, kDialogue, kNotes,
  };
  for (const int id : editors) {
    const HWND control = GetDlgItem(hwnd, id);
    if (control && (focus == control || IsChild(control, focus))) return true;
  }
  return false;
}

#ifndef _WIN32
INT_PTR cue_info_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  switch (message) {
    case WM_INITDIALOG:
      if (!g_controller) return 0;
      fill_combo(hwnd, kCharacter, g_controller->character_choices());
      fill_combo(hwnd, kStatus, core::cue_manager_status_choices());
      fill_combo(hwnd, kCueType, core::cue_manager_type_choices());
      if (!g_controller->refresh()) {
        SetDlgItemText(hwnd, kError, g_controller->error().c_str());
        return 1;
      }
      populate_editors(hwnd);
      SetTimer(hwnd, kTimer, 100, nullptr);
      return 1;

    case WM_TIMER:
      if (wparam == kTimer && g_controller && !g_dirty) {
        const std::string previous = g_loaded_key;
        if (g_controller->refresh()) {
          if (g_controller->view().cue_key != previous) populate_editors(hwnd);
          else update_live(hwnd);
        } else {
          SetDlgItemText(hwnd, kError, g_controller->error().c_str());
        }
      }
      return 1;

    case WM_KEYDOWN:
      if (wparam == VK_SPACE && !editor_has_focus(hwnd) && Main_OnCommand) {
        Main_OnCommand(kTransportPlayStop, 0);
        return 1;
      }
      return 0;

    case WM_COMMAND: {
      const int command = LOWORD(wparam);
      const int notification = HIWORD(wparam);
      if (!g_populating && editor_control(command) &&
          (notification == EN_CHANGE || notification == CBN_SELCHANGE || notification == CBN_EDITCHANGE)) {
        g_dirty = true;
        SetDlgItemText(hwnd, kError, "Unsaved edit - choose Save to apply changes.");
        return 1;
      }
      if (!g_controller) return 0;
      if (command == kSave) {
        if (g_controller->save(read_editors(hwnd))) {
          refresh_character_choices(hwnd);
          populate_editors(hwnd);
        } else {
          SetDlgItemText(hwnd, kError, g_controller->error().c_str());
        }
        return 1;
      }
      if (command == kPrevious || command == kNext) {
        const bool moved = command == kNext ? g_controller->next() : g_controller->previous();
        if (moved) populate_editors(hwnd);
        else SetDlgItemText(hwnd, kError, g_controller->error().c_str());
        return 1;
      }
      if (command == kJump) {
        if (g_controller->jump_to_id(control_text(hwnd, kJumpId))) populate_editors(hwnd);
        else SetDlgItemText(hwnd, kError, g_controller->error().c_str());
        return 1;
      }
      if (command == IDCANCEL) {
        KillTimer(hwnd, kTimer);
        EndDialog(hwnd, 0);
        return 1;
      }
      return 0;
    }

    case WM_CLOSE:
      KillTimer(hwnd, kTimer);
      EndDialog(hwnd, 0);
      return 1;
  }
  return 0;
}

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2(kDialog, SWELL_DLG_WS_FLIPPED,
  "ReaADR Cue Information", 850, 650)
BEGIN
  LTEXT "", kHeader, 18, 14, 800, 22
  LTEXT "", kLiveMetrics, 18, 40, 800, 20

  LTEXT "Cue", -1, 18, 82, 42, 16
  EDITTEXT kCueId, 64, 78, 110, 22, ES_AUTOHSCROLL
  LTEXT "Character", -1, 190, 82, 70, 16
  COMBOBOX kCharacter, 264, 78, 190, 120, CBS_DROPDOWN | WS_VSCROLL
  LTEXT "Status", -1, 470, 82, 52, 16
  COMBOBOX kStatus, 526, 78, 140, 110, CBS_DROPDOWN | WS_VSCROLL
  LTEXT "Type", -1, 680, 82, 38, 16
  COMBOBOX kCueType, 722, 78, 105, 100, CBS_DROPDOWN | WS_VSCROLL

  LTEXT "Start SMPTE / Seconds", -1, 18, 120, 145, 16
  EDITTEXT kStart, 18, 140, 170, 22, ES_AUTOHSCROLL
  LTEXT "End SMPTE / Seconds", -1, 204, 120, 145, 16
  EDITTEXT kEnd, 204, 140, 170, 22, ES_AUTOHSCROLL
  LTEXT "Direction", -1, 390, 120, 70, 16
  EDITTEXT kDirection, 390, 140, 437, 22, ES_AUTOHSCROLL

  LTEXT "Dialogue", -1, 18, 182, 70, 16
  EDITTEXT kDialogue, 18, 202, 809, 130, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL
  LTEXT "Notes", -1, 18, 350, 70, 16
  EDITTEXT kNotes, 18, 370, 809, 130, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL

  LTEXT "Jump to cue", -1, 18, 526, 82, 16
  EDITTEXT kJumpId, 104, 522, 120, 22, ES_AUTOHSCROLL
  PUSHBUTTON "Jump", kJump, 232, 520, 72, 26
  PUSHBUTTON "Previous", kPrevious, 326, 520, 86, 26
  PUSHBUTTON "Next", kNext, 418, 520, 72, 26
  PUSHBUTTON "Save", kSave, 604, 520, 86, 26
  DEFPUSHBUTTON "Close", IDCANCEL, 698, 520, 86, 26

  LTEXT "", kError, 18, 566, 809, 48
END
SWELL_DEFINE_DIALOG_RESOURCE_END2(kDialog)
#endif

} // namespace

bool show_cue_info_window(CueInfoController& controller)
{
#ifndef _WIN32
  if (g_controller) return false;
  g_controller = &controller;
  g_dirty = false;
  g_loaded_key.clear();
  const int result = DialogBoxParam(nullptr, MAKEINTRESOURCE(kDialog), nullptr, cue_info_proc, 0);
  g_controller = nullptr;
  return result >= 0;
#else
  (void)controller;
  return false;
#endif
}

} // namespace reaadr::ui
