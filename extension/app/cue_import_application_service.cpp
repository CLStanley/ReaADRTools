#include "cue_import_application_service.hpp"

#include <algorithm>

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
  if (mode == "selected") {
    std::vector<core::Fields> filtered;
    for (const auto& cue : result.imported.cues) {
      const auto found = cue.find("character");
      if (found != cue.end() && std::find(characters.begin(), characters.end(), found->second) != characters.end())
        filtered.push_back(cue);
    }
    result.imported.cues = std::move(filtered);
  } else if (mode != "all") {
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
