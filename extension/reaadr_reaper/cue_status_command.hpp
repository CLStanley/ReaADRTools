#pragma once

#include <string>
#include <vector>

struct ReaProject;

namespace reaadr::reaper {

struct CueStatusCommandResult {
  std::string cue_key;
  std::string normalized_status;
  bool changed = false;
  std::string event_warning;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

const std::vector<std::string>& native_cue_status_choices();

// Native backend for ReaADR_Set_Cue_Status.lua. It targets only the cue under
// the current play/edit position (no next-cue fallback), then uses the shared
// transactional status application service and overlay refresh pipeline.
CueStatusCommandResult set_cue_status_at_current_position(
  const std::string& status,
  ReaProject* project = nullptr);

} // namespace reaadr::reaper
