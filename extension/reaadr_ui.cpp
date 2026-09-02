#include "reaadr_ui.hpp"

namespace reaadr::ui {
namespace {
MessageBoxFn g_message_box = nullptr;
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
