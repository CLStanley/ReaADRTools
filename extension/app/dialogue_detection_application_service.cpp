#include "dialogue_detection_application_service.hpp"

namespace reaadr::reaper {

DialogueDetectionApplicationResult DialogueDetectionApplicationService::detect_and_generate(
  const DialogueDetectionOptions& detection_options,
  const core::DialogueCueGenerationOptions& cue_options,
  const SessionRenderOptions& render_options)
{
  DialogueDetectionApplicationResult result;
  result.detection = detect_dialogue_from_selected_media(project_, detection_api_, detection_options);
  if (!result.detection) {
    result.error = result.detection.error;
    return result;
  }
  if (result.detection.segments.empty()) {
    result.error = "No dialogue regions were detected with the current settings.";
    return result;
  }

  result.generation = cue_generation_.generate(result.detection.segments, cue_options, render_options);
  if (!result.generation) result.error = result.generation.error;
  return result;
}

} // namespace reaadr::reaper
