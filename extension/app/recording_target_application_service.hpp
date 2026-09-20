#pragma once

#include "../reaadr_core/character_filter.hpp"
#include "../reaadr_core/cue_navigation.hpp"
#include "../reaadr_core/model_repository.hpp"
#include "../reaadr_core/overlay_settings.hpp"
#include "../reaadr_core/recording_target.hpp"

namespace reaadr::reaper {

struct RecordingTargetApplicationResult {
  core::RecordingTargetResult target;
  double preroll_seconds = 3.0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Loads canonical project-local selection/filter/overlay inputs and delegates
// deterministic recording-target and lane resolution to the host-independent
// core shared by the native Record Cue and Cue Info workflows.
class RecordingTargetApplicationService final {
public:
  RecordingTargetApplicationService(core::SessionModelRepository& sessions,
                                    core::CueSelectionRepository& selections,
                                    core::CharacterFilterRepository& filters,
                                    core::OverlaySettingsRepository& overlay_settings)
    : sessions_(sessions), selections_(selections), filters_(filters),
      overlay_settings_(overlay_settings) {}

  RecordingTargetApplicationResult resolve(double timeline_position) const;

private:
  core::SessionModelRepository& sessions_;
  core::CueSelectionRepository& selections_;
  core::CharacterFilterRepository& filters_;
  core::OverlaySettingsRepository& overlay_settings_;
};

} // namespace reaadr::reaper
