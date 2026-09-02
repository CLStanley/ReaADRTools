#pragma once
#include "session_model.hpp"
#include "model_repository.hpp"
#include <cstddef>
#include <cstdint>
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
struct CueManagerViewOptions {
  std::string query;
  std::string character;
  std::string status;
  std::string selected_cue_key;
  std::string sort_key = "start_time";
  bool sort_ascending = true;
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
CueManagerModel build_cue_manager_view(const SessionModel& model,
                                       const CueManagerViewOptions& options);
CueManagerEditResult edit_cue_manager_row(const SessionModel& model,
                                           const CueManagerEditOptions& options);
struct CueManagerCommitOptions {
  CueManagerEditOptions edit;
  std::string snapshot_label = "Edit Cue";
  std::string utc_timestamp;
  bool bump_revision = true;
};
struct CueManagerCommitResult {
  CueManagerEditResult edit;
  SessionSnapshot snapshot;
  std::uint64_t revision = 0;
  bool rolled_back = false;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};
CueManagerCommitResult commit_cue_manager_edit(SessionModelRepository& repository,
                                                const CueManagerCommitOptions& options);
}
