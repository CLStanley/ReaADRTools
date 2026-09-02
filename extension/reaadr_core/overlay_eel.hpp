#pragma once

#include "character_filter.hpp"
#include "overlay_settings.hpp"
#include "session_model.hpp"

#include <cstddef>
#include <string>

namespace reaadr::core {

struct OverlayEelOptions {
  OverlaySettings settings;
  double frame_rate = 24.0;
  std::string selected_region_cue_key;
  std::string selected_item_cue_key;
  std::string active_overlay_cue_key;
  CharacterFilterState character_filter;
};

struct OverlayEelResult {
  std::string video_code;
  std::string selected_cue_key;
  std::size_t displayed_cue_count = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Generates deterministic Video Processor EEL from adr_session_model_v1. The
// character filter affects overlays only when it also hides inactive regions,
// matching the transitional model/view sync pipeline.
OverlayEelResult build_overlay_eel(const SessionModel& model,
                                   const OverlayEelOptions& options = {});

} // namespace reaadr::core
