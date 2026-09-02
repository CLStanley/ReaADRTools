#include "cue_cleanup.hpp"

#include "domain_utils.hpp"

#include <algorithm>
#include <set>

namespace reaadr::core {
namespace {

std::string field(const Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

std::string lowercase_ascii(std::string value)
{
  for (char& byte : value) {
    if (byte >= 'A' && byte <= 'Z') byte = static_cast<char>(byte - 'A' + 'a');
  }
  return value;
}

std::string character_token(const std::string& character)
{
  return lowercase_ascii(sanitize_token(character));
}

bool track_belongs_to_character(const std::string& key, const std::set<std::string>& tokens)
{
  const std::string marker = ".lane";
  const std::size_t position = key.rfind(marker);
  if (position == std::string::npos || position == 0) return false;
  return tokens.count(lowercase_ascii(key.substr(0, position))) != 0;
}

} // namespace

CueCleanupPlanResult build_cue_cleanup_plan(
  const SessionModel& model,
  const ProjectRenderState& existing,
  const std::vector<std::string>& characters)
{
  CueCleanupPlanResult result;
  if (model.session_id().empty()) {
    result.error = "A canonical session ID is required before clearing cues.";
    return result;
  }

  std::set<std::string> selected_characters;
  for (const std::string& character : characters) selected_characters.insert(character);
  if (selected_characters.empty()) {
    result.error = "At least one character must be selected before clearing cues.";
    return result;
  }
  std::set<std::string> selected_tokens;
  for (const std::string& character : selected_characters) {
    const std::string token = character_token(character);
    if (!token.empty()) selected_tokens.insert(token);
  }

  std::set<std::string> cue_keys;
  std::set<std::string> region_names;
  for (const Fields& cue : model.cues) {
    if (selected_characters.count(field(cue, "character")) == 0) {
      result.remaining_cues.push_back(cue);
      continue;
    }
    const std::string key = render_cue_key(cue);
    if (key.empty()) {
      result.error = "A selected cue has no stable generated identity.";
      return result;
    }
    cue_keys.insert(key);
    result.plan.cue_keys.push_back(key);
    const std::string region_name = render_region_name(cue);
    if (!region_names.insert(region_name).second) {
      result.error = "Multiple selected cues resolve to the generated region name: " + region_name;
      return result;
    }
  }
  if (result.plan.cue_keys.empty()) return result;

  for (const ExistingRegion& region : existing.regions) {
    if (region_names.count(region.name) == 0) continue;
    if (std::count_if(existing.regions.begin(), existing.regions.end(),
          [&](const ExistingRegion& candidate) { return candidate.name == region.name; }) != 1) {
      result.error = "Multiple project regions match generated cleanup identity: " + region.name;
      return result;
    }
    result.plan.regions.push_back({region.id, region.name});
  }

  for (const ExistingCueAudioItem& item : existing.cue_audio_items) {
    if (item.role == "cue_audio" && cue_keys.count(item.cue_key) != 0) {
      result.plan.cue_audio_items.push_back({item.track_index, item.item_index, item.cue_key});
    }
  }
  for (const ExistingTrack& track : existing.tracks) {
    if (track.role == "cue_character" && track_belongs_to_character(track.key, selected_tokens)) {
      result.plan.cue_character_tracks.push_back({track.project_index, track.key});
    }
  }
  return result;
}

} // namespace reaadr::core
