#pragma once

#include "reaadr_core/model_repository.hpp"
#include "reaadr_core/recording_setup.hpp"

#include <string>

struct MediaTrack;
struct ReaProject;

namespace reaadr::reaper {

struct RecordingSetupApi {
  int (*count_tracks)(ReaProject*) = nullptr;
  MediaTrack* (*get_track)(ReaProject*, int) = nullptr;
  bool (*validate_track)(ReaProject*, MediaTrack*) = nullptr;
  bool (*get_set_track_string)(MediaTrack*, const char*, char*, bool) = nullptr;
};

struct PreparedRecordingSetup {
  core::RecordingSetupPlan plan;
  MediaTrack* target_track = nullptr;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Inspects only recording-track ownership metadata, delegates selection and
// lane rules to the domain core, then revalidates the chosen handle immediately
// before returning it to the record-arm/transport coordinator.
class RecordingSetupService {
public:
  RecordingSetupService(core::SessionModelRepository& repository,
                        ReaProject* project,
                        RecordingSetupApi api)
    : repository_(repository), project_(project), api_(api) {}

  PreparedRecordingSetup prepare(const core::RecordingSetupOptions& options) const;

private:
  core::SessionModelRepository& repository_;
  ReaProject* project_ = nullptr;
  RecordingSetupApi api_;
};

} // namespace reaadr::reaper
