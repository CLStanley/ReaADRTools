#pragma once

#include <string>
#include <functional>

namespace reaadr::ui {

using MessageBoxFn = int (*)(const char*, const char*, int);

// Optional UI backend. Initialization is deliberately best-effort: a missing
// backend must never make the REAPER extension fail to load.
void initialize(MessageBoxFn message_box);
bool available();
void show_test_window();

// Reads a control using its host-reported byte length, including room for NUL.
// Keeping the reader injected lets tests exercise long UTF-8 text without a GUI.
std::string read_control_text(int length, const std::function<void(char*, int)>& read);

} // namespace reaadr::ui
