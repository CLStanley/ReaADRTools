#include "recording_command.hpp"

#include "recording_command_context.hpp"
#include "reaadr_ui/recording_controller.hpp"
#include "reaadr_ui/recording_window.hpp"

#include <memory>

namespace reaadr::reaper {
namespace {

struct RecordingWindowSession {
  RecordingCommandContext context;
  ui::RecordingController controller;

  RecordingWindowSession() : controller(context) {}
};

std::unique_ptr<RecordingWindowSession> g_recording_session;

} // namespace

bool run_native_record_cue_command()
{
  if (!g_recording_session)
    g_recording_session = std::make_unique<RecordingWindowSession>();

  if (ui::show_recording_window(g_recording_session->controller)) return true;

  g_recording_session.reset();
  return false;
}

bool shutdown_native_record_cue_command()
{
  if (!g_recording_session) return true;
  if (!ui::close_recording_window()) return false;
  g_recording_session.reset();
  return true;
}

} // namespace reaadr::reaper
