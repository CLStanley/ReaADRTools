#pragma once
#include "session_model.hpp"
#include <cstddef>
#include <string>
#include <vector>
namespace reaadr::core {
struct CueManagerRow {
  std::size_t model_index = 0;
  std::string cue_key;
  std::string character;
  std::string dialogue;
  std::string cue_type;
  std::string status;
  std::string start_time;
  std::string end_time;
  bool selected = false;
};
struct CueManagerModel {
  std::string session_id;
  std::vector<CueManagerRow> rows;
  std::string selected_cue_key;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};
struct CueManagerEditOptions {
  std::string cue_key;
  std::string dialogue;
  std::string cue_type;
  std::string status;
  std::string start_time;
  std::string end_time;
};
struct CueManagerEditResult {
  SessionModel model;
  bool changed = false;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};
CueManagerModel build_cue_manager_model(const SessionModel& model,
                                        const std::string& selected_cue_key = {});
CueManagerEditResult edit_cue_manager_row(const SessionModel& model,
                                           const CueManagerEditOptions& options);
}
