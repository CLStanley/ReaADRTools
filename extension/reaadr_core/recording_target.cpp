#include "recording_target.hpp"

#include "lane_assignment.hpp"
#include "render_plan.hpp"

#include <cmath>
#include <vector>

namespace reaadr::core {
namespace {

std::string field(const Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

} // namespace

RecordingTargetResult resolve_recording_target(
  const SessionModel& model,
  const CharacterFilterState& filter,
  const RecordingTargetOptions& options)
{
  RecordingTargetResult result;
  if (!std::isfinite(options.timeline_position) || options.timeline_position < 0.0) {
    result.error = "Recording target resolution requires a valid timeline position.";
    return result;
  }
  if (!std::isfinite(options.preroll_seconds) || options.preroll_seconds < 0.0) {
    result.error = "Recording target resolution requires a non-negative preroll.";
    return result;
  }

  const CueNavigationCatalogResult catalog = build_cue_navigation_catalog(model);
  if (!catalog) {
    result.error = catalog.error;
    return result;
  }
  const LaneAssignmentResult lanes = assign_character_lanes(model.cues, options.preroll_seconds);
  if (!lanes) {
    result.error = lanes.error;
    return result;
  }

  std::vector<CueNavigationEntry> visible;
  std::vector<int> visible_lanes;
  visible.reserve(catalog.cues.size());
  visible_lanes.reserve(catalog.cues.size());
  for (const CueNavigationEntry& entry : catalog.cues) {
    if (entry.model_index >= lanes.lanes.size()) {
      result.error = "Recording target lane assignment no longer matches the canonical cue model.";
      return result;
    }
    const int lane = lanes.lanes[entry.model_index];
    const std::string character = field(entry.cue, "character");
    if (!character_lane_is_active(filter, character, lane)) continue;
    visible.push_back(entry);
    visible_lanes.push_back(lane);
  }

  if (visible.empty()) {
    result.error = filter.enabled()
      ? "No ADR cues are visible through the active character filter."
      : "No ADR cues are available for recording.";
    return result;
  }
  result.visible_cues = visible;

  if (!options.selected_cue_key.empty()) {
    for (std::size_t i = 0; i < visible.size(); ++i) {
      if (visible[i].cue_key != options.selected_cue_key) continue;
      result.cue = visible[i];
      result.lane = visible_lanes[i];
      result.used_selected_cue = true;
      return result;
    }
  }

  if (const CueNavigationEntry* current = find_cue_at_position(visible, options.timeline_position)) {
    const std::size_t index = static_cast<std::size_t>(current - visible.data());
    result.cue = *current;
    result.lane = visible_lanes[index];
    return result;
  }
  if (const CueNavigationEntry* next = find_next_cue(visible, options.timeline_position)) {
    const std::size_t index = static_cast<std::size_t>(next - visible.data());
    result.cue = *next;
    result.lane = visible_lanes[index];
    result.used_next_cue = true;
    return result;
  }

  result.error = "No active ADR cue exists at or after the current timeline position.";
  return result;
}

} // namespace reaadr::core
