#ifndef _WIN32

#include "cue_manager_lifecycle.hpp"
#include "reaadr_reaper/window_docking.hpp"

#include <algorithm>
#include <cstdint>

namespace reaadr::ui {
void swell_manager_end_dialog(HWND hwnd, int result);
}

// Keep the mature SWELL Cue Manager implementation intact while moving its
// exported window lifecycle behind a thin platform shell. Only EndDialog is
// intercepted so the top-level Manager can become modeless; child dialogs keep
// their existing modal behavior through the dispatcher below.
#define EndDialog(hwnd, result) ::reaadr::ui::swell_manager_end_dialog((hwnd), (result))
#define show_cue_manager show_cue_manager_swell_legacy
#include "cue_manager_window.cpp"
#undef show_cue_manager
#undef EndDialog

namespace reaadr::ui {
namespace {
constexpr const char* kSwellDockIdentifier = "reaadr.cue_manager";
constexpr const char* kSwellWindowTitle = "ReaADR Tools - Cue Manager";
constexpr int kSwellMinWidth = 1094;
constexpr int kSwellMinHeight = 750;
constexpr UINT_PTR kSwellRevisionTimer = 1;
constexpr UINT kSwellRevisionPollMs = 250;

HWND lifecycle_window()
{
  const auto handle = cue_manager_lifecycle().window();
  return handle == 0 ? nullptr : reinterpret_cast<HWND>(handle);
}

void restore_swell_manager_layout(HWND hwnd)
{
  if (!g_controller || !hwnd) return;
  const auto layout = g_controller->load_window_layout();
  const int width = (std::max)(kSwellMinWidth, layout.width);
  const int height = (std::max)(kSwellMinHeight, layout.height);

  RECT current{};
  GetWindowRect(hwnd, &current);
  const int x = layout.has_position ? layout.x : static_cast<int>(current.left);
  const int y = layout.has_position ? layout.y : static_cast<int>(current.top);
  SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

  if (layout.dock >= 0)
    reaper::add_window_to_docker(hwnd, kSwellWindowTitle, kSwellDockIdentifier, layout.dock);
}

void save_swell_manager_layout(HWND hwnd)
{
  if (!g_controller || !hwnd) return;
  core::WindowLayout layout = g_controller->load_window_layout();
  const auto dock = reaper::inspect_window_dock_state(hwnd);
  layout.dock = dock.dock_index;

  RECT rect{};
  if (GetWindowRect(hwnd, &rect)) {
    layout.x = static_cast<int>(rect.left);
    layout.y = static_cast<int>(rect.top);
    layout.width = (std::max)(kSwellMinWidth, static_cast<int>(rect.right - rect.left));
    layout.height = (std::max)(kSwellMinHeight, static_cast<int>(rect.bottom - rect.top));
    layout.has_position = true;
  }
  g_controller->save_window_layout(layout);
}

void refresh_swell_manager_if_changed(HWND hwnd)
{
  if (!g_controller || !hwnd) return;
  bool changed = false;
  if (!g_controller->reload_if_revision_changed(changed) || !changed) return;
  refresh_rows(hwnd);
  apply_tab_visibility(hwnd, g_controller->view().active_tab);
  update_tab_details(hwnd);
  update_overlay_controls(hwnd);
  update_quick_action_controls(hwnd);
  update_preference_controls(hwnd);
}

void close_swell_manager(HWND hwnd)
{
  if (!hwnd) return;
  KillTimer(hwnd, kSwellRevisionTimer);
  save_swell_manager_layout(hwnd);
  const auto dock = reaper::inspect_window_dock_state(hwnd);
  if (dock.docked()) reaper::remove_window_from_docker(hwnd);
  cue_manager_lifecycle().closed(reinterpret_cast<CueManagerLifecycle::WindowHandle>(hwnd));
  DestroyWindow(hwnd);
}
} // namespace

void swell_manager_end_dialog(HWND hwnd, int result)
{
  HWND manager = lifecycle_window();
  if (manager && hwnd == manager) {
    close_swell_manager(hwnd);
    return;
  }

  // Columns and other child dialogs remain modal and must keep the real SWELL
  // EndDialog path rather than being destroyed like the modeless Manager.
  EndDialog(hwnd, result);
}

bool show_cue_manager(CueManagerController& controller, double frame_rate)
{
  auto& lifecycle = cue_manager_lifecycle();
  if (lifecycle.is_open()) {
    HWND existing = lifecycle_window();
    if (existing && IsWindow(existing)) {
      if (g_controller && g_controller->reload()) {
        refresh_rows(existing);
        apply_tab_visibility(existing, g_controller->view().active_tab);
        update_tab_details(existing);
        update_overlay_controls(existing);
        update_quick_action_controls(existing);
        update_preference_controls(existing);
      }
      ShowWindow(existing, SW_SHOW);
      reaper::activate_docked_window(existing);
      SetForegroundWindow(existing);
      return true;
    }
    lifecycle.closed(lifecycle.window());
  }

  g_frame_rate = frame_rate;
  g_controller = &controller;
  HWND window = CreateDialogParam(
    nullptr, MAKEINTRESOURCE(kDialog), nullptr, cue_manager_proc, 0);
  if (!window) {
    g_controller = nullptr;
    return false;
  }

  const auto handle = reinterpret_cast<CueManagerLifecycle::WindowHandle>(window);
  if (!lifecycle.begin_open(handle)) {
    DestroyWindow(window);
    g_controller = nullptr;
    return false;
  }

  restore_swell_manager_layout(window);
  ShowWindow(window, SW_SHOW);
  if (reaper::inspect_window_dock_state(window).docked())
    reaper::activate_docked_window(window);
  UpdateWindow(window);
  SetTimer(window, kSwellRevisionTimer, kSwellRevisionPollMs, nullptr);

  MSG message{};
  while (lifecycle.is_open() && IsWindow(window) &&
         GetMessage(&message, nullptr, 0, 0) > 0) {
    if (message.hwnd == window && message.message == WM_TIMER &&
        message.wParam == kSwellRevisionTimer) {
      refresh_swell_manager_if_changed(window);
      continue;
    }
    if (message.hwnd == window && message.message == WM_CLOSE) {
      close_swell_manager(window);
      continue;
    }
    if (!IsDialogMessage(window, &message)) {
      TranslateMessage(&message);
      DispatchMessage(&message);
    }
  }

  if (IsWindow(window)) close_swell_manager(window);
  else lifecycle.closed(handle);
  g_controller = nullptr;
  return true;
}

} // namespace reaadr::ui

#endif
