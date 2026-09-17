#include "cue_import_application_service.hpp"
#include "script_identity.hpp"

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

  const ScriptIdentity script = derive_native_script_identity(source_path, result.imported.cues);
  annotate_imported_cues(result.imported.cues, script);

  const std::string normalized_mode = normalize_import_mode(mode.empty() ? "all" : mode);
  if (normalized_mode == "all") {
    if (!repository_) {
      result.error = "Native full import requires a canonical session repository.";
      return result;
    }
    const auto loaded = repository_->load();
    if (!loaded) {
      result.error = core::session_load_error_message(loaded);
      return result;
    }

    // Import Entire Script is additive once a canonical session exists. Never
    // replace unrelated scripts merely because the caller selected the full
    // incoming sheet. A second import of the same script is a revision and must
    // go through Update Existing Import so stale cues can be removed safely.
    for (const auto& existing : loaded.model.cues) {
      if (cue_field(existing, "script_id") == script.script_id) {
        result.error = "This script is already present in the canonical session. Use Update Existing Import for a revision.";
        return result;
      }
    }

    std::vector<core::Fields> merged = loaded.model.cues;
    std::set<std::string> existing_keys;
    for (const auto& cue : merged) existing_keys.insert(core::render_cue_key(cue));
    for (const auto& incoming : result.imported.cues) {
      const std::string key = core::render_cue_key(incoming);
      if (existing_keys.count(key) != 0) {
        result.error = "Cue " + key +
          " already exists in the canonical session. Resolve the duplicate cue ID before importing this script.";
        return result;
      }
      merged.push_back(incoming);
      existing_keys.insert(key);
    }
    result.imported.cues = std::move(merged);
  } else if (normalized_mode == "selected") {
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

    for (const auto& existing : loaded.model.cues) {
      if (cue_field(existing, "script_id") != script.script_id) continue;
      const std::string existing_character = cue_field(existing, "character");
      if (std::find(characters.begin(), characters.end(), existing_character) != characters.end()) {
        result.error = "Character " + existing_character +
          " is already imported from this script. Use Update Existing Import instead.";
        return result;
      }
    }

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

    std::vector<core::Fields> merged;
    merged.reserve(loaded.model.cues.size() + incoming_cues.size());
    for (const auto& existing : loaded.model.cues) {
      const bool same_script = cue_field(existing, "script_id") == script.script_id;
      const std::string existing_character = cue_field(existing, "character");
      const bool selected_character =
        std::find(characters.begin(), characters.end(), existing_character) != characters.end();
      if (!(same_script && selected_character)) merged.push_back(existing);
    }
    merged.insert(merged.end(), incoming_cues.begin(), incoming_cues.end());
    result.imported.cues = std::move(merged);
  } else {
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
