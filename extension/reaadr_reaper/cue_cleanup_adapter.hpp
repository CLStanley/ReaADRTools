#pragma once

#include "../reaadr_core/cue_cleanup.hpp"
#include "project_transaction.hpp"

#include <string>

struct MediaItem;
struct MediaTrack;
struct ReaProject;

namespace reaadr::reaper {

struct CueCleanupApi {
  int (*count_tracks)(ReaProject*) = nullptr;
  MediaTrack* (*get_track)(ReaProject*, int) = nullptr;
  bool (*get_set_track_string)(MediaTrack*, const char*, char*, bool) = nullptr;
  int (*count_track_items)(MediaTrack*) = nullptr;
  MediaItem* (*get_track_item)(MediaTrack*, int) = nullptr;
  bool (*get_set_item_string)(MediaItem*, const char*, char*, bool) = nullptr;
  bool (*delete_track_item)(MediaTrack*, MediaItem*) = nullptr;
  bool (*delete_track)(ReaProject*, MediaTrack*) = nullptr;
  int (*count_project_markers)(ReaProject*, int*, int*) = nullptr;
  int (*enum_project_markers)(ReaProject*, int, bool*, double*, double*, const char**, int*, int*) = nullptr;
  bool (*delete_project_marker)(ReaProject*, int, bool) = nullptr;
  void (*adjust_track_windows)(bool) = nullptr;
  void (*update_arrange)() = nullptr;
};

struct CueCleanupApplyResult {
  int cues_removed = 0;
  int regions_removed = 0;
  int cue_audio_removed = 0;
  int tracks_removed = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

CueCleanupApplyResult apply_cue_cleanup_plan_transactionally(
  ReaProject* project,
  CueCleanupApi api,
  TransactionApi transaction_api,
  const core::CueCleanupPlan& plan,
  const std::string& description);

} // namespace reaadr::reaper
