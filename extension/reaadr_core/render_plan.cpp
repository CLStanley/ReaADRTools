#include "render_plan.hpp"
#include "domain_utils.hpp"
#include "lane_assignment.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>

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
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f' || byte == '\v';
  };
  std::size_t first = 0;
  while (first < value.size() && is_space(static_cast<unsigned char>(value[first]))) ++first;
  std::size_t last = value.size();
  while (last > first && is_space(static_cast<unsigned char>(value[last - 1]))) --last;
  return value.substr(first, last - first);
}

bool parse_number(const std::string& value, double& output)
{
  const std::string cleaned = trim_ascii(value);
  char* end = nullptr;
  output = std::strtod(cleaned.c_str(), &end);
  return !cleaned.empty() && end && end != cleaned.c_str() && *end == '\0' && std::isfinite(output);
}

RgbColor character_color(const std::string& character)
{
  static constexpr std::array<std::array<int, 3>, 8> kColors = {{
    {{236, 112, 99}},
    {{175, 122, 197}},
    {{72, 201, 176}},
    {{245, 176, 65}},
    {{84, 153, 199}},
    {{220, 118, 51}},
    {{127, 179, 213}},
    {{130, 224, 170}},
  }};
  unsigned int hash = 0;
  for (unsigned char byte : trim_ascii(character)) hash += byte;
  const auto& color = kColors[hash % kColors.size()];
  return {color[0], color[1], color[2], true};
}

struct CueRenderInfo {
  const Fields* cue = nullptr;
  std::string character;
  double start_time = 0.0;
  double end_time = 0.0;
  int lane = 1;
};

bool desired_track_equal(const ExistingTrack& existing, const DesiredTrack& desired)
{
  return existing.role == desired.role && existing.key == desired.key &&
    existing.name == desired.name && existing.color == desired.color;
}

bool desired_region_equal(const ExistingRegion& existing,
                          const DesiredRegion& desired,
                          double timing_epsilon)
{
  return existing.name == desired.name &&
    std::abs(existing.start_time - desired.start_time) <= timing_epsilon &&
    std::abs(existing.end_time - desired.end_time) <= timing_epsilon &&
    existing.color == desired.color;
}

bool desired_lane_equal(const ExistingRulerLane& existing, const DesiredRulerLane& desired)
{
  return existing.index == desired.index && existing.name == desired.name &&
    existing.color == desired.color && existing.hidden == desired.hidden;
}

bool desired_cue_audio_equal(const ExistingCueAudioItem& existing,
                             const DesiredCueAudioItem& desired,
                             double timing_epsilon)
{
  return existing.track_role == "cue_character" && existing.track_key == desired.target_track_key &&
    existing.role == "cue_audio" && existing.cue_key == desired.cue_key &&
    existing.source_path == desired.source_path && existing.take_name == desired.take_name &&
    existing.has_take && !existing.loops_source &&
    std::abs(existing.position - desired.position) <= timing_epsilon &&
    std::abs(existing.length - desired.length) <= timing_epsilon;
}

std::set<std::string> generated_region_names(const SessionModel& model)
{
  std::set<std::string> names;
  for (const Fields& cue : model.cues) {
    // An incomplete historical cue is not sufficient proof that a similarly
    // named project region belongs to ReaADR.
    if (!render_cue_key(cue).empty()) names.insert(render_region_name(cue));
  }
  return names;
}

} // namespace

std::string render_character_name(const Fields& cue)
{
  const std::string name = trim_ascii(field(cue, "character"));
  return name.empty() ? "Unassigned" : name;
}

std::string render_cue_key(const Fields& cue)
{
  const std::string id = sanitize_token(field(cue, "id"));
  return id.empty() ? field(cue, "source_line") : id;
}

std::string render_region_name(const Fields& cue)
{
  return "[ReaADR]:id=" + render_cue_key(cue) + " ADR Cue " + field(cue, "id") + " - " +
    render_character_name(cue);
}

bool operator==(const RgbColor& left, const RgbColor& right)
{
  return left.red == right.red && left.green == right.green && left.blue == right.blue &&
    left.custom == right.custom;
}

bool operator!=(const RgbColor& left, const RgbColor& right)
{
  return !(left == right);
}

RenderPlanResult build_render_plan(const SessionModel& model,
                                   const SessionModel* previous_model,
                                   const ProjectRenderState& existing,
                                   const RenderPlanOptions& options)
{
  RenderPlanResult result;
  if (model.session_id().empty()) {
    result.error = "A canonical session ID is required before rendering.";
    return result;
  }
  if (previous_model && previous_model->session_id() != model.session_id()) {
    result.error = "The previous render model belongs to a different ADR session.";
    return result;
  }
  if (!std::isfinite(options.preroll_seconds) || options.preroll_seconds < 0.0) {
    result.error = "Render preroll must be a finite, non-negative number.";
    return result;
  }
  if (!std::isfinite(options.timing_epsilon) || options.timing_epsilon < 0.0) {
    result.error = "Render timing epsilon must be a finite, non-negative number.";
    return result;
  }
  const bool render_cue_audio = !options.cue_audio_path.empty() || options.cue_audio_duration != 0.0;
  if (render_cue_audio && (options.cue_audio_path.empty() ||
      !std::isfinite(options.cue_audio_duration) || options.cue_audio_duration <= 0.0)) {
    result.error = "Cue-audio rendering requires a path and a positive finite source duration.";
    return result;
  }

  const LaneAssignmentResult lane_assignment = assign_character_lanes(model.cues, options.preroll_seconds);
  if (!lane_assignment) {
    result.error = lane_assignment.error;
    return result;
  }

  std::vector<CueRenderInfo> cues;
  cues.reserve(model.cues.size());
  for (std::size_t index = 0; index < model.cues.size(); ++index) {
    const Fields& cue = model.cues[index];
    CueRenderInfo info;
    info.cue = &cue;
    info.character = render_character_name(cue);
    if (!parse_number(field(cue, "start_time"), info.start_time) ||
        !parse_number(field(cue, "end_time"), info.end_time) ||
        info.end_time < info.start_time) {
      result.error = "Cue " + field(cue, "id") + " has invalid render timing.";
      return result;
    }
    if (render_cue_key(cue).empty()) {
      result.error = "Cue " + field(cue, "id") + " has no stable render key.";
      return result;
    }
    info.lane = lane_assignment.lanes[index];
    cues.push_back(std::move(info));
  }

  std::vector<DesiredTrack> desired_tracks;
  std::set<std::string> desired_track_identities;
  std::map<std::string, std::string> track_key_characters;
  std::vector<DesiredRegion> desired_regions;
  std::set<std::string> desired_region_names;
  std::map<std::string, int> maximum_character_lanes;
  std::vector<DesiredCueAudioItem> desired_cue_audio;
  std::set<std::string> desired_cue_keys;
  for (const CueRenderInfo& info : cues) {
    const int lane = info.lane;
    const std::string lane_text = std::to_string(lane);
    const std::string key = sanitize_token(info.character) + ".lane" + lane_text;
    const auto key_owner = track_key_characters.emplace(key, info.character);
    if (!key_owner.second && key_owner.first->second != info.character) {
      result.error = "Characters " + key_owner.first->second + " and " + info.character +
        " resolve to the same legacy track key: " + key;
      return result;
    }
    const RgbColor color = character_color(info.character);
    maximum_character_lanes[info.character] = (std::max)(maximum_character_lanes[info.character], lane);
    const auto add_track = [&](const std::string& role, const std::string& name) {
      const std::string identity = role + '\n' + key;
      if (desired_track_identities.insert(identity).second) {
        desired_tracks.push_back({role, key, name, color});
      }
    };
    if (options.create_cue_tracks) {
      add_track("cue_character", lane == 1 ? "Cue - " + info.character
                                            : "Cue - " + info.character + " " + lane_text);
    }
    if (options.create_dialogue_tracks) {
      add_track("character", lane == 1 ? info.character : info.character + " " + lane_text);
    }

    DesiredRegion region;
    region.model_region_id = field(*info.cue, "region_id");
    region.name = render_region_name(*info.cue);
    region.start_time = info.start_time;
    region.end_time = info.end_time;
    region.color = color;
    if (!desired_region_names.insert(region.name).second) {
      result.error = "Multiple cues resolve to the generated region name: " + region.name;
      return result;
    }
    desired_regions.push_back(std::move(region));

    if (render_cue_audio) {
      DesiredCueAudioItem item;
      item.cue_key = render_cue_key(*info.cue);
      item.target_track_key = key;
      item.source_path = options.cue_audio_path;
      item.take_name = "[ReaADR]:id=" + item.cue_key + " Cue Audio " +
        field(*info.cue, "id") + " - " + info.character;
      item.position = (std::max)(0.0, info.start_time - options.cue_audio_duration);
      item.length = options.cue_audio_duration;
      if (!desired_cue_keys.insert(item.cue_key).second) {
        result.error = "Multiple cues resolve to the generated cue-audio key: " + item.cue_key;
        return result;
      }
      desired_cue_audio.push_back(std::move(item));
    }
  }

  if (options.configure_ruler_lanes) {
    int lane_index = 0;
    for (const auto& [character, lane_count] : maximum_character_lanes) {
      for (int character_lane = 1; character_lane <= lane_count; ++character_lane) {
        DesiredRulerLane desired;
        desired.index = lane_index++;
        desired.name = character_lane == 1 ? character : character + " #" + std::to_string(character_lane);
        desired.color = character_color(character);
        desired.hidden = false;

        const auto found = std::find_if(existing.ruler_lanes.begin(), existing.ruler_lanes.end(),
          [&](const ExistingRulerLane& lane) { return lane.index == desired.index; });
        if (found == existing.ruler_lanes.end() || !desired_lane_equal(*found, desired)) {
          result.plan.ruler_lane_mutations.push_back(desired);
        }
      }
    }
    const int existing_count = static_cast<int>(existing.ruler_lanes.size());
    if (lane_index > existing_count) result.plan.minimum_ruler_lane_count = lane_index;

    for (const CueRenderInfo& info : cues) {
      int desired_lane_index = 0;
      for (const auto& [character, lane_count] : maximum_character_lanes) {
        if (character == info.character) {
          desired_lane_index += info.lane - 1;
          break;
        }
        desired_lane_index += lane_count;
      }
      const std::string name = render_region_name(*info.cue);
      const auto found = std::find_if(existing.region_lanes.begin(), existing.region_lanes.end(),
        [&](const ExistingRegionLane& assignment) { return assignment.region_name == name; });
      if (found == existing.region_lanes.end() || found->lane_index != desired_lane_index) {
        result.plan.region_lane_mutations.push_back({name, desired_lane_index});
      }
    }
  }

  std::set<std::size_t> claimed_tracks;
  for (const DesiredTrack& desired : desired_tracks) {
    const ExistingTrack* match = nullptr;
    for (const ExistingTrack& track : existing.tracks) {
      if (claimed_tracks.count(track.project_index) == 0 &&
          track.role == desired.role && track.key == desired.key) {
        match = &track;
        break;
      }
    }
    if (!match) {
      // Compatibility adoption retains the Lua behavior for pre-tagged-by-name
      // projects, but an object already owned by another role is never stolen.
      for (const ExistingTrack& track : existing.tracks) {
        if (claimed_tracks.count(track.project_index) == 0 && track.name == desired.name &&
            track.role.empty() && track.key.empty()) {
          match = &track;
          break;
        }
      }
    }

    if (!match) {
      result.plan.track_mutations.push_back({RenderMutationKind::create, 0, desired});
    } else {
      claimed_tracks.insert(match->project_index);
      if (!desired_track_equal(*match, desired)) {
        result.plan.track_mutations.push_back({RenderMutationKind::update, match->project_index, desired});
      }
    }
  }

  std::set<int> claimed_regions;
  for (const DesiredRegion& desired : desired_regions) {
    const ExistingRegion* match = nullptr;
    for (const ExistingRegion& region : existing.regions) {
      if (claimed_regions.count(region.id) == 0 && region.name == desired.name) {
        match = &region;
        break;
      }
    }
    if (!match) {
      result.plan.region_mutations.push_back({RenderMutationKind::create, -1, desired});
    } else {
      claimed_regions.insert(match->id);
      if (!desired_region_equal(*match, desired, options.timing_epsilon)) {
        result.plan.region_mutations.push_back({RenderMutationKind::update, match->id, desired});
      }
    }
  }

  if (previous_model) {
    const std::set<std::string> previously_owned_names = generated_region_names(*previous_model);
    for (const ExistingRegion& region : existing.regions) {
      if (claimed_regions.count(region.id) != 0 ||
          previously_owned_names.count(region.name) == 0 ||
          desired_region_names.count(region.name) != 0) {
        continue;
      }
      DesiredRegion stale;
      stale.name = region.name;
      result.plan.region_mutations.push_back({RenderMutationKind::remove, region.id, std::move(stale)});
    }
  }

  if (render_cue_audio) {
    std::set<std::pair<std::size_t, std::size_t>> claimed_items;
    for (DesiredCueAudioItem desired : desired_cue_audio) {
      const ExistingCueAudioItem* match = nullptr;
      for (const ExistingCueAudioItem& item : existing.cue_audio_items) {
        const auto identity = std::make_pair(item.track_index, item.item_index);
        if (claimed_items.count(identity) == 0 && item.role == "cue_audio" && item.cue_key == desired.cue_key) {
          match = &item;
          break;
        }
      }
      if (!match) {
        desired.replace_source = true;
        result.plan.cue_audio_mutations.push_back({RenderMutationKind::create, 0, 0, std::move(desired)});
      } else {
        claimed_items.insert({match->track_index, match->item_index});
        if (!desired_cue_audio_equal(*match, desired, options.timing_epsilon)) {
          desired.replace_source = !match->has_take || match->source_path != desired.source_path;
          result.plan.cue_audio_mutations.push_back({
            RenderMutationKind::update, match->track_index, match->item_index, std::move(desired),
          });
        }
      }
    }

    std::set<std::string> previously_owned_keys;
    if (previous_model) {
      for (const Fields& cue : previous_model->cues) {
        const std::string key = render_cue_key(cue);
        if (!key.empty()) previously_owned_keys.insert(key);
      }
    }
    for (const ExistingCueAudioItem& item : existing.cue_audio_items) {
      const auto identity = std::make_pair(item.track_index, item.item_index);
      if (item.role != "cue_audio" || item.cue_key.empty() || claimed_items.count(identity) != 0) continue;
      // Duplicates for a current cue are explicitly owned and safe to remove.
      // A fully stale key additionally requires proof from the prior model.
      if (desired_cue_keys.count(item.cue_key) == 0 && previously_owned_keys.count(item.cue_key) == 0) continue;
      DesiredCueAudioItem stale;
      stale.cue_key = item.cue_key;
      result.plan.cue_audio_mutations.push_back({
        RenderMutationKind::remove, item.track_index, item.item_index, std::move(stale),
      });
    }
  }

  return result;
}

} // namespace reaadr::core
