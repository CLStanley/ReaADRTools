#pragma once

namespace reaadr::core {
struct ManagerViewModel;
class ProjectStateStore;
class GlobalStateStore;
}

namespace reaadr::reaper {

struct NativeManagerWindowContext {
  core::ProjectStateStore* project_state = nullptr;
  core::GlobalStateStore* global_state = nullptr;
};

// Opens the native Manager shell. The shell owns only presentation state;
// feature actions continue through the application services until each tab is
// fully cut over from Lua.
void show_native_manager_window(const core::ManagerViewModel* view = nullptr,
                                NativeManagerWindowContext context = {});

} // namespace reaadr::reaper
