#pragma once

#include "session_model.hpp"

#include <string>
#include <vector>

namespace reaadr::core {

struct SessionBuildOptions {
  // Session identity is supplied by the application/repository boundary. This
  // keeps clocks and project extstate out of the deterministic domain builder.
  std::string session_id;
  std::string session_name;
  Fields project_metadata;
  std::string frame_rate = "24";
  std::string refresh_version = "0";
  std::string last_operation = "save_session";
  bool cues_modified = false;
  bool tracks_modified = false;
  bool regions_modified = false;
};

struct SessionBuildResult {
  SessionModel model;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Builds every cue-derived collection in one pass. This is the native
// equivalent of build_adr_session and is the only supported path from imported
// cue rows to a complete canonical model.
SessionBuildResult build_session_model(const std::vector<Fields>& cues, const SessionBuildOptions& options);

} // namespace reaadr::core
