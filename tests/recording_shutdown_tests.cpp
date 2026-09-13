#include "reaadr_core/recording_transport.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message)
{
  if (!condition) {
    std::cerr << "recording_shutdown_tests: " << message << '\n';
    std::exit(1);
  }
}

} // namespace

int main()
{
  using namespace reaadr::core;
  const RecordingTransportContext context{7.0, 10.0, 12.0};

  {
    RecordingTransportState state;
    state.mode = RecordingTransportMode::preroll;
    state.loop_range_active = true;
    state.operation_finalized = false;
    const auto result = advance_recording_transport(
      state, context, {RecordingTransportEvent::abort, 1, 8.0});
    require(static_cast<bool>(result), "abort during preroll should succeed");
    require(result.state.mode == RecordingTransportMode::idle,
            "abort during preroll should return to idle");
    require(result.actions.stop, "abort during active preroll should stop transport");
    require(result.actions.restore_loop_range,
            "abort during preroll should restore the user's loop range");
    require(result.actions.restore_record_arm,
            "abort during preroll should restore record-arm state");
    require(!result.actions.finalize_recorded_takes,
            "abort during preroll should not finalize an unstarted take");
  }

  {
    RecordingTransportState state;
    state.mode = RecordingTransportMode::recording;
    state.loop_range_active = true;
    state.operation_finalized = false;
    state.take_count = 2;
    const auto result = advance_recording_transport(
      state, context, {RecordingTransportEvent::abort, 5, 11.0});
    require(static_cast<bool>(result), "abort during recording should succeed");
    require(result.state.mode == RecordingTransportMode::idle,
            "abort during recording should return to idle");
    require(result.actions.stop, "abort during active recording should stop transport");
    require(result.actions.restore_loop_range,
            "abort during recording should restore the user's loop range");
    require(result.actions.restore_record_arm,
            "abort during recording should restore record-arm state");
    require(result.actions.finalize_recorded_takes,
            "abort after recorded takes should finalize them");
  }

  {
    RecordingTransportState state;
    state.mode = RecordingTransportMode::loop_wait;
    state.loop_enabled = true;
    state.loop_range_active = true;
    state.operation_finalized = false;
    state.take_count = 1;
    const auto result = advance_recording_transport(
      state, context, {RecordingTransportEvent::abort, 0, 12.0});
    require(static_cast<bool>(result), "abort during loop wait should succeed");
    require(!result.actions.stop,
            "abort should not resend Stop after loop transport has already settled");
    require(result.actions.restore_loop_range,
            "loop-wait abort should restore the user's loop range");
    require(result.actions.restore_record_arm,
            "loop-wait abort should restore record-arm state");
    require(result.actions.finalize_recorded_takes,
            "loop-wait abort should finalize completed loop takes");
  }

  {
    RecordingTransportState state;
    const auto result = advance_recording_transport(
      state, context, {RecordingTransportEvent::abort, 0, 0.0});
    require(static_cast<bool>(result), "idle abort should be idempotent");
    require(result.state.mode == RecordingTransportMode::idle,
            "idle abort should remain idle");
    require(!result.actions.stop && !result.actions.finalize_recorded_takes,
            "idle abort should not stop transport or finalize takes");
    require(result.actions.restore_record_arm,
            "idle abort keeps cleanup idempotent by requesting arm restoration");
  }

  std::cout << "recording_shutdown_tests: ok\n";
  return 0;
}
