#pragma once

#include "cue_import_application_service.hpp"
#include "cue_import_request.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace reaadr::reaper {

struct CueImportSourceResult {
  std::string source_path;
  std::string content;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct CueImportPreviewSummaryOptions {
  std::string mode = "all";
  std::vector<std::string> characters;
};

CueImportSourceResult load_cue_import_source(const std::string& source_path);
std::string format_cue_import_preview_summary(
  const CueImportPreviewResult& preview,
  const std::optional<core::ColumnMapping>& requested_mapping,
  const CueImportPreviewSummaryOptions& options = {});

struct CueImportHostUi {
  std::function<bool(std::string& path)> choose_source;
  std::function<bool(const std::string& title, const std::string& caption,
                     std::string& value)> prompt;
  std::function<void(const std::string& title, const std::string& message)> message;
};

struct CueImportWorkflowRequest {
  std::string serialized_mapping;
  bool preview_only = false;
  std::string mode = "all";
  std::string serialized_characters;
  std::string persisted_mapping;
};

struct CueImportWorkflowCallbacks {
  std::function<CueImportPreviewResult(
    const std::string&, const std::string&,
    const std::optional<core::ColumnMapping>&)> preview;
  std::function<CueImportApplicationResult(
    const std::string&, const std::string&,
    const std::optional<core::ColumnMapping>&, const std::string&,
    const std::vector<std::string>&)> import;
};

bool run_cue_import_workflow(
  const CueImportWorkflowRequest& request,
  const CueImportHostUi& ui,
  const CueImportWorkflowCallbacks& callbacks,
  std::string* applied_mapping = nullptr);

} // namespace reaadr::reaper
