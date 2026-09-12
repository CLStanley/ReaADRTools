#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_GetProjExtState
#define REAPERAPI_WANT_GetUserInputs
#define REAPERAPI_WANT_SetProjExtState
#define REAPERAPI_WANT_ShowMessageBox

#include "dialogue_detection_command.hpp"

#include "dialogue_detection_adapter.hpp"
#include "native_host_services.hpp"
#include "overlay_refresh_adapter.hpp"
#include "../app/dialogue_cue_generation_application_service.hpp"
#include "../app/overlay_application_service.hpp"
#include "../reaadr_core/model_repository.hpp"

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

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

std::vector<std::string> split_csv(const std::string& text)
{
  std::vector<std::string> fields;
  std::size_t start = 0;
  while (true) {
    const std::size_t comma = text.find(',', start);
    fields.push_back(text.substr(start, comma == std::string::npos
      ? std::string::npos : comma - start));
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return fields;
}

bool parse_number(const std::string& text, double& output)
{
  errno = 0;
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (errno != 0 || !end || end == text.c_str()) return false;
  while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
  if (*end != '\0') return false;
  output = value;
  return true;
}

} // namespace

DialogueDetectionCommandResult run_dialogue_detection_command(ReaProject* project)
{
  DialogueDetectionCommandResult command;
  if (!GetProjExtState || !SetProjExtState || !GetUserInputs || !ShowMessageBox) {
    command.error = "Required REAPER project-state or dialog APIs are unavailable.";
    return command;
  }

  std::array<char, 1024> input = {};
  const std::string defaults = "ADR,-42,0.25,0.35,0.05";
  std::copy(defaults.begin(), defaults.end(), input.begin());
  if (!GetUserInputs("ReaADR Detect Dialogue", 5,
                     "Character,Threshold dB,Min Speech (s),Min Silence (s),Pad (s)",
                     input.data(), static_cast<int>(input.size()))) {
    command.cancelled = true;
    return command;
  }

  const auto fields = split_csv(input.data());
  if (fields.size() != 5) {
    command.error = "Dialogue detection settings must contain five values.";
    ShowMessageBox(command.error.c_str(), "ReaADR Detect Dialogue", 0);
    return command;
  }

  core::DialogueCueGenerationOptions cue_options;
  cue_options.character = fields[0];
  DialogueDetectionOptions detection_options;
  if (!parse_number(fields[1], detection_options.threshold_db) ||
      !parse_number(fields[2], detection_options.min_speech_seconds) ||
      !parse_number(fields[3], detection_options.min_silence_seconds) ||
      !parse_number(fields[4], detection_options.pad_seconds)) {
    command.error = "Threshold, minimum speech, minimum silence, and pad must be numeric values.";
    ShowMessageBox(command.error.c_str(), "ReaADR Detect Dialogue", 0);
    return command;
  }
  if (detection_options.min_speech_seconds < 0.0 ||
      detection_options.min_silence_seconds < 0.0 ||
      detection_options.pad_seconds < 0.0) {
    command.error = "Minimum speech, minimum silence, and pad cannot be negative.";
    ShowMessageBox(command.error.c_str(), "ReaADR Detect Dialogue", 0);
    return command;
  }

  const auto detection = detect_dialogue_from_selected_media(
    project, native_dialogue_detection_api(), detection_options);
  if (!detection) {
    command.error = detection.error;
    ShowMessageBox(command.error.c_str(), "ReaADR Detect Dialogue", 0);
    return command;
  }

  const auto preview_cues = core::build_dialogue_cues(detection.segments, cue_options);
  command.cue_count = preview_cues.size();
  if (preview_cues.empty()) {
    command.error = "No dialogue regions were detected with the current settings.";
    ShowMessageBox(command.error.c_str(), "ReaADR Detect Dialogue", 0);
    return command;
  }

  ProjectStateStore project_state(project, {GetProjExtState, SetProjExtState});
  core::SessionModelRepository repository(project_state);
  core::EventLogRepository event_log(project_state);
  core::CharacterFilterRepository character_filter(project_state);
  core::OverlaySettingsRepository overlay_settings(project_state);
  core::CueSelectionRepository cue_selection(project_state);

  const core::SessionLoadResult existing = repository.load();
  if (!existing && existing.error != core::SessionLoadError::missing) {
    command.error = core::session_load_error_message(existing);
    ShowMessageBox(command.error.c_str(), "ReaADR Detect Dialogue", 0);
    return command;
  }
  if (existing && !existing.model.cues.empty()) {
    std::ostringstream replacement;
    replacement << "The active ADR session already contains "
                << existing.model.cues.size()
                << " cue(s). Dialogue Detection will replace the canonical cue set with "
                << preview_cues.size() << " detected cue(s).\n\nContinue?";
    if (ShowMessageBox(replacement.str().c_str(), "ReaADR Detect Dialogue", 4) != 6) {
      command.cancelled = true;
      return command;
    }
  }

  std::ostringstream preview;
  preview << "Detected " << preview_cues.size() << " dialogue cue(s) from the selected media.\n\n";
  const std::size_t preview_count = std::min<std::size_t>(preview_cues.size(), 5);
  for (std::size_t i = 0; i < preview_count; ++i) {
    preview << preview_cues[i].at("id") << ": "
            << preview_cues[i].at("start_time") << " - "
            << preview_cues[i].at("end_time") << '\n';
  }
  if (preview_cues.size() > preview_count) preview << "...\n";
  preview << "\nGenerate the full ADR session from these detected cues?";
  if (ShowMessageBox(preview.str().c_str(), "ReaADR Detect Dialogue", 4) != 6) {
    command.cancelled = true;
    return command;
  }

  const OverlayApplicationApi overlay_api = {
    command_frame_rate, empty_overlay_selection, command_refresh_overlay,
  };
  OverlayApplicationService overlay_application(
    repository, overlay_settings, cue_selection, character_filter, overlay_api);

  SessionRenderService renderer(
    repository, event_log, character_filter, project,
    native_track_region_api(), native_ruler_lane_api(), native_cue_audio_api(),
    native_transaction_api());
  DialogueCueGenerationApplicationService generator(renderer);

  SessionRenderOptions render_options;
  render_options.cue_audio_path = native_project_cue_audio_path(project);
  render_options.undo_description = "ReaADR: detect dialogue from selected media";
  render_options.commit.snapshot_label = "Detect Dialogue From Selected Media";
  render_options.commit.utc_timestamp = native_utc_timestamp();
  render_options.commit.replacement.last_operation = "detect_dialogue";
  render_options.commit.replacement.build.frame_rate =
    std::to_string(native_project_frame_rate(project));
  render_options.event.source = "native_detect_dialogue";
  if (existing.error == core::SessionLoadError::missing) {
    render_options.commit.replacement.build.session_id =
      "native-dialogue-" + render_options.commit.utc_timestamp;
    render_options.commit.replacement.build.session_name = "Detected Dialogue";
  }
  render_options.refresh_overlay = [&overlay_application](std::string* error) {
    const auto refreshed = overlay_application.refresh();
    if (!refreshed && error) *error = refreshed.error;
    return static_cast<bool>(refreshed);
  };

  const auto generated = generator.generate(detection.segments, cue_options, render_options);
  if (!generated) {
    command.error = generated.error;
    ShowMessageBox(command.error.c_str(), "ReaADR Detect Dialogue", 0);
    return command;
  }

  std::ostringstream summary;
  summary << "Generated " << generated.cues.size() << " detected dialogue cue(s).\n\n"
          << "Tracks created: " << generated.rendered.render.tracks_and_regions.tracks_created << '\n'
          << "Regions created: " << generated.rendered.render.tracks_and_regions.regions_created << '\n'
          << "Cue-audio items created: " << generated.rendered.render.cue_audio.items_created;
  if (!generated.rendered.event_warning.empty())
    summary << "\n\nWarning: " << generated.rendered.event_warning;
  ShowMessageBox(summary.str().c_str(), "ReaADR Detect Dialogue (Native)", 0);
  return command;
}

} // namespace reaadr::reaper
