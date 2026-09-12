#include "reaadr_core/dialogue_cue_generation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using reaadr::core::DialogueCueGenerationOptions;
using reaadr::core::DialogueSegment;
using reaadr::core::Fields;
using reaadr::core::build_dialogue_cues;

void require(bool condition, const std::string& message)
{
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

double field_number(const Fields& fields, const std::string& key)
{
  return std::stod(fields.at(key));
}

void test_dialogue_cue_shape()
{
  const std::vector<DialogueSegment> segments = {
    {1.25, 2.5},
    {5.0, 6.75},
  };
  const auto cues = build_dialogue_cues(segments);
  require(cues.size() == 2, "valid dialogue segments create cues");
  require(cues[0].at("id") == "001" && cues[1].at("id") == "002",
          "dialogue ids use Lua-compatible three-digit padding");
  require(cues[0].at("character") == "ADR", "default dialogue character is ADR");
  require(cues[0].at("cue_type") == "Dialogue", "dialogue cue type is canonical");
  require(cues[0].at("line").empty(), "detected dialogue starts with an empty line");
  require(cues[0].at("status") == "Not Recorded", "detected dialogue default status");
  require(cues[0].at("notes") == "Detected from selected media", "detection note preserved");
  require(cues[0].at("source_line") == "1", "source line starts at one");
  require(std::abs(field_number(cues[1], "end_time") - 6.75) < 1e-9,
          "dialogue timing preserved");
}

void test_invalid_segments_are_ignored()
{
  const std::vector<DialogueSegment> segments = {
    {1.0, 1.0},
    {4.0, 3.0},
    {5.0, 7.0},
  };
  const auto cues = build_dialogue_cues(segments);
  require(cues.size() == 1, "non-positive dialogue segments are ignored");
  require(cues[0].at("id") == "001", "ids are compacted after filtering invalid segments");
  require(cues[0].at("source_line") == "1", "source lines are compacted after filtering");
}

void test_character_and_notes_options()
{
  DialogueCueGenerationOptions options;
  options.character = "Alice";
  options.notes = "Automatic pass";
  auto cues = build_dialogue_cues({{2.0, 3.0}}, options);
  require(cues[0].at("character") == "Alice", "configured character preserved");
  require(cues[0].at("notes") == "Automatic pass", "configured notes preserved");

  options.character.clear();
  cues = build_dialogue_cues({{2.0, 3.0}}, options);
  require(cues[0].at("character") == "Unknown", "empty character matches Lua fallback");
}

void test_id_width_grows_for_large_detection_sets()
{
  std::vector<DialogueSegment> segments;
  segments.reserve(1000);
  for (int i = 0; i < 1000; ++i) {
    const double start = static_cast<double>(i) * 2.0;
    segments.push_back({start, start + 1.0});
  }
  const auto cues = build_dialogue_cues(segments);
  require(cues.front().at("id") == "0001", "large set expands ID width");
  require(cues.back().at("id") == "1000", "large set keeps final ID width");
}

} // namespace

int main()
{
  test_dialogue_cue_shape();
  test_invalid_segments_are_ignored();
  test_character_and_notes_options();
  test_id_width_grows_for_large_detection_sets();
  std::cout << "dialogue_cue_generation_tests: PASS\n";
  return 0;
}
