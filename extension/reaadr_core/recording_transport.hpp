#pragma once

#include <string>

namespace reaadr::core {

enum class RecordingTransportMode {
  idle,
  preroll,
  recording,
  loop_wait,
};

enum class RecordingTransportEvent {
  start,
  tick,
  stop_requested,
  abort,
  toggle_loop,
  toggle_preroll_each_loop,
};

struct RecordingTransportContext {
  double record_start = 0.0;
  double cue_start = 0.0;
  double cue_end = 0.0;
};

struct RecordingTransportState {
  RecordingTransportMode mode = RecordingTransportMode::idle;
  bool loop_enabled = false;
  bool include_preroll_each_loop = true;
  bool loop_range_active = false;
  bool operation_finalized = true;
  int take_count = 0;
};

struct RecordingTransportInput {
  RecordingTransportEvent event = RecordingTransportEvent::tick;
  int play_state = 0;
  double play_position = 0.0;
};

// Booleans are intents, not side effects. The REAPER executor applies the
// immediate prefix in declaration order; the application coordinator consumes
// the final model/status intents after host execution succeeds.
struct RecordingTransportActions {
  bool stop = false;
  bool restore_loop_range = false;
  bool configure_loop_range = false;
  bool move_cursor = false;
  double cursor_position = 0.0;
  bool isolate_recording_track = false;
  bool play = false;
  bool record = false;
  bool restore_record_arm = false;
  bool refresh_active_cue = false;
  bool finalize_recorded_takes = false;
  bool persist_preroll_preference = false;
};

struct RecordingTransportTransition {
  RecordingTransportState state;
  RecordingTransportActions actions;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Returns the next state and ordered actions. The caller must retain the input
// state if applying any host action fails.
RecordingTransportTransition advance_recording_transport(
  const RecordingTransportState& state,
  const RecordingTransportContext& context,
  const RecordingTransportInput& input);

} // namespace reaadr::core
