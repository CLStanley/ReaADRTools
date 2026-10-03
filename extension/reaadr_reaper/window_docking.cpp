#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_DockIsChildOfDock
#define REAPERAPI_WANT_DockWindowActivate
#define REAPERAPI_WANT_DockWindowAddEx
#define REAPERAPI_WANT_DockWindowRefreshForHWND
#define REAPERAPI_WANT_DockWindowRemove
#define REAPERAPI_WANT_Dock_UpdateDockID

#include "window_docking.hpp"

#include <vector>

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>

namespace reaadr::reaper {
namespace {

std::vector<char> mutable_string(const std::string& value)
{
  std::vector<char> buffer(value.begin(), value.end());
  buffer.push_back('\0');
  return buffer;
}

} // namespace

bool add_window_to_docker(HWND hwnd, const std::string& title,
                          const std::string& identifier,
                          int preferred_dock)
{
  if (!hwnd || !DockWindowAddEx) return false;

  auto mutable_title = mutable_string(title);
  auto mutable_identifier = mutable_string(identifier);

  // REAPER associates the persistent identifier with a preferred docker before
  // DockWindowAddEx resolves that identifier. Keep this host-owned rather than
  // trying to parent the HWND ourselves; that is important for SWELL parity.
  if (preferred_dock >= 0 && Dock_UpdateDockID)
    Dock_UpdateDockID(mutable_identifier.data(), preferred_dock);

  DockWindowAddEx(hwnd, mutable_title.data(), mutable_identifier.data(), true);
  if (DockWindowRefreshForHWND) DockWindowRefreshForHWND(hwnd);

  return inspect_window_dock_state(hwnd).docked();
}

bool remove_window_from_docker(HWND hwnd)
{
  if (!hwnd || !DockWindowRemove) return false;
  const bool was_docked = inspect_window_dock_state(hwnd).docked();
  if (!was_docked) return false;

  DockWindowRemove(hwnd);
  if (DockWindowRefreshForHWND) DockWindowRefreshForHWND(hwnd);
  return !inspect_window_dock_state(hwnd).docked();
}

void activate_docked_window(HWND hwnd)
{
  if (!hwnd || !DockWindowActivate) return;
  if (inspect_window_dock_state(hwnd).docked()) DockWindowActivate(hwnd);
}

WindowDockState inspect_window_dock_state(HWND hwnd)
{
  WindowDockState state;
  if (!hwnd || !DockIsChildOfDock) return state;

  bool floating = false;
  state.dock_index = DockIsChildOfDock(hwnd, &floating);
  state.floating_docker = state.dock_index >= 0 && floating;
  return state;
}

} // namespace reaadr::reaper
