#include "session_builder.hpp"

#include "domain_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <limits>
#include <sstream>
#include <utility>

namespace reaadr::core {
namespace {

std::string trim(const std::string& value)
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

std::string field(const Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

std::string join(const std::vector<std::string>& values)
{
  std::ostringstream output;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) output << ',';
    output << values[index];
  }
  return output.str();
}

std::string number_string(double value)
{
  std::ostringstream output;
  output << std::setprecision(14) << value;
  return output.str();
}

double number_or(const std::string& value, double fallback)
{
  const std::string cleaned = trim(value);
  char* end = nullptr;
  const double parsed = std::strtod(cleaned.c_str(), &end);
  return end && end != cleaned.c_str() && *end == '\0' ? parsed : fallback;
}

int lane_for_cue(const Fields& cue)
{
  const double parsed = number_or(field(cue, "_reaadr_lane"), 1.0);
  if (!std::isfinite(parsed) || parsed < 1.0) return 1;
  if (parsed >= static_cast<double>(std::numeric_limits<int>::max())) return std::numeric_limits<int>::max();
  return static_cast<int>(parsed);
}

struct ScriptAccumulator {
  Fields fields;
  std::vector<std::string> characters;
  int cue_count = 0;
};

struct CharacterAccumulator {
  Fields fields;
  int cue_count = 0;
};

struct TrackAccumulator {
  Fields fields;
  std::vector<std::string> assigned_cues;
};

} // namespace

SessionBuildResult build_session_model(const std::vector<Fields>& cues, const SessionBuildOptions& options)
{
  SessionBuildResult result;
  if (trim(options.session_id).empty()) {
    result.error = "A session ID is required to build the ADR Session Model.";
    return result;
  }

  SessionModel& model = result.model;
  model.session = {{"session_id", options.session_id}, {"session_name", options.session_name}};
  model.project_metadata = options.project_metadata;
  model.timecode = {{"frame_rate", options.frame_rate.empty() ? "24" : options.frame_rate}};
  model.state = {
    {"active_script_id", ""},
    {"refresh_version", options.refresh_version},
    {"last_operation", options.last_operation},
  };
  model.dirty_flags = {
    {"cues_modified", options.cues_modified ? "true" : "false"},
    {"tracks_modified", options.tracks_modified ? "true" : "false"},
    {"regions_modified", options.regions_modified ? "true" : "false"},
  };

  std::vector<ScriptAccumulator> scripts;
  std::map<std::string, std::size_t> script_indexes;
  std::vector<CharacterAccumulator> characters;
  std::map<std::string, std::size_t> character_indexes;
  std::vector<TrackAccumulator> tracks;
  std::map<std::string, std::size_t> track_indexes;

  for (const Fields& source_cue : cues) {
    Fields cue = source_cue;
    std::string script_id = trim(field(cue, "script_id"));
    if (script_id.empty()) script_id = "manual";
    cue["script_id"] = script_id;
    cue["status"] = normalize_status(field(cue, "status"));

    std::size_t script_index = 0;
    const auto existing_script = script_indexes.find(script_id);
    if (existing_script == script_indexes.end()) {
      script_index = scripts.size();
      script_indexes[script_id] = script_index;
      ScriptAccumulator script;
      script.fields = {
        {"script_id", script_id},
        {"script_name", field(cue, "script_name")},
        {"source_file", field(cue, "source_file")},
        {"import_timestamp", field(cue, "import_timestamp")},
        {"revision_id", field(cue, "script_revision")},
        {"metadata", ""},
      };
      scripts.push_back(std::move(script));
    } else {
      script_index = existing_script->second;
    }
    ++scripts[script_index].cue_count;

    std::string character_name = trim(field(cue, "character"));
    if (character_name.empty()) character_name = "Unassigned";
    const std::string character_id = stable_id("character", {script_id, character_name});

    const auto existing_character = character_indexes.find(character_id);
    if (existing_character == character_indexes.end()) {
      const std::size_t index = characters.size();
      character_indexes[character_id] = index;
      CharacterAccumulator character;
      character.fields = {
        {"character_id", character_id},
        {"character_name", character_name},
        {"script_id", script_id},
        {"status", "active"},
        {"import_state", "imported"},
      };
      characters.push_back(std::move(character));
      scripts[script_index].characters.push_back(character_id);
    }
    ++characters[character_indexes[character_id]].cue_count;

    const int lane = lane_for_cue(cue);
    const std::string lane_text = std::to_string(lane);
    const std::string cue_track_id = stable_id("track", {character_id, "cues", lane_text});
    const std::string dialogue_track_id = stable_id("track", {character_id, "dialogue", lane_text});

    auto ensure_track = [&](const std::string& track_id, const char* type, const std::string& name) -> TrackAccumulator& {
      const auto existing = track_indexes.find(track_id);
      if (existing != track_indexes.end()) return tracks[existing->second];
      const std::size_t index = tracks.size();
      track_indexes[track_id] = index;
      TrackAccumulator track;
      track.fields = {
        {"track_id", track_id},
        {"character_id", character_id},
        {"track_type", type},
        {"track_name", name},
      };
      tracks.push_back(std::move(track));
      return tracks.back();
    };

    const std::string cue_track_name = lane > 1
      ? character_name + " Cues #" + lane_text
      : character_name + " Cues";
    const std::string dialogue_track_name = lane > 1
      ? character_name + " #" + lane_text
      : character_name;

    const std::string identity = script_id + ':' + character_id + ':' + trim(field(cue, "id"));
    ensure_track(cue_track_id, "cues", cue_track_name).assigned_cues.push_back(identity);
    ensure_track(dialogue_track_id, "dialogue", dialogue_track_name).assigned_cues.push_back(identity);

    const std::string region_id = stable_id("region", {identity});
    cue.erase("_reaadr_lane");
    cue["character_id"] = character_id;
    cue["track_id"] = cue_track_id;
    cue["region_id"] = region_id;
    cue["session_cue_id"] = identity;
    model.cues.push_back(std::move(cue));

    const double start_time = number_or(field(source_cue, "start_time"), 0.0);
    const double end_time = number_or(field(source_cue, "end_time"), start_time);
    model.regions.push_back({
      {"region_id", region_id},
      {"cue_id", identity},
      {"start_time", number_string(start_time)},
      {"end_time", number_string(end_time)},
      {"color", field(source_cue, "color")},
      {"label", field(source_cue, "id")},
    });
  }

  for (ScriptAccumulator& script : scripts) {
    script.fields["characters"] = join(script.characters);
    script.fields["cue_count"] = std::to_string(script.cue_count);
    model.scripts.push_back(script.fields);
    model.imports.push_back({
      {"script_id", field(script.fields, "script_id")},
      {"file_hash", stable_id("file", {
        field(script.fields, "script_id"),
        field(script.fields, "script_name"),
        std::to_string(script.cue_count),
      })},
      {"import_timestamp", field(script.fields, "import_timestamp")},
      {"imported_characters", join(script.characters)},
      {"cue_snapshot_hash", stable_id("cues", {
        field(script.fields, "script_id"),
        std::to_string(script.cue_count),
        std::to_string(cues.size()),
      })},
    });
  }

  for (CharacterAccumulator& character : characters) {
    character.fields["cue_count"] = std::to_string(character.cue_count);
    model.characters.push_back(std::move(character.fields));
  }
  for (TrackAccumulator& track : tracks) {
    track.fields["assigned_cues"] = join(track.assigned_cues);
    model.tracks.push_back(std::move(track.fields));
  }

  return result;
}

} // namespace reaadr::core
