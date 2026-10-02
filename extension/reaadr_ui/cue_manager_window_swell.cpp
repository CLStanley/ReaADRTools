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
#include <cstring>

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

HWND find_static_label(HWND hwnd, const char* text)
{
  if (!hwnd || !text) return nullptr;
  HWND best = nullptr;
  long best_top = -2147483647L;
  for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
    char value[128] = {};
    GetWindowText(child, value, sizeof(value));
    if (std::strcmp(value, text) != 0) continue;

    // Several Manager tabs reuse words such as Character and Status. The cue
    // editor labels are the bottom-most matching statics in the inherited
    // resource, so select by geometry rather than resource enumeration order.
    RECT rect{};
    if (GetWindowRect(child, &rect) && rect.top > best_top) {
      best = child;
      best_top = rect.top;
    }
  }
  return best;
}

void move_swell_control(HWND hwnd, int id, int x, int y, int width, int height)
{
  if (HWND control = GetDlgItem(hwnd, id))
    SetWindowPos(control, nullptr, x, y, width, height, SWP_NOZORDER);
}

void move_swell_label(HWND hwnd, const char* text, int x, int y, int width, int height)
{
  if (HWND label = find_static_label(hwnd, text))
    SetWindowPos(label, nullptr, x, y, width, height, SWP_NOZORDER);
}

void sync_swell_editor_labels(HWND hwnd)
{
  if (!hwnd || !g_controller) return;
  const bool visible = g_controller->view().active_tab == "cues";
  const char* labels[] = {"Cue ID", "Character", "Dialogue", "Notes", "Type", "Start", "End", "Status"};
  for (const char* text : labels) {
    if (HWND label = find_static_label(hwnd, text))
      ShowWindow(label, visible ? SW_SHOW : SW_HIDE);
  }
}

void layout_swell_manager(HWND hwnd)
{
  if (!hwnd) return;
  RECT client{};
  GetClientRect(hwnd, &client);
  const int width = static_cast<int>(client.right - client.left);
  const int height = static_cast<int>(client.bottom - client.top);
  constexpr int margin = 16;
  constexpr int list_top = 114;
  const int content_width = std::max(120, width - margin * 2);

  // Match the dedicated Win32 presentation: the table consumes the flexible
  // center while the details/editor surface remains anchored to the bottom of
  // whatever floating or REAPER-docked client area SWELL gives us.
  const int action_y = std::max(518, height - 40);
  const int dialogue_y = action_y - 26;
  const int identity_y = dialogue_y - 26;
  const int details_y = identity_y - 32;
  const int rows_height = std::max(260, details_y - 4 - list_top);

  move_swell_control(hwnd, kCueHeader, margin, 98, content_width, 14);
  move_swell_control(hwnd, kRows, margin, list_top, content_width, rows_height);
  move_swell_control(hwnd, kDetails, margin, details_y, content_width, 20);

  move_swell_label(hwnd, "Cue ID", 16, identity_y + 2, 50, 14);
  move_swell_control(hwnd, kEditCueId, 70, identity_y, 120, 20);
  move_swell_label(hwnd, "Character", 200, identity_y + 2, 68, 14);
  move_swell_control(hwnd, kEditCharacter, 270, identity_y, std::max(140, width / 4), 20);

  const int split = std::max(430, width / 2);
  move_swell_label(hwnd, "Dialogue", 16, dialogue_y + 2, 70, 14);
  move_swell_control(hwnd, kEditDialogue, 90, dialogue_y, std::max(180, split - 110), 20);
  move_swell_label(hwnd, "Notes", split + 8, dialogue_y + 2, 50, 14);
  move_swell_control(hwnd, kEditNotes, split + 58, dialogue_y,
                     std::max(160, width - split - 248), 20);
  const int type_x = std::max(split + 230, width - 180);
  move_swell_label(hwnd, "Type", type_x - 40, dialogue_y + 2, 40, 14);
  move_swell_control(hwnd, kEditType, type_x, dialogue_y, 100, 80);

  move_swell_label(hwnd, "Start", 16, action_y + 2, 50, 14);
  move_swell_control(hwnd, kEditStart, 70, action_y, 120, 20);
  move_swell_label(hwnd, "End", 200, action_y + 2, 40, 14);
  move_swell_control(hwnd, kEditEnd, 245, action_y, 120, 20);
  move_swell_label(hwnd, "Status", 380, action_y + 2, 50, 14);
  move_swell_control(hwnd, kEditStatus, 435, action_y, 180, 80);
  move_swell_control(hwnd, kApplyEdit, 630, action_y - 2, 100, 24);

  const int close_x = std::max(660, width - 106);
  const int next_x = std::max(564, close_x - 96);
  const int previous_x = std::max(468, next_x - 96);
  move_swell_control(hwnd, kPrevious, previous_x, action_y - 2, 90, 24);
  move_swell_control(hwnd, kNext, next_x, action_y - 2, 90, 24);
  move_swell_control(hwnd, IDCANCEL, close_x, action_y - 2, 90, 24);
  sync_swell_editor_labels(hwnd);
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
  sync_swell_editor_labels(hwnd);
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

  const INT_PTR handled = cue_manager_proc(hwnd, message, wparam, lparam);
  if (message == WM_INITDIALOG || message == WM_COMMAND)
    sync_swell_editor_labels(hwnd);
  return handled;
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
