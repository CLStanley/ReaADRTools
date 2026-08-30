#include "domain_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <regex>
#include <sstream>

namespace reaadr::core {
namespace {

std::string trim_ascii(const std::string& value)
{
  const auto is_space = [](unsigned char byte) {
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f' || byte == '\v';
  };
  std::size_t first = 0;
  while (first < value.size() && is_space(static_cast<unsigned char>(value[first]))) ++first;
  std::size_t last = value.size();
  while (last > first && is_space(static_cast<unsigned char>(value[last - 1]))) --last;
  return value.substr(first, last - first);
}

std::string lowercase_ascii(std::string value)
{
  for (char& byte : value) {
    if (byte >= 'A' && byte <= 'Z') byte = static_cast<char>(byte - 'A' + 'a');
  }
  return value;
}

std::string collapse_status_separators(const std::string& value)
{
  std::string collapsed;
  bool previous_was_separator = false;
  for (unsigned char byte : value) {
    const bool separator =
      byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f' || byte == '\v' ||
      byte == '_' || byte == '-';
    if (separator) {
      if (!collapsed.empty() && !previous_was_separator) collapsed.push_back(' ');
    } else {
      collapsed.push_back(static_cast<char>(byte));
    }
    previous_was_separator = separator;
  }
  if (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();
  return collapsed;
}

bool parse_number(const std::string& value, double& output)
{
  char* end = nullptr;
  output = std::strtod(value.c_str(), &end);
  return end && end != value.c_str() && *end == '\0' && std::isfinite(output);
}

double capture_number(const std::ssub_match& capture)
{
  return std::strtod(capture.str().c_str(), nullptr);
}

} // namespace

std::string normalize_status(const std::string& status)
{
  const std::string trimmed = trim_ascii(status);
  if (trimmed.empty()) return "Not Recorded";

  const std::string normalized = collapse_status_separators(lowercase_ascii(trimmed));
  if (normalized == "not recorded" || normalized == "notrecorded" || normalized == "pending") {
    return "Not Recorded";
  }
  if (normalized == "in progress" || normalized == "inprogress" || normalized == "recording") {
    return "In Progress";
  }
  if (normalized == "recorded") return "Recorded";
  if (normalized == "needs review" || normalized == "review" || normalized == "needsreview") {
    return "Needs Review";
  }
  if (normalized == "approved") return "Approved";
  if (normalized == "needs retake" || normalized == "retake" || normalized == "needsretake") {
    return "Needs Retake";
  }
  return trimmed;
}

std::string stable_id(const std::string& prefix, const std::vector<std::string>& parts)
{
  std::string text;
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (index != 0) text.push_back('|');
    text += parts[index];
  }

  // This deliberately mirrors the dependency-free Lua fallback used by the
  // project today. uint32_t overflow supplies the modulo-2^32 behavior.
  std::uint32_t hash = 2166136261U;
  for (unsigned char byte : text) {
    hash = (hash + byte) * 16777619U;
  }

  std::ostringstream output;
  output << prefix << '_' << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << hash;
  return output.str();
}

std::string sanitize_token(const std::string& value)
{
  const std::string cleaned = trim_ascii(value);
  std::string result;
  bool in_whitespace = false;
  for (unsigned char byte : cleaned) {
    const bool whitespace =
      byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f' || byte == '\v';
    if (whitespace) {
      if (!result.empty() && !in_whitespace) result.push_back('_');
      in_whitespace = true;
      continue;
    }
    in_whitespace = false;
    // Lua's %w is ASCII in the REAPER environment used by this project.
    const bool alphanumeric =
      (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
      (byte >= '0' && byte <= '9');
    if (alphanumeric || byte == '_' || byte == '-' || byte == '.') {
      result.push_back(static_cast<char>(byte));
    }
  }
  while (!result.empty() && result.back() == '_') result.pop_back();
  return result;
}

TimecodeParseResult parse_timecode(const std::string& value, double frame_rate)
{
  const std::string trimmed = trim_ascii(value);
  if (trimmed.empty()) return {std::nullopt, "Missing time value"};

  double seconds = 0.0;
  if (parse_number(trimmed, seconds)) return {seconds, {}};

  std::smatch match;
  static const std::regex frames_pattern(R"(^(\d+):(\d+):(\d+):(\d+)$)");
  if (std::regex_match(trimmed, match, frames_pattern)) {
    const double safe_frame_rate = std::isfinite(frame_rate) && frame_rate != 0.0 ? frame_rate : 24.0;
    seconds = (capture_number(match[1]) * 3600.0) +
      (capture_number(match[2]) * 60.0) +
      capture_number(match[3]) +
      (capture_number(match[4]) / safe_frame_rate);
    return {seconds, {}};
  }

  static const std::regex hours_pattern(R"(^(\d+):(\d+):(\d+\.?\d*)$)");
  if (std::regex_match(trimmed, match, hours_pattern)) {
    seconds = (capture_number(match[1]) * 3600.0) +
      (capture_number(match[2]) * 60.0) +
      capture_number(match[3]);
    return {seconds, {}};
  }

  static const std::regex minutes_pattern(R"(^(\d+):(\d+\.?\d*)$)");
  if (std::regex_match(trimmed, match, minutes_pattern)) {
    seconds = (capture_number(match[1]) * 60.0) + capture_number(match[2]);
    return {seconds, {}};
  }

  return {std::nullopt, "Unsupported time format: " + trimmed};
}

std::string format_timecode(double seconds, double frame_rate)
{
  if (!std::isfinite(seconds)) seconds = 0.0;
  if (!std::isfinite(frame_rate)) frame_rate = 24.0;
  const double rounded_frame_rate_value = (std::max)(1.0, std::floor(frame_rate + 0.5));
  const std::uint64_t rounded_frame_rate =
    rounded_frame_rate_value >= static_cast<double>(std::numeric_limits<std::uint64_t>::max())
      ? std::numeric_limits<std::uint64_t>::max()
      : static_cast<std::uint64_t>(rounded_frame_rate_value);
  const double nonnegative_seconds = (std::max)(0.0, seconds);
  const double raw_frames = std::floor(nonnegative_seconds * static_cast<double>(rounded_frame_rate) + 0.5);
  const std::uint64_t total_frames = raw_frames >= static_cast<double>(std::numeric_limits<std::uint64_t>::max())
    ? std::numeric_limits<std::uint64_t>::max()
    : static_cast<std::uint64_t>(raw_frames);

  const std::uint64_t frames = total_frames % rounded_frame_rate;
  const std::uint64_t total_seconds = total_frames / rounded_frame_rate;
  const std::uint64_t display_seconds = total_seconds % 60U;
  const std::uint64_t total_minutes = total_seconds / 60U;
  const std::uint64_t display_minutes = total_minutes % 60U;
  const std::uint64_t hours = total_minutes / 60U;

  std::ostringstream output;
  output << std::setfill('0')
         << std::setw(2) << hours << ':'
         << std::setw(2) << display_minutes << ':'
         << std::setw(2) << display_seconds << ':'
         << std::setw(2) << frames;
  return output.str();
}

} // namespace reaadr::core
