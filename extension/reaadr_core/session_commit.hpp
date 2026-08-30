#pragma once

#include "model_repository.hpp"
#include "session_mutation.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace reaadr::core {

struct SessionCommitOptions {
  CueReplacementOptions replacement;
  std::string snapshot_label = "Commit Session Cues";
  std::string utc_timestamp;
  bool bump_revision = true;
};

struct SessionCommitResult {
  SessionModel model;
  SessionSnapshot snapshot;
  std::uint64_t revision = 0;
  std::string error;
  bool rolled_back = false;

  explicit operator bool() const { return error.empty(); }
};

// Commits model intent only. REAPER tracks, regions, items, and FX must be
// rendered by a higher application service inside the same outer transaction.
// SessionRenderService composes this narrow operation with project rendering;
// keeping the model step separate also supports future headless import tests.
SessionCommitResult commit_session_cues(SessionModelRepository& repository,
                                         const std::vector<Fields>& cues,
                                         const SessionCommitOptions& options);

} // namespace reaadr::core
