#pragma once

#include "recording_target_application_service.hpp"
#include "../reaadr_core/cue_info.hpp"
#include "../reaadr_reaper/cue_take_count_adapter.hpp"

class ReaProject;

namespace reaadr::reaper {

struct CueInfoApplicationResult {
  core::CueInfoView view;
  core::RecordingTargetResult target;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Builds the native Cue Info live read model from the same canonical active-cue
// resolver used by Record Cue, then adds the recorded-take count from REAPER.
class CueInfoApplicationService final {
public:
  CueInfoApplicationService(RecordingTargetApplicationService& targets,
                            ReaProject* project,
                            CueTakeCountApi take_count_api)
    : targets_(targets), project_(project), take_count_api_(take_count_api) {}

  CueInfoApplicationResult load(double timeline_position, double frame_rate) const;

private:
  RecordingTargetApplicationService& targets_;
  ReaProject* project_ = nullptr;
  CueTakeCountApi take_count_api_;
};

} // namespace reaadr::reaper
