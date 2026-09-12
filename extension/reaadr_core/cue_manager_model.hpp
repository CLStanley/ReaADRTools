#pragma once
#include "session_model.hpp"
#include "model_repository.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <optional>
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
  std::string notes;
};
struct CueManagerModel {
  std::string session_id;
  std::vector<CueManagerRow> rows;
  std::string selected_cue_key;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};
// Manager footer navigation follows the displayed order and clamps at either end,
// matching Lua independently of the separate timeline-navigation wrap preference.
const CueManagerRow* adjacent_cue_manager_row(const CueManagerModel& view, bool next);

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
  std::string notes;
  std::string cue_type;
  std::string status;
  std::string start_time;
  std::string end_time;
  std::string new_cue_key;
  std::string new_character;
  bool notes_set = false;
  // Empty dialogue normally means "not supplied" for compatibility callers.
  // The native editor sets this flag so users can intentionally clear a line.
  bool dialogue_set = false;
  // Host input rate may differ from the session metadata; omitted uses the session rate.
  std::optional<double> input_frame_rate;
};
struct CueManagerEditResult {
  SessionModel model;
  bool changed = false;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};
struct CueManagerAddOptions {
  std::string cue_key;
  std::string character = "ADR";
  std::string start_time;
  std::string end_time;
  std::string dialogue;
  std::string cue_type = "Dialogue";
  std::string status = "Not Recorded";
  std::string notes;
  std::optional<double> input_frame_rate;
};
struct CueManagerMutationResult {
  SessionModel model;
  Fields affected_cue;
  std::string selected_cue_key;
  std::size_t model_index = 0;
  bool changed = false;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};
CueManagerModel build_cue_manager_model(const SessionModel& model,
                                        const std::string& selected_cue_key = {});
CueManagerModel build_cue_manager_view(const SessionModel& model,
                                       const CueManagerViewOptions& options);
std::vector<std::string> cue_manager_character_choices(const SessionModel& model);
CueManagerEditResult edit_cue_manager_row(const SessionModel& model,
                                           const CueManagerEditOptions& options);
// Returns the first count-based numeric ID that is not already owned by a cue.
std::string next_cue_manager_id(const SessionModel& model);
CueManagerMutationResult add_cue_manager_row(const SessionModel& model,
                                              const CueManagerAddOptions& options);
CueManagerMutationResult remove_cue_manager_row(const SessionModel& model,
                                                 const std::string& cue_key,
                                                 bool renumber = true);
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
