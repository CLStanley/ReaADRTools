#pragma once

#include "model_repository.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace reaadr::core {

struct CueStatusUpdateOptions {
  std::string cue_key;
  std::string status = "Recorded";
  std::string last_operation = "set_cue_status";
};

struct CueStatusUpdateResult {
  SessionModel model;
  Fields cue;
  std::size_t cue_model_index = 0;
  std::string normalized_status;
  bool changed = false;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Updates only canonical cue workflow state. Status does not affect generated
// tracks, regions, or cue audio, so derived records remain byte-for-byte intact.
CueStatusUpdateResult update_cue_status(
  const SessionModel& model,
  const CueStatusUpdateOptions& options);

struct CueStatusCommitOptions {
  CueStatusUpdateOptions update;
  std::string snapshot_label = "Update Cue Status";
  std::string utc_timestamp;
  bool bump_revision = true;
};

struct CueStatusCommitResult {
  CueStatusUpdateResult update;
  SessionSnapshot snapshot;
  std::uint64_t revision = 0;
  bool rolled_back = false;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Persists one status change with the same snapshot/revision rollback contract
// as larger session commits. Repeating an already-applied update is a no-op.
CueStatusCommitResult commit_cue_status(
  SessionModelRepository& repository,
  const CueStatusCommitOptions& options);

} // namespace reaadr::core
