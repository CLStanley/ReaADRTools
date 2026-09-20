#pragma once

#include "session_model.hpp"

#include <cstddef>
#include <string>

namespace reaadr::core {

struct CueInfoOptions {
  double timeline_position = 0.0;
  double frame_rate = 24.0;
  std::size_t take_count = 0;
};

struct CueInfoView {
  std::string cue_key;
  std::string character;
  std::string status;
  std::string cue_type;
  std::string direction;
  std::string dialogue;
  std::string notes;
  double start_time = 0.0;
  double end_time = 0.0;
  double duration = 0.0;
  double countdown = 0.0;
  double timeline_position = 0.0;
  double frame_rate = 24.0;
  std::size_t take_count = 0;
  std::string start_timecode;
  std::string end_timecode;
  std::string position_timecode;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Projects one canonical cue into the native Cue Info live read model. This
// deliberately contains no REAPER/UI dependency: the host supplies the current
// timeline position and recorded-take count, while the UI decides how to
// present and edit the values.
CueInfoView build_cue_info_view(const Fields& cue, const CueInfoOptions& options = {});

} // namespace reaadr::core
