#include "reaadr_ui.hpp"
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

void show_test_window()
{
  if (!g_message_box) return;
  g_message_box(
    "Native ReaADR Tools UI backend is active.\n\n"
    "This test window is intentionally isolated from the Lua Manager and Cue Manager.",
    "ReaADR Tools — Native UI Test", 0);
}

} // namespace reaadr::ui
