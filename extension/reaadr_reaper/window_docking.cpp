#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_DockIsChildOfDock
#define REAPERAPI_WANT_DockWindowActivate
#define REAPERAPI_WANT_DockWindowAdd
#define REAPERAPI_WANT_DockWindowAddEx
#define REAPERAPI_WANT_DockWindowRefreshForHWND
#define REAPERAPI_WANT_DockWindowRemove
#define REAPERAPI_WANT_Dock_UpdateDockID
#define REAPERAPI_WANT_GetConfigWantsDock

#include "window_docking.hpp"

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>

namespace reaadr::reaper {

bool add_window_to_docker(HWND hwnd, const std::string& title,
                          const std::string& identifier,
                          int preferred_dock)
{
  if (!hwnd) return false;

  if (preferred_dock >= 0 && Dock_UpdateDockID)
    Dock_UpdateDockID(identifier.c_str(), preferred_dock);

  if (DockWindowAddEx) {
    DockWindowAddEx(hwnd, title.c_str(), identifier.c_str(), true);
  } else if (DockWindowAdd) {
    const int target = preferred_dock >= 0 ? preferred_dock : 0;
    DockWindowAdd(hwnd, title.c_str(), target, true);
  } else {
    return false;
  }

  if (DockWindowRefreshForHWND) DockWindowRefreshForHWND(hwnd);
  if (DockWindowActivate) DockWindowActivate(hwnd);
  return true;
}

bool remove_window_from_docker(HWND hwnd)
{
  if (!hwnd || !DockWindowRemove) return false;
  DockWindowRemove(hwnd);
  if (DockWindowRefreshForHWND) DockWindowRefreshForHWND(hwnd);
  return true;
}

void activate_docked_window(HWND hwnd)
{
  if (hwnd && DockWindowActivate) DockWindowActivate(hwnd);
}

WindowDockState inspect_window_dock_state(HWND hwnd)
{
  WindowDockState state;
  if (!hwnd || !DockIsChildOfDock) return state;
  bool floating = false;
  state.dock_index = DockIsChildOfDock(hwnd, &floating);
  state.floating_docker = floating;
  return state;
}

} // namespace reaadr::reaper
