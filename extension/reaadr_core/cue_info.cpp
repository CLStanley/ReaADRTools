#include "cue_info.hpp"

#include "domain_utils.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace reaadr::core {
namespace {

std::string field(const Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

bool parse_seconds(const std::string& text, double& value)
{
  if (text.empty()) return false;
  errno = 0;
  char* end = nullptr;
  const double parsed = std::strtod(text.c_str(), &end);
  if (errno != 0 || !end || end == text.c_str()) return false;
  while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
  if (*end != '\0' || !std::isfinite(parsed)) return false;
  value = parsed;
  return true;
}

} // namespace

CueInfoView build_cue_info_view(const Fields& cue, const CueInfoOptions& options)
{
  CueInfoView view;
  if (!std::isfinite(options.timeline_position) || options.timeline_position < 0.0) {
    view.error = "Cue information requires a valid timeline position.";
    return view;
  }
  if (!std::isfinite(options.frame_rate) || options.frame_rate <= 0.0) {
    view.error = "Cue information requires a positive frame rate.";
    return view;
  }

  if (!parse_seconds(field(cue, "start_time"), view.start_time)) {
    view.error = "Cue start time is missing or invalid.";
    return view;
  }
  if (!parse_seconds(field(cue, "end_time"), view.end_time)) {
    view.error = "Cue end time is missing or invalid.";
    return view;
  }
  if (view.end_time <= view.start_time) {
    view.error = "Cue end time must be after start time.";
    return view;
  }

  view.cue_key = field(cue, "id");
  view.character = field(cue, "character");
  view.status = normalize_status(field(cue, "status"));
  view.cue_type = field(cue, "cue_type");
  view.direction = field(cue, "direction");
  view.dialogue = field(cue, "line");
  if (view.dialogue.empty()) view.dialogue = field(cue, "dialogue");
  view.notes = field(cue, "notes");
  view.duration = view.end_time - view.start_time;
  view.timeline_position = options.timeline_position;
  view.countdown = (std::max)(0.0, view.start_time - options.timeline_position);
  view.frame_rate = options.frame_rate;
  view.take_count = options.take_count;
  view.start_timecode = format_timecode(view.start_time, view.frame_rate);
  view.end_timecode = format_timecode(view.end_time, view.frame_rate);
  view.position_timecode = format_timecode(view.timeline_position, view.frame_rate);
  return view;
}

} // namespace reaadr::core
