#pragma once

#include "reaadr_core/record_arm.hpp"
#include "project_transaction.hpp"

#include <string>
#include <vector>

struct MediaTrack;
struct ReaProject;

namespace reaadr::reaper {

struct RecordArmApi {
  int (*count_tracks)(ReaProject*) = nullptr;
  MediaTrack* (*get_track)(ReaProject*, int) = nullptr;
  bool (*validate_track)(ReaProject*, MediaTrack*) = nullptr;
  double (*get_track_value)(MediaTrack*, const char*) = nullptr;
  bool (*set_track_value)(MediaTrack*, const char*, double) = nullptr;
};

struct RecordArmApplyResult {
  int tracks_updated = 0;
  int tracks_skipped = 0;
  bool restored_after_failure = false;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Keeps the original raw track handles only at the REAPER boundary. Domain
// planning uses snapshot indexes, while every host mutation revalidates its
// handle so deleted tracks are never dereferenced during deferred recording.
class RecordArmManager {
public:
  RecordArmManager(ReaProject* project, RecordArmApi api, TransactionApi transaction_api)
    : project_(project), api_(api), transaction_api_(transaction_api) {}

  RecordArmApplyResult capture_and_isolate(MediaTrack* target_track);
  RecordArmApplyResult restore();
  bool has_snapshot() const { return !snapshot_.empty(); }

private:
  struct CapturedTrack {
    MediaTrack* track = nullptr;
    core::RecordArmSnapshotEntry state;
  };

  ReaProject* project_ = nullptr;
  RecordArmApi api_;
  TransactionApi transaction_api_;
  std::vector<CapturedTrack> snapshot_;
};

} // namespace reaadr::reaper
