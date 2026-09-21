#include "recording_controller.hpp"

#include "reaadr_core/domain_utils.hpp"

#include <algorithm>
#include <cmath>

namespace reaadr::ui {
namespace {

std::string field(const core::Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

} // namespace

std::string RecordingController::status_text(
  const core::RecordingTransportState& state)
{
  switch (state.mode) {
    case core::RecordingTransportMode::preroll:
      // Mirror the Lua Record Cue status glyphs and ellipsis as UTF-8; the
      // window renders these as Unicode on both platforms.
      return "\xE2\x96\xB8 Pre-roll\xE2\x80\xA6";
    case core::RecordingTransportMode::recording:
      return "\xE2\x97\x8F Recording take " + std::to_string(state.take_count) +
        "\xE2\x80\xA6";
    case core::RecordingTransportMode::loop_wait:
      return "\xE2\x86\xBA Looping\xE2\x80\xA6";
    case core::RecordingTransportMode::idle:
      if (state.take_count > 0)
        return std::to_string(state.take_count) +
          (state.take_count == 1 ? " take recorded." : " takes recorded.");
      return "";
  }
  return "";
}

void RecordingController::set_error(const std::string& error)
{
  view_.error = error;
  if (!error.empty()) view_.status_text = error;
}

void RecordingController::sync_state()
{
  if (!context_.active()) return;
  const auto& state = context_.state();
  view_.take_count = state.take_count;
  view_.loop_enabled = state.loop_enabled;
  view_.include_preroll_each_loop = state.include_preroll_each_loop;
  view_.mode = state.mode;
  view_.countdown_seconds = state.mode == core::RecordingTransportMode::preroll
    ? (std::max)(0.0, view_.cue_start - context_.timeline_position())
    : 0.0;
  // The generated cue-audio item supplies the audible three-beep count-in.
  // This projection only mirrors its final 1.5 seconds for the native window.
  view_.count_in_beat = state.mode == core::RecordingTransportMode::preroll &&
      view_.countdown_seconds > 0.0 && view_.countdown_seconds <= 1.5
    ? (std::max)(1, static_cast<int>(std::ceil(view_.countdown_seconds / 0.5)))
    : 0;
  view_.status_text = status_text(state);
  view_.error.clear();
}

bool RecordingController::begin()
{
  const auto started = context_.begin();
  if (!started) {
    set_error(started.error);
    return false;
  }

  const auto& cue = started.target.target.cue.cue;
  view_.cue_key = started.target.target.cue.cue_key;
  view_.character = field(cue, "character");
  view_.dialogue = field(cue, "line");
  if (view_.dialogue.empty()) view_.dialogue = field(cue, "dialogue");
  view_.lane = started.target.target.lane;
  view_.cue_start = started.workflow.plan.cue_start;
  view_.cue_end = started.workflow.plan.cue_end;
  view_.preroll_seconds = started.target.preroll_seconds;
  view_.track_key = started.workflow.plan.expected_track_key;
  view_.track_name = started.workflow.target_track_name;
  view_.cue_start_timecode = core::format_timecode(view_.cue_start, context_.frame_rate());
  sync_state();
  return true;
}

bool RecordingController::dispatch(core::RecordingTransportEvent event)
{
  const auto result = context_.dispatch(event);
  if (!result) {
    set_error(result.error);
    return false;
  }
  sync_state();
  return true;
}

bool RecordingController::record()
{
  return dispatch(core::RecordingTransportEvent::start);
}

bool RecordingController::tick()
{
  return dispatch(core::RecordingTransportEvent::tick);
}

bool RecordingController::stop()
{
  return dispatch(core::RecordingTransportEvent::stop_requested);
}

bool RecordingController::toggle_loop()
{
  return dispatch(core::RecordingTransportEvent::toggle_loop);
}

bool RecordingController::toggle_preroll_each_loop()
{
  return dispatch(core::RecordingTransportEvent::toggle_preroll_each_loop);
}

bool RecordingController::retry_pending()
{
  const auto result = context_.retry_pending();
  if (!result) {
    set_error(result.error);
    return false;
  }
  sync_state();
  return true;
}

bool RecordingController::shutdown()
{
  const auto result = context_.shutdown();
  if (!result) {
    set_error(result.error);
    return false;
  }
  view_.mode = core::RecordingTransportMode::idle;
  view_.status_text = status_text(result.state);
  view_.error.clear();
  return true;
}

} // namespace reaadr::ui
