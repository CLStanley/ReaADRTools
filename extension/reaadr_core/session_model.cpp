#include "session_model.hpp"

#include <array>
#include <cctype>
#include <sstream>
#include <utility>

namespace reaadr::core {
namespace {

const std::string kEmptyString;

// Lua writes only this allowlist for cue records. Keeping the same boundary
// prevents transient UI/import fields from leaking into project extstate.
constexpr std::array<const char*, 19> kCueFields = {
  "id",
  "character",
  "start_time",
  "end_time",
  "line",
  "notes",
  "direction",
  "cue_type",
  "source_line",
  "status",
  "script_id",
  "script_name",
  "script_revision",
  "import_timestamp",
  "metadata",
  "character_id",
  "region_id",
  "track_id",
  "session_cue_id",
};

constexpr std::array<const char*, 2> kSessionFields = {
  "session_id",
  "session_name",
};

constexpr std::array<const char*, 5> kTrackFields = {
  "track_id",
  "character_id",
  "track_type",
  "track_name",
  "assigned_cues",
};

bool is_unescaped_field_byte(unsigned char value)
{
  // This mirrors Lua's encode_cache_field: readable ASCII identifiers and
  // spaces remain literal; delimiters, control bytes, and UTF-8 are escaped.
  const bool ascii_alphanumeric =
    (value >= 'a' && value <= 'z') ||
    (value >= 'A' && value <= 'Z') ||
    (value >= '0' && value <= '9');
  return ascii_alphanumeric || value == '-' || value == '_' || value == '.' || value == ' ';
}

bool is_blank_metadata_value(const std::string& value)
{
  for (unsigned char byte : value) {
    if (std::isspace(byte) == 0) return false;
  }
  return true;
}

int hex_value(char value)
{
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

Fields parse_pairs(const std::string& line)
{
  Fields fields;
  std::string::size_type token_start = line.find('\t');
  while (token_start != std::string::npos) {
    ++token_start;
    const std::string::size_type token_end = line.find('\t', token_start);
    const std::string token = line.substr(token_start, token_end - token_start);
    const std::string::size_type equals = token.find('=');
    if (equals != std::string::npos) {
      const std::string key = decode_field(token.substr(0, equals));
      if (!key.empty()) fields[key] = decode_field(token.substr(equals + 1));
    }
    token_start = token_end;
  }
  return fields;
}

std::string serialize_pairs(const char* record_type, const Fields& fields)
{
  std::ostringstream output;
  output << record_type;
  // std::map iteration gives the deterministic key ordering used by Lua's
  // serialize_pairs after table.sort.
  for (const auto& [key, value] : fields) {
    output << '\t' << encode_field(key) << '=' << encode_field(value);
  }
  return output.str();
}

Fields select_fields(const Fields& source, const char* const* names, std::size_t count)
{
  Fields selected;
  for (std::size_t index = 0; index < count; ++index) {
    const auto found = source.find(names[index]);
    if (found != source.end()) selected.emplace(found->first, found->second);
  }
  return selected;
}

void append_record(std::vector<std::string>& lines, const char* type, const Fields& fields)
{
  lines.push_back(serialize_pairs(type, fields));
}

} // namespace

const std::string& SessionModel::session_id() const
{
  const auto found = session.find("session_id");
  return found == session.end() ? kEmptyString : found->second;
}

std::string encode_field(const std::string& value)
{
  constexpr char kHex[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(value.size());
  for (unsigned char byte : value) {
    if (is_unescaped_field_byte(byte)) {
      encoded.push_back(static_cast<char>(byte));
    } else {
      encoded.push_back('%');
      encoded.push_back(kHex[(byte >> 4U) & 0x0FU]);
      encoded.push_back(kHex[byte & 0x0FU]);
    }
  }
  return encoded;
}

std::string decode_field(const std::string& value)
{
  std::string decoded;
  decoded.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] == '%' && index + 2 < value.size()) {
      const int high = hex_value(value[index + 1]);
      const int low = hex_value(value[index + 2]);
      if (high >= 0 && low >= 0) {
        decoded.push_back(static_cast<char>((high << 4) | low));
        index += 2;
        continue;
      }
    }
    // Malformed escape sequences are intentionally preserved. Treating them
    // as fatal would reject models that the previous Lua codec could load.
    decoded.push_back(value[index]);
  }
  return decoded;
}

std::string serialize_metadata(const Fields& metadata)
{
  std::ostringstream output;
  bool first = true;
  for (const auto& [key, value] : metadata) {
    // The Lua serializer omits whitespace-only metadata. Matching that detail
    // avoids creating diffs when C++ becomes the writer for existing models.
    if (is_blank_metadata_value(value)) continue;
    if (!first) output << '&';
    output << encode_field(key) << '=' << encode_field(value);
    first = false;
  }
  return output.str();
}

Fields deserialize_metadata(const std::string& value)
{
  Fields metadata;
  std::string::size_type start = 0;
  while (start < value.size()) {
    const std::string::size_type end = value.find('&', start);
    const std::string pair = value.substr(start, end - start);
    const std::string::size_type equals = pair.find('=');
    if (equals != std::string::npos) {
      const std::string key = decode_field(pair.substr(0, equals));
      if (!key.empty()) metadata[key] = decode_field(pair.substr(equals + 1));
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return metadata;
}

ParseResult parse_session_model(const std::string& value)
{
  ParseResult result;
  if (value.empty()) {
    result.error = ParseError::empty_model;
    return result;
  }

  std::string::size_type start = 0;
  while (start <= value.size()) {
    const std::string::size_type end = value.find('\n', start);
    const std::string line = value.substr(start, end - start);
    if (!line.empty()) {
      const std::string::size_type tab = line.find('\t');
      const std::string record_type = line.substr(0, tab);
      Fields fields = parse_pairs(line);

      if (record_type == "session") result.model.session = std::move(fields);
      else if (record_type == "project_metadata") result.model.project_metadata = std::move(fields);
      else if (record_type == "timecode") result.model.timecode = std::move(fields);
      else if (record_type == "state") result.model.state = std::move(fields);
      else if (record_type == "dirty") {
        // Dirty flags are represented as repeated records in extstate but are
        // easier and safer for callers to mutate as one keyed collection.
        const auto key = fields.find("key");
        const auto item = fields.find("value");
        result.model.dirty_flags[key == fields.end() ? "" : key->second] = item == fields.end() ? "" : item->second;
      }
      else if (record_type == "script") result.model.scripts.push_back(std::move(fields));
      else if (record_type == "character") result.model.characters.push_back(std::move(fields));
      else if (record_type == "cue") result.model.cues.push_back(std::move(fields));
      else if (record_type == "track") result.model.tracks.push_back(std::move(fields));
      else if (record_type == "region") result.model.regions.push_back(std::move(fields));
      else if (record_type == "import") result.model.imports.push_back(std::move(fields));
      else {
        // Do not parse or normalize future records: verbatim preservation is
        // what makes a read/modify/write cycle forward-compatible.
        result.model.unknown_records.push_back(line);
      }
    }

    if (end == std::string::npos) break;
    start = end + 1;
  }

  if (result.model.session_id().empty()) result.error = ParseError::missing_session_id;
  return result;
}

std::string serialize_session_model(const SessionModel& model)
{
  // Record-family order mirrors ReaADR_Core_Persistence.lua. Stable output is
  // useful for project diffs, snapshots, and Lua/C++ parity fixtures.
  std::vector<std::string> lines;
  append_record(lines, "session", select_fields(model.session, kSessionFields.data(), kSessionFields.size()));
  append_record(lines, "project_metadata", model.project_metadata);
  append_record(lines, "timecode", model.timecode);
  append_record(lines, "state", model.state);

  for (const auto& [key, value] : model.dirty_flags) {
    append_record(lines, "dirty", {{"key", key}, {"value", value}});
  }
  for (const Fields& fields : model.scripts) append_record(lines, "script", fields);
  for (const Fields& fields : model.characters) append_record(lines, "character", fields);
  for (const Fields& fields : model.cues) {
    append_record(lines, "cue", select_fields(fields, kCueFields.data(), kCueFields.size()));
  }
  for (const Fields& fields : model.tracks) {
    append_record(lines, "track", select_fields(fields, kTrackFields.data(), kTrackFields.size()));
  }
  for (const Fields& fields : model.regions) append_record(lines, "region", fields);
  for (const Fields& fields : model.imports) append_record(lines, "import", fields);
  lines.insert(lines.end(), model.unknown_records.begin(), model.unknown_records.end());

  std::ostringstream output;
  for (std::size_t index = 0; index < lines.size(); ++index) {
    if (index != 0) output << '\n';
    output << lines[index];
  }
  return output.str();
}

const char* parse_error_message(ParseError error)
{
  switch (error) {
    case ParseError::none: return "";
    case ParseError::empty_model: return "No ADR session model was provided.";
    case ParseError::missing_session_id: return "The ADR session model has no session ID.";
  }
  return "The ADR session model is invalid.";
}

} // namespace reaadr::core
