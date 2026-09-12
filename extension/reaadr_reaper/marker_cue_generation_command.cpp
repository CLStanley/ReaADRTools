#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_GetProjExtState
#define REAPERAPI_WANT_SetProjExtState
#define REAPERAPI_WANT_ShowMessageBox

#include "marker_cue_generation_command.hpp"

#include "native_host_services.hpp"
#include "overlay_refresh_adapter.hpp"
#include "project_state.hpp"
#include "../app/marker_cue_generation_application_service.hpp"
#include "../app/overlay_application_service.hpp"
#include "../reaadr_core/model_repository.hpp"

#include <sstream>
#include <utility>

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
    "ReaADR: refresh video overlay");
  if (!refreshed && error) *error = refreshed.error;
  return static_cast<bool>(refreshed);
}

} // namespace

MarkerCueGenerationCommandResult run_marker_cue_generation_command(ReaProject* project)
{
  MarkerCueGenerationCommandResult command;
  if (!GetProjExtState || !SetProjExtState || !ShowMessageBox) {
    command.error = "Required REAPER project-state or message-box APIs are unavailable.";
    return command;
  }

  ProjectStateStore project_state(project, {GetProjExtState, SetProjExtState});
  core::SessionModelRepository repository(project_state);
  core::EventLogRepository event_log(project_state);
  core::CharacterFilterRepository character_filter(project_state);
  core::OverlaySettingsRepository overlay_settings(project_state);
  core::CueSelectionRepository cue_selection(project_state);

  const OverlayApplicationApi overlay_api = {
    command_frame_rate, empty_overlay_selection, command_refresh_overlay,
  };
  OverlayApplicationService overlay_application(
    repository, overlay_settings, cue_selection, character_filter, overlay_api);

  SessionRenderService renderer(
    repository, event_log, character_filter, project,
    native_track_region_api(), native_ruler_lane_api(), native_cue_audio_api(),
    native_transaction_api());
  MarkerCueGenerationApplicationService generator(
    renderer, project, native_marker_snapshot_api());

  core::MarkerCueGenerationOptions generation_options;
  const auto prepared = generator.preview(generation_options);
  if (!prepared) {
    command.error = prepared.error;
    ShowMessageBox(command.error.c_str(), "ReaADR Generate Cues", 0);
    return command;
  }
  command.cue_count = prepared.cues.size();

  const core::SessionLoadResult existing = repository.load();
  if (!existing && existing.error != core::SessionLoadError::missing) {
    command.error = core::session_load_error_message(existing);
    ShowMessageBox(command.error.c_str(), "ReaADR Generate Cues", 0);
    return command;
  }
  if (existing && !existing.model.cues.empty()) {
    std::ostringstream replacement;
    replacement << "The active ADR session already contains "
                << existing.model.cues.size()
                << " cue(s). Generate Cues will replace the canonical cue set "
                   "with " << prepared.cues.size()
                << " cue(s) collected from project markers/regions.\n\nContinue?";
    if (ShowMessageBox(replacement.str().c_str(), "ReaADR Generate Cues", 4) != 6) {
      command.cancelled = true;
      return command;
    }
  }

  std::ostringstream confirmation;
  confirmation << "Generate the full ADR session from " << prepared.cues.size()
               << " marker/region cue(s)?\n\nThis rebuilds ReaADR-owned cue tracks, "
                  "regions, cue audio, character filtering, and video overlays.";
  if (ShowMessageBox(confirmation.str().c_str(), "ReaADR Generate Cues", 4) != 6) {
    command.cancelled = true;
    return command;
  }

  SessionRenderOptions render_options;
  render_options.cue_audio_path = native_project_cue_audio_path(project);
  render_options.undo_description = "ReaADR: generate cues from markers/regions";
  render_options.commit.snapshot_label = "Generate Cues from Markers/Regions";
  render_options.commit.utc_timestamp = native_utc_timestamp();
  render_options.commit.replacement.last_operation = "generate_cues_from_selection";
  render_options.commit.replacement.build.frame_rate =
    std::to_string(native_project_frame_rate(project));
  render_options.event.source = "native_generate_cues";
  if (existing.error == core::SessionLoadError::missing) {
    render_options.commit.replacement.build.session_id =
      "native-markers-" + render_options.commit.utc_timestamp;
    render_options.commit.replacement.build.session_name = "Markers/Regions";
  }
  render_options.refresh_overlay = [&overlay_application](std::string* error) {
    const auto refreshed = overlay_application.refresh();
    if (!refreshed && error) *error = refreshed.error;
    return static_cast<bool>(refreshed);
  };

  const auto generated = generator.render_prepared(std::move(prepared), render_options);
  if (!generated) {
    command.error = generated.error;
    ShowMessageBox(command.error.c_str(), "ReaADR Generate Cues", 0);
    return command;
  }

  std::ostringstream summary;
  summary << "Generated " << generated.cues.size() << " cue(s).\n\n"
          << "Tracks created: " << generated.rendered.render.tracks_and_regions.tracks_created << '\n'
          << "Regions created: " << generated.rendered.render.tracks_and_regions.regions_created << '\n'
          << "Cue-audio items created: " << generated.rendered.render.cue_audio.items_created;
  if (!generated.rendered.event_warning.empty())
    summary << "\n\nWarning: " << generated.rendered.event_warning;
  ShowMessageBox(summary.str().c_str(), "ReaADR Generate Cues (Native)", 0);
  return command;
}

} // namespace reaadr::reaper
