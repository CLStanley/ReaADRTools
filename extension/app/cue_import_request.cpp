#include "cue_import_request.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace reaadr::reaper {
namespace {

std::string trim(std::string value)
{
  const auto first = value.find_first_not_of(" \t\r\n");
  const auto last = value.find_last_not_of(" \t\r\n");
  return first == std::string::npos ? std::string() : value.substr(first, last - first + 1);
}

} // namespace

std::string normalize_cue_import_mode(std::string value)
{
  value = trim(std::move(value));
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });

  if (value.empty() || value == "1" || value == "all" ||
      value == "import entire script" || value == "import entire sheet") {
    return "all";
  }
  if (value == "2" || value == "character" || value == "characters" ||
      value == "selected" || value == "import selected characters" ||
      value == "add selected characters") {
    return "selected";
  }
  if (value == "3" || value == "update" || value == "merge" ||
      value == "update existing import" ||
      value == "update already imported characters") {
    return "update";
  }
  return value;
}

std::vector<std::string> parse_cue_import_characters(const std::string& serialized)
{
  std::vector<std::string> characters;
  std::stringstream values(serialized);
  std::string value;
  while (std::getline(values, value, ';')) {
    value = trim(std::move(value));
    if (!value.empty()) characters.push_back(std::move(value));
  }
  return characters;
}

CueImportMappingParseResult parse_cue_import_mapping(
  const std::string& serialized,
  bool ignore_invalid_entries)
{
  CueImportMappingParseResult result;
  if (trim(serialized).empty()) return result;

  core::ColumnMapping mapping;
  std::stringstream entries(serialized);
  std::string entry;
  while (std::getline(entries, entry, ';')) {
    const std::size_t equals = entry.find('=');
    if (equals == std::string::npos) {
      if (ignore_invalid_entries) continue;
      result.error = "Mappings must use key=column pairs separated by semicolons.";
      return result;
    }

    const std::string key = trim(entry.substr(0, equals));
    const std::string column = trim(entry.substr(equals + 1));
    if (key.empty() || column.empty()) {
      if (ignore_invalid_entries) continue;
      result.error = "Mappings cannot contain empty keys or columns.";
      return result;
    }
    mapping[key] = column;
  }

  if (!mapping.empty()) result.mapping = std::move(mapping);
  return result;
}

} // namespace reaadr::reaper
