#pragma once

#include "app/overlay_application_service.hpp"
#include "cue_navigation_service.hpp"
#include "cue_take_count_adapter.hpp"
#include "dialogue_detection_adapter.hpp"
#include "marker_snapshot_adapter.hpp"
#include "overlay_refresh_adapter.hpp"
#include "record_arm_adapter.hpp"
#include "recording_setup_adapter.hpp"
#include "recording_transport_executor.hpp"
#include "render_artifact_adapter.hpp"
#include "track_region_adapter.hpp"

#include <string>

class ReaProject;

namespace reaadr::reaper {

// Shared REAPER-host factories used by native commands. Keeping the raw REAPER
// function-pointer wiring here prevents each migrated workflow from rebuilding
// its own parallel host boundary.
TrackRegionApi native_track_region_api();
RulerLaneApi native_ruler_lane_api();
CueAudioApi native_cue_audio_api();
TransactionApi native_transaction_api();
MarkerSnapshotApi native_marker_snapshot_api();
DialogueDetectionApi native_dialogue_detection_api();
CueTakeCountApi native_cue_take_count_api();
CueNavigationApi native_cue_navigation_api();
OverlayRefreshApi native_overlay_refresh_api();
OverlayApplicationApi native_overlay_application_api();
OverlaySelectionInput native_overlay_selection();
bool native_overlay_refresh_callback(
  const core::OverlayRefreshOptions& options,
  std::string* error);
RecordArmApi native_record_arm_api();
RecordingSetupApi native_recording_setup_api();
RecordingTransportApi native_recording_transport_api();

int native_play_state();
double native_play_position();
double native_cursor_position();
void native_set_edit_cursor_position(double position, bool move_view, bool seek_play);
double native_current_project_frame_rate();
double native_project_frame_rate(ReaProject* project = nullptr);
std::string native_project_cue_audio_path(ReaProject* project = nullptr);
std::string native_utc_timestamp();

} // namespace reaadr::reaper