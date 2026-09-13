#pragma once

#include "reaadr_core/session_model.hpp"

#include <cstddef>
#include <string>

class ReaProject;
class MediaTrack;
class MediaItem;

namespace reaadr::reaper {

struct CueTakeCountApi {
  int (*count_tracks)(ReaProject*) = nullptr;
  MediaTrack* (*get_track)(ReaProject*, int) = nullptr;
  bool (*get_set_track_string)(MediaTrack*, const char*, char*, bool) = nullptr;
  int (*count_track_media_items)(MediaTrack*) = nullptr;
  MediaItem* (*get_track_media_item)(MediaTrack*, int) = nullptr;
  double (*get_media_item_info_value)(MediaItem*, const char*) = nullptr;
  int (*count_takes)(MediaItem*) = nullptr;
};

struct CueTakeCountResult {
  std::size_t take_count = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Mirrors ReaADR.count_recorded_takes_for_cue(): inspect only owned character
// tracks for the cue character, count takes on items overlapping the cue range,
// and count an overlapping item as one even when REAPER reports zero takes.
CueTakeCountResult count_recorded_takes_for_cue(
  ReaProject* project,
  const CueTakeCountApi& api,
  const core::Fields& cue);

} // namespace reaadr::reaper
