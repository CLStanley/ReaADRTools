#include "reaadr_ui.hpp"
#ifdef _WIN32
#include "win32_utf8.hpp"
#endif
#include <limits>
#include <stdexcept>

namespace reaadr::ui {
namespace {
MessageBoxFn g_message_box = nullptr;
}

std::string read_control_text(int length, const std::function<void(char*, int)>& read)
{
  if (length < 0 || length == std::numeric_limits<int>::max())
    throw std::length_error("Invalid control text length");
  std::string value(static_cast<std::size_t>(length) + 1, '\0');
  read(value.data(), length + 1);
  value.resize(value.find('\0'));
  return value;
}

void initialize(MessageBoxFn message_box)
{
  g_message_box = message_box;
}

bool available()
{
  return g_message_box != nullptr;
}

int show_message(HWND parent, const std::string& message,
                 const std::string& title, int type)
{
#ifdef _WIN32
  return win32::message_box_utf8(parent, message, title, type);
#else
  return MessageBox(parent, message.c_str(), title.c_str(), type);
#endif
}

void show_test_window()
{
  if (!g_message_box) return;
  g_message_box(
    "Native ReaADR Tools UI backend is active.\n\n"
    "This test window is intentionally isolated from the Lua Manager and Cue Manager.",
    "ReaADR Tools — Native UI Test", 0);
}

} // namespace reaadr::ui
