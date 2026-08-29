#pragma once

#include <optional>
#include <string>
#include <vector>

namespace reaadr::core {

// Canonicalizes the known cue workflow states while preserving an unknown
// trimmed value. Preserving unknown values is important for studio-specific
// statuses and matches the transitional Lua behavior.
std::string normalize_status(const std::string& status);

// Generates the deterministic IDs used for characters, tracks, regions, and
// import snapshots. Parts are joined with '|', matching Lua's stable_id helper.
std::string stable_id(const std::string& prefix, const std::vector<std::string>& parts);

struct TimecodeParseResult {
  std::optional<double> seconds;
  std::string error;

  explicit operator bool() const { return seconds.has_value(); }
};

// Supports seconds, HH:MM:SS:FF, HH:MM:SS[.fraction], and
// MM:SS[.fraction]. This is non-drop-frame timecode, matching the current Lua
// importer; drop-frame support must be introduced as an explicit format change.
TimecodeParseResult parse_timecode(const std::string& value, double frame_rate = 24.0);
std::string format_timecode(double seconds, double frame_rate = 24.0);

} // namespace reaadr::core
