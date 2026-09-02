#pragma once

namespace reaadr::reaper {

// Opens the native Manager shell. The shell owns only presentation state;
// feature actions continue through the application services until each tab is
// fully cut over from Lua.
void show_native_manager_window();

} // namespace reaadr::reaper
