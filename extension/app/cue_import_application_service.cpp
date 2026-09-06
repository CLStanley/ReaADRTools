#include "cue_import_application_service.hpp"

#include <algorithm>
#include <cctype>

namespace {

std::string normalize_import_mode(std::string value)
{
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
    return !std::isspace(ch);
  }));
  value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
    return !std::isspace(ch);
  }).base(), value.end());
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (value == "1" || value == "all" || value == "import entire script" ||
      value == "import entire sheet") return "all";
  if (value == "2" || value == "selected" || value == "import selected characters" ||
      value == "add selected characters") return "selected";
  if (value == "3" || value == "update" || value == "update existing import" ||
      value == "update already imported characters") return "update";
  return value;
}

} // namespace

namespace reaadr::reaper {

CueImportApplicationResult CueImportApplicationService::import_content(
  const std::string& content,
  const std::string& source_path,
  const std::optional<core::ColumnMapping>& mapping,
  const SessionRenderOptions& options,
  const std::string& mode,
  const std::vector<std::string>& characters)
{
  CueImportApplicationResult result;
  result.parsed = core::parse_delimited_content(content, source_path);
  if (!result.parsed) {
    result.error = result.parsed.message;
    return result;
  }
  result.imported = core::import_cues(result.parsed.table, frame_rate_, mapping);
  if (!result.imported) {
    result.error = result.imported.message;
    return result;
  }
  const std::string normalized_mode = normalize_import_mode(mode.empty() ? "all" : mode);
  if (normalized_mode == "selected") {
    std::vector<core::Fields> filtered;
    for (const auto& cue : result.imported.cues) {
      const auto found = cue.find("character");
      if (found != cue.end() && std::find(characters.begin(), characters.end(), found->second) != characters.end())
        filtered.push_back(cue);
    }
    result.imported.cues = std::move(filtered);
  } else if (normalized_mode == "update") {
    if (!repository_) {
      result.error = "Native update import requires a canonical session repository.";
      return result;
    }
    const auto loaded = repository_->load();
    if (!loaded) {
      result.error = core::session_load_error_message(loaded);
      return result;
    }
    std::vector<core::Fields> merged = loaded.model.cues;
    for (const auto& incoming : result.imported.cues) {
      const std::string key = core::render_cue_key(incoming);
      auto existing = std::find_if(merged.begin(), merged.end(), [&key](const core::Fields& cue) {
        return core::render_cue_key(cue) == key;
      });
      if (existing == merged.end()) merged.push_back(incoming);
      else *existing = incoming;
    }
    result.imported.cues = std::move(merged);
  } else if (normalized_mode != "all") {
    result.error = "Unsupported native import mode: " + mode;
    return result;
  }
  if (result.imported.cues.empty()) {
    result.error = "No cues remain after applying the native import selection.";
    return result;
  }
  result.rendered = renderer_.commit_and_render(result.imported.cues, options);
  if (!result.rendered) result.error = result.rendered.error;
  return result;
}

} // namespace reaadr::reaper
