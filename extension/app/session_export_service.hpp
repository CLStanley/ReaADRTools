#pragma once

#include "reaadr_core/session_model.hpp"

#include <string>

namespace reaadr::reaper {

enum class SessionExportKind {
  cue_sheet,
  timing_report,
  session_metadata,
};

struct SessionExportResult {
  std::string content;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};

SessionExportResult format_session_export(
  const core::SessionModel& model,
  SessionExportKind kind);

bool write_session_export(
  const std::string& path,
  const std::string& content,
  std::string* error = nullptr);

} // namespace reaadr::reaper
