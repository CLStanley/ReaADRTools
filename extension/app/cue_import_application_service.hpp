#pragma once

#include "../reaadr_core/cue_import.hpp"
#include "../reaadr_reaper/session_render_service.hpp"

#include <optional>
#include <string>

namespace reaadr::reaper {

struct CueImportApplicationResult {
  core::TableParseResult parsed;
  core::CueImportResult imported;
  SessionRenderResult rendered;
  std::string error;

  explicit operator bool() const { return error.empty() && static_cast<bool>(rendered); }
};

// Converts already-loaded delimited content into canonical cues and renders
// the replacement through the same model/project transaction as native edits.
class CueImportApplicationService final {
public:
  CueImportApplicationService(SessionRenderService& renderer, double frame_rate)
    : renderer_(renderer), frame_rate_(frame_rate) {}

  CueImportApplicationResult import_content(
    const std::string& content,
    const std::string& source_path,
    const std::optional<core::ColumnMapping>& mapping,
    const SessionRenderOptions& options);

private:
  SessionRenderService& renderer_;
  double frame_rate_ = 30.0;
};

} // namespace reaadr::reaper
