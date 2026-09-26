#pragma once

#include "../app/overlay_application_service.hpp"
#include "../app/recording_application_service.hpp"
#include "../app/recording_session_service.hpp"
#include "../app/recording_target_application_service.hpp"
#include "../app/recording_workflow_service.hpp"
#include "../reaadr_core/character_filter.hpp"
#include "../reaadr_core/event_log.hpp"
#include "../reaadr_core/model_repository.hpp"
#include "../reaadr_core/overlay_settings.hpp"
#include "../reaadr_core/recording_preferences.hpp"
#include "../reaadr_core/window_layout.hpp"
#include "project_state.hpp"
#include "record_arm_adapter.hpp"
#include "recording_setup_adapter.hpp"
#include "recording_transport_executor.hpp"

class ReaProject;

namespace reaadr::reaper {

using RecordingWindowLayout = core::WindowLayout;

// Owns the complete native Record Cue service graph for one REAPER project.
// The UI never constructs repositories/adapters itself; it emits semantic
// events into this context and reads immutable state/plan snapshots.
class RecordingCommandContext final {
public:
  explicit RecordingCommandContext(ReaProject* project = nullptr);

  RecordingSessionStartResult begin();
  RecordingWorkflowDispatchResult dispatch(core::RecordingTransportEvent event);
  RecordingWorkflowDispatchResult retry_pending();
  RecordingWorkflowDispatchResult shutdown();

  ReaProject* project() const { return project_; }
  bool active() const { return session_.active(); }
  const core::RecordingTransportState& state() const { return session_.state(); }
  const core::RecordingSetupPlan& plan() const { return session_.plan(); }
  double frame_rate() const;
  double timeline_position() const { return current_timeline_position(); }
  RecordingWindowLayout load_window_layout() const;
  bool save_window_layout(const RecordingWindowLayout& layout);

private:
  double current_timeline_position() const;
  bool refresh_overlay();

  ReaProject* project_ = nullptr;
  ProjectStateStore project_state_;
  core::SessionModelRepository sessions_;
  core::EventLogRepository event_log_;
  core::CharacterFilterRepository filters_;
  core::OverlaySettingsRepository overlay_settings_;
  core::CueSelectionRepository selections_;
  core::RecordingPreferenceRepository recording_preferences_;
  RecordingTargetApplicationService target_application_;
  OverlayApplicationService overlay_application_;
  RecordArmManager record_arm_;
  RecordingSetupService recording_setup_;
  RecordingTransportExecutor transport_;
  RecordingApplicationService recording_application_;
  RecordingWorkflowService workflow_;
  RecordingSessionService session_;
};

} // namespace reaadr::reaper
