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

  // Keep revisions of the same source script under one stable identity. This
  // mirrors the Lua reference's normalized-script-name fallback when studio
  // metadata does not provide an explicit script ID.
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

inline ScriptIdentity derive_native_script_identity(const std::string& source_path)
{
  ScriptIdentity identity;
  identity.script_name = script_name_from_path(source_path);
  std::string stable = normalized_script_identity_text(identity.script_name);
  if (stable.empty()) stable = normalized_script_identity_text(script_path_basename(source_path));
  identity.script_id = "script_" + native_script_hash(stable);
  return identity;
}

inline void annotate_imported_cues(std::vector<core::Fields>& cues, const ScriptIdentity& identity)
{
  for (auto& cue : cues) {
    cue["script_id"] = identity.script_id;
    cue["script_name"] = identity.script_name;
  }
}

inline std::string cue_field(const core::Fields& cue, const char* key)
{
  const auto found = cue.find(key);
  return found == cue.end() ? std::string{} : found->second;
}

} // namespace reaadr::reaper
