#include "lane_assignment.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>

namespace reaadr::core {
namespace {

std::string field(const Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

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

bool parse_number(const std::string& value, double& output)
{
  const std::string cleaned = trim_ascii(value);
  char* end = nullptr;
  output = std::strtod(cleaned.c_str(), &end);
  return !cleaned.empty() && end && end != cleaned.c_str() && *end == '\0' && std::isfinite(output);
}

struct CueWindow {
  std::size_t input_index = 0;
  std::string id;
  std::string character;
  double start_time = 0.0;
  double end_time = 0.0;
};

} // namespace

LaneAssignmentResult assign_character_lanes(const std::vector<Fields>& cues, double preroll_seconds)
{
  LaneAssignmentResult result;
  if (!std::isfinite(preroll_seconds) || preroll_seconds < 0.0) {
    result.error = "Lane-assignment preroll must be a finite, non-negative number.";
    return result;
  }

  std::vector<CueWindow> windows;
  windows.reserve(cues.size());
  for (std::size_t index = 0; index < cues.size(); ++index) {
    CueWindow window;
    window.input_index = index;
    window.id = field(cues[index], "id");
    window.character = trim_ascii(field(cues[index], "character"));
    if (window.character.empty()) window.character = "Unassigned";
    if (!parse_number(field(cues[index], "start_time"), window.start_time) ||
        !parse_number(field(cues[index], "end_time"), window.end_time) ||
        window.end_time < window.start_time) {
      result.error = "Cue " + window.id + " has invalid lane-assignment timing.";
      return result;
    }
    windows.push_back(std::move(window));
  }

  std::stable_sort(windows.begin(), windows.end(), [](const CueWindow& left, const CueWindow& right) {
    if (left.start_time != right.start_time) return left.start_time < right.start_time;
    if (left.id != right.id) return left.id < right.id;
    return left.input_index < right.input_index;
  });

  result.lanes.assign(cues.size(), 1);
  std::map<std::string, std::vector<double>> lane_ends;
  for (const CueWindow& window : windows) {
    std::vector<double>& ends = lane_ends[window.character];
    const double cue_window_start = (std::max)(0.0, window.start_time - preroll_seconds);
    std::size_t lane_index = 0;
    while (lane_index < ends.size() && cue_window_start < ends[lane_index]) ++lane_index;
    if (lane_index == ends.size()) ends.push_back(window.end_time);
    else ends[lane_index] = (std::max)(ends[lane_index], window.end_time);
    result.lanes[window.input_index] = static_cast<int>(lane_index + 1);
  }
  return result;
}

} // namespace reaadr::core
