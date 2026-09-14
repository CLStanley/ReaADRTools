#pragma once

#include <string>

class HWND__;
typedef HWND__* HWND;

namespace reaadr::reaper {

struct WindowDockState {
  int dock_index = -1;
  bool floating_docker = false;

  bool docked() const { return dock_index >= 0; }
};

// Thin wrapper over REAPER's host-provided docker API. Windows remain ordinary
// Win32/SWELL HWNDs; this adapter does not link against SWELL directly.
bool add_window_to_docker(HWND hwnd, const std::string& title,
                          const std::string& identifier,
                          int preferred_dock = -1);
bool remove_window_from_docker(HWND hwnd);
void activate_docked_window(HWND hwnd);
WindowDockState inspect_window_dock_state(HWND hwnd);

} // namespace reaadr::reaper
