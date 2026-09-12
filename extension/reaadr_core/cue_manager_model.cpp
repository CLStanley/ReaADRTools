#include "cue_manager_model.hpp"
#include "reaadr_ui/cue_manager_ui_contract.hpp"
#include "domain_utils.hpp"
#include <utility>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <set>
namespace reaadr::core {
const CueManagerRow* adjacent_cue_manager_row(const CueManagerModel& view, bool next)
{
  if (view.rows.empty()) return nullptr;
  for (std::size_t i = 0; i < view.rows.size(); ++i) {
    if (!view.rows[i].selected) continue;
    const auto target = next ? std::min(i + 1, view.rows.size() - 1) : (i == 0 ? 0 : i - 1);
    return &view.rows[target];
  }
  return &view.rows.front();
}

namespace { const std::string& field(const Fields& cue, const char* key) {
  const auto found = cue.find(key); static const std::string empty;
  return found == cue.end() ? empty : found->second;
} }
namespace { std::string cue_type_field(const Fields& cue) {
  const auto canonical = cue.find("cue_type");
  if (canonical != cue.end()) return canonical->second;
  return field(cue, "type");
} }
namespace { std::string notes_field(const Fields& cue) {
  const auto notes = cue.find("notes");
  if (notes != cue.end()) return notes->second;
  return field(cue, "direction");
} }
namespace { std::string dialogue_field(const Fields& cue) {
  const auto line = cue.find("line");
  if (line != cue.end()) return line->second;
  return field(cue, "dialogue");
} }
namespace { bool contains_case_insensitive(const std::string& value, const std::string& query) {
  if (query.empty()) return true;
  auto lower = [](std::string text) { for (char& c : text) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); return text; };
  return lower(value).find(lower(query)) != std::string::npos;
} }
namespace {
double session_frame_rate(const SessionModel& model)
{
  const auto found = model.timecode.find("frame_rate");
  if (found == model.timecode.end()) return 24.0;
  char* end = nullptr;
  const double parsed = std::strtod(found->second.c_str(), &end);
  return end && end != found->second.c_str() && *end == '\0' && parsed > 0.0 ? parsed : 24.0;
}

std::string canonical_seconds(double seconds)
{
  std::ostringstream output;
  output << std::setprecision(15) << seconds;
  return output.str();
}

bool numeric_id(const std::string& value)
{
  return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char character) {
    return std::isdigit(character) != 0;
  });
}
}
CueManagerModel build_cue_manager_model(const SessionModel& model,
                                        const std::string& selected_cue_key)
{
  CueManagerModel result;
  result.session_id = model.session_id();
  if (result.session_id.empty()) { result.error = "A canonical session ID is required."; return result; }
  result.selected_cue_key = selected_cue_key;
  for (std::size_t index = 0; index < model.cues.size(); ++index) {
    const Fields& cue = model.cues[index];
    const std::string key = field(cue, "id");
      result.rows.push_back({index, key, field(cue, "character"), dialogue_field(cue),
      cue_type_field(cue), field(cue, "status"), field(cue, "start_time"),
      field(cue, "end_time"), !selected_cue_key.empty() && key == selected_cue_key, notes_field(cue)});
  }
  return result;
}

CueManagerModel build_cue_manager_view(const SessionModel& model,
                                       const CueManagerViewOptions& options)
{
  CueManagerModel result;
  result.session_id = model.session_id();
  if (result.session_id.empty()) { result.error = "A canonical session ID is required."; return result; }
  result.selected_cue_key = options.selected_cue_key;
  for (std::size_t index = 0; index < model.cues.size(); ++index) {
    const Fields& cue = model.cues[index];
    const std::string key = field(cue, "id");
    const std::string character = field(cue, "character");
    const std::string status = field(cue, "status");
    if (!options.character.empty() && character != options.character) continue;
    if (!options.status.empty() && status != options.status) continue;
    if (!contains_case_insensitive(key, options.query) &&
        !contains_case_insensitive(character, options.query) &&
        !contains_case_insensitive(dialogue_field(cue), options.query)) continue;
    result.rows.push_back({index, key, character, dialogue_field(cue), cue_type_field(cue),
      status, field(cue, "start_time"), field(cue, "end_time"), key == options.selected_cue_key, notes_field(cue)});
  }
  const auto value_for = [](const CueManagerRow& row, const std::string& key) {
    if (key == "id") return row.cue_key;
    if (key == "character") return row.character;
    if (key == "dialogue" || key == "line") return row.dialogue;
    if (key == "notes") return row.notes;
    if (key == "cue_type" || key == "type") return row.cue_type;
    if (key == "status") return row.status;
    if (key == "end_time") return row.end_time;
    return row.start_time;
  };
  const std::string sort_key = is_cue_manager_sort_key(options.sort_key) ? options.sort_key : "start_time";
  if (!sort_key.empty()) {
    std::stable_sort(result.rows.begin(), result.rows.end(), [&](const CueManagerRow& left, const CueManagerRow& right) {
      const std::string a = value_for(left, sort_key);
      const std::string b = value_for(right, sort_key);
      char* end_a = nullptr; char* end_b = nullptr;
      const double number_a = std::strtod(a.c_str(), &end_a);
      const double number_b = std::strtod(b.c_str(), &end_b);
      const bool numeric = end_a && end_b && *end_a == '\0' && *end_b == '\0';
      const bool less = numeric ? number_a < number_b : a < b;
      const bool greater = numeric ? number_a > number_b : a > b;
      return options.sort_ascending ? less : greater;
    });
  }
  return result;
}

std::vector<std::string> cue_manager_character_choices(const SessionModel& model)
{
  std::set<std::string> unique;
  for (const auto& cue : model.cues) {
    const std::string character = field(cue, "character");
    if (!character.empty()) unique.insert(character);
  }
  return {unique.begin(), unique.end()};
}

CueManagerEditResult edit_cue_manager_row(const SessionModel& model,
                                          const CueManagerEditOptions& options)
{
  CueManagerEditResult result;
  result.model = model;
  if (model.session_id().empty()) { result.error = "A canonical session ID is required."; return result; }
  if (options.cue_key.empty()) { result.error = "A cue ID is required."; return result; }
  std::size_t matches = 0;
  std::size_t selected = 0;
  for (std::size_t index = 0; index < model.cues.size(); ++index) {
    if (field(model.cues[index], "id") == options.cue_key) { ++matches; selected = index; }
  }
  if (matches == 0) { result.error = "The cue ID is not present in the canonical session."; return result; }
  if (matches > 1) { result.error = "Multiple cues match the cue ID."; return result; }
  Fields& cue = result.model.cues[selected];
  if (!options.new_cue_key.empty() && options.new_cue_key != options.cue_key) {
    for (std::size_t index = 0; index < result.model.cues.size(); ++index) {
      if (index != selected && field(result.model.cues[index], "id") == options.new_cue_key) {
        result.error = "The new cue ID is already in use.";
        return result;
      }
    }
    cue["id"] = options.new_cue_key;
    result.changed = true;
  }
  if (!options.new_character.empty() && field(cue, "character") != options.new_character) {
    cue["character"] = options.new_character;
    result.changed = true;
  }
  const double frame_rate = options.input_frame_rate.value_or(session_frame_rate(model));
  if (!std::isfinite(frame_rate) || frame_rate <= 0.0) {
    result.error = "Cue input frame rate must be finite and positive.";
    return result;
  }
  for (const auto* timing : {&options.start_time, &options.end_time}) {
    if (timing->empty()) continue;
    if (!parse_timecode(*timing, frame_rate)) {
      result.error = "Cue timing is invalid.";
      return result;
    }
  }
  const auto parsed_time = [&](const std::string& value) {
    const auto parsed = parse_timecode(value, frame_rate);
    return parsed ? *parsed.seconds : -1.0;
  };
  const double start = options.start_time.empty() ? parsed_time(field(cue, "start_time")) : parsed_time(options.start_time);
  const double end = options.end_time.empty() ? parsed_time(field(cue, "end_time")) : parsed_time(options.end_time);
  if (start >= 0.0 && end >= 0.0 && end <= start) {
    result.error = "Cue end time must be after its start time.";
    return result;
  }
  const auto canonical_time = [&](const std::string& value) {
    if (value.empty()) return value;
    const auto parsed = parse_timecode(value, frame_rate);
    if (!parsed) return value;
    std::ostringstream output;
    output << std::setprecision(15) << *parsed.seconds;
    return output.str();
  };
  const std::string start_value = canonical_time(options.start_time);
  const std::string end_value = canonical_time(options.end_time);
  const std::pair<const char*, const std::string*> updates[] = {
    {"dialogue", &options.dialogue}, {"notes", &options.notes}, {"cue_type", &options.cue_type},
    {"status", &options.status}, {"start_time", &options.start_time},
    {"end_time", &options.end_time},
  };
  for (const auto& update : updates) {
    const char* key = update.first;
    if (std::string(update.first) == "dialogue" && cue.find("dialogue") == cue.end() && cue.find("line") != cue.end()) key = "line";
    if (std::string(update.first) == "cue_type" && cue.find("cue_type") == cue.end() && cue.find("type") != cue.end()) key = "type";
    const std::string* value = update.second;
    if (std::string(update.first) == "start_time") value = &start_value;
    else if (std::string(update.first) == "end_time") value = &end_value;
    const bool explicitly_empty =
      (std::string(update.first) == "notes" && options.notes_set) ||
      (std::string(update.first) == "dialogue" && options.dialogue_set);
    if (value->empty() && !explicitly_empty) continue;
    if (field(cue, key) == *value) continue;
    cue[key] = *value;
    result.changed = true;
  }
  if (result.changed) {
    result.model.state["last_operation"] = "edit_cue";
    result.model.dirty_flags["cues_modified"] = "true";
  }
  return result;
}

std::string next_cue_manager_id(const SessionModel& model)
{
  std::set<std::string> existing;
  for (const auto& cue : model.cues) existing.insert(field(cue, "id"));
  std::size_t candidate = model.cues.size() + 1;
  while (existing.count(std::to_string(candidate)) != 0) ++candidate;
  return std::to_string(candidate);
}

CueManagerMutationResult add_cue_manager_row(const SessionModel& model,
                                              const CueManagerAddOptions& options)
{
  CueManagerMutationResult result;
  result.model = model;
  if (model.session_id().empty()) {
    result.error = "A canonical session ID is required.";
    return result;
  }

  const std::string cue_key = options.cue_key.empty() ? next_cue_manager_id(model) : options.cue_key;
  if (cue_key.empty()) {
    result.error = "A cue ID is required.";
    return result;
  }
  for (const auto& cue : model.cues) {
    if (field(cue, "id") == cue_key) {
      result.error = "The cue ID is already in use.";
      return result;
    }
  }

  const double frame_rate = options.input_frame_rate.value_or(session_frame_rate(model));
  if (!std::isfinite(frame_rate) || frame_rate <= 0.0) {
    result.error = "Cue input frame rate must be finite and positive.";
    return result;
  }
  const auto parsed_start = parse_timecode(options.start_time, frame_rate);
  if (!parsed_start) {
    result.error = "Cue start time is invalid.";
    return result;
  }
  const double start = *parsed_start.seconds;
  double end = start + 2.0;
  if (!options.end_time.empty()) {
    const auto parsed_end = parse_timecode(options.end_time, frame_rate);
    if (!parsed_end) {
      result.error = "Cue end time is invalid.";
      return result;
    }
    end = *parsed_end.seconds;
  }
  // Match the transitional Cue Manager: an inverted/zero range becomes a
  // two-second cue instead of creating an unusable region.
  if (end <= start) end = start + 2.0;

  Fields cue;
  cue["id"] = cue_key;
  cue["character"] = options.character.empty() ? "ADR" : options.character;
  cue["start_time"] = canonical_seconds(start);
  cue["end_time"] = canonical_seconds(end);
  cue["line"] = options.dialogue;
  cue["direction"] = "";
  cue["cue_type"] = options.cue_type.empty() ? "Dialogue" : options.cue_type;
  cue["status"] = normalize_status(options.status);
  cue["notes"] = options.notes;
  cue["source_line"] = std::to_string(model.cues.size() + 1);
  result.model.cues.push_back(cue);
  std::stable_sort(result.model.cues.begin(), result.model.cues.end(), [&](const Fields& left, const Fields& right) {
    const auto left_time = parse_timecode(field(left, "start_time"), frame_rate);
    const auto right_time = parse_timecode(field(right, "start_time"), frame_rate);
    const double a = left_time ? *left_time.seconds : 0.0;
    const double b = right_time ? *right_time.seconds : 0.0;
    if (a != b) return a < b;
    return field(left, "id") < field(right, "id");
  });
  for (std::size_t index = 0; index < result.model.cues.size(); ++index) {
    if (field(result.model.cues[index], "id") == cue_key) {
      result.model_index = index;
      break;
    }
  }
  result.model.state["last_operation"] = "add_cached_cue";
  result.model.dirty_flags["cues_modified"] = "true";
  result.affected_cue = cue;
  result.selected_cue_key = cue_key;
  result.changed = true;
  return result;
}

CueManagerMutationResult remove_cue_manager_row(const SessionModel& model,
                                                 const std::string& cue_key,
                                                 bool renumber)
{
  CueManagerMutationResult result;
  result.model = model;
  if (model.session_id().empty()) {
    result.error = "A canonical session ID is required.";
    return result;
  }
  if (cue_key.empty()) {
    result.error = "A cue ID is required.";
    return result;
  }

  std::size_t matches = 0;
  std::size_t selected = 0;
  for (std::size_t index = 0; index < model.cues.size(); ++index) {
    if (field(model.cues[index], "id") == cue_key) {
      ++matches;
      selected = index;
    }
  }
  if (matches == 0) {
    result.error = "The cue ID is not present in the canonical session.";
    return result;
  }
  if (matches > 1) {
    result.error = "Multiple cues match the cue ID.";
    return result;
  }

  result.affected_cue = result.model.cues[selected];
  result.model_index = selected;
  result.model.cues.erase(result.model.cues.begin() + static_cast<std::ptrdiff_t>(selected));
  if (renumber) {
    bool all_numeric = true;
    std::size_t width = 0;
    for (const auto& cue : result.model.cues) {
      const std::string id = field(cue, "id");
      all_numeric = all_numeric && numeric_id(id);
      width = std::max(width, id.size());
    }
    if (!all_numeric) width = 0;
    for (std::size_t index = 0; index < result.model.cues.size(); ++index) {
      std::ostringstream id;
      if (width > 0) id << std::setw(static_cast<int>(width)) << std::setfill('0');
      id << index + 1;
      result.model.cues[index]["id"] = id.str();
      result.model.cues[index]["source_line"] = std::to_string(index + 1);
    }
  }
  if (!result.model.cues.empty()) {
    const std::size_t next = std::min(selected, result.model.cues.size() - 1);
    result.selected_cue_key = field(result.model.cues[next], "id");
  }
  result.model.state["last_operation"] = "remove_cached_cue";
  result.model.dirty_flags["cues_modified"] = "true";
  result.changed = true;
  return result;
}

CueManagerCommitResult commit_cue_manager_edit(SessionModelRepository& repository,
                                                const CueManagerCommitOptions& options)
{
  CueManagerCommitResult result;
  const auto loaded = repository.load();
  if (!loaded) { result.error = session_load_error_message(loaded); return result; }
  result.edit = edit_cue_manager_row(loaded.model, options.edit);
  if (!result.edit) { result.error = result.edit.error; return result; }
  if (!result.edit.changed) {
    const auto revision = repository.revision();
    if (!revision) result.error = revision.error; else result.revision = revision.revision;
    return result;
  }
  const auto snapshot = repository.create_snapshot(options.snapshot_label, options.utc_timestamp);
  if (!snapshot) { result.error = snapshot.error; return result; }
  result.snapshot = snapshot.snapshot;
  if (!repository.save(result.edit.model)) {
    result.error = "Could not persist the Cue Manager edit.";
    repository.restore_snapshot(result.snapshot); result.rolled_back = true; return result;
  }
  const auto revision = options.bump_revision ? repository.bump_revision() : repository.revision();
  if (!revision) {
    result.error = revision.error;
    repository.restore_snapshot(result.snapshot); result.rolled_back = true; return result;
  }
  result.revision = revision.revision;
  return result;
}
}
