#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_GetProjExtState
#define REAPERAPI_WANT_SetProjExtState

#include "recording_command_context.hpp"

#include "native_host_services.hpp"
#include "overlay_refresh_adapter.hpp"

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>

namespace reaadr::reaper {
namespace {

double command_frame_rate()
{
  return native_project_frame_rate(nullptr);
}

OverlaySelectionInput empty_overlay_selection()
{
  return {};
}

bool command_refresh_overlay(const core::OverlayRefreshOptions& options,
                             std::string* error)
{
  const auto refreshed = refresh_generated_overlay_transactionally(
    nullptr, native_overlay_refresh_api(), native_transaction_api(), options,
    "ReaADR: refresh recording overlay");
  if (!refreshed && error) *error = refreshed.error;
  return static_cast<bool>(refreshed);
}

} // namespace

RecordingCommandContext::RecordingCommandContext(ReaProject* project)
  : project_(project),
    project_state_(project, {GetProjExtState, SetProjExtState}),
    sessions_(project_state_),
    event_log_(project_state_),
    filters_(project_state_),
    overlay_settings_(project_state_),
    selections_(project_state_),
    recording_preferences_(project_state_),
    target_application_(sessions_, selections_, filters_, overlay_settings_),
    overlay_application_(sessions_, overlay_settings_, selections_, filters_,
                         {command_frame_rate, empty_overlay_selection,
                          command_refresh_overlay}),
    record_arm_(project, native_record_arm_api(), native_transaction_api()),
    recording_setup_(sessions_, project, native_recording_setup_api()),
    transport_(record_arm_, native_recording_transport_api()),
    recording_application_(
      sessions_, selections_, recording_preferences_, event_log_, project,
      native_transaction_api(), {[this]() { return refresh_overlay(); }}),
    workflow_(recording_setup_, transport_, recording_application_),
    session_(target_application_, recording_preferences_, workflow_)
{
}

double RecordingCommandContext::current_timeline_position() const
{
  return (native_play_state() & 1) != 0
    ? native_play_position()
    : native_cursor_position();
}

double RecordingCommandContext::frame_rate() const
{
  return native_project_frame_rate(project_);
}

RecordingWindowLayout RecordingCommandContext::load_window_layout() const
{
  core::WindowLayoutRepository layouts(
    const_cast<ProjectStateStore&>(project_state_), "record_cue", 570, 300);
  const auto loaded = layouts.load();
  return loaded ? loaded.layout : RecordingWindowLayout{570, 300, -1, 0, 0, false};
}

bool RecordingCommandContext::save_window_layout(const RecordingWindowLayout& layout)
{
  core::WindowLayoutRepository layouts(project_state_, "record_cue", 570, 300);
  return layouts.save(layout);
}

bool RecordingCommandContext::refresh_overlay()
{
  return static_cast<bool>(overlay_application_.refresh());
}

RecordingSessionStartResult RecordingCommandContext::begin()
{
  RecordingSessionOptions options;
  options.status_commit.snapshot_label = "Record ADR Cue";
  options.status_commit.utc_timestamp = native_utc_timestamp();
  options.status_commit.update.last_operation = "record_cue";
  options.event.source = "native_recording";
  options.event.utc_timestamp = options.status_commit.utc_timestamp;
  options.undo_description = "ReaADR: record cue";
  return session_.begin(current_timeline_position(), options);
}

RecordingWorkflowDispatchResult RecordingCommandContext::dispatch(
  core::RecordingTransportEvent event)
{
  return session_.dispatch(event, native_play_state(), native_play_position());
}

RecordingWorkflowDispatchResult RecordingCommandContext::retry_pending()
{
  return session_.retry_pending();
}

RecordingWorkflowDispatchResult RecordingCommandContext::shutdown()
{
  return session_.shutdown(native_play_state(), native_play_position());
}

} // namespace reaadr::reaper
