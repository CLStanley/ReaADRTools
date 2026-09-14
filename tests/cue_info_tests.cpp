#include "reaadr_core/cue_info.hpp"
#include "reaadr_ui/cue_info_controller.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void check(bool condition, const char* message)
{
  if (condition) return;
  std::cerr << "FAILED: " << message << '\n';
  std::exit(1);
}

reaadr::core::Fields sample_cue()
{
  return {
    {"id", "A12"},
    {"character", "Actor"},
    {"status", "needs_review"},
    {"cue_type", "Dialogue"},
    {"direction", "whisper"},
    {"start_time", "10.0"},
    {"end_time", "12.5"},
    {"line", "Hello there"},
    {"notes", "Keep it soft"},
  };
}

void test_projection()
{
  const auto view = reaadr::core::build_cue_info_view(sample_cue(), {8.5, 24.0, 3});
  check(static_cast<bool>(view), "valid cue should project");
  check(view.cue_key == "A12", "cue ID should project");
  check(view.character == "Actor", "character should project");
  check(view.status == "Needs Review", "status should normalize");
  check(view.cue_type == "Dialogue", "cue type should project");
  check(view.direction == "whisper", "direction should project");
  check(view.dialogue == "Hello there", "dialogue should project");
  check(view.notes == "Keep it soft", "notes should project");
  check(view.duration == 2.5, "duration should derive from cue bounds");
  check(view.countdown == 1.5, "countdown should derive from timeline position");
  check(view.take_count == 3, "take count should project");
  check(view.start_timecode == "00:00:10:00", "start timecode should format");
  check(view.end_timecode == "00:00:12:12", "end timecode should format");
  check(view.position_timecode == "00:00:08:12", "position timecode should format");
}

void test_countdown_clamps_after_start()
{
  const auto view = reaadr::core::build_cue_info_view(sample_cue(), {11.0, 24.0, 0});
  check(static_cast<bool>(view), "active cue should project");
  check(view.countdown == 0.0, "countdown should not go negative");
}

void test_dialogue_fallback()
{
  auto cue = sample_cue();
  cue.erase("line");
  cue["dialogue"] = "Fallback dialogue";
  const auto view = reaadr::core::build_cue_info_view(cue, {0.0, 30.0, 0});
  check(static_cast<bool>(view), "cue with dialogue field should project");
  check(view.dialogue == "Fallback dialogue", "legacy dialogue field should be supported");
}

void test_invalid_values()
{
  auto cue = sample_cue();
  cue["start_time"] = "bad";
  check(!reaadr::core::build_cue_info_view(cue), "invalid start should fail");

  cue = sample_cue();
  cue["end_time"] = "9.0";
  check(!reaadr::core::build_cue_info_view(cue), "end before start should fail");

  check(!reaadr::core::build_cue_info_view(sample_cue(), {-1.0, 24.0, 0}),
        "negative timeline position should fail");
  check(!reaadr::core::build_cue_info_view(sample_cue(), {0.0, 0.0, 0}),
        "non-positive frame rate should fail");
}

void test_window_layout_defaults_to_floating()
{
  const reaadr::ui::CueInfoWindowLayout layout;
  check(layout.dock == -1, "Cue Info should default to a floating REAPER window");
  check(layout.width == 1100 && layout.height == 740,
        "Cue Info should retain its native default window size");
  check(!layout.has_position, "Cue Info should not invent a persisted position");
}

} // namespace

int main()
{
  test_projection();
  test_countdown_clamps_after_start();
  test_dialogue_fallback();
  test_invalid_values();
  test_window_layout_defaults_to_floating();
  std::cout << "cue_info_tests passed\n";
  return 0;
}