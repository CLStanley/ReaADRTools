#pragma once

#include "reaadr_core/cue_import.hpp"

#include <optional>
#include <string>
#include <vector>

namespace reaadr::reaper {

struct CueImportMappingParseResult {
  std::optional<core::ColumnMapping> mapping;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

std::string normalize_cue_import_mode(std::string value);
std::vector<std::string> parse_cue_import_characters(const std::string& serialized);
CueImportMappingParseResult parse_cue_import_mapping(
  const std::string& serialized,
  bool ignore_invalid_entries = false);

} // namespace reaadr::reaper
