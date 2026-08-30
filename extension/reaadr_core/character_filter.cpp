#include "character_filter.hpp"

#include "domain_utils.hpp"
#include "lane_assignment.hpp"
#include "render_plan.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace reaadr::core {
namespace {

std::string lowercase_ascii(std::string value)
{
  for (char& byte : value) {
    if (byte >= 'A' && byte <= 'Z') byte = static_cast<char>(byte - 'A' + 'a');
  }
  return value;
}

bool parse_track_lane_key(const std::string& key, std::string& character, int& lane)
{
  const std::string marker = ".lane";
  const std::string::size_type position = key.rfind(marker);
  if (position == std::string::npos || position == 0 || position + marker.size() >= key.size()) {
    return false;
  }
  const std::string lane_text = key.substr(position + marker.size());
  if (!std::all_of(lane_text.begin(), lane_text.end(),
                   [](unsigned char byte) { return byte >= '0' && byte <= '9'; })) {
    return false;
  }
  char* end = nullptr;
  const long parsed = std::strtol(lane_text.c_str(), &end, 10);
  if (!end || *end != '\0' || parsed < 1 || parsed > 100000) return false;
  character = key.substr(0, position);
  lane = static_cast<int>(parsed);
  return true;
}

bool read_optional(ProjectStateStore& store, const char* key, std::string& value, std::string& error)
{
  const StateReadResult stored = store.read(CharacterFilterRepository::kNamespace, key);
  if (stored) {
    value = stored.value;
    return true;
  }
  if (stored.error == StateReadError::not_found) {
    value.clear();
    return true;
  }
  error = stored.error == StateReadError::value_too_large
    ? "A character-filter project value is too large to load safely."
    : "REAPER project extstate is unavailable while loading the character filter.";
  return false;
}

} // namespace

std::string character_filter_key(const std::string& character)
{
  return lowercase_ascii(sanitize_token(character));
}

std::string character_filter_target_key(const std::string& character, int lane)
{
  return character_filter_key(character) + ".lane" + std::to_string(lane);
}

std::string encode_character_filter_tokens(std::vector<std::string> tokens)
{
  tokens.erase(std::remove_if(tokens.begin(), tokens.end(),
    [](const std::string& token) { return token.empty(); }), tokens.end());
  std::sort(tokens.begin(), tokens.end());
  std::ostringstream output;
  for (std::size_t index = 0; index < tokens.size(); ++index) {
    if (index != 0) output << ',';
    output << tokens[index];
  }
  return output.str();
}

CharacterFilterState parse_character_filter_state(const std::string& encoded_selection,
                                                  bool hide_inactive_regions)
{
  CharacterFilterState state;
  state.encoded_selection = encoded_selection;
  state.hide_inactive_regions = hide_inactive_regions;
  std::string::size_type start = 0;
  while (start < encoded_selection.size()) {
    const std::string::size_type end = encoded_selection.find(',', start);
    const std::string token = encoded_selection.substr(start, end - start);
    if (!token.empty()) state.active_tokens.insert(token);
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return state;
}

bool character_lane_is_active(const CharacterFilterState& state,
                              const std::string& character,
                              int lane)
{
  if (!state.enabled()) return true;
  return state.active_tokens.count(character_filter_target_key(character, lane)) != 0 ||
    state.active_tokens.count(character_filter_key(character)) != 0;
}

CharacterFilterLoadResult CharacterFilterRepository::load() const
{
  CharacterFilterLoadResult result;
  std::string selection;
  std::string hide_regions;
  if (!read_optional(store_, kSelectionKey, selection, result.error) ||
      !read_optional(store_, kHideRegionsKey, hide_regions, result.error)) {
    return result;
  }
  result.state = parse_character_filter_state(selection, hide_regions == "1");
  return result;
}

bool CharacterFilterRepository::save(const CharacterFilterState& state)
{
  return store_.write(kNamespace, kSelectionKey, state.encoded_selection) &&
    store_.write(kNamespace, kHideRegionsKey, state.hide_inactive_regions ? "1" : "0");
}

CharacterFilterPlanResult build_character_filter_plan(
  const SessionModel& model,
  const CharacterFilterState& filter,
  const CharacterFilterProjectState& existing,
  double preroll_seconds)
{
  CharacterFilterPlanResult result;
  const LaneAssignmentResult lanes = assign_character_lanes(model.cues, preroll_seconds);
  if (!lanes) {
    result.error = lanes.error;
    return result;
  }

  for (const ExistingFilterTrack& track : existing.tracks) {
    if (track.role != "character" && track.role != "cue_character") continue;
    std::string character = track.key;
    int lane = 1;
    parse_track_lane_key(track.key, character, lane);
    const bool muted = !character_lane_is_active(filter, character, lane);
    if (muted != track.muted) {
      result.plan.track_mutations.push_back({
        track.project_index, track.role, track.key, muted,
      });
    }
  }

  for (std::size_t cue_index = 0; cue_index < model.cues.size(); ++cue_index) {
    const Fields& cue = model.cues[cue_index];
    if (render_cue_key(cue).empty()) continue;
    const std::string name = render_region_name(cue);
    const auto found = std::find_if(existing.regions.begin(), existing.regions.end(),
      [&](const ExistingFilterRegion& region) { return region.name == name; });
    if (found == existing.regions.end()) continue;
    const bool hidden = filter.hide_inactive_regions &&
      !character_lane_is_active(filter, render_character_name(cue), lanes.lanes[cue_index]);
    if (hidden != found->hidden) {
      result.plan.region_mutations.push_back({found->id, found->name, hidden});
    }
  }
  return result;
}

} // namespace reaadr::core
