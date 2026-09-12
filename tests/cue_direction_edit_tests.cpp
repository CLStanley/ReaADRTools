#include "reaadr_core/cue_manager_model.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void check(bool condition, const char* message)
{
  if (condition) return;
  std::cerr << "FAILED: " << message << '\n';
  std::exit(1);
}

reaadr::core::SessionModel model()
{
  reaadr::core::SessionModel value;
  value.session["session_id"] = "test-session";
  value.timecode["frame_rate"] = "24";
  value.cues = {{{"id", "1"}, {"character", "Actor"}, {"start_time", "1"},
                 {"end_time", "2"}, {"line", "Hello"}, {"direction", "Original"},
                 {"status", "Not Recorded"}, {"cue_type", "Dialogue"}}};
  return value;
}

void test_direction_update()
{
  auto options = reaadr::core::CueManagerEditOptions{};
  options.cue_key = "1";
  options.direction = "Whisper";
  options.direction_set = true;
  const auto result = reaadr::core::edit_cue_manager_row(model(), options);
  check(result && result.changed, "direction edit should change the cue");
  check(result.model.cues[0].at("direction") == "Whisper", "direction should persist");
}

void test_direction_clear()
{
  auto options = reaadr::core::CueManagerEditOptions{};
  options.cue_key = "1";
  options.direction.clear();
  options.direction_set = true;
  const auto result = reaadr::core::edit_cue_manager_row(model(), options);
  check(result && result.changed, "explicit empty direction should change the cue");
  check(result.model.cues[0].at("direction").empty(), "direction should be clearable");
}

void test_omitted_direction_is_preserved()
{
  auto options = reaadr::core::CueManagerEditOptions{};
  options.cue_key = "1";
  options.status = "Recorded";
  const auto result = reaadr::core::edit_cue_manager_row(model(), options);
  check(result && result.changed, "unrelated edit should succeed");
  check(result.model.cues[0].at("direction") == "Original", "omitted direction must be preserved");
}

} // namespace

int main()
{
  test_direction_update();
  test_direction_clear();
  test_omitted_direction_is_preserved();
  std::cout << "cue_direction_edit_tests passed\n";
  return 0;
}
