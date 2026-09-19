#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_Main_OnCommand

#include "recording_window.hpp"

#include "recording_controller.hpp"
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
constexpr int kDialog = 48200;
constexpr int kCue = 48201;
constexpr int kDialogue = 48202;
constexpr int kTiming = 48203;
constexpr int kTrack = 48204;
constexpr int kStatus = 48205;
constexpr int kRecord = 48206;
constexpr int kLoop = 48207;
constexpr int kPreroll = 48208;
constexpr int kStop = 48209;
constexpr int kTimer = 1;
constexpr int kTransportPlayStop = 40044;
constexpr int kMinWindowWidth = 530;
constexpr int kMinWindowHeight = 260;
constexpr const char* kDockIdentifier = "reaadr.record_cue";
constexpr const char* kWindowTitle = "ReaADR Record Cue";

RecordingController* g_controller = nullptr;
HWND g_window = nullptr;

std::string number(double value, int precision = 1)
{
  std::ostringstream output;
  output << std::fixed << std::setprecision(precision) << value;
  return output.str();
}

std::string dialogue_preview(const std::string& dialogue)
{
  if (dialogue.empty()) return "(no dialogue)";
  if (dialogue.size() <= 72) return dialogue;
  return dialogue.substr(0, 69) + "...";
}

void set_text(HWND hwnd, int id, const std::string& value)
{
#ifdef _WIN32
  win32::set_dlg_item_text_utf8(hwnd, id, value);
#else
  SetDlgItemText(hwnd, id, value.c_str());
#endif
}

int show_message(HWND hwnd, const std::string& message,
                 const std::string& title, UINT type)
{
#ifdef _WIN32
  return win32::message_box_utf8(hwnd, message, title, type);
#else
  return MessageBox(hwnd, message.c_str(), title.c_str(), type);
#endif
}

void update_window(HWND hwnd)
{
  if (!g_controller) return;
  const auto& view = g_controller->view();
  set_text(hwnd, kCue, "Cue " + view.cue_key + " - " + view.character);
  set_text(hwnd, kDialogue, dialogue_preview(view.dialogue));
  const double duration = view.cue_end - view.cue_start;
  set_text(hwnd, kTiming, view.cue_start_timecode + "   " + number(duration) +
    "s cue  +  " + number(view.preroll_seconds) + "s preroll");
  const std::string track_label = view.track_name.empty() ? view.track_key : view.track_name;
  set_text(hwnd, kTrack, "Track: " + track_label);
  set_text(hwnd, kStatus, view.status_text);
  set_text(hwnd, kLoop, view.loop_enabled ? "Loop: ON" : "Loop: OFF");
  set_text(hwnd, kPreroll,
           view.include_preroll_each_loop ? "Pre-roll Each Loop" : "Default Repeat");
  EnableWindow(GetDlgItem(hwnd, kRecord),
               view.mode == core::RecordingTransportMode::idle);
}

void activate_recording_window()
{
  if (!g_window || !IsWindow(g_window)) return;
  ShowWindow(g_window, SW_SHOW);
  reaper::activate_docked_window(g_window);
  SetForegroundWindow(g_window);
}

void restore_window_geometry(HWND hwnd)
{
  if (!g_controller) return;
  const auto saved = g_controller->load_window_layout();
  RECT current{};
  if (!GetWindowRect(hwnd, &current)) return;
  const int width = (std::max)(kMinWindowWidth, saved.width);
  const int height = (std::max)(kMinWindowHeight, saved.height);
  const int x = saved.has_position ? saved.x : static_cast<int>(current.left);
  const int y = saved.has_position ? saved.y : static_cast<int>(current.top);
  SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void restore_window_docking(HWND hwnd)
{
  if (!g_controller) return;
  const auto saved = g_controller->load_window_layout();
  if (saved.dock >= 0)
    reaper::add_window_to_docker(hwnd, kWindowTitle, kDockIdentifier, saved.dock);
}

void save_window_geometry(HWND hwnd)
{
  if (!g_controller) return;
  RECT rect{};
  if (!GetWindowRect(hwnd, &rect)) return;
  reaper::RecordingWindowLayout layout;
  layout.x = static_cast<int>(rect.left);
  layout.y = static_cast<int>(rect.top);
  layout.width = (std::max)(kMinWindowWidth, static_cast<int>(rect.right - rect.left));
  layout.height = (std::max)(kMinWindowHeight, static_cast<int>(rect.bottom - rect.top));
  layout.dock = reaper::inspect_window_dock_state(hwnd).dock_index;
  layout.has_position = true;
  g_controller->save_window_layout(layout);
}

bool show_error(HWND hwnd)
{
  if (!g_controller || g_controller->view().error.empty()) return false;
  show_message(hwnd, g_controller->view().error, "ReaADR Record Cue", MB_OK | MB_ICONERROR);
  return true;
}

bool close_recording(HWND hwnd)
{
  if (g_controller && !g_controller->shutdown()) {
    show_error(hwnd);
    update_window(hwnd);
    return false;
  }
  save_window_geometry(hwnd);
  const auto dock = reaper::inspect_window_dock_state(hwnd);
  if (dock.docked()) reaper::remove_window_from_docker(hwnd);
  KillTimer(hwnd, kTimer);
  if (g_window == hwnd) g_window = nullptr;
  DestroyWindow(hwnd);
  return true;
}

bool handle_recording_command(HWND hwnd, int command)
{
  if (!g_controller) return false;
  bool changed = false;
  if (command == kRecord) {
    changed = g_controller->record();
  } else if (command == kStop) {
    changed = g_controller->stop();
  } else if (command == kPreroll) {
    changed = g_controller->toggle_preroll_each_loop();
  } else if (command == kLoop) {
    if (!g_controller->view().loop_enabled) {
      const int answer = show_message(
        hwnd,
        "Loop recording should create new takes or lanes for each pass.\n\n"
        "If REAPER is configured to replace or trim overlapping recordings, prior passes can be overwritten.\n\n"
        "Check Options > New recording that overlaps existing media items before continuing.\n\n"
        "Enable loop recording?",
        "ReaADR Loop Recording", MB_YESNO | MB_ICONWARNING);
      if (answer != IDYES) return true;
    }
    changed = g_controller->toggle_loop();
  } else if (command == IDCANCEL) {
    close_recording(hwnd);
    return true;
  } else {
    return false;
  }
  if (!changed) show_error(hwnd);
  update_window(hwnd);
  return true;
}

#ifndef _WIN32
INT_PTR recording_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM)
{
  switch (message) {
    case WM_INITDIALOG:
      if (!g_controller || !g_controller->begin()) {
        show_error(hwnd);
        DestroyWindow(hwnd);
        return 1;
      }
      g_window = hwnd;
      restore_window_geometry(hwnd);
      restore_window_docking(hwnd);
      update_window(hwnd);
      SetTimer(hwnd, kTimer, 30, nullptr);
      return 1;

    case WM_TIMER:
      if (wparam == kTimer && g_controller) {
        g_controller->tick();
        update_window(hwnd);
      }
      return 1;

    case WM_KEYDOWN:
      if (wparam == VK_SPACE && Main_OnCommand) {
        Main_OnCommand(kTransportPlayStop, 0);
        return 1;
      }
      return 0;

    case WM_COMMAND:
      return handle_recording_command(hwnd, LOWORD(wparam)) ? 1 : 0;

    case WM_CLOSE:
      close_recording(hwnd);
      return 1;

    case WM_DESTROY:
      KillTimer(hwnd, kTimer);
      if (g_window == hwnd) g_window = nullptr;
      g_controller = nullptr;
      return 1;
  }
  return 0;
}

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2(kDialog, SWELL_DLG_WS_FLIPPED,
                                   "ReaADR Record Cue", 570, 300)
BEGIN
  LTEXT "", kCue, 20, 18, 520, 22
  LTEXT "", kDialogue, 20, 50, 520, 38
  LTEXT "", kTiming, 20, 96, 520, 20
  LTEXT "", kTrack, 20, 124, 520, 20
  LTEXT "", kStatus, 20, 158, 520, 24
  PUSHBUTTON "Record", kRecord, 20, 220, 105, 32
  PUSHBUTTON "Loop: OFF", kLoop, 137, 220, 105, 32
  PUSHBUTTON "Pre-roll Each Loop", kPreroll, 254, 220, 145, 32
  PUSHBUTTON "Stop", kStop, 435, 220, 105, 32
  DEFPUSHBUTTON "Close", IDCANCEL, 435, 262, 105, 24
END
SWELL_DEFINE_DIALOG_RESOURCE_END2(kDialog)
#else

constexpr wchar_t kRecordingWindowClass[] = L"ReaADRRecordingWindow";

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
  const int width = static_cast<int>(client.right - client.left);
  const int height = static_cast<int>(client.bottom - client.top);
  const int content_width = (std::max)(100, width - 40);

  SetWindowPos(GetDlgItem(hwnd, kCue), nullptr, 20, 16, content_width, 22, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kDialogue), nullptr, 20, 44, content_width, 38, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kTiming), nullptr, 20, 88, content_width, 20, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kTrack), nullptr, 20, 114, content_width, 20, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kStatus), nullptr, 20, 142, content_width, 24, SWP_NOZORDER);

  const int button_y = (std::max)(176, height - 74);
  const int close_y = (std::max)(214, height - 36);
  SetWindowPos(GetDlgItem(hwnd, kRecord), nullptr, 20, button_y, 105, 30, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kLoop), nullptr, 137, button_y, 105, 30, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kPreroll), nullptr, 254, button_y, 145, 30, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, kStop), nullptr, (std::max)(410, width - 125), button_y, 105, 30, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, IDCANCEL), nullptr, (std::max)(410, width - 125), close_y, 105, 24, SWP_NOZORDER);
}

LRESULT CALLBACK recording_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  switch (message) {
    case WM_CREATE:
      create_child(hwnd, "STATIC", "", SS_LEFT, kCue);
      create_child(hwnd, "STATIC", "", SS_LEFT, kDialogue);
      create_child(hwnd, "STATIC", "", SS_LEFT, kTiming);
      create_child(hwnd, "STATIC", "", SS_LEFT, kTrack);
      create_child(hwnd, "STATIC", "", SS_LEFT, kStatus);
      create_child(hwnd, "BUTTON", "Record", WS_TABSTOP | BS_PUSHBUTTON, kRecord);
      create_child(hwnd, "BUTTON", "Loop: OFF", WS_TABSTOP | BS_PUSHBUTTON, kLoop);
      create_child(hwnd, "BUTTON", "Pre-roll Each Loop", WS_TABSTOP | BS_PUSHBUTTON, kPreroll);
      create_child(hwnd, "BUTTON", "Stop", WS_TABSTOP | BS_PUSHBUTTON, kStop);
      create_child(hwnd, "BUTTON", "Close", WS_TABSTOP | BS_DEFPUSHBUTTON, IDCANCEL);
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
      if (wparam == kTimer && g_controller) {
        g_controller->tick();
        update_window(hwnd);
      }
      return 0;

    case WM_KEYDOWN:
      if (wparam == VK_SPACE && Main_OnCommand) {
        Main_OnCommand(kTransportPlayStop, 0);
        return 0;
      }
      break;

    case WM_COMMAND:
      if (handle_recording_command(hwnd, LOWORD(wparam))) return 0;
      break;

    case WM_GETDLGCODE:
      if (wparam == VK_SPACE) return DLGC_WANTALLKEYS;
      break;

    case WM_CLOSE:
      close_recording(hwnd);
      return 0;

    case WM_NCDESTROY:
      KillTimer(hwnd, kTimer);
      if (g_controller) g_controller->shutdown();
      if (g_window == hwnd) g_window = nullptr;
      g_controller = nullptr;
      break;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool register_windows_recording_class()
{
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = recording_window_proc;
  window_class.hInstance = GetModuleHandleW(nullptr);
  window_class.lpszClassName = kRecordingWindowClass;
  window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  if (RegisterClassW(&window_class)) return true;
  return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}
#endif

} // namespace

bool show_recording_window(RecordingController& controller)
{
  if (g_controller) {
    if (g_window && IsWindow(g_window)) {
      activate_recording_window();
      update_window(g_window);
      return true;
    }
    return false;
  }

#ifndef _WIN32
  g_controller = &controller;
  g_window = CreateDialogParam(
    nullptr, MAKEINTRESOURCE(kDialog), nullptr, recording_proc, 0);
  if (!g_window) {
    if (g_controller) g_controller->shutdown();
    g_controller = nullptr;
    return false;
  }
  ShowWindow(g_window, SW_SHOW);
  if (reaper::inspect_window_dock_state(g_window).docked())
    reaper::activate_docked_window(g_window);
  SetForegroundWindow(g_window);
  return true;
#else
  if (!register_windows_recording_class()) return false;
  g_controller = &controller;
  if (!g_controller->begin()) {
    show_error(nullptr);
    g_controller = nullptr;
    return false;
  }

  const auto layout = g_controller->load_window_layout();
  const int width = (std::max)(kMinWindowWidth, layout.width);
  const int height = (std::max)(kMinWindowHeight, layout.height);
  const int x = layout.has_position ? layout.x : CW_USEDEFAULT;
  const int y = layout.has_position ? layout.y : CW_USEDEFAULT;
  HWND owner = GetForegroundWindow();
  HWND hwnd = CreateWindowExW(
    WS_EX_DLGMODALFRAME,
    kRecordingWindowClass,
    L"ReaADR Record Cue",
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
    x, y, width, height, owner, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!hwnd) {
    g_controller->shutdown();
    g_controller = nullptr;
    return false;
  }
  g_window = hwnd;

  restore_window_geometry(hwnd);
  restore_window_docking(hwnd);
  ShowWindow(hwnd, SW_SHOW);
  if (reaper::inspect_window_dock_state(hwnd).docked())
    reaper::activate_docked_window(hwnd);
  UpdateWindow(hwnd);
  update_window(hwnd);
  SetTimer(hwnd, kTimer, 30, nullptr);

  // Keep the native window modeless, like the SWELL implementation and REAPER's
  // other extension windows. The host owns the application message pump; running
  // a nested GetMessage loop here blocks the command hook that opened Record Cue
  // and makes Windows behave differently from Linux/macOS.
  return true;
#endif
}

} // namespace reaadr::ui