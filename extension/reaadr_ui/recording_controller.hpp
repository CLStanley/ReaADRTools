#pragma once

#include "reaadr_reaper/recording_command_context.hpp"

#include <string>

namespace reaadr::ui {

struct RecordingViewState {
  std::string cue_key;
  std::string character;
  std::string dialogue;
  std::string track_key;
  double cue_start = 0.0;
  double cue_end = 0.0;
  double preroll_seconds = 0.0;
  int lane = 1;
  int take_count = 0;
  bool loop_enabled = false;
  bool include_preroll_each_loop = true;
  core::RecordingTransportMode mode = core::RecordingTransportMode::idle;
  std::string status_text;
  std::string error;
};

// Presentation controller for the future SWELL Record Cue window. It contains
// no recording rules: button/timer events are translated into semantic events
// for RecordingCommandContext, and display state is projected from the accepted
// native workflow state.
class RecordingController final {
public:
  explicit RecordingController(reaper::RecordingCommandContext& context)
    : context_(context) {}

  bool begin();
  bool record();
  bool tick();
  bool stop();
  bool toggle_loop();
  bool toggle_preroll_each_loop();
  bool retry_pending();
  bool shutdown();

  const RecordingViewState& view() const { return view_; }

private:
  bool dispatch(core::RecordingTransportEvent event);
  void sync_state();
  void set_error(const std::string& error);
  static std::string status_text(const core::RecordingTransportState& state);

  reaper::RecordingCommandContext& context_;
  RecordingViewState view_;
};

} // namespace reaadr::ui
