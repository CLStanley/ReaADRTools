#include "character_filter_window.hpp"

#include "cue_manager_controller.hpp"
#include "cue_manager_ui_contract.hpp"

#include <reaper_plugin.h>

#ifndef _WIN32
#include <swell/swell-dlggen.h>
#endif

#include <string>
#include <vector>

namespace reaadr::ui {
namespace {

#ifndef _WIN32
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
  return IsDlgButtonChecked(hwnd, kHideInactiveRegions) == BST_CHECKED;
}

void set_status(HWND hwnd, const std::string& value)
{
  SetDlgItemText(hwnd, kStatus, value.c_str());
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
    SendDlgItemMessage(hwnd, kItems, LB_ADDSTRING, 0,
                       reinterpret_cast<LPARAM>(label.c_str()));
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
    if (!error.empty()) MessageBox(hwnd, error.c_str(), "ReaADR Character Filter", 0);
    return;
  }
  refresh_filter_items(hwnd);
}

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
      else if (!error.empty())
        MessageBox(hwnd, error.c_str(), "ReaADR Character Filter", 0);
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
    if (!g_controller) return 1;
    const auto catalog = g_controller->character_filter_catalog();
    if (!catalog) {
      if (!catalog.error.empty()) MessageBox(hwnd, catalog.error.c_str(), "ReaADR Character Filter", 0);
      return 1;
    }
    std::vector<std::string> tokens;
    if (!catalog.show_all) {
      for (const auto& group : catalog.groups)
        for (const auto& target : group.targets)
          if (target.active) tokens.push_back(target.key);
    }
    std::string error;
    if (!g_controller->apply_character_filter(tokens, hide_inactive_regions(hwnd), error) &&
        !error.empty())
      MessageBox(hwnd, error.c_str(), "ReaADR Character Filter", 0);
    refresh_filter_items(hwnd);
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
#endif

} // namespace

bool show_character_filter_window(CueManagerController& controller)
{
#ifndef _WIN32
  g_controller = &controller;
  g_items.clear();
  const int result = DialogBoxParam(nullptr, MAKEINTRESOURCE(kDialog), nullptr,
                                    character_filter_proc, 0);
  g_items.clear();
  g_controller = nullptr;
  return result >= 0;
#else
  (void)controller;
  return false;
#endif
}

} // namespace reaadr::ui
