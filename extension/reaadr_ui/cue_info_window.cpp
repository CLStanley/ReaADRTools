#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_Main_OnCommand

#include "cue_info_window.hpp"

#include "cue_info_controller.hpp"
#include "cue_manager_ui_contract.hpp"
#include "reaadr_ui.hpp"
#include "reaadr_reaper/window_docking.hpp"
#ifdef _WIN32
#include "win32_utf8.hpp"
#endif

#include <algorithm>
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
constexpr int kVkR = 0x52;
constexpr int kMinWindowWidth = 820;
constexpr int kMinWindowHeight = 560;
constexpr const char* kDockIdentifier = "reaadr.cue_info";
constexpr const char* kWindowTitle = "ReaADR Cue Information";
#ifdef _WIN32
constexpr int kLabelCue = 48500;
constexpr int kLabelCharacter = 48501;
constexpr int kLabelStatus = 48502;
constexpr int kLabelType = 48503;
constexpr int kLabelStart = 48504;
constexpr int kLabelEnd = 48505;
constexpr int kLabelDirection = 48506;
constexpr int kLabelDialogue = 48507;
constexpr int kLabelNotes = 48508;
constexpr int kLabelJump = 48509;
#endif

CueInfoController* g_controller = nullptr;
HWND g_window = nullptr;
bool g_populating = false;
bool g_dirty = false;
bool g_close_on_save = false;
std::string g_loaded_key;

void set_text(HWND hwnd, int id, const std::string& value)
{
#ifdef _WIN32
  win32::set_dlg_item_text_utf8(hwnd, id, value);
#else
  SetDlgItemText(hwnd, id, value.c_str());
#endif
}

std::string control_text(HWND hwnd, int id)
{
#ifdef _WIN32
  return win32::dlg_item_text_utf8(hwnd, id);
#else
  const HWND control = GetDlgItem(hwnd, id);
  return read_control_text(GetWindowTextLength(control), [control](char* text, int capacity) {
    GetWindowText(control, text, capacity);
  });
#endif
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
#ifdef _WIN32
  SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  for (const auto& value : values) {
    const std::wstring wide = win32::to_wide(value);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
  }
#else
  SendMessage(combo, CB_RESETCONTENT, 0, 0);
  for (const auto& value : values)
    SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str()));
#endif
}

void refresh_character_choices(HWND hwnd)
{
  if (!g_controller) return;
  const std::string current = control_text(hwnd, kCharacter);
  fill_combo(hwnd, kCharacter, g_controller->character_choices());
  set_text(hwnd, kCharacter, current);
}

void update_live(HWND hwnd)
{
  if (!g_controller) return;
  const auto& view = g_controller->view();
  set_text(hwnd, kHeader, "Cue " + view.cue_key + " | " + view.character + " | " + view.status);
  set_text(hwnd, kLiveMetrics,
    "Length " + number(view.duration) + "s   Position " + view.position_timecode +
    "   Countdown " + number(view.countdown) + "s   Takes " + std::to_string(view.take_count));
  set_text(hwnd, kError, g_controller->error());
}

void populate_editors(HWND hwnd)
{
  if (!g_controller) return;
  const CueInfoEditValues values = g_controller->edit_values();
  g_populating = true;
  set_text(hwnd, kCueId, values.cue_key);
  set_text(hwnd, kCharacter, values.character);
  set_text(hwnd, kStatus, values.status);
  set_text(hwnd, kCueType, values.cue_type);
  set_text(hwnd, kStart, values.start_time);
  set_text(hwnd, kEnd, values.end_time);
  set_text(hwnd, kDirection, values.direction);
  set_text(hwnd, kDialogue, values.dialogue);
  set_text(hwnd, kNotes, values.notes);
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

void activate_cue_info_window()
{
  if (!g_window || !IsWindow(g_window)) return;
  ShowWindow(g_window, SW_SHOW);
  reaper::activate_docked_window(g_window);
  SetForegroundWindow(g_window);
}

void restore_window_geometry(HWND hwnd)
{
  if (!g_controller) return;
  const CueInfoWindowLayout saved = g_controller->load_window_layout();
  RECT current{};
  if (!GetWindowRect(hwnd, &current)) return;
  const int width = (std::max)(kMinWindowWidth, saved.width);
  const int height = (std::max)(kMinWindowHeight, saved.height);
  const int x = saved.has_position ? saved.x : current.left;
  const int y = saved.has_position ? saved.y : current.top;
  SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void restore_window_docking(HWND hwnd)
{
  if (!g_controller) return;
  const CueInfoWindowLayout saved = g_controller->load_window_layout();
  if (saved.dock >= 0)
    reaper::add_window_to_docker(hwnd, kWindowTitle, kDockIdentifier, saved.dock);
}

void save_window_geometry(HWND hwnd)
{
  if (!g_controller) return;
  RECT rect{};
  if (!GetWindowRect(hwnd, &rect)) return;
  CueInfoWindowLayout layout;
  layout.x = rect.left;
  layout.y = rect.top;
  layout.width = (std::max)(kMinWindowWidth, static_cast<int>(rect.right - rect.left));
  layout.height = (std::max)(kMinWindowHeight, static_cast<int>(rect.bottom - rect.top));
  layout.dock = reaper::inspect_window_dock_state(hwnd).dock_index;
  layout.has_position = true;
  g_controller->save_window_layout(layout);
}

bool close_window(HWND hwnd)
{
  if (g_dirty) {
    const int answer = show_message(hwnd,
      "This cue has unsaved changes. Close Cue Info and discard them?",
      "ReaADR Cue Information", MB_YESNO | MB_ICONWARNING);
    if (answer != IDYES) return false;
  }
  save_window_geometry(hwnd);
  const auto dock = reaper::inspect_window_dock_state(hwnd);
  if (dock.docked()) reaper::remove_window_from_docker(hwnd);
  KillTimer(hwnd, kTimer);
  if (g_window == hwnd) g_window = nullptr;
  DestroyWindow(hwnd);
  return true;
}

void refresh_live_state(HWND hwnd)
{
  if (!g_controller || g_dirty) return;
  const std::string previous = g_loaded_key;
  if (g_controller->refresh()) {
    if (g_controller->view().cue_key != previous) populate_editors(hwnd);
    else update_live(hwnd);
  } else {
    set_text(hwnd, kError, g_controller->error());
  }
}

bool confirm_discard_edits(HWND hwnd, const char* action)
{
  if (!g_dirty) return true;
  const std::string message = std::string("This cue has unsaved changes. Discard them and ") + action + "?";
  return show_message(hwnd, message, "ReaADR Cue Information", MB_YESNO | MB_ICONWARNING) == IDYES;
}

bool handle_nav_key(HWND hwnd, int key)
{
  if (!g_controller || editor_has_focus(hwnd)) return false;
  const bool is_refresh = key == kVkR && (GetAsyncKeyState(VK_CONTROL) & 0x8000);
  if (key != VK_LEFT && key != VK_RIGHT && !is_refresh) return false;
  if (g_dirty && !confirm_discard_edits(hwnd, is_refresh ? "refresh the cue" : "navigate to another cue")) return true;
  const bool ok = key == VK_LEFT ? g_controller->previous()
                   : key == VK_RIGHT ? g_controller->next()
                   : g_controller->refresh();
  if (ok) populate_editors(hwnd);
  else set_text(hwnd, kError, g_controller->error());
  return true;
}

bool handle_cue_info_command(HWND hwnd, int command, int notification)
{
  if (!g_populating && editor_control(command) &&
      (notification == EN_CHANGE || notification == CBN_SELCHANGE || notification == CBN_EDITCHANGE)) {
    g_dirty = true;
    set_text(hwnd, kError, "Unsaved edit - choose Save to apply changes.");
    return true;
  }
  if (!g_controller) return false;
  if (command == kSave) {
    if (g_controller->save(read_editors(hwnd))) {
      // The persisted model now owns the editor values. Clear dirty before a
      // close-on-save request so a successful Save never asks the user to
      // discard the changes that were just committed.
      g_dirty = false;
      if (g_close_on_save) {
        close_window(hwnd);
        return true;
      }
      refresh_character_choices(hwnd);
      populate_editors(hwnd);
    } else {
      set_text(hwnd, kError, g_controller->error());
    }
    return true;
  }
  if (command == kPrevious || command == kNext) {
    if (!confirm_discard_edits(hwnd, "navigate to another cue")) return true;
    const bool moved = command == kNext ? g_controller->next() : g_controller->previous();
    if (moved) populate_editors(hwnd);
    else set_text(hwnd, kError, g_controller->error());
    return true;
  }
  if (command == kJump) {
    if (!confirm_discard_edits(hwnd, "jump to another cue")) return true;
    if (g_controller->jump_to_id(control_text(hwnd, kJumpId))) populate_editors(hwnd);
    else set_text(hwnd, kError, g_controller->error());
    return true;
  }
  if (command == IDCANCEL) {
    close_window(hwnd);
    return true;
  }
  return false;
}

#ifndef _WIN32
INT_PTR cue_info_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  switch (message) {
    case WM_INITDIALOG: {
      if (!g_controller) return 0;
      g_window = hwnd;
      const CueInfoLaunchOptions launch = g_controller->consume_launch_options();
      g_close_on_save = launch.close_on_save;
      fill_combo(hwnd, kCharacter, g_controller->character_choices());
      fill_combo(hwnd, kStatus, core::cue_manager_status_choices());
      fill_combo(hwnd, kCueType, core::cue_manager_type_choices());
      if (!g_controller->refresh()) {
        set_text(hwnd, kError, g_controller->error());
        return 1;
      }
      restore_window_geometry(hwnd);
      restore_window_docking(hwnd);
      populate_editors(hwnd);
      SetTimer(hwnd, kTimer, 100, nullptr);
      return 1;
    }

    case WM_TIMER:
      if (wparam == kTimer) refresh_live_state(hwnd);
      return 1;

    case WM_KEYDOWN:
      if (!editor_has_focus(hwnd)) {
        if (wparam == VK_SPACE && Main_OnCommand) {
          Main_OnCommand(kTransportPlayStop, 0);
          return 1;
        }
        if (handle_nav_key(hwnd, static_cast<int>(wparam))) return 1;
      }
      return 0;

    case WM_COMMAND:
      return handle_cue_info_command(hwnd, LOWORD(wparam), HIWORD(wparam)) ? 1 : 0;

    case WM_CLOSE:
      close_window(hwnd);
      return 1;

    case WM_DESTROY:
      KillTimer(hwnd, kTimer);
      if (g_window == hwnd) g_window = nullptr;
      g_controller = nullptr;
      g_close_on_save = false;
      g_dirty = false;
      g_loaded_key.clear();
      return 1;
  }
  return 0;
}

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2(kDialog, SWELL_DLG_WS_FLIPPED,
  "ReaADR Cue Information", 1100, 740)
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
#else

constexpr wchar_t kCueInfoWindowClass[] = L"ReaADRCueInfoWindow";

void set_default_font(HWND control)
{
  if (!control) return;
  SendMessage(control, WM_SETFONT,
              reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

HWND create_child(HWND parent, const char* class_name, const char* text,
                  DWORD style, int id)
{
  const std::wstring wide_class = win32::to_wide(class_name ? class_name : "");
  const std::wstring wide_text = win32::to_wide(text ? text : "");
  HWND child = CreateWindowExW(
    0, wide_class.c_str(), wide_text.c_str(), WS_CHILD | WS_VISIBLE | style,
    0, 0, 10, 10, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
    GetModuleHandleW(nullptr), nullptr);
  set_default_font(child);
  return child;
}

void layout_windows_controls(HWND hwnd)
{
  RECT client{};
  if (!GetClientRect(hwnd, &client)) return;
  const int width = client.right - client.left;
  const int height = client.bottom - client.top;
  const int right = (std::max)(780, width - 18);
  const int full_width = (std::max)(300, right - 18);

  SetWindowPos(GetDlgItem(hwnd, kHeader), nullptr, 18, 12, full_width, 22, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kLiveMetrics), nullptr, 18, 38, full_width, 20, SWP_NOZORDER);

  SetWindowPos(GetDlgItem(hwnd, kLabelCue), nullptr, 18, 70, 42, 18, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kCueId), nullptr, 18, 90, 110, 24, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kLabelCharacter), nullptr, 144, 70, 80, 18, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kCharacter), nullptr, 144, 90, 190, 220, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kLabelStatus), nullptr, 350, 70, 60, 18, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kStatus), nullptr, 350, 90, 150, 180, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kLabelType), nullptr, 516, 70, 60, 18, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kCueType), nullptr, 516, 90, (std::max)(130, right - 516), 180, SWP_NOZORDER);

  SetWindowPos(GetDlgItem(hwnd, kLabelStart), nullptr, 18, 124, 150, 18, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kStart), nullptr, 18, 144, 170, 24, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kLabelEnd), nullptr, 204, 124, 150, 18, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kEnd), nullptr, 204, 144, 170, 24, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kLabelDirection), nullptr, 390, 124, 80, 18, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kDirection), nullptr, 390, 144, (std::max)(250, right - 390), 24, SWP_NOZORDER);

  const int footer_top = (std::max)(500, height - 104);
  const int notes_height = (std::max)(90, footer_top - 370);
  SetWindowPos(GetDlgItem(hwnd, kLabelDialogue), nullptr, 18, 180, 80, 18, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kDialogue), nullptr, 18, 200, full_width, 130, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kLabelNotes), nullptr, 18, 342, 80, 18, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kNotes), nullptr, 18, 362, full_width, notes_height, SWP_NOZORDER);

  const int action_y = footer_top + 8;
  SetWindowPos(GetDlgItem(hwnd, kLabelJump), nullptr, 18, action_y + 4, 82, 18, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kJumpId), nullptr, 104, action_y, 120, 24, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kJump), nullptr, 232, action_y - 1, 72, 26, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kPrevious), nullptr, 326, action_y - 1, 86, 26, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kNext), nullptr, 418, action_y - 1, 72, 26, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kSave), nullptr, (std::max)(516, right - 190), action_y - 1, 86, 26, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, IDCANCEL), nullptr, (std::max)(610, right - 96), action_y - 1, 86, 26, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kError), nullptr, 18, action_y + 34, full_width, 44, SWP_NOZORDER);
}

LRESULT CALLBACK cue_info_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  switch (message) {
    case WM_CREATE:
      create_child(hwnd, "STATIC", "", SS_LEFT, kHeader);
      create_child(hwnd, "STATIC", "", SS_LEFT, kLiveMetrics);
      create_child(hwnd, "STATIC", "Cue", SS_LEFT, kLabelCue);
      create_child(hwnd, "EDIT", "", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, kCueId);
      create_child(hwnd, "STATIC", "Character", SS_LEFT, kLabelCharacter);
      create_child(hwnd, "COMBOBOX", "", WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL, kCharacter);
      create_child(hwnd, "STATIC", "Status", SS_LEFT, kLabelStatus);
      create_child(hwnd, "COMBOBOX", "", WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL, kStatus);
      create_child(hwnd, "STATIC", "Cue Type", SS_LEFT, kLabelType);
      create_child(hwnd, "COMBOBOX", "", WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL, kCueType);
      create_child(hwnd, "STATIC", "Start SMPTE / Seconds", SS_LEFT, kLabelStart);
      create_child(hwnd, "EDIT", "", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, kStart);
      create_child(hwnd, "STATIC", "End SMPTE / Seconds", SS_LEFT, kLabelEnd);
      create_child(hwnd, "EDIT", "", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, kEnd);
      create_child(hwnd, "STATIC", "Direction", SS_LEFT, kLabelDirection);
      create_child(hwnd, "EDIT", "", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, kDirection);
      create_child(hwnd, "STATIC", "Dialogue", SS_LEFT, kLabelDialogue);
      create_child(hwnd, "EDIT", "", WS_BORDER | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, kDialogue);
      create_child(hwnd, "STATIC", "Notes", SS_LEFT, kLabelNotes);
      create_child(hwnd, "EDIT", "", WS_BORDER | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, kNotes);
      create_child(hwnd, "STATIC", "Jump to cue", SS_LEFT, kLabelJump);
      create_child(hwnd, "EDIT", "", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, kJumpId);
      create_child(hwnd, "BUTTON", "Jump", WS_TABSTOP | BS_PUSHBUTTON, kJump);
      create_child(hwnd, "BUTTON", "Previous", WS_TABSTOP | BS_PUSHBUTTON, kPrevious);
      create_child(hwnd, "BUTTON", "Next", WS_TABSTOP | BS_PUSHBUTTON, kNext);
      create_child(hwnd, "BUTTON", "Save", WS_TABSTOP | BS_PUSHBUTTON, kSave);
      create_child(hwnd, "BUTTON", "Close", WS_TABSTOP | BS_DEFPUSHBUTTON, IDCANCEL);
      create_child(hwnd, "STATIC", "", SS_LEFT, kError);
      fill_combo(hwnd, kCharacter, g_controller ? g_controller->character_choices() : std::vector<std::string>{});
      fill_combo(hwnd, kStatus, core::cue_manager_status_choices());
      fill_combo(hwnd, kCueType, core::cue_manager_type_choices());
      layout_windows_controls(hwnd);
      return 0;

    case WM_SIZE:
      layout_windows_controls(hwnd);
      return 0;

    case WM_GETMINMAXINFO: {
      auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
      if (info) {
        info->ptMinTrackSize.x = kMinWindowWidth;
        info->ptMinTrackSize.y = kMinWindowHeight;
      }
      return 0;
    }

    case WM_TIMER:
      if (wparam == kTimer) refresh_live_state(hwnd);
      return 0;

    case WM_KEYDOWN:
      if (!editor_has_focus(hwnd)) {
        if (wparam == VK_SPACE && Main_OnCommand) {
          Main_OnCommand(kTransportPlayStop, 0);
          return 0;
        }
        if (handle_nav_key(hwnd, static_cast<int>(wparam))) return 0;
      }
      break;

    case WM_COMMAND:
      if (handle_cue_info_command(hwnd, LOWORD(wparam), HIWORD(wparam))) return 0;
      break;

    case WM_GETDLGCODE:
      if (wparam == VK_LEFT || wparam == VK_RIGHT || wparam == VK_SPACE)
        return DLGC_WANTALLKEYS;
      break;

    case WM_CLOSE:
      close_window(hwnd);
      return 0;

    case WM_NCDESTROY:
      KillTimer(hwnd, kTimer);
      if (g_window == hwnd) g_window = nullptr;
      g_controller = nullptr;
      g_close_on_save = false;
      g_dirty = false;
      g_loaded_key.clear();
      break;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool register_windows_cue_info_class()
{
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = cue_info_window_proc;
  window_class.hInstance = GetModuleHandleW(nullptr);
  window_class.lpszClassName = kCueInfoWindowClass;
  window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  if (RegisterClassW(&window_class)) return true;
  return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}
#endif

} // namespace

bool show_cue_info_window(CueInfoController& controller)
{
  if (g_controller) {
    if (g_window && IsWindow(g_window)) {
      activate_cue_info_window();
      if (!g_dirty) refresh_live_state(g_window);
      return true;
    }
    return false;
  }

#ifndef _WIN32
  g_controller = &controller;
  g_dirty = false;
  g_close_on_save = false;
  g_loaded_key.clear();
  g_window = CreateDialogParam(nullptr, MAKEINTRESOURCE(kDialog), nullptr, cue_info_proc, 0);
  if (!g_window) {
    g_controller = nullptr;
    g_close_on_save = false;
    return false;
  }
  ShowWindow(g_window, SW_SHOW);
  if (reaper::inspect_window_dock_state(g_window).docked())
    reaper::activate_docked_window(g_window);
  SetForegroundWindow(g_window);
  return true;
#else
  if (!register_windows_cue_info_class()) return false;
  g_controller = &controller;
  g_dirty = false;
  g_loaded_key.clear();
  const CueInfoLaunchOptions launch = g_controller->consume_launch_options();
  g_close_on_save = launch.close_on_save;

  const CueInfoWindowLayout layout = g_controller->load_window_layout();
  const int width = (std::max)(kMinWindowWidth, layout.width);
  const int height = (std::max)(kMinWindowHeight, layout.height);
  const int x = layout.has_position ? layout.x : CW_USEDEFAULT;
  const int y = layout.has_position ? layout.y : CW_USEDEFAULT;
  HWND owner = GetForegroundWindow();
  HWND hwnd = CreateWindowExW(
    WS_EX_DLGMODALFRAME,
    kCueInfoWindowClass,
    L"ReaADR Cue Information",
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
    x, y, width, height, owner, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!hwnd) {
    g_controller = nullptr;
    g_close_on_save = false;
    return false;
  }
  g_window = hwnd;

  if (g_controller->refresh()) populate_editors(hwnd);
  else set_text(hwnd, kError, g_controller->error());
  restore_window_geometry(hwnd);
  restore_window_docking(hwnd);
  ShowWindow(hwnd, SW_SHOW);
  if (reaper::inspect_window_dock_state(hwnd).docked())
    reaper::activate_docked_window(hwnd);
  UpdateWindow(hwnd);
  SetTimer(hwnd, kTimer, 100, nullptr);

  // REAPER owns the application message pump. Keep Cue Info modeless so opening
  // the native window does not block the action hook and so Windows matches the
  // persistent SWELL window behavior on Linux/macOS.
  return true;
#endif
}

bool close_cue_info_window()
{
  if (!g_window || !IsWindow(g_window)) return true;
  if (!close_window(g_window)) return false;
  return !g_window || !IsWindow(g_window);
}

} // namespace reaadr::ui