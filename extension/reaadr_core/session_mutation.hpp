#pragma once

#include "session_builder.hpp"

#include <string>
#include <vector>

namespace reaadr::core {

struct CueReplacementOptions {
  SessionBuildOptions build;
  std::string last_operation = "save_cues";
};

// Replaces only cue-derived state. When an existing model is supplied, its
// session envelope and future/unknown records survive unchanged. This function
// has no persistence side effects; callers save its result transactionally.
SessionBuildResult replace_session_cues(const SessionModel* existing,
                                        const std::vector<Fields>& cues,
                                        const CueReplacementOptions& options);

} // namespace reaadr::core
