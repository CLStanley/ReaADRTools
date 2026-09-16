#pragma once

#include "reaadr_core/session_model.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace reaadr::reaper {

struct ScriptIdentity {
  std::string script_id;
  std::string script_name;
  std::string script_revision;
};

inline std::string script_path_basename(const std::string& path)
{
  const std::size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

inline std::string script_name_from_path(const std::string& path)
{
  std::string name = script_path_basename(path);
  const std::size_t dot = name.find_last_of('.');
  if (dot != std::string::npos) name.erase(dot);

  std::string lowered = name;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });

  const std::string markers[] = {"_revision", "-revision", " revision", "_rev", "-rev", " rev"};
  for (const auto& marker : markers) {
    const std::size_t pos = lowered.rfind(marker);
    if (pos == std::string::npos) continue;
    std::size_t digits = pos + marker.size();
    while (digits < lowered.size() && (lowered[digits] == '_' || lowered[digits] == '-' || lowered[digits] == ' ')) ++digits;
    if (digits < lowered.size() && std::isdigit(static_cast<unsigned char>(lowered[digits]))) {
      std::size_t end = digits;
      while (end < lowered.size() && std::isdigit(static_cast<unsigned char>(lowered[end]))) ++end;
      if (end == lowered.size()) {
        name.erase(pos);
        break;
      }
    }
  }
  return name.empty() ? std::string("Imported Script") : name;
}

inline std::string normalized_script_identity_text(std::string value)
{
  std::string normalized;
  bool separator = false;
  for (unsigned char ch : value) {
    if (std::isalnum(ch)) {
      normalized.push_back(static_cast<char>(std::tolower(ch)));
      separator = false;
    } else if (!normalized.empty() && !separator) {
      normalized.push_back('_');
      separator = true;
    }
  }
  while (!normalized.empty() && normalized.back() == '_') normalized.pop_back();
  return normalized;
}

inline std::string native_script_hash(const std::string& value)
{
  std::uint32_t hash = 2166136261u;
  for (unsigned char ch : value) {
    hash ^= static_cast<std::uint32_t>(ch);
    hash *= 16777619u;
  }
  std::ostringstream output;
  output << std::hex << std::nouppercase << std::setfill('0') << std::setw(8) << hash;
  return output.str();
}

inline std::string imported_metadata_value(const core::Fields& cue, const char* key)
{
  const auto direct = cue.find(key);
  if (direct != cue.end() && !direct->second.empty()) return direct->second;

  const auto serialized = cue.find("metadata");
  if (serialized == cue.end() || serialized->second.empty()) return {};

  // Imported cue metadata uses the canonical session field codec: key=value
  // pairs separated by tabs with backslash escaping. Decode just enough here
  // to recover script identity without introducing a second public parser.
  std::string field;
  bool escaped = false;
  const std::string prefix = std::string(key) + "=";
  for (std::size_t index = 0; index <= serialized->second.size(); ++index) {
    const char ch = index < serialized->second.size() ? serialized->second[index] : '\t';
    if (escaped) {
      if (ch == 't') field.push_back('\t');
      else if (ch == 'n') field.push_back('\n');
      else if (ch == 'r') field.push_back('\r');
      else field.push_back(ch);
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '\t') {
      if (field.compare(0, prefix.size(), prefix) == 0) return field.substr(prefix.size());
      field.clear();
      continue;
    }
    field.push_back(ch);
  }
  return {};
}

inline ScriptIdentity derive_native_script_identity(const std::string& source_path,
                                                     const std::vector<core::Fields>& cues = {})
{
  ScriptIdentity identity;
  identity.script_name = script_name_from_path(source_path);

  if (!cues.empty()) {
    const std::string explicit_id = imported_metadata_value(cues.front(), "script_id");
    const std::string explicit_name = imported_metadata_value(cues.front(), "script_name");
    const std::string explicit_revision = imported_metadata_value(cues.front(), "revision_number");
    if (!explicit_id.empty()) identity.script_id = explicit_id;
    if (!explicit_name.empty()) identity.script_name = explicit_name;
    identity.script_revision = explicit_revision;
  }

  if (identity.script_id.empty()) {
    std::string stable = normalized_script_identity_text(identity.script_name);
    if (stable.empty()) stable = normalized_script_identity_text(script_path_basename(source_path));
    identity.script_id = "script_" + native_script_hash(stable);
  }
  return identity;
}

inline void annotate_imported_cues(std::vector<core::Fields>& cues, const ScriptIdentity& identity)
{
  for (auto& cue : cues) {
    cue["script_id"] = identity.script_id;
    cue["script_name"] = identity.script_name;
    if (!identity.script_revision.empty()) cue["script_revision"] = identity.script_revision;
  }
}

inline std::string cue_field(const core::Fields& cue, const char* key)
{
  const auto found = cue.find(key);
  return found == cue.end() ? std::string{} : found->second;
}

} // namespace reaadr::reaper
