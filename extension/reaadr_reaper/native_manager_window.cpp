#include "native_manager_window.hpp"

#include "reaadr_core/manager_navigation.hpp"
#include "reaadr_core/manager_view_model.hpp"
#include "reaadr_core/manager_preferences.hpp"
#include <reaper_plugin.h>
#include <string>
#include <vector>

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
constexpr int kRememberLayout = 47023;
constexpr int kHoverPreview = 47024;
constexpr int kTooltips = 47025;
constexpr int kNavigationWrap = 47026;
const core::ManagerViewModel* g_view = nullptr;
NativeManagerWindowContext g_context;
std::vector<std::string> g_cue_keys;

#ifndef _WIN32
INT_PTR manager_dialog_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  if (message == WM_INITDIALOG) {
    ShowWindow(GetDlgItem(hwnd, kRememberLayout), SW_HIDE);
    ShowWindow(GetDlgItem(hwnd, kHoverPreview), SW_HIDE);
    ShowWindow(GetDlgItem(hwnd, kTooltips), SW_HIDE);
    ShowWindow(GetDlgItem(hwnd, kNavigationWrap), SW_HIDE);
    std::string intro = "Select a Manager tab to configure ReaADR Tools.\r\n\r\n"
      "This native shell is driven by the C++ session and preference model.";
    if (g_view) {
      intro = "Session: " + g_view->session_name + "\r\n" +
        "Cues: " + std::to_string(g_view->total_cues) +
        "   Revision: " + g_view->revision + "\r\n\r\n" +
        "Select a Manager tab to continue.";
      CheckDlgButton(hwnd, kRememberLayout, g_view->preferences.remember_layout ? BST_CHECKED : BST_UNCHECKED);
      CheckDlgButton(hwnd, kHoverPreview, g_view->preferences.hover_preview ? BST_CHECKED : BST_UNCHECKED);
      CheckDlgButton(hwnd, kTooltips, g_view->preferences.tooltips ? BST_CHECKED : BST_UNCHECKED);
      CheckDlgButton(hwnd, kNavigationWrap, g_view->preferences.navigation_wrap ? BST_CHECKED : BST_UNCHECKED);
      for (const auto& row : g_view->cues.rows) {
        g_cue_keys.push_back(row.cue_key);
        const std::string line = (row.selected ? "> " : "  ") + row.cue_key + "  " +
          row.character + "  [" + row.status + "]  " + row.dialogue;
        SendDlgItemMessage(hwnd, kCueList, LB_ADDSTRING, 0,
          reinterpret_cast<LPARAM>(line.c_str()));
      }
      std::size_t selected_index = g_cue_keys.size();
      for (std::size_t i = 0; i < g_cue_keys.size(); ++i)
        if (g_cue_keys[i] == g_view->cues.selected_cue_key) { selected_index = i; break; }
      if (selected_index < g_cue_keys.size())
        SendDlgItemMessage(hwnd, kCueList, LB_SETCURSEL,
          static_cast<WPARAM>(selected_index), 0);
    }
    SetDlgItemText(hwnd, kBody, intro.c_str());
    return 1;
  }
  if (message == WM_COMMAND) {
    const int command = LOWORD(wparam);
    if (command == kCueList && HIWORD(wparam) == LBN_SELCHANGE &&
        g_context.project_state) {
      const LRESULT selected = SendDlgItemMessage(hwnd, kCueList, LB_GETCURSEL, 0, 0);
      if (selected >= 0 && static_cast<std::size_t>(selected) < g_cue_keys.size())
        g_context.project_state->write(core::SessionModelRepository::kNamespace,
          "manager_selected_cue_key", g_cue_keys[static_cast<std::size_t>(selected)]);
      return 1;
    }
    if (g_view && g_context.project_state &&
        (command == kRememberLayout || command == kHoverPreview ||
         command == kTooltips || command == kNavigationWrap)) {
      const char* key = command == kRememberLayout ? "remember_window_layout" :
        command == kHoverPreview ? "cue_hover_preview" :
        command == kTooltips ? "tooltips_enabled" : "navigation_wrap_enabled";
      const bool enabled = IsDlgButtonChecked(hwnd, command) == BST_CHECKED;
      core::ManagerPreferencesRepository repository(*g_context.project_state, g_context.global_state);
      const auto update = core::update_manager_preferences(
        g_view->preferences, key, enabled ? "1" : "0");
      if (update) repository.save(update.preferences);
      return 1;
    }
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
      ShowWindow(GetDlgItem(hwnd, kCueList), command == kTabCues ? SW_SHOW : SW_HIDE);
      ShowWindow(GetDlgItem(hwnd, kRememberLayout), command == kTabPreferences ? SW_SHOW : SW_HIDE);
      ShowWindow(GetDlgItem(hwnd, kHoverPreview), command == kTabPreferences ? SW_SHOW : SW_HIDE);
      ShowWindow(GetDlgItem(hwnd, kTooltips), command == kTabPreferences ? SW_SHOW : SW_HIDE);
      ShowWindow(GetDlgItem(hwnd, kNavigationWrap), command == kTabPreferences ? SW_SHOW : SW_HIDE);
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
  CHECKBOX "Remember Manager window layout per project", kRememberLayout, 12, 72, 300, 16
  CHECKBOX "Enable hover previews", kHoverPreview, 12, 94, 220, 16
  CHECKBOX "Show tooltips", kTooltips, 12, 116, 220, 16
  CHECKBOX "Wrap keyboard navigation", kNavigationWrap, 12, 138, 240, 16
  DEFPUSHBUTTON "Close", kClose, 430, 262, 78, 24
END
SWELL_DEFINE_DIALOG_RESOURCE_END2(kManagerDialog)
#endif
}

void show_native_manager_window(const core::ManagerViewModel* view,
                                NativeManagerWindowContext context)
{
  g_view = view;
  g_context = context;
  g_cue_keys.clear();
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
  g_context = {};
  g_cue_keys.clear();
}

} // namespace reaadr::reaper
