#include "reaadr_core/recording_target.hpp"
#include "reaadr_core/character_filter.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message)
{
  if (!condition) {
    std::cerr << "recording_target_tests: " << message << '\n';
    std::exit(1);
  }
}

reaadr::core::Fields cue(const char* id, const char* character,
                         const char* start, const char* end)
{
  return {
    {"id", id}, {"character", character}, {"start_time", start},
    {"end_time", end}, {"line", id}, {"status", "Not Recorded"},
    {"cue_type", "Dialogue"},
  };
}

reaadr::core::SessionModel model()
{
  reaadr::core::SessionModel value;
  value.session["session_id"] = "recording-target-tests";
  value.cues = {
    cue("1", "Alice", "1.0", "2.0"),
    cue("2", "Bob", "3.0", "4.0"),
    cue("3", "Alice", "5.0", "6.0"),
  };
  return value;
}

} // namespace

int main()
{
  using namespace reaadr::core;

  {
    RecordingTargetOptions options;
    options.selected_cue_key = "2";
    options.timeline_position = 0.0;
    const auto result = resolve_recording_target(model(), {}, options);
    require(static_cast<bool>(result), "selected cue should resolve");
    require(result.cue.cue_key == "2", "selected cue must take precedence");
    require(result.used_selected_cue, "selected cue result should be marked selected");
  }

  {
    RecordingTargetOptions options;
    options.timeline_position = 3.5;
    const auto result = resolve_recording_target(model(), {}, options);
    require(static_cast<bool>(result), "cue under cursor should resolve");
    require(result.cue.cue_key == "2", "cursor should resolve the containing cue");
    require(!result.used_next_cue, "containing cue is not a next-cue fallback");
  }

  {
    RecordingTargetOptions options;
    options.timeline_position = 2.5;
    const auto result = resolve_recording_target(model(), {}, options);
    require(static_cast<bool>(result), "next cue should resolve between cues");
    require(result.cue.cue_key == "2", "next timeline cue should be selected");
    require(result.used_next_cue, "next-cue fallback should be reported");
  }

  {
    CharacterFilterState filter;
    filter.encoded_selection = "alice";
    filter.active_tokens.insert(character_filter_key("Alice"));
    RecordingTargetOptions options;
    options.selected_cue_key = "2";
    options.timeline_position = 0.0;
    const auto result = resolve_recording_target(model(), filter, options);
    require(static_cast<bool>(result), "visible filtered cue should resolve");
    require(result.cue.cue_key == "1", "filtered-out selected cue must not bypass the active filter");
    require(result.used_next_cue, "filter fallback should use next visible cue");
  }

  {
    SessionModel overlap;
    overlap.session["session_id"] = "recording-overlap-tests";
    overlap.cues = {
      cue("10", "Alice", "10.0", "12.0"),
      cue("11", "Alice", "11.0", "13.0"),
    };
    CharacterFilterState filter;
    filter.encoded_selection = character_filter_target_key("Alice", 2);
    filter.active_tokens.insert(character_filter_target_key("Alice", 2));
    RecordingTargetOptions options;
    options.timeline_position = 0.0;
    options.preroll_seconds = 0.0;
    const auto result = resolve_recording_target(overlap, filter, options);
    require(static_cast<bool>(result), "lane-filtered overlap cue should resolve");
    require(result.cue.cue_key == "11", "lane 2 filter should select the second overlapping cue");
    require(result.lane == 2, "resolved overlap cue should retain lane 2");

    const auto catalog = build_character_filter_catalog(overlap, filter, 0.0);
    require(static_cast<bool>(catalog), "character filter catalog should build for overlapping cues");
    require(catalog.groups.size() == 1, "overlapping Alice cues should form one character group");
    require(catalog.groups[0].targets.size() == 2, "overlapping Alice cues should expose two lane targets");
    require(!catalog.groups[0].all_active && catalog.groups[0].partially_active,
            "single active lane should produce a partial character selection");
    require(!catalog.groups[0].targets[0].active && catalog.groups[0].targets[1].active,
            "lane 2 selection should be reflected in the native catalog");
  }

  {
    const auto catalog = build_character_filter_catalog(model(), {}, 0.0);
    require(static_cast<bool>(catalog), "unfiltered character catalog should build");
    require(catalog.show_all, "empty filter must represent Show All");
    require(catalog.groups.size() == 2, "catalog should group unique characters");
    require(catalog.groups[0].all_active && catalog.groups[1].all_active,
            "Show All should mark every character group active");
  }

  {
    const std::string encoded = encode_character_filter_tokens({"bob.lane1", "alice.lane1", "bob.lane1"});
    require(encoded == "alice.lane1,bob.lane1",
            "character filter encoding should be deterministic and deduplicate tokens");
  }

  {
    CharacterFilterState filter;
    filter.encoded_selection = "nobody";
    filter.active_tokens.insert("nobody");
    RecordingTargetOptions options;
    options.timeline_position = 0.0;
    const auto result = resolve_recording_target(model(), filter, options);
    require(!result, "empty active filter result should fail closed");
  }

  std::cout << "recording_target_tests: ok\n";
  return 0;
}
