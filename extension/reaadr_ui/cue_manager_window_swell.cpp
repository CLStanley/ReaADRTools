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

HWND find_label_by_vertical_order(HWND hwnd, const char* text, bool bottommost)
{
  if (!hwnd || !text) return nullptr;
  HWND best = nullptr;
  long best_top = bottommost ? -2147483647L : 2147483647L;
  for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
    char value[128] = {};
    GetWindowText(child, value, sizeof(value));
    if (std::strcmp(value, text) != 0) continue;

    RECT rect{};
    if (!GetWindowRect(child, &rect)) continue;
    if ((bottommost && rect.top > best_top) || (!bottommost && rect.top < best_top)) {
      best = child;
      best_top = rect.top;
    }
  }
  return best;
}

HWND find_static_label(HWND hwnd, const char* text)
{
  return find_label_by_vertical_order(hwnd, text, true);
}

void set_label_visible(HWND hwnd, const char* text, bool visible, bool bottommost = true)
{
  if (HWND label = find_label_by_vertical_order(hwnd, text, bottommost))
    ShowWindow(label, visible ? SW_SHOW : SW_HIDE);
}

void move_swell_control(HWND hwnd, int id, int x, int y, int width, int height)
{
  if (HWND control = GetDlgItem(hwnd, id))
    SetWindowPos(control, nullptr, x, y, width, height, SWP_NOZORDER);
}

void move_swell_label(HWND hwnd, const char* text, int x, int y, int width, int height, bool bottommost = true)
{
  if (HWND label = find_label_by_vertical_order(hwnd, text, bottommost))
    SetWindowPos(label, nullptr, x, y, width, height, SWP_NOZORDER);
}

void sync_swell_tab_labels(HWND hwnd)
{
  if (!hwnd || !g_controller) return;
  const std::string& tab = g_controller->view().active_tab;
  const bool cues = tab == "cues";
  const bool import = tab == "import";
  const bool overlay = tab == "overlay";
  const bool preferences = tab == "preferences";
  const bool help = tab == "help";

  const char* editor_labels[] = {"Cue ID", "Character", "Dialogue", "Notes", "Type", "Start", "End", "Status"};
  for (const char* text : editor_labels)
    set_label_visible(hwnd, text, cues, true);
  set_label_visible(hwnd, "Search", cues);
  set_label_visible(hwnd, "Jump", cues);
  set_label_visible(hwnd, "Character", cues, false);
  set_label_visible(hwnd, "Status", cues, false);

  const char* import_labels[] = {
    "Cue sheet import uses the native transactional parser and renderer.",
    "Mapping (optional)", "Mode (all/selected/update)", "Characters (; separated)"
  };
  for (const char* text : import_labels)
    set_label_visible(hwnd, text, import);

  const char* overlay_labels[] = {
    "Text Backgrounds", "Text Color", "Metadata Fields (comma separated)", "Preroll (seconds)"
  };
  for (const char* text : overlay_labels)
    set_label_visible(hwnd, text, overlay);

  const char* preference_labels[] = {"Quick Action 1", "Quick Action 2", "Quick Action 3", "Quick Action 4"};
  for (const char* text : preference_labels)
    set_label_visible(hwnd, text, preferences);

  set_label_visible(hwnd, "Search Help", help);
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

  // Keep all seven native Manager tabs reachable when REAPER gives a narrow
  // docker. The legacy resource assumed 1180px and pushed Help off-screen.
  const int tab_ids[] = {kTabImport, kTabCues, kTabSession, kTabReports, kTabOverlay, kTabPreferences, kTabHelp};
  constexpr int tab_count = static_cast<int>(sizeof(tab_ids) / sizeof(tab_ids[0]));
  constexpr int tab_gap = 4;
  const int tab_left = width >= 980 ? 320 : margin;
  const int available_tabs = std::max(420, width - tab_left - margin);
  const int tab_width = std::max(58, (available_tabs - tab_gap * (tab_count - 1)) / tab_count);
  for (int index = 0; index < tab_count; ++index)
    move_swell_control(hwnd, tab_ids[index], tab_left + index * (tab_width + tab_gap), 10, tab_width, 22);

  // The cue filter/jump row also needs to contract with the docker rather than
  // retaining the old 1180px coordinates. At the supported minimum width the
  // fields stay usable and Go remains visible.
  const int search_width = std::max(120, std::min(220, width / 5));
  move_swell_label(hwnd, "Search", margin, 42, 48, 14, false);
  move_swell_control(hwnd, kSearchFilter, 66, 40, search_width, 20);
  const int character_label_x = 74 + search_width;
  move_swell_label(hwnd, "Character", character_label_x, 42, 68, 14, false);
  const int character_x = character_label_x + 70;
  const int character_width = std::max(100, std::min(170, width / 7));
  move_swell_control(hwnd, kCharacterFilter, character_x, 40, character_width, 20);
  const int status_label_x = character_x + character_width + 8;
  move_swell_label(hwnd, "Status", status_label_x, 42, 48, 14, false);
  const int status_x = status_label_x + 50;
  const int status_width = std::max(95, std::min(145, width / 8));
  move_swell_control(hwnd, kStatusFilter, status_x, 40, status_width, 120);
  const int apply_x = status_x + status_width + 8;
  move_swell_control(hwnd, kApplyFilter, apply_x, 40, 58, 20);
  move_swell_control(hwnd, kResetFilter, apply_x + 62, 40, 58, 20);
  const int jump_label_x = apply_x + 128;
  move_swell_label(hwnd, "Jump", jump_label_x, 42, 38, 14, false);
  const int go_width = 50;
  const int jump_x = jump_label_x + 40;
  const int jump_width = std::max(70, width - jump_x - go_width - margin - 6);
  move_swell_control(hwnd, kJumpCueId, jump_x, 40, jump_width, 20);
  move_swell_control(hwnd, kJump, jump_x + jump_width + 6, 40, go_width, 20);

  // Keep the right-side workflow actions reachable at narrower widths.
  const int cue_info_width = 90;
  const int record_width = 140;
  const int cue_info_x = std::max(650, width - margin - cue_info_width);
  const int record_x = std::max(504, cue_info_x - 6 - record_width);
  move_swell_control(hwnd, kCueRecord, record_x, 70, record_width, 24);
  move_swell_control(hwnd, kCueInfo, cue_info_x, 70, cue_info_width, 24);

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
  sync_swell_tab_labels(hwnd);
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
  sync_swell_tab_labels(hwnd);
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
    sync_swell_tab_labels(hwnd);
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
