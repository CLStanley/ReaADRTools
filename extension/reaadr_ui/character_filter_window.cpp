#include "character_filter_window.hpp"

#include "cue_manager_controller.hpp"
#include "cue_manager_ui_contract.hpp"

#include <reaper_plugin.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "win32_utf8.hpp"
#else
#include <swell/swell-dlggen.h>
#endif

#include <string>
#include <vector>

namespace reaadr::ui {
namespace {

constexpr int kDialog = 48200;
constexpr int kItems = 48201;
constexpr int kShowAll = 48202;
constexpr int kToggle = 48203;
constexpr int kHideInactiveRegions = 48204;
constexpr int kStatus = 48205;

CueManagerController* g_controller = nullptr;
std::vector<core::CueManagerCharacterFilterItem> g_items;

bool hide_inactive_regions(HWND hwnd)
{
  return SendDlgItemMessage(hwnd, kHideInactiveRegions, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void set_status(HWND hwnd, const std::string& value)
{
#ifdef _WIN32
  win32::set_dlg_item_text_utf8(hwnd, kStatus, value);
#else
  SetDlgItemText(hwnd, kStatus, value.c_str());
#endif
}

void show_error(HWND hwnd, const std::string& value)
{
  if (value.empty()) return;
#ifdef _WIN32
  win32::message_box_utf8(hwnd, value, "ReaADR Character Filter", MB_OK | MB_ICONERROR);
#else
  MessageBox(hwnd, value.c_str(), "ReaADR Character Filter", 0);
#endif
}

void add_filter_item(HWND hwnd, const std::string& label)
{
#ifdef _WIN32
  win32::listbox_add_utf8(GetDlgItem(hwnd, kItems), label);
#else
  SendDlgItemMessage(hwnd, kItems, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(label.c_str()));
#endif
}

void refresh_filter_items(HWND hwnd)
{
  if (!g_controller) return;
  const auto catalog = g_controller->character_filter_catalog();
  if (!catalog) {
    g_items.clear();
    SendDlgItemMessage(hwnd, kItems, LB_RESETCONTENT, 0, 0);
    set_status(hwnd, catalog.error);
    return;
  }

  g_items = core::cue_manager_character_filter_items(catalog);
  SendDlgItemMessage(hwnd, kItems, LB_RESETCONTENT, 0, 0);
  for (const auto& item : g_items) {
    std::string label;
    if (item.kind == core::CueManagerCharacterFilterItem::Kind::lane)
      label = "    ";
    const char* marker = item.partial ? "[-] " : item.checked ? "[x] " : "[ ] ";
    label += marker;
    label += item.label;
    add_filter_item(hwnd, label);
  }
  set_status(hwnd, catalog.show_all
    ? "All character lanes are active."
    : "Double-click a character or lane to toggle it.");
}

void apply_selected_toggle(HWND hwnd)
{
  if (!g_controller) return;
  const LRESULT selected = SendDlgItemMessage(hwnd, kItems, LB_GETCURSEL, 0, 0);
  if (selected < 0 || static_cast<std::size_t>(selected) >= g_items.size()) return;
  const auto item = g_items[static_cast<std::size_t>(selected)];
  std::string error;
  bool changed = false;
  if (item.kind == core::CueManagerCharacterFilterItem::Kind::show_all) {
    changed = g_controller->show_all_character_filter(hide_inactive_regions(hwnd), error);
  } else if (item.kind == core::CueManagerCharacterFilterItem::Kind::character) {
    changed = g_controller->toggle_character_filter_group(
      item.character, hide_inactive_regions(hwnd), error);
  } else {
    changed = g_controller->toggle_character_filter_target(
      item.target_key, hide_inactive_regions(hwnd), error);
  }
  if (!changed) {
    show_error(hwnd, error);
    return;
  }
  refresh_filter_items(hwnd);
}

void apply_region_visibility_toggle(HWND hwnd)
{
  if (!g_controller) return;
  const auto catalog = g_controller->character_filter_catalog();
  if (!catalog) {
    show_error(hwnd, catalog.error);
    return;
  }
  std::vector<std::string> tokens;
  if (!catalog.show_all) {
    for (const auto& group : catalog.groups)
      for (const auto& target : group.targets)
        if (target.active) tokens.push_back(target.key);
  }
  std::string error;
  if (!g_controller->apply_character_filter(tokens, hide_inactive_regions(hwnd), error))
    show_error(hwnd, error);
  refresh_filter_items(hwnd);
}

#ifndef _WIN32
INT_PTR character_filter_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  if (message == WM_INITDIALOG) {
    if (g_controller) {
      const auto state = g_controller->character_filter_state();
      if (state)
        CheckDlgButton(hwnd, kHideInactiveRegions,
                       state.state.hide_inactive_regions ? BST_CHECKED : BST_UNCHECKED);
      else if (!state.error.empty())
        set_status(hwnd, state.error);
    }
    refresh_filter_items(hwnd);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kShowAll) {
    if (g_controller) {
      std::string error;
      if (g_controller->show_all_character_filter(hide_inactive_regions(hwnd), error))
        refresh_filter_items(hwnd);
      else
        show_error(hwnd, error);
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kToggle) {
    apply_selected_toggle(hwnd);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kItems && HIWORD(wparam) == LBN_DBLCLK) {
    apply_selected_toggle(hwnd);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kHideInactiveRegions) {
    apply_region_visibility_toggle(hwnd);
    return 1;
  }
  if (message == WM_COMMAND && (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL)) {
    EndDialog(hwnd, 0);
    return 1;
  }
  return 0;
}

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2(kDialog, SWELL_DLG_WS_FLIPPED,
  "ReaADR Character Filter", 500, 420)
BEGIN
  LTEXT "Active Characters and Lanes", -1, 16, 14, 220, 18
  LTEXT "Choose the characters or overlapping lanes that should stay active for this recording pass.",
        -1, 16, 36, 460, 32
  LISTBOX kItems, 16, 74, 468, 252, LBS_NOTIFY | WS_VSCROLL | WS_BORDER | WS_TABSTOP
  CHECKBOX "Hide regions for inactive character lanes", kHideInactiveRegions, 16, 338, 280, 20
  PUSHBUTTON "Show All", kShowAll, 304, 334, 82, 24
  PUSHBUTTON "Toggle Selected", kToggle, 392, 334, 92, 24
  LTEXT "", kStatus, 16, 366, 468, 18
  DEFPUSHBUTTON "Close", IDCANCEL, 404, 390, 80, 24
END
SWELL_DEFINE_DIALOG_RESOURCE_END2(kDialog)
#else

constexpr wchar_t kWindowClass[] = L"ReaADRCharacterFilterWindow";

void create_child(HWND parent, const char* class_name, const char* text,
                  DWORD style, int x, int y, int width, int height, int id)
{
  const std::wstring wide_class = win32::to_wide(class_name ? class_name : "");
  const std::wstring wide_text = win32::to_wide(text ? text : "");
  CreateWindowExW(0, wide_class.c_str(), wide_text.c_str(), WS_CHILD | WS_VISIBLE | style,
                  x, y, width, height, parent,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                  GetModuleHandle(nullptr), nullptr);
}

LRESULT CALLBACK character_filter_wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  switch (message) {
    case WM_CREATE: {
      create_child(hwnd, "STATIC", "Active Characters and Lanes", 0, 16, 14, 280, 20, -1);
      create_child(hwnd, "STATIC",
        "Choose the characters or overlapping lanes that should stay active for this recording pass.",
        0, 16, 38, 490, 34, -1);
      create_child(hwnd, "LISTBOX", "", LBS_NOTIFY | WS_VSCROLL | WS_BORDER | WS_TABSTOP,
                   16, 78, 492, 270, kItems);
      create_child(hwnd, "BUTTON", "Hide regions for inactive character lanes",
                   BS_AUTOCHECKBOX | WS_TABSTOP, 16, 360, 292, 24, kHideInactiveRegions);
      create_child(hwnd, "BUTTON", "Show All", BS_PUSHBUTTON | WS_TABSTOP,
                   314, 358, 88, 28, kShowAll);
      create_child(hwnd, "BUTTON", "Toggle Selected", BS_PUSHBUTTON | WS_TABSTOP,
                   408, 358, 100, 28, kToggle);
      create_child(hwnd, "STATIC", "", 0, 16, 394, 492, 20, kStatus);
      create_child(hwnd, "BUTTON", "Close", BS_DEFPUSHBUTTON | WS_TABSTOP,
                   428, 420, 80, 28, IDCANCEL);
      if (g_controller) {
        const auto state = g_controller->character_filter_state();
        if (state)
          SendDlgItemMessageW(hwnd, kHideInactiveRegions, BM_SETCHECK,
                              state.state.hide_inactive_regions ? BST_CHECKED : BST_UNCHECKED, 0);
        else if (!state.error.empty())
          set_status(hwnd, state.error);
      }
      refresh_filter_items(hwnd);
      return 0;
    }
    case WM_COMMAND: {
      const int command = LOWORD(wparam);
      if (command == kShowAll && g_controller) {
        std::string error;
        if (g_controller->show_all_character_filter(hide_inactive_regions(hwnd), error))
          refresh_filter_items(hwnd);
        else
          show_error(hwnd, error);
        return 0;
      }
      if (command == kToggle || (command == kItems && HIWORD(wparam) == LBN_DBLCLK)) {
        apply_selected_toggle(hwnd);
        return 0;
      }
      if (command == kHideInactiveRegions) {
        apply_region_visibility_toggle(hwnd);
        return 0;
      }
      if (command == IDCANCEL || command == IDOK) {
        DestroyWindow(hwnd);
        return 0;
      }
      break;
    }
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool show_win32_character_filter_window()
{
  HINSTANCE instance = GetModuleHandle(nullptr);
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = character_filter_wnd_proc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  window_class.lpszClassName = kWindowClass;
  if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return false;

  HWND owner = GetForegroundWindow();
  HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, kWindowClass,
    L"ReaADR Character Filter", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
    CW_USEDEFAULT, CW_USEDEFAULT, 548, 500, owner, nullptr, instance, nullptr);
  if (!window) return false;

  if (owner) EnableWindow(owner, FALSE);
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);
  MSG message{};
  while (IsWindow(window) && GetMessage(&message, nullptr, 0, 0) > 0) {
    if (!IsDialogMessage(window, &message)) {
      TranslateMessage(&message);
      DispatchMessage(&message);
    }
  }
  if (owner) {
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
  }
  return true;
}
#endif

} // namespace

bool show_character_filter_window(CueManagerController& controller)
{
  g_controller = &controller;
  g_items.clear();
#ifndef _WIN32
  const int result = DialogBoxParam(nullptr, MAKEINTRESOURCE(kDialog), nullptr,
                                    character_filter_proc, 0);
  const bool shown = result >= 0;
#else
  const bool shown = show_win32_character_filter_window();
#endif
  g_items.clear();
  g_controller = nullptr;
  return shown;
}

} // namespace reaadr::ui