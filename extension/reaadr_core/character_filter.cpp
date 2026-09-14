#include "character_filter.hpp"

#include "domain_utils.hpp"
#include "lane_assignment.hpp"
#include "render_plan.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
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

std::vector<std::string> normalized_active_tokens(const CharacterFilterCatalogResult& catalog)
{
  std::vector<std::string> tokens;
  for (const auto& group : catalog.groups)
    for (const auto& target : group.targets)
      if (target.active) tokens.push_back(target.key);
  std::sort(tokens.begin(), tokens.end());
  tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
  return tokens;
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
  tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
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

CharacterFilterCatalogResult build_character_filter_catalog(
  const SessionModel& model,
  const CharacterFilterState& state,
  double preroll_seconds)
{
  CharacterFilterCatalogResult result;
  result.show_all = !state.enabled();
  const LaneAssignmentResult lanes = assign_character_lanes(model.cues, preroll_seconds);
  if (!lanes) {
    result.error = lanes.error;
    return result;
  }

  std::map<std::string, std::set<int>> lanes_by_character;
  for (std::size_t index = 0; index < model.cues.size(); ++index) {
    const std::string character = render_character_name(model.cues[index]);
    if (character.empty()) continue;
    lanes_by_character[character].insert(lanes.lanes[index]);
  }

  for (const auto& entry : lanes_by_character) {
    CharacterFilterGroup group;
    group.character = entry.first;
    std::size_t active_count = 0;
    for (const int lane : entry.second) {
      CharacterFilterTarget target;
      target.character = entry.first;
      target.lane = lane;
      target.key = character_filter_target_key(entry.first, lane);
      target.active = character_lane_is_active(state, entry.first, lane);
      if (target.active) ++active_count;
      group.targets.push_back(std::move(target));
    }
    group.all_active = !group.targets.empty() && active_count == group.targets.size();
    group.partially_active = active_count > 0 && active_count < group.targets.size();
    result.groups.push_back(std::move(group));
  }

  return result;
}

std::vector<std::string> active_character_filter_tokens(
  const CharacterFilterCatalogResult& catalog)
{
  if (!catalog) return {};
  return normalized_active_tokens(catalog);
}

std::vector<std::string> toggle_character_filter_group(
  const CharacterFilterCatalogResult& catalog,
  const std::string& character)
{
  if (!catalog) return {};
  std::vector<std::string> tokens = normalized_active_tokens(catalog);
  const auto group = std::find_if(catalog.groups.begin(), catalog.groups.end(),
    [&](const CharacterFilterGroup& value) { return value.character == character; });
  if (group == catalog.groups.end()) return tokens;

  const bool disable_group = group->all_active;
  for (const auto& target : group->targets) {
    const auto found = std::find(tokens.begin(), tokens.end(), target.key);
    if (disable_group) {
      if (found != tokens.end()) tokens.erase(found);
    } else if (found == tokens.end()) {
      tokens.push_back(target.key);
    }
  }
  std::sort(tokens.begin(), tokens.end());
  return tokens;
}

std::vector<std::string> toggle_character_filter_target(
  const CharacterFilterCatalogResult& catalog,
  const std::string& target_key)
{
  if (!catalog) return {};
  std::vector<std::string> tokens = normalized_active_tokens(catalog);
  const auto found = std::find(tokens.begin(), tokens.end(), target_key);
  if (found == tokens.end()) tokens.push_back(target_key);
  else tokens.erase(found);
  std::sort(tokens.begin(), tokens.end());
  return tokens;
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
