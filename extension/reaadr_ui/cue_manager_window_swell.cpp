#ifndef _WIN32

// Reuse the mature SWELL presentation while replacing its modal action-stack
// lifetime with the persistent CueManagerSessionHost lifetime. The included
// implementation remains the parity source for controls and behavior; this
// shell owns the host-loop modeless window lifecycle.
#define show_cue_manager show_cue_manager_swell_legacy
#include "cue_manager_window.cpp"
#undef show_cue_manager

#include "reaadr_reaper/window_docking.hpp"

#include <algorithm>

namespace reaadr::ui {
namespace {

constexpr const char* kSwellDockIdentifier = "reaadr.cue_manager";
constexpr const char* kSwellWindowTitle = "ReaADR Tools - Cue Manager";
constexpr int kSwellMinWidth = 760;
constexpr int kSwellMinHeight = 560;
constexpr UINT_PTR kRevisionTimer = 1;
constexpr UINT kRevisionPollMs = 250;

CueManagerLifecycle::WindowHandle lifecycle_handle(HWND hwnd)
{
  return reinterpret_cast<CueManagerLifecycle::WindowHandle>(hwnd);
}

void layout_swell_manager(HWND hwnd)
{
  if (!hwnd) return;
  RECT client{};
  GetClientRect(hwnd, &client);
  const int width = static_cast<int>(client.right - client.left);
  const int height = static_cast<int>(client.bottom - client.top);
  const int content_width = std::max(120, width - 32);

  // Keep the Lua/SWELL control arrangement, but anchor the cue table and edit
  // surface to the current client size so docked or manually resized Managers
  // do not leave a fixed 1180x820 island inside the host docker.
  SetWindowPos(GetDlgItem(hwnd, kCueHeader), nullptr, 16, 98, content_width, 14, SWP_NOZORDER);
  const int action_y = std::max(518, height - 42);
  const int dialogue_y = action_y - 24;
  const int identity_y = dialogue_y - 26;
  const int details_y = identity_y - 32;
  const int rows_height = std::max(260, details_y - 4 - 114);
  SetWindowPos(GetDlgItem(hwnd, kRows), nullptr, 16, 114, content_width, rows_height, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kDetails), nullptr, 16, details_y, content_width, 20, SWP_NOZORDER);

  SetWindowPos(GetDlgItem(hwnd, kEditCueId), nullptr, 70, identity_y, 120, 20, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kEditCharacter), nullptr, 270, identity_y, std::max(140, width / 4), 20, SWP_NOZORDER);
  const int type_x = std::max(560, width - 320);
  SetWindowPos(GetDlgItem(hwnd, kEditType), nullptr, type_x, identity_y + 26, 100, 80, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kEditStatus), nullptr, 435, action_y, 180, 80, SWP_NOZORDER);

  const int split = std::max(430, width / 2);
  SetWindowPos(GetDlgItem(hwnd, kEditDialogue), nullptr, 90, dialogue_y, std::max(180, split - 110), 20, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kEditNotes), nullptr, split + 50, dialogue_y,
               std::max(160, width - split - 66), 20, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kEditStart), nullptr, 70, action_y, 120, 20, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kEditEnd), nullptr, 245, action_y, 120, 20, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kApplyEdit), nullptr, 630, action_y - 2, 100, 24, SWP_NOZORDER);

  const int close_x = std::max(660, width - 106);
  const int next_x = std::max(564, close_x - 96);
  const int previous_x = std::max(468, next_x - 96);
  SetWindowPos(GetDlgItem(hwnd, kPrevious), nullptr, previous_x, action_y - 2, 90, 24, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kNext), nullptr, next_x, action_y - 2, 90, 24, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, IDCANCEL), nullptr, close_x, action_y - 2, 90, 24, SWP_NOZORDER);
}

void restore_swell_layout(HWND hwnd)
{
  if (!g_controller || !hwnd) return;
  const auto layout = g_controller->load_window_layout();
  const int width = std::max(kSwellMinWidth, layout.width);
  const int height = std::max(kSwellMinHeight, layout.height);

  RECT rect{};
  GetWindowRect(hwnd, &rect);
  const int x = layout.has_position ? layout.x : static_cast<int>(rect.left);
  const int y = layout.has_position ? layout.y : static_cast<int>(rect.top);
  SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

  if (layout.dock >= 0)
    reaper::add_window_to_docker(hwnd, kSwellWindowTitle, kSwellDockIdentifier, layout.dock);
  layout_swell_manager(hwnd);
}

void save_swell_layout(HWND hwnd)
{
  if (!g_controller || !hwnd) return;
  core::WindowLayout layout = g_controller->load_window_layout();
  const auto dock = reaper::inspect_window_dock_state(hwnd);
  layout.dock = dock.dock_index;

  RECT rect{};
  if (GetWindowRect(hwnd, &rect)) {
    layout.x = static_cast<int>(rect.left);
    layout.y = static_cast<int>(rect.top);
    layout.width = std::max(kSwellMinWidth, static_cast<int>(rect.right - rect.left));
    layout.height = std::max(kSwellMinHeight, static_cast<int>(rect.bottom - rect.top));
    layout.has_position = true;
  }
  g_controller->save_window_layout(layout);
}

void refresh_external_revision(HWND hwnd)
{
  if (!g_controller || !hwnd) return;
  bool changed = false;
  if (!g_controller->reload_if_revision_changed(changed) || !changed) return;
  refresh_rows(hwnd);
  apply_tab_visibility(hwnd, g_controller->view().active_tab);
  update_tab_details(hwnd);
}

void close_swell_manager(HWND hwnd)
{
  if (!hwnd) return;
  KillTimer(hwnd, kRevisionTimer);
  save_swell_layout(hwnd);
  const auto dock = reaper::inspect_window_dock_state(hwnd);
  if (dock.docked()) reaper::remove_window_from_docker(hwnd);
  DestroyWindow(hwnd);
}

INT_PTR modeless_cue_manager_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  if (message == WM_SIZE) {
    layout_swell_manager(hwnd);
    return 1;
  }

  if (message == WM_TIMER && static_cast<UINT_PTR>(wparam) == kRevisionTimer) {
    refresh_external_revision(hwnd);
    return 1;
  }

  if (message == WM_CLOSE ||
      (message == WM_COMMAND && (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL))) {
    close_swell_manager(hwnd);
    return 1;
  }

  if (message == WM_NCDESTROY) {
    KillTimer(hwnd, kRevisionTimer);
    cue_manager_lifecycle().closed(lifecycle_handle(hwnd));
    g_controller = nullptr;
    return 0;
  }

  return cue_manager_proc(hwnd, message, wparam, lparam);
}

} // namespace

bool show_cue_manager(CueManagerController& controller, double frame_rate)
{
  auto& lifecycle = cue_manager_lifecycle();
  if (lifecycle.is_open()) {
    HWND existing = reinterpret_cast<HWND>(lifecycle.window());
    if (existing && IsWindow(existing)) {
      g_controller = &controller;
      g_frame_rate = frame_rate;
      controller.reload();
      refresh_rows(existing);
      apply_tab_visibility(existing, controller.view().active_tab);
      update_tab_details(existing);
      layout_swell_manager(existing);
      SetTimer(existing, kRevisionTimer, kRevisionPollMs, nullptr);
      ShowWindow(existing, SW_SHOW);
      reaper::activate_docked_window(existing);
      SetForegroundWindow(existing);
      return true;
    }
    lifecycle.closed(lifecycle.window());
  }

  g_controller = &controller;
  g_frame_rate = frame_rate;
  HWND window = CreateDialogParam(
    nullptr, MAKEINTRESOURCE(kDialog), nullptr, modeless_cue_manager_proc, 0);
  if (!window) {
    g_controller = nullptr;
    return false;
  }

  if (!lifecycle.begin_open(lifecycle_handle(window))) {
    DestroyWindow(window);
    g_controller = nullptr;
    return false;
  }

  restore_swell_layout(window);
  SetTimer(window, kRevisionTimer, kRevisionPollMs, nullptr);
  ShowWindow(window, SW_SHOW);
  if (reaper::inspect_window_dock_state(window).docked())
    reaper::activate_docked_window(window);
  UpdateWindow(window);
  return true;
}

} // namespace reaadr::ui

#endif
