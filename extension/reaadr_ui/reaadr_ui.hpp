#pragma once

#include <string>
#include <functional>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <swell/swell.h>
#endif

namespace reaadr::ui {

using MessageBoxFn = int (*)(const char*, const char*, int);

// Optional UI backend. Initialization is deliberately best-effort: a missing
// backend must never make the REAPER extension fail to load.
void initialize(MessageBoxFn message_box);
bool available();
void show_test_window();

// Cross-platform message helper used by native windows that need a parented
// confirmation dialog. Windows performs the UTF-8 conversion locally; SWELL
// accepts the UTF-8 strings directly.
int show_message(HWND parent, const std::string& message,
                 const std::string& title, int type);

// Reads a control using its host-reported byte length, including room for NUL.
// Keeping the reader injected lets tests exercise long UTF-8 text without a GUI.
std::string read_control_text(int length, const std::function<void(char*, int)>& read);

} // namespace reaadr::ui
