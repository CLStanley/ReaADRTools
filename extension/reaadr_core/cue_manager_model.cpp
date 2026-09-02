#include "cue_manager_model.hpp"
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
}
