#include "recording_command.hpp"

#include "recording_command_context.hpp"
#include "reaadr_ui/recording_controller.hpp"
#include "reaadr_ui/recording_window.hpp"

namespace reaadr::reaper {

bool run_native_record_cue_command()
{
  RecordingCommandContext context;
  ui::RecordingController controller(context);
  return ui::show_recording_window(controller);
}

} // namespace reaadr::reaper
