#include "cue_import.hpp"

#include "domain_utils.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <set>
#include <sstream>
#include <utility>

namespace reaadr::core {
namespace {

using Aliases = std::map<std::string, std::vector<std::string>>;

const Aliases kFieldAliases = {
  {"cue_id", {"cue_id", "cue_number", "cue_num", "cue_no", "cue", "id", "number"}},
  {"character", {"character", "char", "actor", "speaker", "performer", "talent", "role"}},
  {"start", {"start", "start_time", "start_timecode", "timecode", "tc", "in_time", "in_timecode", "in"}},
  {"end", {"end", "end_time", "end_timecode", "out_time", "out_timecode", "out"}},
  {"line", {"line", "dialogue", "dialog", "text", "script"}},
  {"notes", {"notes", "note"}},
  {"direction", {"direction", "performance_direction", "perf_direction"}},
  {"cue_type", {"cue_type", "type", "category"}},
  {"status", {"status", "cue_status"}},
};

const std::vector<std::string> kRequiredFields = {"cue_id", "character", "start", "end"};

bool is_ascii_space(unsigned char byte)
{
  return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f' || byte == '\v';
}

std::string trim(const std::string& value)
{
  std::size_t first = 0;
  while (first < value.size() && is_ascii_space(static_cast<unsigned char>(value[first]))) ++first;
  std::size_t last = value.size();
  while (last > first && is_ascii_space(static_cast<unsigned char>(value[last - 1]))) --last;
  return value.substr(first, last - first);
}

std::string lowercase_ascii(std::string value)
{
  for (char& byte : value) {
    if (byte >= 'A' && byte <= 'Z') byte = static_cast<char>(byte - 'A' + 'a');
  }
  return value;
}

std::string normalize_header(const std::string& value)
{
  const std::string lowered = lowercase_ascii(trim(value));
  std::string normalized;
  bool previous_separator = false;
  for (unsigned char byte : lowered) {
    const bool word =
      (byte >= 'a' && byte <= 'z') ||
      (byte >= '0' && byte <= '9');
    if (word) {
      normalized.push_back(static_cast<char>(byte));
      previous_separator = false;
    } else if (!normalized.empty() && !previous_separator) {
      normalized.push_back('_');
      previous_separator = true;
    }
  }
  while (!normalized.empty() && normalized.back() == '_') normalized.pop_back();
  return normalized;
}

std::string sanitize_cue_id(const std::string& value)
{
  std::string sanitized;
  bool in_whitespace = false;
  for (unsigned char byte : trim(value)) {
    if (is_ascii_space(byte)) {
      if (!in_whitespace) sanitized.push_back('_');
      in_whitespace = true;
      continue;
    }
    in_whitespace = false;
    const bool allowed =
      (byte >= 'a' && byte <= 'z') ||
      (byte >= 'A' && byte <= 'Z') ||
      (byte >= '0' && byte <= '9') ||
      byte == '_' || byte == '-' || byte == '.';
    if (allowed) sanitized.push_back(static_cast<char>(byte));
  }
  return sanitized;
}

std::vector<std::string> split_delimited_line(const std::string& line, char delimiter)
{
  std::vector<std::string> fields;
  std::string field;
  bool in_quotes = false;
  for (std::size_t index = 0; index < line.size(); ++index) {
    const char byte = line[index];
    const char next = index + 1 < line.size() ? line[index + 1] : '\0';
    if (byte == '"') {
      if (in_quotes && next == '"') {
        field.push_back('"');
        ++index;
      } else {
        in_quotes = !in_quotes;
      }
    } else if (byte == delimiter && !in_quotes) {
      fields.push_back(field);
      field.clear();
    } else {
      field.push_back(byte);
    }
  }
  fields.push_back(field);
  return fields;
}

std::string extension_for_path(const std::string& path)
{
  const std::size_t slash = path.find_last_of("/\\");
  const std::size_t dot = path.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return {};
  return lowercase_ascii(path.substr(dot + 1));
}

char detect_delimiter(const std::string& content, const std::string& path)
{
  const std::string extension = extension_for_path(path);
  if (extension == "tsv" || extension == "tab") return '\t';

  std::size_t start = 0;
  while (start < content.size() && (content[start] == '\n' || content[start] == '\r')) ++start;
  const std::size_t end = content.find_first_of("\n\r", start);
  const std::string first_line = content.substr(start, end - start);
  const auto tabs = static_cast<std::size_t>(std::count(first_line.begin(), first_line.end(), '\t'));
  const auto commas = static_cast<std::size_t>(std::count(first_line.begin(), first_line.end(), ','));
  return tabs > commas ? '\t' : ',';
}

std::string normalize_newlines(const std::string& content)
{
  std::string normalized;
  normalized.reserve(content.size());
  for (std::size_t index = 0; index < content.size(); ++index) {
    if (content[index] == '\r') {
      if (index + 1 < content.size() && content[index + 1] == '\n') ++index;
      normalized.push_back('\n');
    } else {
      normalized.push_back(content[index]);
    }
  }
  return normalized;
}

ColumnMapping normalize_mapping(const ColumnMapping& mapping)
{
  ColumnMapping normalized;
  for (const auto& [field, source] : mapping) {
    const std::string header = normalize_header(source);
    if (!header.empty()) normalized[field] = header;
  }
  return normalized;
}

std::string row_value(const DelimitedRow& row, const ColumnMapping& mapping, const char* field)
{
  const auto mapped = mapping.find(field);
  if (mapped == mapping.end()) return {};
  const auto value = row.values.find(mapped->second);
  return value == row.values.end() ? std::string() : value->second;
}

std::string number_string(double value)
{
  std::ostringstream output;
  output << std::setprecision(14) << value;
  return output.str();
}

std::string join(const std::vector<std::string>& values, const char* separator)
{
  std::ostringstream output;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) output << separator;
    output << values[index];
  }
  return output.str();
}

} // namespace

TableParseResult parse_delimited_content(const std::string& content, const std::string& source_path)
{
  TableParseResult result;
  const std::string normalized_content = normalize_newlines(content);
  result.table.delimiter = detect_delimiter(normalized_content, source_path);
  result.table.delimiter_name = result.table.delimiter == '\t' ? "TSV" : "CSV";

  bool have_headers = false;
  int line_number = 0;
  std::size_t start = 0;
  while (start <= normalized_content.size()) {
    const std::size_t end = normalized_content.find('\n', start);
    std::string line = normalized_content.substr(start, end - start);
    ++line_number;

    // UTF-8 BOM removal is performed per physical line to mirror the current
    // Lua parser's anchored replacement behavior.
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEFU &&
        static_cast<unsigned char>(line[1]) == 0xBBU &&
        static_cast<unsigned char>(line[2]) == 0xBFU) {
      line.erase(0, 3);
    }

    if (!trim(line).empty()) {
      const std::vector<std::string> fields = split_delimited_line(line, result.table.delimiter);
      if (!have_headers) {
        have_headers = true;
        for (const std::string& header : fields) {
          result.table.raw_headers.push_back(trim(header));
          result.table.headers.push_back(normalize_header(header));
        }
      } else {
        DelimitedRow row;
        row.line_number = line_number;
        for (std::size_t index = 0; index < result.table.headers.size(); ++index) {
          const std::string value = index < fields.size() ? trim(fields[index]) : std::string();
          const std::string& header = result.table.headers[index];
          row.values[header] = value;
          row.raw_cells[header] = {
            value,
            result.table.raw_headers[index].empty() ? header : result.table.raw_headers[index],
          };
        }
        result.table.rows.push_back(std::move(row));
      }
    }

    if (end == std::string::npos) break;
    start = end + 1;
  }

  if (!have_headers) {
    result.error = TableParseError::empty;
    result.message = "Cue sheet is empty";
  }
  return result;
}

ColumnMapping default_column_mapping(const std::vector<std::string>& normalized_headers)
{
  const std::set<std::string> present(normalized_headers.begin(), normalized_headers.end());
  ColumnMapping mapping;
  for (const auto& [field, aliases] : kFieldAliases) {
    for (const std::string& alias : aliases) {
      if (present.count(alias) != 0) {
        mapping[field] = alias;
        break;
      }
    }
  }
  return mapping;
}

std::vector<std::string> missing_required_mapping(const ColumnMapping& mapping)
{
  std::vector<std::string> missing;
  for (const std::string& field : kRequiredFields) {
    const auto found = mapping.find(field);
    if (found == mapping.end() || found->second.empty()) missing.push_back(field);
  }
  return missing;
}

CueImportResult import_cues(const DelimitedTable& table,
                            double frame_rate,
                            const std::optional<ColumnMapping>& requested_mapping)
{
  CueImportResult result;
  result.mapping = normalize_mapping(requested_mapping ? *requested_mapping : default_column_mapping(table.headers));
  const std::vector<std::string> missing = missing_required_mapping(result.mapping);
  if (!missing.empty()) {
    result.error = CueImportError::missing_required_mapping;
    result.message = "Cue sheet is missing required column mapping(s):\n\n" + join(missing, "\n");
    return result;
  }

  std::set<std::string> seen_cue_keys;
  std::set<std::string> used_headers;
  for (const auto& [field, source] : result.mapping) {
    static_cast<void>(field);
    used_headers.insert(source);
  }

  for (const DelimitedRow& row : table.rows) {
    std::string cue_id = row_value(row, result.mapping, "cue_id");
    if (trim(cue_id).empty()) cue_id = std::to_string(result.cues.size() + 1);
    std::string character = row_value(row, result.mapping, "character");
    if (trim(character).empty()) character = "Unassigned";
    const std::string cue_key = sanitize_cue_id(cue_id);

    if (cue_key.empty()) {
      result.error = CueImportError::invalid_row;
      result.message = "Line " + std::to_string(row.line_number) + ": cue_id is required";
      return result;
    }
    if (seen_cue_keys.count(cue_key) != 0) {
      result.error = CueImportError::invalid_row;
      result.message = "Line " + std::to_string(row.line_number) + " cue " + cue_id + ": duplicate cue_id";
      return result;
    }

    const TimecodeParseResult start_time = parse_timecode(row_value(row, result.mapping, "start"), frame_rate);
    if (!start_time) {
      result.error = CueImportError::invalid_row;
      result.message = "Line " + std::to_string(row.line_number) + " cue " + cue_id + ": " + start_time.error;
      return result;
    }
    const TimecodeParseResult end_time = parse_timecode(row_value(row, result.mapping, "end"), frame_rate);
    if (!end_time) {
      result.error = CueImportError::invalid_row;
      result.message = "Line " + std::to_string(row.line_number) + " cue " + cue_id + ": " + end_time.error;
      return result;
    }
    if (*end_time.seconds <= *start_time.seconds) {
      result.error = CueImportError::invalid_row;
      result.message = "Line " + std::to_string(row.line_number) + " cue " + cue_id + ": end must be after start";
      return result;
    }

    Fields metadata;
    for (const auto& [header, raw] : row.raw_cells) {
      if (used_headers.count(header) == 0 && !raw.value.empty()) metadata[raw.label] = raw.value;
    }

    Fields cue = {
      {"id", trim(cue_id)},
      {"character", trim(character)},
      {"start_time", number_string(*start_time.seconds)},
      {"end_time", number_string(*end_time.seconds)},
      {"line", row_value(row, result.mapping, "line")},
      {"notes", row_value(row, result.mapping, "notes")},
      {"direction", row_value(row, result.mapping, "direction")},
      {"cue_type", row_value(row, result.mapping, "cue_type")},
      {"status", normalize_status(row_value(row, result.mapping, "status"))},
      {"source_line", std::to_string(row.line_number)},
      {"metadata", serialize_metadata(metadata)},
    };
    result.cues.push_back(std::move(cue));
    seen_cue_keys.insert(cue_key);
  }

  if (result.cues.empty()) {
    result.error = CueImportError::no_cues;
    result.message = "Cue sheet contains no cues";
  }
  return result;
}

} // namespace reaadr::core
