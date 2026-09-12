#pragma once

#include "reaadr_core/dialogue_cue_generation.hpp"

#include <string>
#include <vector>

struct ReaProject;
struct MediaItem;
struct MediaItem_Take;
struct AudioAccessor;

namespace reaadr::reaper {

struct DialogueDetectionApi {
  int (*count_selected_media_items)(ReaProject*) = nullptr;
  MediaItem* (*get_selected_media_item)(ReaProject*, int) = nullptr;
  MediaItem_Take* (*get_active_take)(MediaItem*) = nullptr;
  AudioAccessor* (*create_take_audio_accessor)(MediaItem_Take*) = nullptr;
  void (*destroy_audio_accessor)(AudioAccessor*) = nullptr;
  double (*get_audio_accessor_start_time)(AudioAccessor*) = nullptr;
  double (*get_audio_accessor_end_time)(AudioAccessor*) = nullptr;
  int (*get_audio_accessor_samples)(AudioAccessor*, int, int, double, int, double*) = nullptr;
  double (*get_media_item_info_value)(MediaItem*, const char*) = nullptr;
};

struct DialogueDetectionOptions {
  double threshold_db = -42.0;
  double min_speech_seconds = 0.25;
  double min_silence_seconds = 0.35;
  double pad_seconds = 0.05;
  int sample_rate = 12000;
};

struct DialogueDetectionResult {
  std::vector<core::DialogueSegment> segments;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Scans the first selected media item through REAPER's audio-accessor API and
// returns project-timeline dialogue segments. Cue construction remains in the
// domain core and session rendering remains in the application layer.
DialogueDetectionResult detect_dialogue_from_selected_media(
  ReaProject* project,
  DialogueDetectionApi api,
  DialogueDetectionOptions options = {});

} // namespace reaadr::reaper
