#include "reaadr_reaper/dialogue_detection_adapter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

struct ReaProject {};
struct MediaItem {};
struct MediaItem_Take {};
struct AudioAccessor {};

namespace {

ReaProject project;
MediaItem item;
MediaItem_Take take;
AudioAccessor accessor;
bool has_selection = true;
bool has_take = true;
bool accessor_created = true;
bool accessor_destroyed = false;
bool sample_error = false;
double item_position = 10.0;
double item_length = 2.0;
double accessor_start = 100.0;
double accessor_end = 102.0;

int count_selected(ReaProject*) { return has_selection ? 1 : 0; }
MediaItem* get_selected(ReaProject*, int index) { return has_selection && index == 0 ? &item : nullptr; }
MediaItem_Take* get_take(MediaItem*) { return has_take ? &take : nullptr; }
AudioAccessor* create_accessor(MediaItem_Take*) { return accessor_created ? &accessor : nullptr; }
void destroy_accessor(AudioAccessor*) { accessor_destroyed = true; }
double get_accessor_start(AudioAccessor*) { return accessor_start; }
double get_accessor_end(AudioAccessor*) { return accessor_end; }
double get_item_value(MediaItem*, const char* key)
{
  return std::string(key) == "D_POSITION" ? item_position : item_length;
}

int get_samples(AudioAccessor*, int, int, double t, int count, double* buffer)
{
  if (sample_error) return -1;
  // Two speech islands in accessor time: 100.20-100.60 and 101.20-101.55.
  const bool loud = (t >= 100.20 && t < 100.60) || (t >= 101.20 && t < 101.55);
  std::fill(buffer, buffer + count, loud ? 0.5 : 0.0);
  return 1;
}

reaadr::reaper::DialogueDetectionApi fake_api()
{
  return {count_selected, get_selected, get_take, create_accessor, destroy_accessor,
          get_accessor_start, get_accessor_end, get_samples, get_item_value};
}

void reset()
{
  has_selection = true;
  has_take = true;
  accessor_created = true;
  accessor_destroyed = false;
  sample_error = false;
  item_position = 10.0;
  item_length = 2.0;
  accessor_start = 100.0;
  accessor_end = 102.0;
}

void require(bool condition, const std::string& message)
{
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

void test_detects_timeline_segments_and_releases_accessor()
{
  reset();
  reaadr::reaper::DialogueDetectionOptions options;
  options.threshold_db = -20.0;
  options.min_speech_seconds = 0.20;
  options.min_silence_seconds = 0.10;
  options.pad_seconds = 0.05;
  options.sample_rate = 4000;
  const auto result = reaadr::reaper::detect_dialogue_from_selected_media(&project, fake_api(), options);
  require(static_cast<bool>(result), "dialogue scan succeeds");
  require(result.segments.size() == 2, "dialogue scan finds two speech islands");
  require(result.segments[0].start_time >= 10.14 && result.segments[0].start_time <= 10.21,
          "first segment start is translated to project time with padding");
  require(result.segments[0].end_time >= 10.60 && result.segments[0].end_time <= 10.70,
          "first segment end is translated to project time with padding");
  require(accessor_destroyed, "audio accessor is released after successful scan");
}

void test_selection_and_take_errors()
{
  reset();
  has_selection = false;
  auto result = reaadr::reaper::detect_dialogue_from_selected_media(&project, fake_api());
  require(!result && result.error.find("Select one") != std::string::npos,
          "missing selection is reported");

  reset();
  has_take = false;
  result = reaadr::reaper::detect_dialogue_from_selected_media(&project, fake_api());
  require(!result && result.error.find("active take") != std::string::npos,
          "missing active take is reported");
}

void test_accessor_and_sample_errors_release_resources()
{
  reset();
  accessor_created = false;
  auto result = reaadr::reaper::detect_dialogue_from_selected_media(&project, fake_api());
  require(!result && result.error.find("audio accessor") != std::string::npos,
          "accessor creation failure is reported");

  reset();
  sample_error = true;
  result = reaadr::reaper::detect_dialogue_from_selected_media(&project, fake_api());
  require(!result && result.segments.empty(), "sample read failure does not return partial segments");
  require(accessor_destroyed, "audio accessor is released after sample failure");
}

void test_missing_api_is_rejected()
{
  reset();
  const auto result = reaadr::reaper::detect_dialogue_from_selected_media(&project, {});
  require(!result && result.error.find("unavailable") != std::string::npos,
          "missing audio APIs are rejected");
}

} // namespace

int main()
{
  test_detects_timeline_segments_and_releases_accessor();
  test_selection_and_take_errors();
  test_accessor_and_sample_errors_release_resources();
  test_missing_api_is_rejected();
  std::cout << "dialogue_detection_adapter_tests: PASS\n";
  return 0;
}
