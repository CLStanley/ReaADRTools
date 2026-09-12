#pragma once

#include "dialogue_cue_generation_application_service.hpp"
#include "../reaadr_reaper/dialogue_detection_adapter.hpp"

struct ReaProject;

namespace reaadr::reaper {

struct DialogueDetectionApplicationResult {
  DialogueDetectionResult detection;
  DialogueCueGenerationApplicationResult generation;
  std::string error;

  explicit operator bool() const {
    return error.empty() && static_cast<bool>(detection) && static_cast<bool>(generation);
  }
};

// End-to-end native dialogue workflow below the host UI boundary. The host
// owns settings prompts and replacement confirmation; this service owns audio
// scanning, canonical cue construction, and transactional session rendering.
class DialogueDetectionApplicationService final {
public:
  DialogueDetectionApplicationService(SessionRenderService& renderer,
                                      ReaProject* project,
                                      DialogueDetectionApi detection_api)
    : cue_generation_(renderer), project_(project), detection_api_(detection_api) {}

  DialogueDetectionApplicationResult detect_and_generate(
    const DialogueDetectionOptions& detection_options,
    const core::DialogueCueGenerationOptions& cue_options,
    const SessionRenderOptions& render_options);

private:
  DialogueCueGenerationApplicationService cue_generation_;
  ReaProject* project_ = nullptr;
  DialogueDetectionApi detection_api_;
};

} // namespace reaadr::reaper
