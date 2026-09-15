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
