#include "cue_import_application_service.hpp"

#include <algorithm>
#include <cctype>
#include <set>

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
    if (!repository_) {
      result.error = "Native selected import requires a canonical session repository.";
      return result;
    }

    std::vector<core::Fields> selected;
    for (const auto& cue : result.imported.cues) {
      const auto found = cue.find("character");
      if (found != cue.end() && std::find(characters.begin(), characters.end(), found->second) != characters.end())
        selected.push_back(cue);
    }
    if (selected.empty()) {
      result.error = "No cues remain after applying the native import selection.";
      return result;
    }

    const auto loaded = repository_->load();
    if (!loaded) {
      result.error = core::session_load_error_message(loaded);
      return result;
    }

    // Selected-character import is additive. The Lua reference preserved the
    // existing session and refused to silently duplicate already-imported
    // material; replacing the canonical session with only the selected rows
    // would discard unrelated cues. Preserve the existing model and reject a
    // cue-key collision so an explicit update import is required instead.
    std::vector<core::Fields> merged = loaded.model.cues;
    std::set<std::string> existing_keys;
    for (const auto& cue : merged) existing_keys.insert(core::render_cue_key(cue));
    for (const auto& incoming : selected) {
      const std::string key = core::render_cue_key(incoming);
      if (existing_keys.count(key) != 0) {
        result.error = "Cue " + key +
          " already exists in the canonical session. Use Update Existing Import to replace imported cue data.";
        return result;
      }
      merged.push_back(incoming);
      existing_keys.insert(key);
    }
    result.imported.cues = std::move(merged);
  } else if (normalized_mode == "update") {
    if (!repository_) {
      result.error = "Native update import requires a canonical session repository.";
      return result;
    }
    if (characters.empty()) {
      result.error = "Native update import requires at least one selected character.";
      return result;
    }
    const auto loaded = repository_->load();
    if (!loaded) {
      result.error = core::session_load_error_message(loaded);
      return result;
    }

    // The reference workflow updates only the characters chosen by the user.
    // Require an explicit selection and filter the incoming revision before
    // merging so an empty selection can never become an accidental all-sheet
    // update and unrelated characters cannot be added or replaced.
    std::vector<core::Fields> incoming_cues;
    for (const auto& cue : result.imported.cues) {
      const auto found = cue.find("character");
      if (found != cue.end() &&
          std::find(characters.begin(), characters.end(), found->second) != characters.end()) {
        incoming_cues.push_back(cue);
      }
    }
    if (incoming_cues.empty()) {
      result.error = "No cues remain after applying the native update selection.";
      return result;
    }

    std::vector<core::Fields> merged = loaded.model.cues;
    for (const auto& incoming : incoming_cues) {
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
