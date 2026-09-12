#include "dialogue_cue_generation.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace reaadr::core {
namespace {

std::string number_string(double value)
{
  std::ostringstream output;
  output << std::setprecision(14) << value;
  return output.str();
}

std::string padded_id(std::size_t index, std::size_t count)
{
  const std::size_t digits = std::to_string(std::max<std::size_t>(1, count)).size();
  const int width = static_cast<int>(std::max<std::size_t>(3, digits));
  std::ostringstream output;
  output << std::setw(width) << std::setfill('0') << index;
  return output.str();
}

} // namespace

std::vector<Fields> build_dialogue_cues(
  const std::vector<DialogueSegment>& segments,
  DialogueCueGenerationOptions options)
{
  if (options.character.empty()) options.character = "Unknown";

  std::vector<DialogueSegment> valid_segments;
  valid_segments.reserve(segments.size());
  for (const auto& segment : segments) {
    if (segment.end_time > segment.start_time) valid_segments.push_back(segment);
  }

  std::vector<Fields> cues;
  cues.reserve(valid_segments.size());
  for (std::size_t i = 0; i < valid_segments.size(); ++i) {
    const auto& segment = valid_segments[i];
    cues.push_back({
      {"id", padded_id(i + 1, valid_segments.size())},
      {"character", options.character},
      {"cue_type", "Dialogue"},
      {"start_time", number_string(segment.start_time)},
      {"end_time", number_string(segment.end_time)},
      {"line", ""},
      {"status", "Not Recorded"},
      {"notes", options.notes},
      {"source_line", std::to_string(i + 1)},
    });
  }
  return cues;
}

} // namespace reaadr::core
