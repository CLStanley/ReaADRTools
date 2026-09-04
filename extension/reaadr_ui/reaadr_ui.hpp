#pragma once

#include <string>

namespace reaadr::ui {

using MessageBoxFn = int (*)(const char*, const char*, int);

// Optional UI backend. Initialization is deliberately best-effort: a missing
// backend must never make the REAPER extension fail to load.
void initialize(MessageBoxFn message_box);
bool available();
void show_test_window();

} // namespace reaadr::ui
