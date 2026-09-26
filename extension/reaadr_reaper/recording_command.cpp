#include "recording_command.hpp"

#include "native_runtime.hpp"
#include "recording_command_context.hpp"
#include "reaadr_ui/recording_controller.hpp"
#include "reaadr_ui/recording_window.hpp"

#include <memory>

namespace reaadr::reaper {
namespace {

struct RecordingWindowSession {
  RecordingCommandContext context;
  ui::RecordingController controller;

  explicit RecordingWindowSession(ReaProject* project)
    : context(project), controller(context) {}
};

std::unique_ptr<RecordingWindowSession> g_recording_session;

} // namespace

bool run_native_record_cue_command()
{
  // Record Cue owns repositories, arm snapshots, transport state, and window
  // layout for exactly one project. Resolve a concrete project identity rather
  // than relying on REAPER's nullptr/current-project convention so a project-tab
  // switch cannot leave the persistent window operating on the old service graph.
  ReaProject* project = active_reaper_project();
  if (!project) return false;

  if (g_recording_session && g_recording_session->context.project() != project) {
    if (!shutdown_native_record_cue_command()) return false;
  }

  if (!g_recording_session)
    g_recording_session = std::make_unique<RecordingWindowSession>(project);

  if (ui::show_recording_window(g_recording_session->controller)) return true;

  g_recording_session.reset();
  return false;
}

bool shutdown_native_record_cue_command()
{
  if (!g_recording_session) return true;
  if (!ui::force_close_recording_window()) return false;
  g_recording_session.reset();
  return true;
}

} // namespace reaadr::reaper
