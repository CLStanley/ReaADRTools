#pragma once

#include "reaadr_core/recording_transport.hpp"
#include "record_arm_adapter.hpp"

#include <string>

struct MediaTrack;

namespace reaadr::reaper {

struct RecordingTransportApi {
  bool (*get_loop_time_range)(double*, double*) = nullptr;
  bool (*set_loop_time_range)(double, double) = nullptr;
  bool (*set_edit_cursor_position)(double, bool, bool) = nullptr;
  bool (*run_command)(int) = nullptr;
};

struct PendingRecordingApplicationActions {
  bool refresh_active_cue = false;
  bool finalize_recorded_takes = false;
  bool persist_preroll_preference = false;
};

struct RecordingTransportExecutionResult {
  bool state_accepted = false;
  PendingRecordingApplicationActions pending;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Applies only immediate REAPER transport actions. Canonical cue/status writes
// remain pending for the future application coordinator so they can use the
// model-first synchronization pipeline rather than this asynchronous boundary.
class RecordingTransportExecutor {
public:
  RecordingTransportExecutor(RecordArmManager& record_arm, RecordingTransportApi api)
    : record_arm_(record_arm), api_(api) {}

  RecordingTransportExecutionResult apply(
    const core::RecordingTransportTransition& transition,
    const core::RecordingTransportContext& context,
    MediaTrack* target_track);

  bool has_active_loop_range() const { return loop_range_active_; }

private:
  bool configure_loop_range(const core::RecordingTransportContext& context,
                            std::string& error);
  bool restore_loop_range(std::string& error);
  void compensate_start_failure(bool restore_arm,
                                bool restore_loop,
                                std::string& error);

  RecordArmManager& record_arm_;
  RecordingTransportApi api_;
  bool loop_range_saved_ = false;
  bool loop_range_active_ = false;
  double saved_loop_start_ = 0.0;
  double saved_loop_end_ = 0.0;
};

} // namespace reaadr::reaper
