#pragma once

#include "cue_manager_controller.hpp"
#include "cue_manager_lifecycle.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <swell/swell.h>
#endif

namespace reaadr::ui {

#ifdef _WIN32
namespace {
// The Win32 overlay menu invokes this helper before its definition in the
// implementation file. Keep the declaration TU-local to match that definition.
void edit_overlay_settings(HWND hwnd);
}
#endif

bool show_cue_manager(CueManagerController& controller, double frame_rate = 24.0);

// Requests a synchronous close for the persistent native Manager window. Both
// Win32 and SWELL presentations participate in CueManagerLifecycle, so callers
// may release the persistent controller/service graph only after ownership has
// cleared.
inline bool request_close_cue_manager()
{
  auto& lifecycle = cue_manager_lifecycle();
  if (!lifecycle.is_open()) return true;

  HWND window = reinterpret_cast<HWND>(lifecycle.window());
  if (!window || !IsWindow(window)) {
    lifecycle.closed(lifecycle.window());
    return true;
  }
#ifdef _WIN32
  SendMessageW(window, WM_CLOSE, 0, 0);
#else
  SendMessage(window, WM_CLOSE, 0, 0);
#endif
  return !lifecycle.is_open();
}

} // namespace reaadr::ui
