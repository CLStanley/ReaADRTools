#pragma once

#include "reaadr_core/cue_wav.hpp"
#include "reaadr_core/event_log.hpp"
#include "reaadr_core/session_commit.hpp"
#include "render_artifact_adapter.hpp"

#include <string>
#include <vector>

struct ReaProject;

namespace reaadr::reaper {

struct SessionRenderOptions {
  core::SessionCommitOptions commit;
  core::RenderPlanOptions render;
  core::CueWavOptions cue_wav;
  core::EventPublishOptions event;
  std::string cue_audio_path;
  std::string undo_description = "ReaADR: Commit and render session cues";
  bool publish_events = true;
};

struct SessionRenderResult {
  core::SessionCommitResult commit;
  core::RenderPlan plan;
  CompleteRenderApplyResult render;
  core::CueWavResult cue_wav;
  std::vector<core::EventPublishResult> events;
  // Event history is observational and must never turn an already successful
  // model/project commit into a retryable failure. Publication problems are
  // therefore surfaced separately from the operation error.
  std::string event_warning;
  std::string error;
  bool model_rolled_back = false;

  explicit operator bool() const { return error.empty(); }
};

// Coordinates the canonical extstate commit and every visible REAPER artifact
// under one project undo block. Model restoration happens only after REAPER's
// rollback completes, matching the ordering used by the established Lua flow.
class SessionRenderService {
public:
  SessionRenderService(core::SessionModelRepository& repository,
                       core::EventLogRepository& event_log,
                       ReaProject* project,
                       TrackRegionApi track_region_api,
                       RulerLaneApi ruler_lane_api,
                       CueAudioApi cue_audio_api,
                       TransactionApi transaction_api)
    : repository_(repository),
      event_log_(event_log),
      project_(project),
      track_region_api_(track_region_api),
      ruler_lane_api_(ruler_lane_api),
      cue_audio_api_(cue_audio_api),
      transaction_api_(transaction_api)
  {
  }

  SessionRenderResult commit_and_render(const std::vector<core::Fields>& cues,
                                        const SessionRenderOptions& options);

private:
  core::SessionModelRepository& repository_;
  core::EventLogRepository& event_log_;
  ReaProject* project_ = nullptr;
  TrackRegionApi track_region_api_;
  RulerLaneApi ruler_lane_api_;
  CueAudioApi cue_audio_api_;
  TransactionApi transaction_api_;
};

} // namespace reaadr::reaper
