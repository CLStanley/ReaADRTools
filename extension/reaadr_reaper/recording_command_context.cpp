#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_GetProjExtState
#define REAPERAPI_WANT_SetProjExtState

#include "recording_command_context.hpp"

#include "native_host_services.hpp"
#include "overlay_refresh_adapter.hpp"

#include <cstdlib>
#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>

namespace reaadr::reaper {
namespace {
constexpr const char* kStateNamespace = "ReaADRTools";
constexpr const char* kRememberLayoutKey = "ui.remember_window_layout";
constexpr const char* kWindowWidthKey = "ui.window.record_cue.width";
constexpr const char* kWindowHeightKey = "ui.window.record_cue.height";
constexpr const char* kWindowDockKey = "ui.window.record_cue.dock";
constexpr const char* kWindowXKey = "ui.window.record_cue.x";
constexpr const char* kWindowYKey = "ui.window.record_cue.y";

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

bool parse_int(const core::StateReadResult& value, int& output)
{
  if (!value || value.value.empty()) return false;
  char* end = nullptr;
  const long parsed = std::strtol(value.value.c_str(), &end, 10);
  if (!end || *end != '\0') return false;
  output = static_cast<int>(parsed);
  return true;
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

bool RecordingCommandContext::remember_window_layout() const
{
  const auto value = project_state_.read(kStateNamespace, kRememberLayoutKey);
  if (!value) return false;
  return value.value == "1" || value.value == "true" || value.value == "yes";
}

RecordingWindowLayout RecordingCommandContext::load_window_layout() const
{
  RecordingWindowLayout layout;
  if (!remember_window_layout()) return layout;

  int value = 0;
  if (parse_int(project_state_.read(kStateNamespace, kWindowWidthKey), value) && value > 0)
    layout.width = value;
  if (parse_int(project_state_.read(kStateNamespace, kWindowHeightKey), value) && value > 0)
    layout.height = value;
  if (parse_int(project_state_.read(kStateNamespace, kWindowDockKey), value))
    layout.dock = value;

  bool has_x = parse_int(project_state_.read(kStateNamespace, kWindowXKey), layout.x);
  bool has_y = parse_int(project_state_.read(kStateNamespace, kWindowYKey), layout.y);
  layout.has_position = has_x && has_y;
  return layout;
}

bool RecordingCommandContext::save_window_layout(const RecordingWindowLayout& layout)
{
  if (!remember_window_layout()) return false;
  return project_state_.write(kStateNamespace, kWindowDockKey, "0") &&
    project_state_.write(kStateNamespace, kWindowXKey, std::to_string(layout.x)) &&
    project_state_.write(kStateNamespace, kWindowYKey, std::to_string(layout.y)) &&
    project_state_.write(kStateNamespace, kWindowWidthKey, std::to_string(layout.width)) &&
    project_state_.write(kStateNamespace, kWindowHeightKey, std::to_string(layout.height));
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
