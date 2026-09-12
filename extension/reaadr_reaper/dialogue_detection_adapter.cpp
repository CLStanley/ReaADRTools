#include "dialogue_detection_adapter.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace reaadr::reaper {

DialogueDetectionResult detect_dialogue_from_selected_media(
  ReaProject* project,
  DialogueDetectionApi api,
  DialogueDetectionOptions options)
{
  DialogueDetectionResult result;
  if (!api.count_selected_media_items || !api.get_selected_media_item ||
      !api.get_active_take || !api.create_take_audio_accessor ||
      !api.destroy_audio_accessor || !api.get_audio_accessor_start_time ||
      !api.get_audio_accessor_end_time || !api.get_audio_accessor_samples ||
      !api.get_media_item_info_value) {
    result.error = "Required REAPER audio accessor APIs are unavailable.";
    return result;
  }

  if (api.count_selected_media_items(project) <= 0) {
    result.error = "Select one audio or video media item to analyze.";
    return result;
  }

  MediaItem* item = api.get_selected_media_item(project, 0);
  MediaItem_Take* take = item ? api.get_active_take(item) : nullptr;
  if (!item || !take) {
    result.error = "The selected media item does not have an active take.";
    return result;
  }

  AudioAccessor* accessor = api.create_take_audio_accessor(take);
  if (!accessor) {
    result.error = "Could not create an audio accessor for the selected media.";
    return result;
  }

  struct AccessorGuard {
    AudioAccessor* accessor = nullptr;
    DialogueDetectionApi api;
    ~AccessorGuard() { if (accessor && api.destroy_audio_accessor) api.destroy_audio_accessor(accessor); }
  } guard{accessor, api};

  const int sample_rate = options.sample_rate > 0 ? options.sample_rate : 12000;
  const int channels = 1;
  const int block_samples = std::max(64, static_cast<int>(sample_rate * 0.025 + 0.5));
  const double block_duration = static_cast<double>(block_samples) / sample_rate;
  const double threshold = std::pow(10.0, options.threshold_db / 20.0);
  const double min_speech = std::max(0.0, options.min_speech_seconds);
  const double min_silence = std::max(0.0, options.min_silence_seconds);
  const double pad = std::max(0.0, options.pad_seconds);
  const double item_position = api.get_media_item_info_value(item, "D_POSITION");
  const double item_length = std::max(0.0, api.get_media_item_info_value(item, "D_LENGTH"));
  const double item_end = item_position + item_length;
  const double accessor_start = api.get_audio_accessor_start_time(accessor);
  const double scan_end = std::min(api.get_audio_accessor_end_time(accessor), accessor_start + item_length);

  if (scan_end <= accessor_start) {
    result.error = "The selected media item has no readable audio in the placed item range.";
    return result;
  }

  std::vector<double> buffer(static_cast<std::size_t>(block_samples), 0.0);
  double active_start = -1.0;
  double last_loud_end = -1.0;
  double t = accessor_start;

  const auto timeline_time = [item_position, accessor_start](double accessor_time) {
    return item_position + std::max(0.0, accessor_time - accessor_start);
  };

  const auto flush_segment = [&]() {
    if (active_start >= 0.0 && last_loud_end >= 0.0 &&
        (last_loud_end - active_start) >= min_speech) {
      core::DialogueSegment segment;
      segment.start_time = std::max(item_position, timeline_time(active_start - pad));
      segment.end_time = std::min(item_end, timeline_time(last_loud_end + pad));
      if (segment.end_time > segment.start_time) result.segments.push_back(segment);
    }
    active_start = -1.0;
    last_loud_end = -1.0;
  };

  while (t < scan_end) {
    std::fill(buffer.begin(), buffer.end(), 0.0);
    const double block_end = std::min(scan_end, t + block_duration);
    const int got = api.get_audio_accessor_samples(
      accessor, sample_rate, channels, t, block_samples, buffer.data());
    if (got < 0) {
      result.segments.clear();
      result.error = "REAPER returned an error while reading the selected media.";
      return result;
    }

    bool loud = false;
    if (got == 1) {
      double sum = 0.0;
      for (const double sample : buffer) sum += sample * sample;
      const double rms = std::sqrt(sum / static_cast<double>(block_samples));
      loud = rms >= threshold;
    }

    if (loud) {
      if (active_start < 0.0) active_start = t;
      last_loud_end = block_end;
    } else if (active_start >= 0.0 && last_loud_end >= 0.0 &&
               (t - last_loud_end) >= min_silence) {
      flush_segment();
    }
    t = block_end;
  }

  flush_segment();
  return result;
}

} // namespace reaadr::reaper
