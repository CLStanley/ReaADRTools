#include "recording_transport.hpp"

#include <cmath>

namespace reaadr::core {
namespace {

void finalize_operation(RecordingTransportTransition& transition, bool finalize_takes)
{
  if (transition.state.loop_range_active) {
    transition.actions.restore_loop_range = true;
    transition.state.loop_range_active = false;
  }
  transition.actions.restore_record_arm = true;
  transition.state.mode = RecordingTransportMode::idle;
  if (!transition.state.operation_finalized) {
    transition.state.operation_finalized = true;
    transition.actions.finalize_recorded_takes =
      finalize_takes && transition.state.take_count > 0;
  }
}

void start_take(RecordingTransportTransition& transition,
                const RecordingTransportContext& context)
{
  if (transition.state.mode == RecordingTransportMode::idle) {
    transition.state.operation_finalized = false;
  }
  const bool use_preroll = transition.state.take_count == 0 ||
    transition.state.include_preroll_each_loop;
  if (transition.state.loop_enabled && transition.state.include_preroll_each_loop) {
    transition.actions.configure_loop_range = true;
    transition.state.loop_range_active = true;
  } else if (!transition.state.loop_enabled && transition.state.loop_range_active) {
    transition.actions.restore_loop_range = true;
    transition.state.loop_range_active = false;
  }

  transition.actions.move_cursor = true;
  transition.actions.cursor_position = use_preroll && context.record_start < context.cue_start
    ? context.record_start
    : context.cue_start;
  transition.actions.isolate_recording_track = true;
  if (use_preroll && transition.actions.cursor_position < context.cue_start) {
    transition.actions.play = true;
    transition.state.mode = RecordingTransportMode::preroll;
  } else {
    transition.actions.record = true;
    transition.state.mode = RecordingTransportMode::recording;
    ++transition.state.take_count;
  }
  transition.actions.refresh_active_cue = true;
}

bool valid_context(const RecordingTransportContext& context)
{
  return std::isfinite(context.record_start) && std::isfinite(context.cue_start) &&
    std::isfinite(context.cue_end) && context.record_start >= 0.0 &&
    context.record_start <= context.cue_start && context.cue_end >= context.cue_start;
}

} // namespace

RecordingTransportTransition advance_recording_transport(
  const RecordingTransportState& state,
  const RecordingTransportContext& context,
  const RecordingTransportInput& input)
{
  RecordingTransportTransition transition;
  transition.state = state;
  if (!valid_context(context)) {
    transition.error = "Recording transport requires a valid cue and preroll window.";
    return transition;
  }
  if (!std::isfinite(input.play_position)) {
    transition.error = "REAPER returned an invalid recording transport position.";
    return transition;
  }

  switch (input.event) {
    case RecordingTransportEvent::start:
      if (state.mode != RecordingTransportMode::idle &&
          state.mode != RecordingTransportMode::loop_wait) {
        transition.error = "A recording take is already active.";
        return transition;
      }
      start_take(transition, context);
      return transition;

    case RecordingTransportEvent::tick:
      if (state.mode == RecordingTransportMode::preroll) {
        if (input.play_state == 0) {
          finalize_operation(transition, false);
        } else if (input.play_position >= context.cue_start) {
          transition.actions.record = true;
          transition.state.mode = RecordingTransportMode::recording;
          ++transition.state.take_count;
        }
      } else if (state.mode == RecordingTransportMode::recording) {
        if (input.play_state == 0) {
          finalize_operation(transition, true);
        } else if (input.play_position >= context.cue_end) {
          transition.actions.stop = true;
          if (state.loop_enabled) {
            transition.state.mode = RecordingTransportMode::loop_wait;
          } else {
            finalize_operation(transition, true);
          }
        }
      } else if (state.mode == RecordingTransportMode::loop_wait && input.play_state == 0) {
        start_take(transition, context);
      }
      return transition;

    case RecordingTransportEvent::stop_requested:
      if (state.mode != RecordingTransportMode::idle && input.play_state != 0) {
        transition.actions.stop = true;
      }
      finalize_operation(transition, state.mode != RecordingTransportMode::preroll);
      return transition;

    case RecordingTransportEvent::abort:
      if (state.mode != RecordingTransportMode::idle && input.play_state != 0) {
        transition.actions.stop = true;
      }
      finalize_operation(transition, state.mode != RecordingTransportMode::preroll);
      return transition;

    case RecordingTransportEvent::toggle_loop:
      transition.state.loop_enabled = !state.loop_enabled;
      if (transition.state.loop_enabled && state.include_preroll_each_loop) {
        transition.actions.configure_loop_range = true;
        transition.state.loop_range_active = true;
      } else if (state.loop_range_active) {
        transition.actions.restore_loop_range = true;
        transition.state.loop_range_active = false;
      }
      return transition;

    case RecordingTransportEvent::toggle_preroll_each_loop:
      transition.state.include_preroll_each_loop = !state.include_preroll_each_loop;
      transition.actions.persist_preroll_preference = true;
      if (state.loop_enabled && transition.state.include_preroll_each_loop) {
        transition.actions.configure_loop_range = true;
        transition.state.loop_range_active = true;
      } else if (state.loop_range_active) {
        transition.actions.restore_loop_range = true;
        transition.state.loop_range_active = false;
      }
      return transition;
  }

  transition.error = "Unsupported recording transport event.";
  return transition;
}

} // namespace reaadr::core
