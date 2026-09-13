#include "reaadr_reaper/cue_take_count_adapter.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct ReaProject {};
struct MediaItem {
  double position = 0.0;
  double length = 0.0;
  int takes = 0;
};
struct MediaTrack {
  std::map<std::string, std::string> ext;
  std::vector<MediaItem> items;
};

namespace {

std::vector<MediaTrack> g_tracks;

void check(bool condition, const char* message)
{
  if (condition) return;
  std::cerr << "FAILED: " << message << '\n';
  std::exit(1);
}

int count_tracks(ReaProject*) { return static_cast<int>(g_tracks.size()); }
MediaTrack* get_track(ReaProject*, int index)
{
  return index >= 0 && static_cast<std::size_t>(index) < g_tracks.size()
    ? &g_tracks[static_cast<std::size_t>(index)] : nullptr;
}
bool get_set_track_string(MediaTrack* track, const char* key, char* value, bool set)
{
  if (!track || !key || !value || set) return false;
  const auto found = track->ext.find(key);
  const std::string text = found == track->ext.end() ? std::string() : found->second;
  std::strcpy(value, text.c_str());
  return true;
}
int count_track_media_items(MediaTrack* track)
{
  return track ? static_cast<int>(track->items.size()) : 0;
}
MediaItem* get_track_media_item(MediaTrack* track, int index)
{
  if (!track || index < 0 || static_cast<std::size_t>(index) >= track->items.size()) return nullptr;
  return &track->items[static_cast<std::size_t>(index)];
}
double get_media_item_info_value(MediaItem* item, const char* key)
{
  if (!item || !key) return 0.0;
  return std::strcmp(key, "D_POSITION") == 0 ? item->position : item->length;
}
int count_takes(MediaItem* item) { return item ? item->takes : 0; }

reaadr::reaper::CueTakeCountApi api()
{
  return {
    count_tracks, get_track, get_set_track_string, count_track_media_items,
    get_track_media_item, get_media_item_info_value, count_takes,
  };
}

reaadr::core::Fields cue()
{
  return {{"id", "12"}, {"character", "Actor"}, {"start_time", "10"}, {"end_time", "12"}};
}

void test_counts_matching_overlaps_only()
{
  g_tracks = {
    {{{"P_EXT:ReaADR.role", "character"}, {"P_EXT:ReaADR.key", "actor.lane1"}},
      {{9.5, 1.0, 2}, {11.0, 0.5, 3}, {12.0, 1.0, 9}}},
    {{{"P_EXT:ReaADR.role", "character"}, {"P_EXT:ReaADR.key", "other.lane1"}},
      {{10.0, 1.0, 7}}},
    {{{"P_EXT:ReaADR.role", "cue_character"}, {"P_EXT:ReaADR.key", "actor.lane1"}},
      {{10.0, 1.0, 11}}},
  };
  const auto result = reaadr::reaper::count_recorded_takes_for_cue(nullptr, api(), cue());
  check(static_cast<bool>(result), "valid project should count takes");
  check(result.take_count == 5, "only overlapping matching character-track takes should count");
}

void test_zero_take_item_counts_as_one()
{
  g_tracks = {{{{"P_EXT:ReaADR.role", "character"}, {"P_EXT:ReaADR.key", "ACTOR.lane2"}},
               {{10.2, 0.5, 0}}}};
  const auto result = reaadr::reaper::count_recorded_takes_for_cue(nullptr, api(), cue());
  check(result && result.take_count == 1, "zero-take overlapping item should contribute one");
}

void test_blank_character_matches_all_character_tracks()
{
  auto blank = cue();
  blank["character"] = "";
  g_tracks = {
    {{{"P_EXT:ReaADR.role", "character"}, {"P_EXT:ReaADR.key", "a.lane1"}}, {{10.0, 1.0, 1}}},
    {{{"P_EXT:ReaADR.role", "character"}, {"P_EXT:ReaADR.key", "b.lane2"}}, {{10.0, 1.0, 2}}},
  };
  const auto result = reaadr::reaper::count_recorded_takes_for_cue(nullptr, api(), blank);
  check(result && result.take_count == 3, "blank cue character should match all character tracks");
}

void test_invalid_timing_and_api()
{
  auto invalid = cue();
  invalid["end_time"] = "9";
  check(!reaadr::reaper::count_recorded_takes_for_cue(nullptr, api(), invalid),
        "invalid cue timing should fail");
  check(!reaadr::reaper::count_recorded_takes_for_cue(nullptr, {}, cue()),
        "missing host callbacks should fail");
}

} // namespace

int main()
{
  test_counts_matching_overlaps_only();
  test_zero_take_item_counts_as_one();
  test_blank_character_matches_all_character_tracks();
  test_invalid_timing_and_api();
  std::cout << "cue_take_count_adapter_tests passed\n";
  return 0;
}
