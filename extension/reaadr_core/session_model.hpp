#pragma once

#include <map>
#include <string>
#include <vector>

namespace reaadr::core {

// Model fields stay string-valued because adr_session_model_v1 is a persisted,
// text-based compatibility boundary. Domain services may add typed accessors,
// but the codec must not silently reinterpret values while loading a project.
using Fields = std::map<std::string, std::string>;

// In-memory representation of every v1 record family. Vectors preserve record
// order, while Fields uses sorted keys to match the canonical Lua serializer.
struct SessionModel {
  Fields session;
  Fields project_metadata;
  Fields timecode;
  Fields state;
  Fields dirty_flags;
  std::vector<Fields> scripts;
  std::vector<Fields> characters;
  std::vector<Fields> cues;
  std::vector<Fields> tracks;
  std::vector<Fields> regions;
  std::vector<Fields> imports;
  // Unknown record types are retained verbatim so a newer project can pass
  // through this codec without losing fields this version does not understand.
  std::vector<std::string> unknown_records;

  const std::string& session_id() const;
};

enum class ParseError {
  none,
  empty_model,
  missing_session_id,
};

struct ParseResult {
  SessionModel model;
  ParseError error = ParseError::none;

  explicit operator bool() const { return error == ParseError::none; }
};

// Percent encoding is part of the on-disk v1 contract, not URL encoding. Keep
// these functions compatible with encode_cache_field in ReaADR_Core.lua.
std::string encode_field(const std::string& value);
std::string decode_field(const std::string& value);

// Metadata is nested inside a model field as its own encoded key/value list.
std::string serialize_metadata(const Fields& metadata);
Fields deserialize_metadata(const std::string& value);

// Parse/serialize operate only on strings and deliberately have no REAPER SDK
// dependency, allowing project compatibility to be tested outside REAPER.
ParseResult parse_session_model(const std::string& value);
std::string serialize_session_model(const SessionModel& model);
const char* parse_error_message(ParseError error);

} // namespace reaadr::core
