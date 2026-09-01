#include "recording_setup.hpp"

#include "character_filter.hpp"
#include "lane_assignment.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace reaadr::core {
namespace {

std::string field(const Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

std::string trim_ascii(const std::string& value)
{
  const auto is_space = [](unsigned char byte) {
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' ||
      byte == '\f' || byte == '\v';
  };
  std::size_t first = 0;
  while (first < value.size() && is_space(static_cast<unsigned char>(value[first]))) ++first;
  std::size_t last = value.size();
  while (last > first && is_space(static_cast<unsigned char>(value[last - 1]))) --last;
  return value.substr(first, last - first);
}

std::string lowercase_ascii(std::string value)
{
  for (char& byte : value) {
    if (byte >= 'A' && byte <= 'Z') byte = static_cast<char>(byte - 'A' + 'a');
  }
  return value;
}

bool parse_number(const std::string& value, double& output)
{
  const std::string cleaned = trim_ascii(value);
  char* end = nullptr;
  output = std::strtod(cleaned.c_str(), &end);
  return !cleaned.empty() && end && end != cleaned.c_str() && *end == '\0' &&
    std::isfinite(output);
}

std::string track_character_key(const std::string& key)
{
  const std::string lowered = lowercase_ascii(key);
  const std::string::size_type lane = lowered.find(".lane");
  return lane == std::string::npos ? std::string() : lowered.substr(0, lane);
}

} // namespace

RecordingSetupPlanResult build_recording_setup_plan(
  const SessionModel& model,
  const std::vector<ExistingTrack>& project_tracks,
  const RecordingSetupOptions& options)
{
  RecordingSetupPlanResult result;
  if (model.session_id().empty()) {
    result.error = "A canonical session ID is required before recording.";
    return result;
  }
  if (options.cue_key.empty()) {
    result.error = "A selected cue key is required before recording.";
    return result;
  }
  if (!std::isfinite(options.preroll_seconds) || options.preroll_seconds < 0.0) {
    result.error = "Recording preroll must be a finite, non-negative number.";
    return result;
  }

  const LaneAssignmentResult lanes =
    assign_character_lanes(model.cues, options.preroll_seconds);
  if (!lanes) {
    result.error = lanes.error;
    return result;
  }

  std::size_t selected_index = model.cues.size();
  for (std::size_t index = 0; index < model.cues.size(); ++index) {
    if (render_cue_key(model.cues[index]) != options.cue_key) continue;
    if (selected_index != model.cues.size()) {
      result.error = "Multiple cues match the selected recording key: " + options.cue_key;
      return result;
    }
    selected_index = index;
  }
  if (selected_index == model.cues.size()) {
    result.error = "The selected recording cue is not present in the canonical session.";
    return result;
  }

  RecordingSetupPlan& plan = result.plan;
  plan.cue = model.cues[selected_index];
  plan.cue_model_index = selected_index;
  plan.cue_key = options.cue_key;
  plan.character = render_character_name(plan.cue);
  plan.lane = lanes.lanes[selected_index];
  if (!parse_number(field(plan.cue, "start_time"), plan.cue_start) ||
      !parse_number(field(plan.cue, "end_time"), plan.cue_end) ||
      plan.cue_end < plan.cue_start) {
    result.error = "The selected cue has invalid recording timing.";
    return result;
  }
  plan.record_start = (std::max)(0.0, plan.cue_start - options.preroll_seconds);

  const std::string wanted_key = character_filter_target_key(plan.character, plan.lane);
  const ExistingTrack* exact = nullptr;
  const ExistingTrack* fallback = nullptr;
  for (const ExistingTrack& track : project_tracks) {
    if (track.role != "character") continue;
    const std::string lowered_key = lowercase_ascii(track.key);
    if (lowered_key == wanted_key) {
      if (exact) {
        result.error = "Multiple ADR recording tracks match " + wanted_key + ".";
        return result;
      }
      exact = &track;
      continue;
    }
    if (!fallback && track_character_key(track.key) == character_filter_key(plan.character)) {
      fallback = &track;
    }
  }

  const ExistingTrack* target = exact ? exact : fallback;
  if (!target) {
    result.error = "No ADR recording track is available for " + plan.character + ".";
    return result;
  }
  plan.track_project_index = target->project_index;
  plan.expected_track_role = target->role;
  plan.expected_track_key = target->key;
  plan.used_lane_fallback = !exact;
  return result;
}

} // namespace reaadr::core
