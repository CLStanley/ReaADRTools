#include "cue_manager_model.hpp"
#include <utility>
namespace reaadr::core {
namespace { const std::string& field(const Fields& cue, const char* key) {
  const auto found = cue.find(key); static const std::string empty;
  return found == cue.end() ? empty : found->second;
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
      field(cue, "type"), field(cue, "status"), field(cue, "start_time"),
      field(cue, "end_time"), !selected_cue_key.empty() && key == selected_cue_key});
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
    {"dialogue", &options.dialogue}, {"type", &options.cue_type},
    {"status", &options.status}, {"start_time", &options.start_time},
    {"end_time", &options.end_time},
  };
  for (const auto& update : updates) {
    if (update.second->empty() || field(cue, update.first) == *update.second) continue;
    cue[update.first] = *update.second;
    result.changed = true;
  }
  if (result.changed) {
    result.model.state["last_operation"] = "edit_cue";
    result.model.dirty_flags["cues_modified"] = "true";
  }
  return result;
}
}
