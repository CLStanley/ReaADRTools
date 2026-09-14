#pragma once

#include "cue_manager_controller.hpp"
#include "cue_manager_lifecycle.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace reaadr::ui {

bool show_cue_manager(CueManagerController& controller, double frame_rate = 24.0);

// Requests a synchronous close for the persistent native Manager window. The
// Win32 window procedure owns layout persistence/docker removal before it
// destroys the HWND, so callers may release the controller graph only after
// this function reports that lifecycle ownership has cleared.
inline bool request_close_cue_manager()
{
  auto& lifecycle = cue_manager_lifecycle();
  if (!lifecycle.is_open()) return true;

#ifdef _WIN32
  HWND window = reinterpret_cast<HWND>(lifecycle.window());
  if (!window || !IsWindow(window)) {
    lifecycle.closed(lifecycle.window());
    return true;
  }
  SendMessageW(window, WM_CLOSE, 0, 0);
  return !lifecycle.is_open();
#else
  // The current SWELL shell is still a modal DialogBoxParam compatibility
  // bridge and therefore never leaves an independently-owned window alive
  // after show_cue_manager() returns. Once it becomes modeless, its HWND will
  // participate in CueManagerLifecycle and this branch can send WM_CLOSE too.
  return true;
#endif
}

} // namespace reaadr::ui
