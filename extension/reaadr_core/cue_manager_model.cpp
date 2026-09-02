#include "cue_manager_model.hpp"
#include <utility>
#include <algorithm>
#include <cstdlib>
namespace reaadr::core {
namespace { const std::string& field(const Fields& cue, const char* key) {
  const auto found = cue.find(key); static const std::string empty;
  return found == cue.end() ? empty : found->second;
} }
namespace { std::string cue_type_field(const Fields& cue) {
  const auto canonical = cue.find("cue_type");
  if (canonical != cue.end()) return canonical->second;
  return field(cue, "type");
} }
namespace { bool contains_case_insensitive(const std::string& value, const std::string& query) {
  if (query.empty()) return true;
  auto lower = [](std::string text) { for (char& c : text) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); return text; };
  return lower(value).find(lower(query)) != std::string::npos;
} }
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
      result.rows.push_back({index, key, field(cue, "character"), field(cue, "dialogue"),
      cue_type_field(cue), field(cue, "status"), field(cue, "start_time"),
      field(cue, "end_time"), !selected_cue_key.empty() && key == selected_cue_key});
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
        !contains_case_insensitive(field(cue, "dialogue"), options.query)) continue;
    result.rows.push_back({index, key, character, field(cue, "dialogue"), cue_type_field(cue),
      status, field(cue, "start_time"), field(cue, "end_time"), key == options.selected_cue_key});
  }
  const auto value_for = [](const CueManagerRow& row, const std::string& key) {
    if (key == "id") return row.cue_key;
    if (key == "character") return row.character;
    if (key == "dialogue" || key == "line") return row.dialogue;
    if (key == "cue_type" || key == "type") return row.cue_type;
    if (key == "status") return row.status;
    if (key == "end_time") return row.end_time;
    return row.start_time;
  };
  if (!options.sort_key.empty()) {
    std::stable_sort(result.rows.begin(), result.rows.end(), [&](const CueManagerRow& left, const CueManagerRow& right) {
      const std::string a = value_for(left, options.sort_key);
      const std::string b = value_for(right, options.sort_key);
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
  const std::pair<const char*, const std::string*> updates[] = {
    {"dialogue", &options.dialogue}, {"cue_type", &options.cue_type},
    {"status", &options.status}, {"start_time", &options.start_time},
    {"end_time", &options.end_time},
  };
  for (const auto& update : updates) {
    const char* key = update.first;
    if (std::string(update.first) == "cue_type" && cue.find("cue_type") == cue.end() && cue.find("type") != cue.end()) key = "type";
    if (update.second->empty() || field(cue, key) == *update.second) continue;
    cue[key] = *update.second;
    result.changed = true;
  }
  if (result.changed) {
    result.model.state["last_operation"] = "edit_cue";
    result.model.dirty_flags["cues_modified"] = "true";
  }
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
