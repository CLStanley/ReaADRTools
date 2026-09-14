#pragma once

#include <cstdint>

namespace reaadr::ui {

// Platform-neutral single-window lifecycle state for the native Cue Manager.
// Window procedures store their opaque native handle here so subsequent launch
// requests can activate the existing Manager instead of creating duplicates.
class CueManagerLifecycle {
public:
  using WindowHandle = std::uintptr_t;

  bool begin_open(WindowHandle window)
  {
    if (window == 0 || window_ != 0) return false;
    window_ = window;
    return true;
  }

  void closed(WindowHandle window)
  {
    if (window_ == window) window_ = 0;
  }

  bool is_open() const { return window_ != 0; }
  WindowHandle window() const { return window_; }

private:
  WindowHandle window_ = 0;
};

inline CueManagerLifecycle& cue_manager_lifecycle()
{
  static CueManagerLifecycle lifecycle;
  return lifecycle;
}

} // namespace reaadr::ui
